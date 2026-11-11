#!/usr/bin/env python3
"""Render BTstack public header declarations as a structured API reference."""
import re
import sys
import textwrap
from pathlib import Path

def comment_before(lines, line_number):
    """Read the documentation block immediately preceding a declaration."""
    index = line_number - 2
    while index >= 0 and not lines[index].strip():
        index -= 1
    if index < 0 or not lines[index].strip().endswith('*/'):
        return []
    end = index
    while index >= 0 and not re.match(r'\s*/\*\*', lines[index]):
        index -= 1
    if index < 0:
        return []
    return [re.sub(r'^\s*\*\s?', '', line).rstrip() for line in lines[index + 1:end]]


def parse_comment(lines):
    sections = []
    current = None
    for line in lines:
        match = re.match(r'@(\w+)\s*:?[ \t]*(.*)', line)
        if match:
            current = [match.group(1), match.group(2)]
            sections.append(current)
        elif current is not None:
            current[1] += '\n' + line
        elif line.strip():
            sections.append(['text', line])
    return [(tag, textwrap.dedent(text).strip()) for tag, text in sections]


def render_function(out, name, signature, line_number, lines, source_url, level, in_toc=True):
    sections = parse_comment(comment_before(lines, line_number))
    if in_toc:
        out.write('#' * level + ' ' + name + ' {#api:' + name + '}\n\n')
    else:
        out.write('<a id="api:' + name + '"></a>\n\n**' + name + '**\n\n')
    out.write('    ' + signature.replace('\n', '\n    ') + '\n\n')

    for tag, text in sections:
        if not text or tag in ('param', 'return', 'result'):
            continue
        if tag == 'note' and text.startswith('btstack_type '):
            continue  # Generator metadata, not a user-facing note.
        if tag in ('brief', 'text'):
            out.write(text + '\n\n')
        elif tag == 'note':
            # Comment continuation lines are often indented for readability.
            paragraphs = re.split(r'\n\s*\n', text)
            out.write('**Note:** ' + '\n\n'.join(' '.join(line.strip() for line in p.splitlines()) for p in paragraphs) + '\n\n')
        else:
            out.write('**@' + tag + ':** ' + text + '\n\n')

    params = [text for tag, text in sections if tag == 'param' and text]
    if params:
        out.write('**Parameters**\n\n')
        for param in params:
            param_name, _, description = param.partition(' ')
            description = ' '.join(description.split())
            out.write('- ' + param_name + (': ' + description if description else '') + '\n')
        out.write('\n')

    return_type = signature.split(name, 1)[0].strip()
    return_type = re.sub(r'^(?:(?:static|inline|extern)\s+)+', '', return_type)
    if return_type != 'void':
        returns = [text for tag, text in sections if tag in ('return', 'result') and text]
        out.write('**Returns**\n\n- ' + return_type)
        if returns:
            out.write(': ' + ' '.join(' '.join(text.split()) for text in returns))
        out.write('\n\n')
    out.write('[Source](' + source_url + '#L' + str(line_number) + ')\n\n---\n\n')



def render_header(out, path, source_url, level, group_events=False):
    """Render functions and preserve other public declarations as C excerpts."""
    lines = Path(path).read_text().splitlines()
    in_api = False
    event_prefixes = []
    current_event = None
    if group_events:
        root = Path(path).resolve().parents[1]
        sys.path.insert(0, str(root / 'tool'))
        import btstack_parser
        btstack_parser.set_btstack_root(str(root))
        events, subevents, _ = btstack_parser.parse_events()
        for _, event_name, _, _ in events + subevents:
            prefix = event_name.lower()
            if 'event' not in prefix:
                prefix += '_event'
            event_prefixes.append((prefix + '_', event_name))
        event_prefixes.sort(key=lambda entry: len(entry[0]), reverse=True)
    pending = []
    index = 0

    def flush():
        while pending and not pending[0].strip():
            pending.pop(0)
        while pending and not pending[-1].strip():
            pending.pop()
        if pending:
            out.write('\n'.join('    ' + line for line in pending) + '\n\n')
            pending.clear()

    while index < len(lines):
        line = lines[index]
        if not in_api:
            if 'API_START' in line:
                in_api = True
            index += 1
            continue
        if 'API_END' in line:
            break

        # Functions start with a return type and name. Collect multiline
        # declarations without treating callback typedefs as functions.
        match = re.match(r'^\s*(?!typedef\b)(?!return\b)(?!#)(?:[A-Za-z_]\w*[ \t*]+)+([A-Za-z_]\w*)\s*\(', line)
        if match:
            name = match.group(1)
            signature_lines = [line.strip()]
            end = index
            while not re.search(r'[;{]', signature_lines[-1]) and end + 1 < len(lines):
                end += 1
                signature_lines.append(lines[end].strip())
            signature = '\n'.join(signature_lines)
            if re.search(r'[;{]', signature):
                # Remove the immediately preceding documentation block from
                # the raw excerpt: render_function reads it from the header.
                if comment_before(lines, index + 1):
                    while pending and not pending[-1].strip():
                        pending.pop()
                    if pending and pending[-1].strip().endswith('*/'):
                        while pending:
                            previous = pending.pop()
                            if re.match(r'\s*/\*\*', previous):
                                break
                flush()
                is_body = '{' in signature
                signature = signature.split('{', 1)[0].rstrip()
                if not signature.endswith(';'):
                    signature += ';'
                if group_events:
                    event = next((macro for prefix, macro in event_prefixes
                                  if name.startswith(prefix)), None)
                    if event and event != current_event:
                        out.write('#' * level + ' ' + event + ' {#event:' + event + '}\n\n')
                        current_event = event
                render_function(out, name, signature, index + 1, lines, source_url, level,
                                in_toc=not group_events)
                index = end + 1
                if is_body:
                    depth = lines[end].count('{') - lines[end].count('}')
                    while depth > 0 and index < len(lines):
                        depth += lines[index].count('{') - lines[index].count('}')
                        index += 1
                continue
        pending.append(line)
        index += 1
    flush()

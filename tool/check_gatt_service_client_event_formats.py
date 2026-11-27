#!/usr/bin/env python3
"""Check the formats of events forwarded by the GATT service client.

Run from any directory. The checker follows same-file helper calls back to a
GATTSERVICE_SUBEVENT_CLIENT_* case. Ambiguous or unsupported C constructs are
reported as errors instead of silently skipping a replacement.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPLACE = 'gatt_service_client_replace_subevent_id_and_emit'
IDENT = r'[A-Za-z_]\w*'
FUNCTION = re.compile(r'\b(' + IDENT + r')\s*\([^;{}]*\)\s*\{')
CALL = re.compile(r'\b(' + IDENT + r')\s*\(([^()]*)\)')
CASE = re.compile(r'\bcase\s+(' + IDENT + r')\s*:|\bdefault\s*:|\bbreak\s*;')
COMMENT_OR_STRING = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def strip_comments_and_strings(text):
    return COMMENT_OR_STRING.sub(lambda m: re.sub(r'[^\n]', ' ', m.group()), text)


def functions(text):
    """Return function bodies and offsets, preserving source positions."""
    result = []
    pos = 0
    while True:
        match = FUNCTION.search(text, pos)
        if not match:
            return result
        depth = 1
        end = match.end()
        while end < len(text) and depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        if depth:
            raise ValueError('unclosed function body')
        result.append((match.group(1), match.end(), text[match.end():end - 1]))
        pos = end


def source_at(body, position):
    """Find the enclosing event case conservatively within a function."""
    last = None
    for match in CASE.finditer(body, 0, position):
        last = match.group(1)
    if last and last.startswith('GATTSERVICE_SUBEVENT_CLIENT_'):
        return last
    return None


def check_file(path, formats):
    text = strip_comments_and_strings(path.read_text(encoding='utf-8'))
    bodies = functions(text)
    callers = {}
    replacements = []
    for name, offset, body in bodies:
        for match in CALL.finditer(body):
            called, arguments = match.groups()
            source = source_at(body, match.start())
            if called == REPLACE:
                replacements.append((name, offset + match.start(), arguments, source))
            else:
                callers.setdefault(called, []).append((name, source))

    def sources(name, visited):
        if name in visited:
            return set(), True
        visited = visited | {name}
        found = set()
        unknown = False
        sites = callers.get(name, [])
        if not sites:
            return found, True
        for caller, source in sites:
            if source:
                found.add(source)
            else:
                inherited, unresolved = sources(caller, visited)
                found.update(inherited)
                unknown |= unresolved
        return found, unknown

    errors = 0
    for name, offset, arguments, source in replacements:
        line = text.count('\n', 0, offset) + 1
        location = f'{path.relative_to(ROOT)}:{line}'
        args = [arg.strip() for arg in arguments.split(',')]
        if len(args) != 4 or not re.fullmatch(IDENT, args[-1]):
            print(f'{location}: error: cannot determine replacement event')
            errors += 1
            continue
        target = args[-1]
        origins, unknown = ({source}, False) if source else sources(name, set())
        if unknown or not origins:
            print(f'{location}: error: cannot determine original event for {target}')
            errors += 1
        for origin in sorted(origins):
            if origin not in formats or target not in formats:
                print(f'{location}: error: missing event format for {origin} or {target}')
                errors += 1
            elif formats[origin] != formats[target]:
                print(f'{location}: error: {origin} format {formats[origin]} != {target} format {formats[target]}')
                errors += 1
    # A call outside a recognized function or with nested arguments must not disappear.
    occurrences = len(re.findall(r'\b' + REPLACE + r'\s*\(', text))
    # Exclude the definition in gatt_service_client.c.
    occurrences -= sum(1 for name, _, _ in bodies if name == REPLACE)
    if occurrences != len(replacements):
        print(f'{path.relative_to(ROOT)}: error: could not parse every replacement call')
        errors += 1
    return len(replacements), errors


def main():
    header = (ROOT / 'src/btstack_defines.h').read_text(encoding='utf-8')
    formats = {}
    for match in re.finditer(r'/\*\*(.*?)\*/\s*#define\s+(' + IDENT + r')\b', header, re.S):
        fmt = re.search(r'@format\s+(\S+)', match.group(1))
        if fmt:
            formats[match.group(2)] = fmt.group(1)
    count = errors = 0
    for path in sorted((ROOT / 'src').rglob('*.c')):
        if REPLACE not in path.read_text(encoding='utf-8'):
            continue
        checked, failures = check_file(path, formats)
        count += checked
        errors += failures
    print(f'Checked {count} replacement call(s); {errors} error(s).')
    return int(errors != 0)


if __name__ == '__main__':
    sys.exit(main())

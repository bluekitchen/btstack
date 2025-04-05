#!/usr/bin/env python3

import os, sys, getopt, re

class State:
    SearchTitle = 0
    SearchEndTitle = 1
    SearchStartAPI = 2
    SearchEndAPI = 3
    DoneAPI = 4


def isEndOfComment(line):
    parts = re.match(r'\s*\*\/.*', line)
    if parts:
        return True
    return False


def isStartOfComment(line):
    parts = re.match(r'\s*\/\*\*.*', line)
    if parts:
        return True
    return False


def isComment(line):
    parts = re.match(r'\s*\/\/.*', line)
    if parts:
        return True
    parts = re.match(r'\s*\*.*', line)
    if parts:
        return True

    return isStartOfComment(line) or isEndOfComment(line)


def isTypedefStart(line):
    return re.match(r'.*typedef\s+struct.*', line)


def filename_stem(filepath):
    return os.path.splitext(os.path.basename(filepath))[0]


def clean_text(text):
    return re.sub(r'\s+', ' ', text).strip()


def validate_function(name, text):
    if text.startswith('#define'):
        print("Error: function name with #define: ", name)
        return False
    if name in ['PBAP_FEATURES_NOT_PRESENT', 'PROFILE_FEATURES_NOT_PRESENT', 'Q7_TO_FLOAT', '_attribute__']:
        print("Error: function name with reserved name: ", name)
        return False
    if '\t' in name:
        print("Error: function name with tabs: ", name)
        return False
    if "PGM_" in text:
        return False
    return True


def parse_header(header_filepath):
    '''
    Parses a header file to extract function definitions and typedefs.
    :param path:
    :return: (title, functions, typedefs)

    This function reads a header file, identifies function definitions and typedefs.
    It returns the file @title if found and two dictionaries:
    - functions: A dictionary where keys are function names and values are (function signature, line number, is_api).
    - typedefs: A dictionary where keys are typedef names and values are their definitions.

    '''

    functions = {}
    typedefs = {}
    title = None
    state = State.SearchStartAPI
    multiline_function_name = None
    multiline_function_text = ''
    typedefFound = False
    typedef_text = ''
    multiline_function_terminator = ''
    is_api = False

    with open(header_filepath, 'rt') as fin:
        for line_num, line in enumerate(fin, start=1):

            if multiline_function_name:
                multiline_function_text += "\n" + clean_text(line)
                if multiline_function_terminator in line:
                    if validate_function(multiline_function_name, multiline_function_text):
                        functions[multiline_function_name] = (multiline_function_text, line_num, is_api)
                    multiline_function_name = None
                continue

            if line.startswith('#define'):
                continue

            if state == State.SearchStartAPI:
                parts = re.match(r'.*API_START.*', line)
                if parts:
                    state = State.SearchEndAPI
                    is_api = True
                continue

            if state == State.SearchEndAPI:
                parts = re.match(r'.*API_END.*', line)
                if parts:
                    state = State.DoneAPI
                    is_api = False
                    continue

            if isComment(line):
                continue

            param = re.match(r".*@brief.*", line)
            if param:
                continue
            param = re.match(r".*@param.*", line)
            if param:
                continue
            param = re.match(r".*@return.*", line)
            if param:
                continue
            param = re.match(r".*@result.*", line)
            if param:
                continue
            param = re.match(r".*@note.*", line)
            if param:
                continue
            param = re.match(r".*return.*", line)
            if param:
                continue

            # search typedef struct begin
            if isTypedefStart(line):
                typedefFound = True

            # search typedef struct end
            if typedefFound:
                typedef = re.match(r'}\s*(.*);\n', line)
                if typedef:
                    typedefFound = False
                    typdefef_name = typedef.group(1)
                    typedefs[typdefef_name] = typedef_text
                    typedef_text = ''
                continue

            # filter callback
            callback_function_definition = re.match(r'(.*?)\s*\(\s*\*.*\(*.*;\n', line)
            if callback_function_definition:
                continue

            one_line_function_definition = re.match(r'(.*?)\s*\(.*\(*.*;\n', line)
            if one_line_function_definition:
                parts = one_line_function_definition.group(1).split(" ")
                name = parts[len(parts) - 1].replace('*', '')
                if len(name) == 0:
                    print(parts)
                    sys.exit(10)
                # ignore typedef for callbacks
                if parts[0] == 'typedef':
                    continue
                text = clean_text(line)
                if validate_function(name, text):
                    functions[name] = (text, line_num, is_api)
                continue

            multi_line_function_definition = re.match(r'.(.*?)\s*\(.*\(*.*', line)
            if multi_line_function_definition:
                parts = multi_line_function_definition.group(1).split(" ")
                name = parts[len(parts) - 1].replace('*', '')
                if len(name) == 0:
                    print(parts)
                    sys.exit(10)
                multiline_function_name = name
                multiline_function_text = clean_text(line)
                if line.startswith('static inline'):
                    multiline_function_terminator = '};'
                else:
                    multiline_function_terminator = ';'


    return (title, functions, typedefs)


def dump_typedefs(typedefs):
    for typedef in sorted(typedefs.keys()):
        print(typedef, "--", typedefs[typedef])


def dump_functions(functions):
    for function in sorted(functions.keys()):
        (text, linenr, is_api) = functions[function]
        print('--', function, "--")
        print(linenr, text, '\n')


files_to_ignore = []

def main(btstack_root):
    btstack_srcfolder = btstack_root + "/src/"

    print('BTstack src:  ' + btstack_srcfolder)

    # create a dictionary of header files {file_path : []}
    header_files = {}
    for root, dirs, files in os.walk(btstack_srcfolder, topdown=True):
        for f in files:
            if not f.endswith(".h"):
                continue
            if not root.endswith("/"):
                root = root + "/"
            if f in files_to_ignore:
                continue
            header_filepath = root + f
            with open(header_filepath, 'rt') as fin:
                header_files[header_filepath] = []

    for header_filepath in sorted(header_files.keys()):
        # get filename without path
        (title, functions, typedefs) = parse_header(header_filepath)
        print("\n\n==== Parsing file: ", header_filepath)
        print("Title: ", title)
        dump_functions(functions)
        # dump_typedefs(typedefs)

if __name__ == "__main__":
    btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../..')
    main(btstack_root)
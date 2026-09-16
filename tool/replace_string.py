#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2025

# This script replaces a string in a file with another string, introduced to create Ozone project files from CMakeLists.txt

import sys

def replace_string(input_file, output_file, search_string, replacement_string):
    with open(input_file, 'r') as infile:
        content = infile.read()
    modified_content = content.replace(search_string, replacement_string)
    with open(output_file, 'w') as outfile:
        outfile.write(modified_content)

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python3 replace_string.py <input_file> <output_file> <search_string> <replacement_string>")
        sys.exit(1)

    replace_string(*sys.argv[1:])

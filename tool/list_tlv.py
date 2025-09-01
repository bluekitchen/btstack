#!/usr/bin/env python3

# list @tlv tags in BTstack header files

import os
import re
import sys

def find_tlv_tags(root_dir):
    tlv_entries = []
    
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith(".h"):
                file_path = os.path.join(subdir, file)
                with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                    for line_number, line in enumerate(f, start=1):
                        if "@TLV" in line:
                            tlv_entries.append((file_path, line_number, line.strip()))
    
    return tlv_entries

def main():
    btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
    results = find_tlv_tags(btstack_root + '/src')
    
    if results:
        print("Found the following @TLV tags:")
        for file_path, line_number, line in results:
            file_path_local = file_path.replace(btstack_root,'')
            tlv = line.replace('* @TLV', '')
            print(f"{file_path_local:40} {tlv}")
    else:
        print("No @TLV tags found.")

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
import os
import re
import sys

# Regex to match: hci_send_cmd(&HCI_SOME_COMMAND, ...)
pattern = re.compile(r"hci_send_cmd\s*\(\s*\&([A-Za-z0-9_]+)")

def scan_folder(path):
    result = set()
    for root, _, files in os.walk(path):
        for f in files:
            if f.endswith(".c"):
                full_path = os.path.join(root, f)
                with open(full_path, "r", errors="ignore") as fp:
                    for line in fp:
                        match = pattern.search(line)
                        if match:
                            result.add(match.group(1))
    return result

if __name__ == "__main__":
    if len(sys.argv) == 1:
        btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
        src_folder = os.path.join(btstack_root, 'src')
    else:
        src_folder = sys.argv[1]

    print("Scanning {}\nused commands:\n\n".format(src_folder))
    cmd_set = scan_folder(src_folder)
    for cmd in sorted(cmd_set):
        print(cmd)

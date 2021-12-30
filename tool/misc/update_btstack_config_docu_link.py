#!/usr/bin/env python3

import os
import sys
import re

config_header_template = """// btstack_config.h for PORT_NAME port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
"""

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../../port/')

def write_configuration(full_path, configuration):
    if configuration:
        with open(full_path, "wb") as fout:
            bytes = configuration.encode('utf-8')
            fout.write(bytes)

def get_line_ending(full_path):
    with open(full_path, "r", newline='') as fin:
        line = fin.readline()
        if line.endswith('\r\n'):
            return '\r\n'
        if line.endswith('\r'):
            return '\r'
        return '\n'

def read_and_update_configuration(full_path, line_ending, root):
    configuration = ""

    port_name = root.split("port/")[1].split("/")[0]
    if port_name.startswith("archive"):
        return

    with open(full_path, "rt") as fin:
        for unstripped_line in fin:
            line = unstripped_line.strip()

            parts = re.match('(//\s*btstack_config.h\s)(\w*)', line)
            if parts and len(parts.groups()) == 2:
                configuration += config_header_template.replace("PORT_NAME", port_name).replace("\r\n", line_ending)
            else:
                configuration += unstripped_line
    return configuration

for root, dirs, files in os.walk(btstack_root, topdown=True):
    for f in files:
        if f.endswith("btstack_config.h"):
            config_file = root + "/" + f
            line_ending = get_line_ending(config_file)
            configuration = read_and_update_configuration(config_file, line_ending, root)
            write_configuration(config_file, configuration)

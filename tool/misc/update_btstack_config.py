#!/usr/bin/env python3
import fileinput
import os
import re
import sys

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../../')

defines_to_add = []
defines_to_remove = []

# dictionary with existing lines as keys, and new text as value
lines_to_replace = {}

def get_line_ending(full_path):
    with open(full_path, "r", newline='') as fin:
        line = fin.readline()
        if line.endswith('\r\n'):
            return '\r\n'
        if line.endswith('\r'):
            return '\r'
        return '\n'


def configuration_from_block(block, line_ending):
    global defines_to_add, defines_to_remove
    configuration = ""
    
    if len(block) == 0:
        return ""

    for item in defines_to_remove:
        if item in block:
            block.remove(item)

    for item in temp_defines_to_add:
        if item in block:
            temp_defines_to_add.remove(item)
            continue

        if len(block) > 0:
            prefix = block[0].split("_")[0]
            if item.startswith(prefix):
                block.append(item)
                temp_defines_to_add.remove(item)
    
    block.sort()
    for item in block:
        configuration += ("#define %s%s" % (item, line_ending))

    configuration += ("%s" % (line_ending))
    return configuration

def read_and_update_configuration(full_path, line_ending):
    global lines_to_replace, temp_defines_to_add

    configuration = ""
    block = []
    temp_defines_to_add = defines_to_add.copy()
    
    with open(full_path, "rt") as fin:
        for unstripped_line in fin:
            line = unstripped_line.strip()

            if not line:
                if len(block) == 0:
                    # we have newline to deal with
                    configuration += line_ending
                else:
                    configuration += configuration_from_block(block, line_ending)
                block = []
            else:
                if line in lines_to_replace.keys():
                    configuration += ("%s%s" % (lines_to_replace[line], line_ending))
                    continue

                parts = re.match("#define\\s*(.*)", line)
                if parts:
                    block.append(parts[1])
                else:
                    # if commented code follows directly block, process block, and start a new one
                    configuration += configuration_from_block(block, line_ending)
                    block = []
                    configuration += ("%s%s" % (line, line_ending))
    
    if len(temp_defines_to_add) > 0:
        print("Cannot add defines: " % temp_defines_to_add)
        sys.exit(10)

    # if end of file could not be detected, process last block
    configuration += configuration_from_block(block, line_ending)
    return configuration

def write_configuration(full_path, config_file):
    with open(full_path, "wb") as fout:
        bytes = configuration.encode('utf-8')
        fout.write(bytes)


for root, dirs, files in os.walk(btstack_root, topdown=True):
    for f in files:
        if f.endswith("btstack_config.h"):
            config_file = root + "/" + f
            line_ending = get_line_ending(config_file)
            configuration = read_and_update_configuration(config_file, line_ending)
            write_configuration(config_file, configuration)

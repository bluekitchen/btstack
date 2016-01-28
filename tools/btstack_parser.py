#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

import re

# paths
hci_cmds_h_path = 'include/btstack/hci_cmds.h'
hci_cmds_c_path = 'src/hci_cmds.c'
hci_h_path      = 'src/hci.h'

btstack_root = '..'

def set_btstack_root(path):
    btstack_root = path

def cap(x):
    if x.lower() == 'btstack':
        return 'BTstack'
    acronyms = ['GAP', 'GATT', 'HCI', 'L2CAP', 'LE', 'RFCOMM', 'SM', 'SDP', 'UUID16', 'UUID128']
    if x.upper() in acronyms:
        return x.upper()
    return x.capitalize()

def camel_case(name):
    return ''.join(map(cap, name.split('_')))

def camel_case_var(name):
    if name in ['uuid128', 'uuid16']:
        return name
    camel = camel_case(name)
    return camel[0].lower() + camel[1:]

def read_defines(infile):
    defines = dict()
    with open (infile, 'rt') as fin:
        for line in fin:
            parts = re.match('#define\s+(\w+)\s+(\w*)',line)
            if parts and len(parts.groups()) == 2:
                (key, value) = parts.groups()
                defines[key] = value
    return defines

def parse_defines():
    global btstack_root
    defines = dict()
    defines.update(read_defines(btstack_root + '/' + hci_cmds_h_path))
    defines.update(read_defines(btstack_root + '/' + hci_h_path))
    return defines

def my_parse_events(path):
    events = []
    le_events = []
    params = []
    event_types = set()
    format = None
    with open (path, 'rt') as fin:
        for line in fin:
            parts = re.match('.*@format\s*(\w*)\s*', line)
            if parts and len(parts.groups()) == 1:
                format = parts.groups()[0]
            parts = re.match('.*@param\s*(\w*)\s*', line)
            if parts and len(parts.groups()) == 1:
                param = parts.groups()[0]
                params.append(param)
            parts = re.match('\s*#define\s+(\w+)\s+(\w*)',line)
            if parts and len(parts.groups()) == 2:
                (key, value) = parts.groups()
                if format != None:
                    if key.lower().startswith('hci_subevent_'):
                        le_events.append((value, key.lower().replace('hci_subevent_', 'hci_event_'), format, params))
                    else:
                        events.append((value, key, format, params))
                    event_types.add(key)
                params = []
                format = None
    return (events, le_events, event_types)

def parse_events():
    global btstack_root
    return my_parse_events(btstack_root + '/' + hci_cmds_h_path)

def my_parse_commands(infile):
    commands = []
    with open (infile, 'rt') as fin:

        params = []
        for line in fin:

            parts = re.match('.*@param\s*(\w*)\s*', line)
            if parts and len(parts.groups()) == 1:
                param = parts.groups()[0]
                params.append(camel_case_var(param))
                continue

            declaration = re.match('const\s+hci_cmd_t\s+(\w+)[\s=]+', line)
            if declaration:
                command_name = camel_case(declaration.groups()[0])
                if command_name.endswith('Cmd'):
                    command_name = command_name[:-len('Cmd')]
                continue

            definition = re.match('\s*OPCODE\\(\s*(\w+)\s*,\s+(\w+)\s*\\)\s*,\s\\"(\w*)\\".*', line)
            if definition:
                (ogf, ocf, format) = definition.groups()
                if len(params) != len(format):
                    params = []
                    arg_counter = 1
                    for f in format:
                        arg_name = 'arg%u' % arg_counter
                        params.append(arg_name)
                        arg_counter += 1
                commands.append((command_name, ogf, ocf, format, params))
                params = []
                continue
    return commands

def parse_commands():
    global btstack_root
    return my_parse_commands(btstack_root + '/' + hci_cmds_c_path)

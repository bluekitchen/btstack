#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2014

import re
import os
import sys

# paths
bluetooth_h_path = 'src/bluetooth.h'
btstack_defines_h_path = 'src/btstack_defines.h'
hci_cmds_c_path = 'src/hci_cmd.c'
hci_cmds_h_path = 'src/hci_cmd.h'
hci_h_path = 'src/hci.h'

open_bracket = '['
closing_bracket = ']'

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
print ("BTstack root %s" % btstack_root)

def set_btstack_root(path):
    global btstack_root
    btstack_root = path

def assert_dir(path):
    if not os.access(path, os.R_OK):
        os.makedirs(path)

def resolve_path(path):
    if path is None:
        return None
    if os.path.isabs(path):
        return path
    return os.path.join(btstack_root, path)

def cap(x):
    if x.lower() == 'btstack':
        return 'BTstack'
    acronyms = ['ATT', 'GAP', 'GATT', 'HCI', 'L2CAP', 'LE', 'RFCOMM', 'SM', 'SDP', 'UUID16', 'UUID128', 'HSP', 'HFP', 'ANCS']
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
            parts = re.match(r'#define\s+(\w+)\s+(\w*)[u]',line)
            if parts and len(parts.groups()) == 2:
                (key, value) = parts.groups()
                defines[key] = value.lower()
    return defines

def parse_defines(extra_header_path=None):
    global btstack_root
    defines = dict()
    defines.update(read_defines(resolve_path(hci_cmds_h_path)))
    defines.update(read_defines(resolve_path(hci_h_path)))
    defines.update(read_defines(resolve_path(bluetooth_h_path)))
    defines.update(read_defines(resolve_path(btstack_defines_h_path)))
    if extra_header_path is not None:
        defines.update(read_defines(resolve_path(extra_header_path)))
    return defines

def my_parse_events(path):
    events = []
    subevents = []
    params = []
    event_types = set()
    format = None
    format_pattern = re.compile(r'^\s*\*\s*@format\s*([a-zA-Z0-9_' + re.escape(open_bracket) + re.escape(closing_bracket) + r']*)\s*.*$')
    param_pattern  = re.compile(r'^\s*\*\s*@param\s*(\w*)\s*')
    array_pattern  = re.compile(r'^\s*\*\s*@array\s*(\w*)\s*')
    field_pattern  = re.compile(r'^\s*\*\s*@field\s*(\w*)\s*')
    with open (path, 'rt') as fin:
        for line in fin:
            parts = format_pattern.match(line)
            if parts and len(parts.groups()) == 1:
                format = parts.groups()[0]
            parts = param_pattern.match(line)
            if parts and len(parts.groups()) == 1:
                param = parts.groups()[0]
                params.append(param)
            parts = array_pattern.match(line)
            if parts and len(parts.groups()) == 1:
                array = parts.groups()[0]
                params.append(array)
            parts = field_pattern.match(line)
            if parts and len(parts.groups()) == 1:
                field = parts.groups()[0]
                params.append(field)
            parts = re.match(r'\s*#define\s+(\w+)\s+(\w*)[u]',line)
            if parts and len(parts.groups()) == 2:
                (key, value) = parts.groups()
                if format != None:
                    # renaming needed by Java Binding (... subevents are just enumerated with others due to event factory)
                    if "_subevent_" in key.lower():
                        subevents.append((value, key, format, params))
                    else:
                        events.append((value, key, format, params))
                    event_types.add(key)
                    params = []
                    format = None
    return (events, subevents, event_types)

def parse_events():
    global btstack_root
    
    # parse bluetooth.h to get used events
    (bluetooth_events, bluetooth_subevents, bluetooth_event_types) = my_parse_events(resolve_path(bluetooth_h_path))

    # parse btstack_defines to get events
    (btstack_events, btstack_subevents, btstack_event_types) = my_parse_events(resolve_path(btstack_defines_h_path))

    # concat lists
    (events, subvents, event_types) = (bluetooth_events + btstack_events, bluetooth_subevents + btstack_subevents, bluetooth_event_types | btstack_event_types)

    return (events, subvents, event_types)
def my_parse_opcodes(infile, convert_to_camel_case):
    opcodes = {}
    with open (infile, 'rt') as fin:
        for line in fin:
            definition = re.match(r'\s*((?:BTSTACK|DAEMON)_OPCODE_\w+)\s*=\s*(?:BTSTACK|DAEMON)_OPCODE\s*\(\s*(\w+)\s*\).*', line)
            if definition:
                (opcode, extra_ocf) = definition.groups()
                opcodes[opcode] = ('OGF_BTSTACK', extra_ocf)
            definition = re.match(r'\s*(HCI_OPCODE_\w+)\s*=\s*HCI_OPCODE\s*\(\s*(\w+)\s*,\s*(\w+)\s*\).*', line)
            if definition:
                (opcode, ogf, ocf) = definition.groups()
                # opcodes.append((opcode, ogf, ocf))
                opcodes[opcode] = (ogf, ocf)
    return opcodes

def parse_opcodes(camel_case=True, extra_header_path=None):
    global btstack_root
    opcodes = {}
    opcodes.update(my_parse_opcodes(resolve_path(hci_cmds_h_path), camel_case))
    if extra_header_path is not None:
        opcodes.update(my_parse_opcodes(resolve_path(extra_header_path), camel_case))
    return opcodes

def my_parse_commands(infile, opcodes, convert_to_camel_case):
    commands = []
    param_pattern = re.compile(r'^\s*\*\s*@param\s*(\w*)\s*')
    with open (infile, 'rt') as fin:

        params = []
        for line in fin:

            parts = param_pattern.match(line)
            if parts and len(parts.groups()) == 1:
                param = parts.groups()[0]
                if convert_to_camel_case:
                    param = camel_case_var(param)
                else:
                    param = param.lower()
                params.append(param)
                continue

            declaration = re.match(r'const\s+hci_cmd_t\s+(\w+)[\s=]+', line)
            if declaration:
                command_name = declaration.groups()[0]
                # drop _cmd suffix for BTstack commands
                if command_name.endswith('_cmd'):
                    command_name = command_name[:-len('_cmd')]
                if convert_to_camel_case:
                    command_name = camel_case(command_name)
                else:
                    command_name = command_name.lower()
                continue

            # HCI_OPCODE or extra BTstack opcode definition
            definition = re.match(r'\s*(HCI_OPCODE_\w+|(?:BTSTACK|DAEMON)_OPCODE_\w+)\s*,\s\"(\w*)\".*', line)
            if definition:
                (opcode, format) = definition.groups()
                if len(params) != len(format):
                    params = []
                    arg_counter = 1
                    for f in format:
                        arg_name = 'arg%u' % arg_counter
                        params.append(arg_name)
                        arg_counter += 1
                (ogf, ocf) = opcodes[opcode]
                commands.append((command_name, ogf, ocf, format, params))
                params = []
                continue

    return commands

def parse_commands(camel_case=True, extra_commands_c_path=None, extra_commands_h_path=None):
    global btstack_root
    opcodes = parse_opcodes(camel_case, extra_commands_h_path)

    commands = []
    commands += my_parse_commands(resolve_path(hci_cmds_c_path), opcodes, camel_case)
    if extra_commands_c_path is not None:
        commands += my_parse_commands(resolve_path(extra_commands_c_path), opcodes, camel_case)
    return commands

def parse_daemon_commands(camel_case=True, commands_c_path=None, commands_h_path=None):
    global btstack_root
    if commands_c_path is None or commands_h_path is None:
        return []
    opcodes = parse_opcodes(camel_case, commands_h_path)
    return my_parse_commands(resolve_path(commands_c_path), opcodes, camel_case)

def print_opcode_enum(commands):
    print("typedef enum {")
    for command in commands:
        (name, ogf, ocf, format, params) = command
        print("    BTSTACK_OPCODE_%s = HCI_OPCODE (%s, %s)," % (name.upper(), ogf, ocf))
    print("} btstack_opcode_t;")

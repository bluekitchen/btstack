#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

import re
import os
import sys

# paths
bluetooth_h_path = 'src/bluetooth.h'
btstack_defines_h_path = 'src/btstack_defines.h'
daemon_cmds_c_path = 'platform/daemon/src/daemon_cmds.c'
hci_cmds_c_path = 'src/hci_cmd.c'
hci_cmds_h_path = 'src/hci_cmd.h'
hci_h_path = 'src/hci.h'

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
print ("BTstack root %s" % btstack_root)

def set_btstack_root(path):
    global btstack_root
    btstack_root = path

def assert_dir(path):
    if not os.access(path, os.R_OK):
        os.makedirs(path)

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
    defines.update(read_defines(btstack_root + '/' + bluetooth_h_path))
    defines.update(read_defines(btstack_root + '/' + btstack_defines_h_path))
    return defines

def my_parse_events(path):
    events = []
    subevents = []
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
    (bluetooth_events, bluetooth_subevents, bluetooth_event_types) = my_parse_events(btstack_root + '/' + bluetooth_h_path)

    # parse btstack_defines to get events
    (btstack_events, btstack_subevents, btstack_event_types) = my_parse_events(btstack_root + '/' + btstack_defines_h_path)

    # concat lists
    (events, subvents, event_types) = (bluetooth_events + btstack_events, bluetooth_subevents + btstack_subevents, bluetooth_event_types | btstack_event_types)

    return (events, subvents, event_types)

def my_parse_commands(infile, convert_to_camel_case):
    commands = []
    with open (infile, 'rt') as fin:

        params = []
        for line in fin:

            parts = re.match('.*@param\s*(\w*)\s*', line)
            if parts and len(parts.groups()) == 1:
                param = parts.groups()[0]
                if convert_to_camel_case:
                    param = camel_case_var(param)
                else:
                    param = param.lower()
                params.append(param)
                continue

            declaration = re.match('const\s+hci_cmd_t\s+(\w+)[\s=]+', line)
            if declaration:
                command_name = declaration.groups()[0]
                # drop _cmd suffix for daemon commands
                if command_name.endswith('_cmd'):
                    command_name = command_name[:-len('_cmd')]
                if convert_to_camel_case:
                    command_name = camel_case(command_name)
                else:
                    command_name = command_name.lower()
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

def parse_commands(camel_case=True):
    global btstack_root
    commands = []
    commands = commands = my_parse_commands(btstack_root + '/' + hci_cmds_c_path, camel_case)
    commands = commands = my_parse_commands(btstack_root + '/' + daemon_cmds_c_path, camel_case)
    return commands

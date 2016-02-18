
#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

import re
import os

# paths
bluetooth_h_path = 'src/bluetooth.h'
btstack_defines_h_path = 'src/btstack_defines.h'
daemon_cmds_c_path = 'platform/daemon/daemon_cmds.c'
hci_cmds_c_path = 'src/hci_cmd.c'
hci_cmds_h_path = 'src/hci_cmd.h'
hci_h_path = 'src/hci.h'

btstack_root = '../..'

def set_btstack_root(path):
    global btstack_root
    btstack_root = path

def assert_dir(path):
    if not os.access(path, os.R_OK):
        os.makedirs(path)

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
    defines.update(read_defines(btstack_root + '/' + bluetooth_h_path))
    defines.update(read_defines(btstack_root + '/' + btstack_defines_h_path))
    return defines

def my_parse_events(path):
    events = []
    le_events = []
    hfp_events = []
    hsp_events = []
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
                    elif key.lower().startswith('hsp_subevent_'):
                        hsp_events.append((value, key.lower().replace('hsp_subevent_', 'hsp_event_'), format, params))
                    elif key.lower().startswith('hfp_subevent_'):
                        hfp.append((value, key.lower().replace('hfp_subevent_', 'hfp_event_'), format, params))
                    else:
                        events.append((value, key, format, params))
                    event_types.add(key)
                params = []
                format = None
    return (events, le_events, event_types)

def parse_events():
    global btstack_root
    
    # parse bluetooth.h to get used events
    (bluetooth_events, bluetooth_le_events, bluetooth_event_types) = my_parse_events(btstack_root + '/' + bluetooth_h_path)

    # parse btstack_defines to get events
    (btstack_events, btstack_le_events, btstack_event_types) = my_parse_events(btstack_root + '/' + btstack_defines_h_path)

    # concat lists
    (events, le_events, event_types) = (bluetooth_events + btstack_events, bluetooth_le_events + btstack_le_events, bluetooth_event_types | btstack_event_types)

    return (events, le_events, event_types)

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
    commands = []
    commands = commands = my_parse_commands(btstack_root + '/' + hci_cmds_c_path)
    commands = commands = my_parse_commands(btstack_root + '/' + daemon_cmds_c_path)
    return commands
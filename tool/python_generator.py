#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2018

import glob
import re
import sys
import os

import btstack_parser as parser

print('''
Python binding generator for BTstack Server
Copyright 2018, BlueKitchen GmbH
''')

# com.bluekitchen.btstack.BTstack.java templates
command_builder_header = \
'''#!/usr/bin/env python3

import struct

def opcode(ogf, ocf):
    return ocf | (ogf << 10)

def pack24(value):
    return struct.pack("B", value & 0xff) + struct.pack("<H", value >> 8)

def name248(str):
    arg = str.encode('utf-8')
    return arg[:248] + bytes(248-len(arg))

# Command Builder

class CommandBuilder(object):

    def __init__(self):
        pass

    def send_command(command):
        return FALSE

'''

command_builder_command = '''
    def {name}(self, {args}):
        cmd_args = bytes()
{args_builder}
        cmd = struct.pack("<HB", opcode(self.{ogf}, self.{ocf}), len(cmd_args)) + cmd_args
        return self.send_hci_command(cmd)
'''

# com.bluekitchen.btstack.EventFactory template
event_factory_template = \
'''

# dictionary to map hci event types to event classes
event_class_for_type = {{
{1}}}

# dictionary to map hci le event types to event classes
le_event_class_for_type = {{
{2}}}

# list of all event types - not actually used
{0}

def event_for_payload(payload):
    event_type  = payload[0]
    event_class = btstack.btstack_types.Event
    # LE Subevent
    if event_type == 0x3e:
        subevent_type = payload[2]
        event_class = le_event_class_for_type.get(subevent_type, event_class)
    else:
        event_class = event_class_for_type.get(event_type, event_class)
    return event_class(payload)
'''
event_factory_event =  \
'''    {0} : {1},
'''
event_factory_subevent = \
'''    {0} : {1},
'''

event_header = '''
import struct
import btstack.btstack_types

def hex_string(bytes):
    return " ".join([('%02x' % a) for a in bytes])

'''

event_template = \
'''

class {0}(btstack.btstack_types.Event):

    def __init__(self, payload):
        super().__init__(payload)
    {1}
    {2}
'''

event_getter = \
'''
    def get_{0}(self):
        {1}
'''

event_getter_data = '''return self.payload[{offset}:{offset}+self.get_{length_name}()]
'''

event_getter_data_fixed = \
'''return self.payload[{offset}:{offset}+{size}]
'''

event_to_string = \
'''def __repr__(self):
        repr  = '{0} < type=0x%02x' % self.get_event_type()
{1}     
        repr += " >"
        return repr
'''


# global variables/defines
package  ='com.bluekitchen.btstack'
gen_path = 'gen/' + package.replace('.', '/')

defines = dict()
defines_used = set()

def size_for_type(type):
    param_sizes = { '1' : 1, '2' : 2, '3' : 3, '4' : 4, 'H' : 2, 'B' : 6, 'D' : 8, 'E' : 240, 'N' : 248, 'P' : 16,
                    'A' : 31, 'S' : -1, 'V': -1, 'J' : 1, 'L' : 2, 'Q' : 32, 'K' : 16, 'U' : 16, 'X' : 20, 'Y' : 24, 'Z' : 18, 'T':-1}
    return param_sizes[type]

def create_command_python(fout, name, ogf, ocf, format, params):
    global command_builder_command

    ind = '        '
    param_store = {
     '1' : 'cmd_args += struct.pack("B", %s)',
     'J' : 'cmd_args += struct.pack("B", %s)',
     '2' : 'cmd_args += struct.pack("<H", %s)',
     'H' : 'cmd_args += struct.pack("<H", %s)',
     'L' : 'cmd_args += struct.pack("<H", %s)',
     '3' : 'cmd_args += pack24(%s)',
     '4' : 'cmd_args += struct.pack("<H", %s)',
     'N' : 'cmd_args += name248(%s)',
     'B' : 'cmd_args += %s.get_bytes()',
     'U' : 'cmd_args += %s.get_bytes()', 
     'X' : 'cmd_args += %s.get_bytes()', 
     'Y' : 'cmd_args += %s.get_bytes()', 
     'Z' : 'cmd_args += %s.get_bytes()', 
     'S' : 'cmd_args += %s',
     # TODO: support serialization for these
     'D' : '# D / TODO Util.storeBytes(command, offset, %s, 8)',
     'E' : '# E / TODO Util.storeBytes(command, offset, %s, 240)',
     'P' : '# P / TODO Util.storeBytes(command, offset, %s, 16)',
     'Q' : '# Q / TODO Util.storeBytes(command, offset, %s, 32)',
     'K' : '# Q / TODO Util.storeBytes(command, offset, %s, 32)',
     'A' : '# A / TODO Util.storeBytes(command, offset, %s, 31)',
     }
    # method arguments 
    arg_counter = 1
    args = []
    for param_type, arg_name in zip(format, params):
        arg_size = size_for_type(param_type)
        arg = (param_type, arg_size, arg_name)
        args.append(arg)
        arg_counter += 1

    # method argument declaration 
    args2 = []
    for arg in args:
        args2.append(arg[2])
    args_string = ', '.join(args2)

    # command size (opcode, len)
    size_fixed = 3
    size_var = ''
    for arg in args:
        size = arg[1]
        if size > 0:
            size_fixed += size
        else:
            size_var += ' + %s.length' % arg[2]
    size_string = '%u%s' % (size_fixed, size_var)

    store_params = ''

    length_name = ''
    for (param_type, arg_size, arg_name) in args:
        if param_type in ['L', 'J']:
            length_name = arg_name
        if param_type == 'V':
            store_params += ind + 'Util.storeBytes(command, offset, %s, %s);' % (arg_name, length_name) + '\n';
            length_name = ''
        else:
            store_params += ind + (param_store[param_type] % arg_name) + '\n';
            size = arg_size

    fout.write( command_builder_command.format(name=name, args=args_string, ogf=ogf, ocf=ocf, args_builder=store_params))

def mark_define_as_used(term):
    if term.startswith('0'):
        return
    defines_used.add(term)

def python_define_string(key):
    global defines
    if key in defines:
        return '    %s = %s\n' % (key, defines[key])
    else:
        return '    # defines[%s] not set\n' % key

def python_defines_string(keys):
    return '\n'.join( map(python_define_string, sorted(keys)))

def create_command_builder(commands):
    global gen_path
    parser.assert_dir(gen_path)
    
    outfile = '%s/command_builder.py' % gen_path

    with open(outfile, 'wt') as fout:
    
        fout.write(command_builder_header)

        for command in commands:
                (command_name, ogf, ocf, format, params) = command
                create_command_python(fout, command_name, ogf, ocf, format, params);
                mark_define_as_used(ogf)
                mark_define_as_used(ocf)

        fout.write('\n    # defines used\n\n')
        for key in sorted(defines_used):
            fout.write(python_define_string(key))

def create_event(fout, event_name, format, args):
    global gen_path
    global event_template

    param_read = {
     '1' : 'return self.payload[{offset}]',
     'J' : 'return self.payload[{offset}]',
     '2' : 'return struct.unpack("<H", self.payload[{offset} : {offset}+2])',
     'H' : 'return struct.unpack("<H", self.payload[{offset} : {offset}+2])',
     'L' : 'return struct.unpack("<H", self.payload[{offset} : {offset}+2])',
     '3' : 'return btstack.btstack_types.unpack24(self.payload[{offset}:3])',
     '4' : 'return struct.unpack("<I", self.payload[{offset} : {offset}+4])',
     'B' : 'data = bytearray(self.payload[{offset}:{offset}+6]); data.reverse(); return btstack.btstack_types.BD_ADDR(data)',
     'X' : 'return btstack.btstack_types.GATTService(self.payload[{offset}:20])',
     'Y' : 'return btstack.btstack_types.GATTCharacteristic(self.payload[{offset}:24])',
     'Z' : 'return btstack.btstack_types.GATTCharacteristicDescriptor(self.payload[{offset}:18])',
     'T' : 'return self.payload[{offset}:-1].decode("utf-8")',
     'N' : 'return self.payload[{offset}:{offset}+248].decode("utf-8")',
     # 'D' : 'Util.storeBytes(self.payload, %u, 8);',
     # 'Q' : 'Util.storeBytes(self.payload, %u, 32);',
     # 'E' : 'Util.storeBytes(data, %u, 240);',
     # 'P' : 'Util.storeBytes(data, %u, 16);',
     # 'A' : 'Util.storeBytes(data, %u, 31);',
     # 'S' : 'Util.storeBytes(data, %u);'
     'R' : 'return self.payload[{offset}:]', 
     }

    offset = 2
    getters = ''
    length_name = ''
    for f, arg in zip(format, args):
        # just remember name
        if f in ['L','J']:
            length_name = arg.lower()
        if f == 'R':    
            # remaining data
            access = param_read[f].format(offset=offset)
            size = 0
        elif f == 'V':
            access = event_getter_data.format(length_name=length_name, offset=offset)
            size = 0
        elif f in ['D', 'Q', 'K']:
            size = size_for_type(f)
            access = event_getter_data_fixed.format(size=size, offset=offset)
        else: 
            access = param_read[f].format(offset=offset)
            size = size_for_type(f)
        getters += event_getter.format(arg.lower(), access)
        offset += size
    to_string_args = ''
    for f, arg in zip(format, args):
        to_string_args += '        repr += ", %s = "\n' % arg
        if f in ['1','2','3','4','H']:
            to_string_args += '        repr += hex(self.get_%s())\n' % arg.lower()
        elif f in ['R', 'V', 'D', 'Q']:
            to_string_args += '        repr += hex_string(self.get_%s())\n' % arg.lower()
        else:
            to_string_args += '        repr += str(self.get_%s())\n' % arg.lower()
    to_string_method = event_to_string.format(event_name, to_string_args)
    fout.write('# %s - %s' % (event_name, format))
    fout.write(event_template.format(event_name, getters, to_string_method))

def event_supported(event_name):
    parts = event_name.split('_')
    return parts[0] in ['ATT', 'BTSTACK', 'DAEMON', 'L2CAP', 'RFCOMM', 'SDP', 'GATT', 'GAP', 'HCI', 'SM', 'BNEP']
        
def class_name_for_event(event_name):
    return parser.camel_case(event_name.replace('SUBEVENT','EVENT'))

def create_events(fout, events):
    global gen_path
    gen_path_events = gen_path + '/event'
    parser.assert_dir(gen_path_events)

    for event_type, event_name, format, args in events:
        if not event_supported(event_name):
            continue
        class_name = class_name_for_event(event_name)
        create_event(fout, class_name, format, args)


def create_event_factory(events, subevents, defines):
    global gen_path
    global event_factory_event
    global event_factory_template

    outfile = '%s/event_factory.py' % gen_path


    cases = ''
    for event_type, event_name, format, args in events:
        event_name = parser.camel_case(event_name)
        cases += event_factory_event.format(event_type, event_name)
    subcases = ''
    for event_type, event_name, format, args in subevents:
        if not event_supported(event_name):
            continue
        class_name = class_name_for_event(event_name)
        subcases += event_factory_subevent.format(event_type, class_name)

    with open(outfile, 'wt') as fout:
        # header
        fout.write(event_header)
        # event classes
        create_events(fout, events)
        create_events(fout, subevents)
        # 
        defines_text = ''
        # python_defines_string(,defines)
        fout.write(event_factory_template.format(defines_text, cases, subcases))

# find root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/platform/daemon/binding/python/btstack/'


# read defines from hci_cmds.h and hci.h
defines = parser.parse_defines()

# parse commands
commands = parser.parse_daemon_commands(camel_case=False)

# parse bluetooth.h to get used events
(events, le_events, event_types) = parser.parse_events()

# create events, le meta events, event factory, and 
create_event_factory(events, le_events, event_types)
create_command_builder(commands)

# done
print('Done!')

#!/usr/bin/env python
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
java_event_factory_template = \
'''package {0};

import {0}.event.*;

public class EventFactory {{

    /** @brief event codes */

{1}
    public static Event eventForPacket(Packet packet){{
        int eventType = Util.readByte(packet.getBuffer(), 0);
        switch (eventType){{
{2}        
        case 0x3e:  // LE_META_EVENT
            int subEventType = Util.readByte(packet.getBuffer(), 2);
            switch (subEventType){{
{3}            
            default:
                return new Event(packet);
            }}

        default:
            return new Event(packet);
        }}
    }}
}}
'''
java_event_factory_event = '''
        case {0}:
            return new {1}(packet);
'''
java_event_factory_subevent = '''
            case {0}:
                return new {1}(packet);
'''

# com.bluekitchen.btstack.events.* template
java_event_template = \
'''package {0}.event;

import {0}.*;

public class {1} extends Event {{

    public {1}(Packet packet) {{
        super(packet);
    }}
    {2}
    {3}
}}
'''

java_event_getter = \
'''
    /**
     * @return {1} as {0}
     */
    public {0} get{1}(){{
        {2}
    }}
'''

java_event_getter_data = \
'''int len = get{0}();
        byte[] result = new byte[len];
        System.arraycopy(data, {1}, result, 0, len);
        return result;'''

java_event_getter_data_fixed = \
'''int len = {0};
        byte[] result = new byte[len];
        System.arraycopy(data, {1}, result, 0, len);
        return result;'''

java_event_getter_remaining_data = \
'''int len = getPayloadLen() - {0};
        byte[] result = new byte[len];
        System.arraycopy(data, {0}, result, 0, len);
        return result;'''

java_event_to_string = \
'''
    public String toString(){{
        StringBuffer t = new StringBuffer();
        t.append("{0} < type = ");
        t.append(String.format("0x%02x, ", getEventType()));
        t.append(getEventType());
{1}        t.append(" >");
        return t.toString();
    }}
'''


# global variables/defines
package  ='com.bluekitchen.btstack'
gen_path = 'gen/' + package.replace('.', '/')

defines = dict()
defines_used = set()

def java_type_for_btstack_type(type):
    param_types = { '1' : 'int', '2' : 'int', '3' : 'int', '4' : 'long', 'H' : 'int', 'B' : 'BD_ADDR',
                    'D' : 'byte []', 'E' : 'byte [] ', 'N' : 'String' , 'P' : 'byte []', 'A' : 'byte []',
                    'R' : 'byte []', 'S' : 'byte []', 'Q' : 'byte []',
                    'J' : 'int', 'L' : 'int', 'V' : 'byte []', 'U' : 'BT_UUID',
                    'X' : 'GATTService', 'Y' : 'GATTCharacteristic', 'Z' : 'GATTCharacteristicDescriptor',
                    'T' : 'String'}
    return param_types[type]

def size_for_type(type):
    param_sizes = { '1' : 1, '2' : 2, '3' : 3, '4' : 4, 'H' : 2, 'B' : 6, 'D' : 8, 'E' : 240, 'N' : 248, 'P' : 16,
                    'A' : 31, 'S' : -1, 'V': -1, 'J' : 1, 'L' : 2, 'Q' : 32, 'U' : 16, 'X' : 20, 'Y' : 24, 'Z' : 18, 'T':-1}
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
    print(args)

    # method argument declaration 
    args2 = []
    for arg in args:
        args2.append(arg[2])
    args_string = ', '.join(args2)
    print(args_string)

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
    print(size_string)

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

def java_defines_string(keys):
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

def create_event(event_name, format, args):
    global gen_path
    global package
    global java_event_template

    param_read = {
     '1' : 'return Util.readByte(data, %u);',
     'J' : 'return Util.readByte(data, %u);',
     '2' : 'return Util.readBt16(data, %u);',
     'H' : 'return Util.readBt16(data, %u);',
     'L' : 'return Util.readBt16(data, %u);',
     '3' : 'return Util.readBt24(data, %u);',
     '4' : 'return Util.readBt32(data, %u);',
     'B' : 'return Util.readBdAddr(data, %u);',
     'X' : 'return Util.readGattService(data, %u);',
     'Y' : 'return Util.readGattCharacteristic(data, %u);',
     'Z' : 'return Util.readGattCharacteristicDescriptor(data, %u);',
     'T' : 'int offset = %u; \n        return Util.getText(data, offset, getPayloadLen()-offset);',
     'N' : 'return Util.getText(data, %u, 248);',
     'D' : 'Util.storeBytes(data, %u, 8);',
     'Q' : 'Util.storeBytes(data, %u, 32);',
     # 'E' : 'Util.storeBytes(data, %u, 240);',
     # 'P' : 'Util.storeBytes(data, %u, 16);',
     # 'A' : 'Util.storeBytes(data, %u, 31);',
     # 'S' : 'Util.storeBytes(data, %u);'
     }

    gen_event_path = '%s/event' % gen_path
    outfile = '%s/%s.java' % (gen_event_path, event_name)
    with open(outfile, 'wt') as fout:
        offset = 2
        getters = ''
        length_name = ''
        for f, arg in zip(format, args):
            # just remember name
            if f in ['L','J']:
                length_name = parser.camel_case(arg)
            if f == 'R':    
                # remaining data
                access = java_event_getter_remaining_data.format(offset)
                size = 0
            elif f == 'V':
                access = java_event_getter_data.format(length_name, offset)
                size = 0
            elif f in ['D', 'Q']:
                size = size_for_type(f)
                access = java_event_getter_data_fixed.format(size, offset)
            else: 
                access = param_read[f] % offset
                size = size_for_type(f)
            getters += java_event_getter.format(java_type_for_btstack_type(f), parser.camel_case(arg), access)
            offset += size
        to_string_args = ''
        for arg in args:
            to_string_args += '        t.append(", %s = ");\n' % arg
            to_string_args += '        t.append(get%s());\n' % parser.camel_case(arg)
        to_string_method = java_event_to_string.format(event_name, to_string_args)
        fout.write(java_event_template.format(package, event_name, getters, to_string_method))

def event_supported(event_name):
    parts = event_name.split('_')
    return parts[0] in ['ATT', 'BTSTACK', 'DAEMON', 'L2CAP', 'RFCOMM', 'SDP', 'GATT', 'GAP', 'HCI', 'SM', 'BNEP']
        
def class_name_for_event(event_name):
    return parser.camel_case(event_name.replace('SUBEVENT','EVENT'))

def create_events(events):
    global gen_path
    gen_path_events = gen_path + '/event'
    parser.assert_dir(gen_path_events)

    for event_type, event_name, format, args in events:
        if not event_supported(event_name):
            continue
        class_name = class_name_for_event(event_name)
        create_event(class_name, format, args)


def create_event_factory(events, subevents, defines):
    global gen_path
    global package
    global java_event_factory_event
    global java_event_factory_template

    outfile = '%s/EventFactory.java' % gen_path

    cases = ''
    for event_type, event_name, format, args in events:
        event_name = parser.camel_case(event_name)
        cases += java_event_factory_event.format(event_type, event_name)
    subcases = ''
    for event_type, event_name, format, args in subevents:
        if not event_supported(event_name):
            continue
        class_name = class_name_for_event(event_name)
        print class_name
        subcases += java_event_factory_subevent.format(event_type, class_name)

    with open(outfile, 'wt') as fout:
        defines_text = java_defines_string(defines)
        fout.write(java_event_factory_template.format(package, defines_text, cases, subcases))

# find root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
gen_path = btstack_root + '/platform/daemon/binding/python/btstack/'


# read defines from hci_cmds.h and hci.h
defines = parser.parse_defines()

# parse commands
commands = parser.parse_commands()

# parse bluetooth.h to get used events
# (events, le_events, event_types) = parser.parse_events()

# create events, le meta events, event factory, and 
# create_events(events)
# create_events(le_events)
# create_event_factory(events, le_events, event_types)
create_command_builder(commands)

# done
print('Done!')

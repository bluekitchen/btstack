#!/usr/bin/env python
#
# BLE GATT configuration generator for use with BTstack, v0.1
# Copyright 2011 Matthias Ringwald
#
# Format of input file:
# PRIMARY_SERVICE, SERVICE_UUID
# CHARACTERISTIC, ATTRIBUTE_TYPE_UUID, [READ | WRITE | DYNAMIC], VALUE

import re

header = '''
// {0} generated from {1} for BTstack

// binary representation
// attribute size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

#include <stdint.h>

const uint8_t profile_data[] =
'''

usage = '''
Usage: ./compile-gatt.py profile.gatt profile.h
'''

import re
import sys

print '''
BLE configuration generator for use with BTstack, v0.1
Copyright 2011 Matthias Ringwald
'''

assigned_uuids = {
    'GAP_SERVICE'          : 0x1800,
    'GATT_SERVICE'         : 0x1801, 
    'GAP_DEVICE_NAME'      : 0x2a00,
    'GAP_APPEARANCE'       : 0x2a01,
    'GATT_SERVICE_CHANGED' : 0x2a05
}

property_flags = {
    'BROADCAST' :                  0x01,
    'READ' :                       0x02,
    'WRITE_WITHOUT_RESPONSE' :     0x04,
    'WRITE' :                      0x08,
    'NOTIFY':                      0x10,
    'INDICATE' :                   0x20,
    'AUTHENTICATED_SIGNED_WRITE' : 0x40,
    'EXTENDED_PROPERTIES' :        0x80,
    # custom BTstack extension
    'DYNAMIC':                    0x100,
    'LONG_UUID':                  0x200,
}

handle = 1
total_size = 0

def twoByteLEFor(value):
    return [ (value & 0xff), (value >> 8)]

def is_128bit_uuid(text):
    if re.match("[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}", text):
        return True
    return False

def parseUUID128(uuid):
    parts = re.match("([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})", uuid)
    uuid_bytes = []
    for i in range(8, 0, -1):
        uuid_bytes = uuid_bytes + twoByteLEFor(int(parts.group(i),16))
    return uuid_bytes

def parseUUID(uuid):
    if uuid in assigned_uuids:
        return twoByteLEFor(assigned_uuids[uuid])
    if is_128bit_uuid(uuid):
        return parseUUID128(uuid)
    uuidInt = int(uuid, 16)
    return twoByteLEFor(uuidInt)
    
def parseProperties(properties):
    value = 0
    parts = properties.split("|")
    for property in parts:
        property = property.strip()
        if property in property_flags:
            value |= property_flags[property]
        else:
            print "WARNING: property %s undefined" % (property)
    return value

def write_8(fout, value):
    fout.write( "0x%02x, " % (value & 0xff))

def write_16(fout, value):
    fout.write('0x%02x, 0x%02x, ' % (value & 0xff, value >> 8))

def write_uuid(uuid):
    for byte in uuid:
        fout.write( "0x%02x, " % byte)

def write_string(fout, text):
    for l in text.lstrip('"').rstrip('"'):
        write_8(fout, ord(l))

def write_sequence(fout, text):
    parts = text.split()
    for part in parts:
        fout.write("0x%s, " % (part.strip()))

def write_indent(fout):
    fout.write("    ")

def is_string(text):
    if not text.startswith('"'):
        return False
    if not text.endswith('"'):
        return False
    return True

def parsePrimaryService(fout, parts):
    global handle
    global total_size

    property = property_flags['READ'];
    
    write_indent(fout)
    fout.write('// 0x%04x %s\n' % (handle, '-'.join(parts)))

    uuid = parseUUID(parts[1])
    uuid_size = len(uuid)

    size = 2 + 2 + 2 + uuid_size + 2

    if uuid_size == 16:
        property = property | property_flags['LONG_UUID'];
    
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property)
    write_16(fout, handle)
    write_16(fout, 0x2800)
    write_uuid(uuid)
    fout.write("\n")
    handle = handle + 1
    total_size = total_size + size
    
def parseCharacteristic(fout, parts):
    global handle
    global total_size

    property_read = property_flags['READ'];

    uuid       = parseUUID(parts[1])
    uuid_size  = len(uuid)
    properties = parseProperties(parts[2])
    value      = parts[3]

    write_indent(fout)
    fout.write('// 0x%04x %s\n' % (handle, '-'.join(parts[0:len(parts)-1])))
    
    size = 2 + 2 + 2 + 2 + (1+2+uuid_size)
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property_read)
    write_16(fout, handle)
    write_16(fout, 0x2803)
    write_8(fout, properties)
    write_16(fout, handle+1)
    write_uuid(uuid)
    fout.write("\n")
    handle = handle + 1
    total_size = total_size + size

    size = 2 + 2 + 2 + uuid_size
    if is_string(value):
        size = size + len(value) - 2
    else:
        size = size + len(value.split())

    if uuid_size == 16:
        properties = properties | property_flags['LONG_UUID'];

    write_indent(fout)
    fout.write('// 0x%04x VALUE-%s\n' % (handle, '-'.join(parts[1:len(parts)])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, properties)
    write_16(fout, handle)
    write_uuid(uuid)
    if is_string(value):
        write_string(fout, value)
    else:
        write_sequence(fout,value)
    fout.write("\n")
    handle = handle + 1

def parse(fname_in, fin, fname_out, fout):
    global handle
    global total_size
    
    fout.write(header.format(fname_out, fname_in))
    fout.write('{\n')
    
    for line in fin:
        line = line.strip("\n\r ")
        
        if line.startswith("#") or line.startswith("//"):
            fout.write("//" + line.lstrip('/#') + '\n')
            continue
            
        if len(line) == 0:
            continue
            
        parts = line.split(',')
        for index, object in enumerate(parts):
            parts[index] = object.strip()
        
        if parts[0] == 'PRIMARY_SERVICE':
            parsePrimaryService(fout, parts)
            continue

        if parts[0] == 'CHARACTERISTIC':
            parseCharacteristic(fout, parts)
            continue
        
        print "WARNING: unknown token: %s\n" % (parts[0])

    write_indent(fout)
    fout.write("// END\n");
    write_indent(fout)
    write_16(fout,0)
    fout.write("\n")
    total_size = total_size + 2
    
    fout.write("}; // total size %u bytes \n" % total_size);
    fout.close()
    print 'Created', fname_out

if (len(sys.argv) < 3):
    print usage
    sys.exit(1)
try:
    fin  = open (sys.argv[1], 'r')
    fout = open (sys.argv[2], 'w')
    parse(sys.argv[1], fin, sys.argv[2], fout)

except IOError as e:
    print usage
    sys.exit(1)

print 'Compilation successful!\n'
    



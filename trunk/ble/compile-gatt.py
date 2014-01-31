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
    'GATT_SERVICE_CHANGED' : 0x2a05,
}

property_flags = {
    'BROADCAST' :                   0x01,
    'READ' :                        0x02,
    'WRITE_WITHOUT_RESPONSE' :      0x04,
    'WRITE' :                       0x08,
    'NOTIFY':                       0x10,
    'INDICATE' :                    0x20,
    'AUTHENTICATED_SIGNED_WRITE' :  0x40,
    'EXTENDED_PROPERTIES' :         0x80,
    # custom BTstack extension
    'DYNAMIC':                     0x100,
    'LONG_UUID':                   0x200,
    'AUTHENTICATION_REQUIRED':     0x400,
    'AUTHORIZATION_REQUIRED':      0x800,
    'ENCRYPTION_KEY_SIZE_7':      0x6000,
    'ENCRYPTION_KEY_SIZE_8':      0x7000,
    'ENCRYPTION_KEY_SIZE_9':      0x8000,
    'ENCRYPTION_KEY_SIZE_10':     0x9000,
    'ENCRYPTION_KEY_SIZE_11':     0xa000,
    'ENCRYPTION_KEY_SIZE_12':     0xb000,
    'ENCRYPTION_KEY_SIZE_13':     0xc000,
    'ENCRYPTION_KEY_SIZE_14':     0xd000,
    'ENCRYPTION_KEY_SIZE_15':     0xe000,
    'ENCRYPTION_KEY_SIZE_16':     0xf000,
    # only used by gatt compiler >= 0xffff
    # Extended Properties
    'RELIABLE_WRITE':             0x10000,
}

services = dict()
presentation_formats = dict()
current_service_uuid = list()
current_service_start_handle = 0

handle = 1
total_size = 0

def keyForUUID(uuid):
    keyUUID = ""
    for i in uuid:
        keyUUID += "%02x" % i
    return keyUUID
 

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
    fout.write('0x%02x, 0x%02x, ' % (value & 0xff, (value >> 8) & 0xff))

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

def add_client_characteristic_configuration(properties):
    return properties & (property_flags['NOTIFY'] | property_flags['INDICATE'])

def parseService(fout, parts, service_type):
    global handle
    global total_size
    global current_service_uuid
    global current_service_start_handle
    global services

    keyUUID = keyForUUID(current_service_uuid)
    if keyUUID:
        fout.write("\n")
        # print "append service %s = [%d, %d]\n" % (keyUUID, current_service_start_handle, handle-1)
        services[keyUUID] = [current_service_start_handle, handle-1]

    property = property_flags['READ'];
    
    write_indent(fout)
    fout.write('// 0x%04x %s\n' % (handle, '-'.join(parts)))

    uuid = parseUUID(parts[1])
    uuid_size = len(uuid)
    
    size = 2 + 2 + 2 + uuid_size + 2

    if service_type == 0x2802:
        size += 4

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property)
    write_16(fout, handle)
    write_16(fout, service_type)
    write_uuid(uuid)
    fout.write("\n")

    current_service_uuid = uuid
    current_service_start_handle = handle
    handle = handle + 1
    total_size = total_size + size

def parsePrimaryService(fout, parts):
    parseService(fout, parts, 0x2800)

def parseSecondaryService(fout, parts):
    parseService(fout, parts, 0x2801)

def parseIncludeService(fout, parts):
    global handle
    global total_size
    
    property = property_flags['READ'];
    
    write_indent(fout)
    fout.write('// 0x%04x %s\n' % (handle, '-'.join(parts)))

    uuid = parseUUID(parts[1])
    uuid_size = len(uuid)
    # print "Include Service ", keyForUUID(uuid)

    size = 2 + 2 + 2 + uuid_size + 2 + 4

    keyUUID = keyForUUID(uuid)

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property)
    write_16(fout, handle)
    write_16(fout, 0x2802)
    write_16(fout, services[keyUUID][0])
    write_16(fout, services[keyUUID][1])
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

    # reliable writes is defined in an extended properties
    if (properties & property_flags['RELIABLE_WRITE']):
        properties = properties | property_flags['EXTENDED_PROPERTIES']

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
    fout.write('// 0x%04x VALUE-%s\n' % (handle, '-'.join(parts[1:])))
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

    if add_client_characteristic_configuration(properties):
        size = 2 + 2 + 2 + 2 + 2
        write_indent(fout)
        fout.write('// 0x%04x CLIENT_CHARACTERISTIC_CONFIGURATION\n' % (handle))
        write_indent(fout)
        write_16(fout, size)
        write_16(fout, property_flags['READ'] | property_flags['WRITE'] | property_flags['DYNAMIC'])
        write_16(fout, handle)
        write_16(fout, 0x2902)
        write_16(fout, 0)
        fout.write("\n")
        handle = handle + 1

    if properties & property_flags['RELIABLE_WRITE']:
        size = 2 + 2 + 2 + 2 + 2
        write_indent(fout)
        fout.write('// 0x%04x CHARACTERISTIC_EXTENDED_PROPERTIES\n' % (handle))
        write_indent(fout)
        write_16(fout, size)
        write_16(fout, property_flags['READ'])
        write_16(fout, handle)
        write_16(fout, 0x2900)
        write_16(fout, 1)   # Reliable Write
        fout.write("\n")
        handle = handle + 1



def parseCharacteristicUserDescription(fout, parts):
    global handle
    global total_size

    properties = parseProperties(parts[1])
    value      = parts[2]

    size = 2 + 2 + 2 + 2
    if is_string(value):
        size = size + len(value) - 2
    else:
        size = size + len(value.split())

    write_indent(fout)
    fout.write('// 0x%04x CHARACTERISTIC_USER_DESCRIPTION-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, properties)
    write_16(fout, handle)
    write_16(fout, 0x2901)
    if is_string(value):
        write_string(fout, value)
    else:
        write_sequence(fout,value)
    fout.write("\n")
    handle = handle + 1

def parseServerCharacteristicConfiguration(fout, parts):
    global handle
    global total_size

    properties = parseProperties(parts[1])
    properties = properties | property_flags['DYNAMIC']
    size = 2 + 2 + 2 + 2

    write_indent(fout)
    fout.write('// 0x%04x SERVER_CHARACTERISTIC_CONFIGURATION-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, properties)
    write_16(fout, handle)
    write_16(fout, 0x2903)
    fout.write("\n")
    handle = handle + 1

def parseCharacteristicFormat(fout, parts):
    global handle
    global total_size

    property_read = property_flags['READ'];

    identifier = parts[1]
    presentation_formats[identifier] = handle
    # print "format '%s' with handle %d\n" % (identifier, handle) 

    format     = parts[2]
    exponent   = parts[3]
    unit       = parseUUID(parts[4])
    name_space = parts[5]
    description = parseUUID(parts[6])

    size = 2 + 2 + 2 + 2 + 7

    write_indent(fout)
    fout.write('// 0x%04x CHARACTERISTIC_FORMAT-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property_read)
    write_16(fout, handle)
    write_16(fout, 0x2904)
    write_sequence(fout, format)
    write_sequence(fout, exponent)
    write_uuid(unit)
    write_sequence(fout, name_space)
    write_uuid(description)
    fout.write("\n")
    handle = handle + 1


def parseCharacteristicAggregateFormat(fout, parts):
    global handle
    global total_size

    property_read = property_flags['READ'];
    size = 2 + 2 + 2 + 2 + (len(parts)-1) * 2

    write_indent(fout)
    fout.write('// 0x%04x CHARACTERISTIC_AGGREGATE_FORMAT-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, property_read)
    write_16(fout, handle)
    write_16(fout, 0x2905)
    for identifier in parts[1:]:
        format_handle = presentation_formats[identifier]
        if format == 0:
            print "ERROR: identifier '%s' in CHARACTERISTIC_AGGREGATE_FORMAT undefined" % identifier
            sys.exit(1)
        write_16(fout, format_handle)
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

        if parts[0] == 'SECONDARY_SERVICE':
            parseSecondaryService(fout, parts)
            continue

        if parts[0] == 'INCLUDE_SERVICE':
            parseIncludeService(fout, parts)
            continue

        if parts[0] == 'CHARACTERISTIC':
            parseCharacteristic(fout, parts)
            continue
        
        if parts[0] == 'CHARACTERISTIC_USER_DESCRIPTION':
            parseCharacteristicUserDescription(fout, parts)
            continue

        if parts[0] == 'SERVER_CHARACTERISTIC_CONFIGURATION':
            parseServerCharacteristicConfiguration(fout, parts)
            continue

        if parts[0] == 'CHARACTERISTIC_FORMAT':
            parseCharacteristicFormat(fout, parts)
            continue

        if parts[0] == 'CHARACTERISTIC_AGGREGATE_FORMAT':
            parseCharacteristicAggregateFormat(fout, parts)
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
    



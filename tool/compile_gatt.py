#!/usr/bin/env python3
#
# BLE GATT configuration generator for use with BTstack
# Copyright 2019 BlueKitchen GmbH
#
# Format of input file:
# PRIMARY_SERVICE, SERVICE_UUID
# CHARACTERISTIC, ATTRIBUTE_TYPE_UUID, [READ | WRITE | DYNAMIC], VALUE

# dependencies:
# - pip3 install pycryptodomex
# alternatively, the pycryptodome package can be used instead
# - pip3 install pycryptodome

import codecs
import csv
import io
import os
import re
import string
import sys
import argparse
import tempfile

have_crypto = True
# try to import PyCryptodome independent from PyCrypto
try:
    from Cryptodome.Cipher import AES
    from Cryptodome.Hash import CMAC
except ImportError:
    # fallback: try to import PyCryptodome as (an almost drop-in) replacement for the PyCrypto library
    try:
        from Crypto.Cipher import AES
        from Crypto.Hash import CMAC
    except ImportError:
        have_crypto = False
        print("\n[!] PyCryptodome required to calculate GATT Database Hash but not installed (using random value instead)")
        print("[!] Please install PyCryptodome, e.g. 'pip3 install pycryptodomex' or 'pip3 install pycryptodome'\n")

header = '''
// clang-format off
// {0} generated from {1} for BTstack
// it needs to be regenerated when the .gatt file is updated. 

// To generate {0}:
// {2} {1} {0}

// att db format version 1

// binary attribute representation:
// - size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

#include <stdint.h>

// Reference: https://en.cppreference.com/w/cpp/feature_test
#if __cplusplus >= 200704L
constexpr
#endif
const uint8_t profile_data[] =
'''

print('''
BLE configuration generator for use with BTstack
Copyright 2018 BlueKitchen GmbH
''')

assigned_uuids = {
    'GAP_SERVICE'          : 0x1800,
    'GATT_SERVICE'         : 0x1801, 
    'GAP_DEVICE_NAME'      : 0x2a00,
    'GAP_APPEARANCE'       : 0x2a01,
    'GAP_PERIPHERAL_PRIVACY_FLAG' : 0x2A02,
    'GAP_RECONNECTION_ADDRESS'    : 0x2A03,
    'GAP_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS' : 0x2A04,
    'GAP_CENTRAL_ADDRESS_RESOLUTION' : 0x2aa6,
    'GAP_RESOLVABLE_PRIVATE_ADDRESS_ONLY' : 0x2AC9,
    'GAP_ENCRYPTED_DATA_KEY_MATERIAL' : 0x2B88,
    'GAP_LE_GATT_SECURITY_LEVELS' : 0x2BF5,
    'GATT_SERVICE_CHANGED' : 0x2a05,
    'GATT_CLIENT_SUPPORTED_FEATURES' : 0x2b29,
    'GATT_SERVER_SUPPORTED_FEATURES' : 0x2b3a,
    'GATT_DATABASE_HASH' : 0x2b2a
}

security_permsission = ['ANYBODY','ENCRYPTED', 'AUTHENTICATED', 'AUTHORIZED', 'AUTHENTICATED_SC']

property_flags = {
    # GATT Characteristic Properties
    'BROADCAST' :                   0x01,
    'READ' :                        0x02,
    'WRITE_WITHOUT_RESPONSE' :      0x04,
    'WRITE' :                       0x08,
    'NOTIFY':                       0x10,
    'INDICATE' :                    0x20,
    'AUTHENTICATED_SIGNED_WRITE' :  0x40,
    'EXTENDED_PROPERTIES' :         0x80,
    # custom BTstack extension
    'DYNAMIC':                      0x100,
    'LONG_UUID':                    0x200,

    # read permissions
    'READ_PERMISSION_BIT_0':        0x400,
    'READ_PERMISSION_BIT_1':        0x800,

    # 
    'ENCRYPTION_KEY_SIZE_7':       0x6000,
    'ENCRYPTION_KEY_SIZE_8':       0x7000,
    'ENCRYPTION_KEY_SIZE_9':       0x8000,
    'ENCRYPTION_KEY_SIZE_10':      0x9000,
    'ENCRYPTION_KEY_SIZE_11':      0xa000,
    'ENCRYPTION_KEY_SIZE_12':      0xb000,
    'ENCRYPTION_KEY_SIZE_13':      0xc000,
    'ENCRYPTION_KEY_SIZE_14':      0xd000,
    'ENCRYPTION_KEY_SIZE_15':      0xe000,
    'ENCRYPTION_KEY_SIZE_16':      0xf000,
    'ENCRYPTION_KEY_SIZE_MASK':    0xf000,
    
    # only used by gatt compiler >= 0xffff
    # Extended Properties
    'RELIABLE_WRITE':              0x00010000,
    'AUTHENTICATION_REQUIRED':     0x00020000,
    'AUTHORIZATION_REQUIRED':      0x00040000,
    'READ_ANYBODY':                0x00080000,
    'READ_ENCRYPTED':              0x00100000,
    'READ_AUTHENTICATED':          0x00200000,
    'READ_AUTHENTICATED_SC':       0x00400000,
    'READ_AUTHORIZED':             0x00800000,
    'WRITE_ANYBODY':               0x01000000,
    'WRITE_ENCRYPTED':             0x02000000,
    'WRITE_AUTHENTICATED':         0x04000000,
    'WRITE_AUTHENTICATED_SC':      0x08000000,
    'WRITE_AUTHORIZED':            0x10000000,

    # Broadcast, Notify, Indicate, Extended Properties are only used to describe a GATT Characteristic, but are free to use with att_db
    # - write permissions
    'WRITE_PERMISSION_BIT_0':      0x01,
    'WRITE_PERMISSION_BIT_1':      0x10,
    # - SC required
    'READ_PERMISSION_SC':          0x20,
    'WRITE_PERMISSION_SC':         0x80,
}

services = dict()
characteristic_indices = dict()
presentation_formats = dict()
current_service_uuid_string = ""
current_service_start_handle = 0
current_characteristic_uuid_string = ""
defines_for_characteristics = []
defines_for_services = []
include_paths = []
database_hash_message = bytearray()
service_counter = {}

handle = 1
total_size = 0

def aes_cmac(key, n):
    if have_crypto:
        cobj = CMAC.new(key, ciphermod=AES)
        cobj.update(n)
        return cobj.digest()
    else:
        # return random value
        return os.urandom(16)

def read_defines(infile):
    defines = dict()
    with open (infile, 'rt') as fin:
        for line in fin:
            parts = re.match('#define\\s+(\\w+)\\s+(\\w+)',line)
            if parts and len(parts.groups()) == 2:
                (key, value) = parts.groups()
                defines[key] = int(value, 16)
    return defines

def keyForUUID(uuid):
    keyUUID = ""
    for i in uuid:
        keyUUID += "%02x" % i
    return keyUUID
 
def c_string_for_uuid(uuid):
    return uuid.replace('-', '_')

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
    uuid_upper = uuid.upper().replace('.','_')
    if uuid_upper in bluetooth_gatt:
        return twoByteLEFor(bluetooth_gatt[uuid_upper])
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
            print("WARNING: property %s undefined" % (property))

    return value

def prettyPrintProperties(properties):
    value = ""
    parts = properties.split("|")
    for property in parts:
        property = property.strip()
        if property in property_flags:
            if value != "":
                value += " | "
            value += property
        else:
            print("WARNING: property %s undefined" % (property))

    return value


def gatt_characteristic_properties(properties):
    return properties & 0xff

def att_flags(properties):
    # drop Broadcast (0x01), Notify (0x10), Indicate (0x20), Extended Properties (0x80) - not used for flags 
    properties &= 0xffffff4e 

    # rw permissions distinct
    distinct_permissions_used = properties & (
        property_flags['READ_AUTHORIZED'] |
        property_flags['READ_AUTHENTICATED_SC'] |
        property_flags['READ_AUTHENTICATED'] |
        property_flags['READ_ENCRYPTED'] |
        property_flags['READ_ANYBODY'] |
        property_flags['WRITE_AUTHORIZED'] |
        property_flags['WRITE_AUTHENTICATED'] |
        property_flags['WRITE_AUTHENTICATED_SC'] |
        property_flags['WRITE_ENCRYPTED'] |
        property_flags['WRITE_ANYBODY']
    ) != 0

    # post process properties
    encryption_key_size_specified = (properties & property_flags['ENCRYPTION_KEY_SIZE_MASK']) != 0

    # if distinct permissions not used and encyrption key size specified -> set READ/WRITE Encrypted
    if encryption_key_size_specified and not distinct_permissions_used:
        properties |= property_flags['READ_ENCRYPTED'] | property_flags['WRITE_ENCRYPTED']

    # if distinct permissions not used and authentication is requires -> set READ/WRITE Authenticated
    if properties & property_flags['AUTHENTICATION_REQUIRED'] and not distinct_permissions_used:
        properties |= property_flags['READ_AUTHENTICATED'] | property_flags['WRITE_AUTHENTICATED']

    # if distinct permissions not used and authorized is requires -> set READ/WRITE Authorized
    if properties & property_flags['AUTHORIZATION_REQUIRED'] and not distinct_permissions_used:
        properties |= property_flags['READ_AUTHORIZED'] | property_flags['WRITE_AUTHORIZED']

    # determine read/write security requirements
    read_security_level  = 0
    write_security_level = 0 
    read_requires_sc     = False
    write_requires_sc    = False
    if properties & property_flags['READ_AUTHORIZED']:
        read_security_level = 3
    elif properties & property_flags['READ_AUTHENTICATED']:
        read_security_level = 2
    elif properties & property_flags['READ_AUTHENTICATED_SC']:
        read_security_level = 2
        read_requires_sc = True
    elif properties & property_flags['READ_ENCRYPTED']:
        read_security_level = 1
    if properties & property_flags['WRITE_AUTHORIZED']:
        write_security_level = 3
    elif properties & property_flags['WRITE_AUTHENTICATED']:
        write_security_level = 2
    elif properties & property_flags['WRITE_AUTHENTICATED_SC']:
        write_security_level = 2
        write_requires_sc = True
    elif properties & property_flags['WRITE_ENCRYPTED']:
        write_security_level = 1

    # map security requirements to flags
    if read_security_level & 2:
        properties |= property_flags['READ_PERMISSION_BIT_1']
    if read_security_level & 1:
        properties |= property_flags['READ_PERMISSION_BIT_0']
    if read_requires_sc:
        properties |= property_flags['READ_PERMISSION_SC']
    if write_security_level & 2:
        properties |= property_flags['WRITE_PERMISSION_BIT_1']
    if write_security_level & 1:
        properties |= property_flags['WRITE_PERMISSION_BIT_0']
    if write_requires_sc:
        properties |= property_flags['WRITE_PERMISSION_SC']

    return properties

def write_permissions_and_key_size_flags_from_properties(properties):
    return att_flags(properties) & (property_flags['ENCRYPTION_KEY_SIZE_MASK'] | property_flags['WRITE_PERMISSION_BIT_0'] | property_flags['WRITE_PERMISSION_BIT_1'])

def write_8(fout, value):
    fout.write( "0x%02x, " % (value & 0xff))

def write_16(fout, value):
    fout.write('0x%02x, 0x%02x, ' % (value & 0xff, (value >> 8) & 0xff))

def write_uuid(fout, uuid):
    for byte in uuid:
        fout.write( "0x%02x, " % byte)

def write_string(fout, text):
    for l in text.lstrip('"').rstrip('"'):
        write_8(fout, ord(l))

def write_sequence(fout, text):
    parts = text.split()
    for part in parts:
        fout.write("0x%s, " % (part.strip()))

def write_database_hash(fout):
    fout.write("THE-DATABASE-HASH")

def write_indent(fout):
    fout.write("    ")

def read_permissions_from_flags(flags):
    permissions = 0
    if flags & property_flags['READ_PERMISSION_BIT_0']:
        permissions |= 1
    if flags & property_flags['READ_PERMISSION_BIT_1']:
        permissions |= 2
    if flags & property_flags['READ_PERMISSION_SC'] and permissions == 2:
        permissions = 4
    return permissions

def write_permissions_from_flags(flags):
    permissions = 0
    if flags & property_flags['WRITE_PERMISSION_BIT_0']:
        permissions |= 1
    if flags & property_flags['WRITE_PERMISSION_BIT_1']:
        permissions |= 2
    if flags & property_flags['WRITE_PERMISSION_SC'] and permissions == 2:
        permissions = 4
    return permissions

def encryption_key_size_from_flags(flags):
    encryption_key_size = (flags & 0xf000) >> 12
    if encryption_key_size > 0:
        encryption_key_size += 1
    return encryption_key_size

def is_string(text):
    for item in text.split(" "):
        if not all(c in string.hexdigits for c in item):
            return True
    return False

def add_client_characteristic_configuration(properties):
    return properties & (property_flags['NOTIFY'] | property_flags['INDICATE'])

def serviceDefinitionComplete(fout):
    global services
    if current_service_uuid_string:
        # fout.write("\n")
        # update num instances for this service
        count = 1
        if current_service_uuid_string in service_counter:
            count = service_counter[current_service_uuid_string] + 1
        service_counter[current_service_uuid_string] = count
        # add old defines without service counter for first instance for backward compatibility
        if count == 1:
            defines_for_services.append('#define ATT_SERVICE_%s_START_HANDLE 0x%04x' % (current_service_uuid_string, current_service_start_handle))
            defines_for_services.append('#define ATT_SERVICE_%s_END_HANDLE 0x%04x' % (current_service_uuid_string, handle-1))

        # unified defines indicating instance
        defines_for_services.append('#define ATT_SERVICE_%s_%02x_START_HANDLE 0x%04x' % (current_service_uuid_string, count, current_service_start_handle))
        defines_for_services.append('#define ATT_SERVICE_%s_%02x_END_HANDLE 0x%04x' % (current_service_uuid_string, count, handle-1))
        services[current_service_uuid_string+"_" + str(count)] = [current_service_start_handle, handle - 1, count]

def dump_flags(fout, flags):
    global security_permsission
    encryption_key_size = encryption_key_size_from_flags(flags)
    read_permissions    = security_permsission[read_permissions_from_flags(flags)]
    write_permissions   = security_permsission[write_permissions_from_flags(flags)]
    write_indent(fout)
    fout.write('// ')
    first = 1
    if flags & property_flags['READ']:
        fout.write('READ_%s' % read_permissions)
        first = 0
    if flags & (property_flags['WRITE'] | property_flags['WRITE_WITHOUT_RESPONSE']):
        if not first:
            fout.write(', ')
        first = 0
        fout.write('WRITE_%s' % write_permissions)
    if encryption_key_size > 0:
        if not first:
            fout.write(', ')
        first = 0
        fout.write('ENCRYPTION_KEY_SIZE=%u' % encryption_key_size)
    fout.write('\n')

def database_hash_append_uint8(value):
    global database_hash_message
    database_hash_message.append(value)

def database_hash_append_uint16(value):
    global database_hash_message
    database_hash_append_uint8(value & 0xff)
    database_hash_append_uint8((value >> 8) & 0xff)

def database_hash_append_value(value):
    global database_hash_message
    for byte in value:
        database_hash_append_uint8(byte)

def parseService(fout, parts, service_type):
    global handle
    global total_size
    global current_service_uuid_string
    global current_service_start_handle

    serviceDefinitionComplete(fout)

    read_only_anybody_flags = property_flags['READ'];
    
    write_indent(fout)
    fout.write('// 0x%04x %s\n' % (handle, '-'.join(parts)))

    uuid = parseUUID(parts[1])
    uuid_size = len(uuid)
    
    size = 2 + 2 + 2 + uuid_size + 2

    if service_type == 0x2802:
        size += 4

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, service_type)
    write_uuid(fout, uuid)
    fout.write("\n")

    database_hash_append_uint16(handle)
    database_hash_append_uint16(service_type)
    database_hash_append_value(uuid)

    current_service_uuid_string = c_string_for_uuid(parts[1])
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
    
    read_only_anybody_flags = property_flags['READ'];

    uuid = parseUUID(parts[1])
    uuid_size = len(uuid)
    if uuid_size > 2:
        uuid_size = 0

    size = 2 + 2 + 2 + 2 + 4 + uuid_size

    keyUUID = c_string_for_uuid(parts[1])
    keys_to_delete = []

    for (serviceUUID, service) in services.items():
        if serviceUUID.startswith(keyUUID):
            write_indent(fout)
            fout.write('// 0x%04x %s - range [0x%04x, 0x%04x]\n' % (handle, '-'.join(parts), services[serviceUUID][0], services[serviceUUID][1]))

            write_indent(fout)
            write_16(fout, size)
            write_16(fout, read_only_anybody_flags)
            write_16(fout, handle)
            write_16(fout, 0x2802)
            write_16(fout, services[serviceUUID][0])
            write_16(fout, services[serviceUUID][1])
            if uuid_size > 0:
                write_uuid(fout, uuid)
            fout.write("\n")

            database_hash_append_uint16(handle)
            database_hash_append_uint16(0x2802)
            database_hash_append_uint16(services[serviceUUID][0])
            database_hash_append_uint16(services[serviceUUID][1])
            if uuid_size > 0:
                database_hash_append_value(uuid)

            keys_to_delete.append(serviceUUID)
            
            handle = handle + 1
            total_size = total_size + size

    for key in keys_to_delete:
        services.pop(key)


def parseCharacteristic(fout, parts):
    global handle
    global total_size
    global current_characteristic_uuid_string
    global characteristic_indices

    read_only_anybody_flags = property_flags['READ'];

    # enumerate characteristics with same UUID, using optional name tag if available
    current_characteristic_uuid_string = c_string_for_uuid(parts[1]);
    index = 1
    if current_characteristic_uuid_string in characteristic_indices:
        index = characteristic_indices[current_characteristic_uuid_string] + 1
    characteristic_indices[current_characteristic_uuid_string] = index
    if len(parts) > 4:
        current_characteristic_uuid_string += '_' + parts[4].upper().replace(' ','_')
    else:
        current_characteristic_uuid_string += ('_%02x' % index)

    uuid       = parseUUID(parts[1])
    uuid_size  = len(uuid)
    properties = parseProperties(parts[2])
    value = ', '.join([str(x) for x in parts[3:]])

    # reliable writes is defined in an extended properties
    if (properties & property_flags['RELIABLE_WRITE']):
        properties = properties | property_flags['EXTENDED_PROPERTIES']

    write_indent(fout)
    fout.write('// 0x%04x %s - %s\n' % (handle, '-'.join(parts[0:2]), prettyPrintProperties(parts[2])))
    

    characteristic_properties = gatt_characteristic_properties(properties)
    size = 2 + 2 + 2 + 2 + (1+2+uuid_size)
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2803)
    write_8(fout, characteristic_properties)
    write_16(fout, handle+1)
    write_uuid(fout, uuid)
    fout.write("\n")
    total_size = total_size + size

    database_hash_append_uint16(handle)
    database_hash_append_uint16(0x2803)
    database_hash_append_uint8(characteristic_properties)
    database_hash_append_uint16(handle+1)
    database_hash_append_value(uuid)

    handle = handle + 1

    uuid_is_database_hash = len(uuid) == 2 and uuid[0] == 0x2a and uuid[1] == 0x2b

    size = 2 + 2 + 2 + uuid_size
    if uuid_is_database_hash:
        size +=  16
    else:
        if is_string(value):
            size = size + len(value)
        else:
            size = size + len(value.split())

    value_flags = att_flags(properties)

    # add UUID128 flag for value handle
    if uuid_size == 16:
        value_flags = value_flags | property_flags['LONG_UUID'];

    write_indent(fout)
    properties_string = prettyPrintProperties(parts[2])
    if "DYNAMIC" in properties_string:
        fout.write('// 0x%04x VALUE %s - %s\n' % (handle, '-'.join(parts[0:2]), prettyPrintProperties(parts[2])))
    else:
        fout.write('// 0x%04x VALUE %s - %s -'"'%s'"'\n' % (
        handle, '-'.join(parts[0:2]), prettyPrintProperties(parts[2]), value))

    dump_flags(fout, value_flags)

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, value_flags)
    write_16(fout, handle)
    write_uuid(fout, uuid)
    if uuid_is_database_hash:
        write_database_hash(fout)
    else:
        if is_string(value):
            write_string(fout, value)
        else:
            write_sequence(fout,value)

    fout.write("\n")
    defines_for_characteristics.append('#define ATT_CHARACTERISTIC_%s_VALUE_HANDLE 0x%04x' % (current_characteristic_uuid_string, handle))
    handle = handle + 1

    if add_client_characteristic_configuration(properties):
        # use write permissions and encryption key size from attribute value and set READ_ANYBODY | READ | WRITE | DYNAMIC
        flags  = write_permissions_and_key_size_flags_from_properties(properties)
        flags |= property_flags['READ']
        flags |= property_flags['WRITE']
        flags |= property_flags['WRITE_WITHOUT_RESPONSE']
        flags |= property_flags['DYNAMIC']
        size = 2 + 2 + 2 + 2 + 2

        write_indent(fout)
        fout.write('// 0x%04x CLIENT_CHARACTERISTIC_CONFIGURATION\n' % (handle))

        dump_flags(fout, flags)

        write_indent(fout)
        write_16(fout, size)
        write_16(fout, flags)
        write_16(fout, handle)
        write_16(fout, 0x2902)
        write_16(fout, 0)
        fout.write("\n")

        database_hash_append_uint16(handle)
        database_hash_append_uint16(0x2902)

        defines_for_characteristics.append('#define ATT_CHARACTERISTIC_%s_CLIENT_CONFIGURATION_HANDLE 0x%04x' % (current_characteristic_uuid_string, handle))
        handle = handle + 1


    if properties & property_flags['RELIABLE_WRITE']:
        size = 2 + 2 + 2 + 2 + 2
        write_indent(fout)
        fout.write('// 0x%04x CHARACTERISTIC_EXTENDED_PROPERTIES\n' % (handle))
        write_indent(fout)
        write_16(fout, size)
        write_16(fout, read_only_anybody_flags)
        write_16(fout, handle)
        write_16(fout, 0x2900)
        write_16(fout, 1)   # Reliable Write
        fout.write("\n")

        database_hash_append_uint16(handle)
        database_hash_append_uint16(0x2900)
        database_hash_append_uint16(1)

        handle = handle + 1

def parseGenericDynamicDescriptor(fout, parts, uuid, name):
    global handle
    global total_size
    global current_characteristic_uuid_string

    properties = parseProperties(parts[1])
    size = 2 + 2 + 2 + 2

    # use write permissions and encryption key size from attribute value and set READ, WRITE, DYNAMIC, READ_ANYBODY
    flags  = write_permissions_and_key_size_flags_from_properties(properties)
    flags |= property_flags['READ']
    flags |= property_flags['WRITE']
    flags |= property_flags['DYNAMIC']

    write_indent(fout)
    fout.write('// 0x%04x %s-%s\n' % (handle, name, '-'.join(parts[1:])))

    dump_flags(fout, flags)

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, flags)
    write_16(fout, handle)
    write_16(fout, uuid)
    fout.write("\n")

    database_hash_append_uint16(handle)
    database_hash_append_uint16(uuid)

    defines_for_characteristics.append('#define ATT_CHARACTERISTIC_%s_%s_HANDLE 0x%04x' % (current_characteristic_uuid_string, name, handle))
    handle = handle + 1

def parseGenericDynamicReadOnlyDescriptor(fout, parts, uuid, name):
    global handle
    global total_size
    global current_characteristic_uuid_string

    properties = parseProperties(parts[1])
    size = 2 + 2 + 2 + 2

    # use write permissions and encryption key size from attribute value and set READ, DYNAMIC, READ_ANYBODY
    flags  = write_permissions_and_key_size_flags_from_properties(properties)
    flags |= property_flags['READ']
    flags |= property_flags['DYNAMIC']

    write_indent(fout)
    fout.write('// 0x%04x %s-%s\n' % (handle, name, '-'.join(parts[1:])))

    dump_flags(fout, flags)

    write_indent(fout)
    write_16(fout, size)
    write_16(fout, flags)
    write_16(fout, handle)
    write_16(fout, uuid)
    fout.write("\n")

    database_hash_append_uint16(handle)
    database_hash_append_uint16(uuid)

    defines_for_characteristics.append('#define ATT_CHARACTERISTIC_%s_%s_HANDLE 0x%04x' % (current_characteristic_uuid_string, name, handle))
    handle = handle + 1

def parseServerCharacteristicConfiguration(fout, parts):
    parseGenericDynamicDescriptor(fout, parts, 0x2903, 'SERVER_CONFIGURATION')

def parseCharacteristicFormat(fout, parts):
    global handle
    global total_size

    read_only_anybody_flags = property_flags['READ'];

    identifier = parts[1]
    presentation_formats[identifier] = handle
    # print("format '%s' with handle %d\n" % (identifier, handle))

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
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2904)
    write_sequence(fout, format)
    write_sequence(fout, exponent)
    write_uuid(fout, unit)
    write_sequence(fout, name_space)
    write_uuid(fout, description)
    fout.write("\n")

    database_hash_append_uint16(handle)
    database_hash_append_uint16(0x2904)

    handle = handle + 1


def parseCharacteristicAggregateFormat(fout, parts):
    global handle
    global total_size

    read_only_anybody_flags = property_flags['READ'];
    size = 2 + 2 + 2 + 2 + (len(parts)-1) * 2

    write_indent(fout)
    fout.write('// 0x%04x CHARACTERISTIC_AGGREGATE_FORMAT-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2905)
    for identifier in parts[1:]:
        if not identifier in presentation_formats:
            print(parts)
            print("ERROR: identifier '%s' in CHARACTERISTIC_AGGREGATE_FORMAT undefined" % identifier)
            sys.exit(1)
        format_handle = presentation_formats[identifier]
        write_16(fout, format_handle)
    fout.write("\n")

    database_hash_append_uint16(handle)
    database_hash_append_uint16(0x2905)

    handle = handle + 1

def parseExternalReportReference(fout, parts):
    global handle
    global total_size

    read_only_anybody_flags = property_flags['READ'];
    size = 2 + 2 + 2 + 2 + 2
    
    report_uuid = int(parts[2], 16)
    
    write_indent(fout)
    fout.write('// 0x%04x EXTERNAL_REPORT_REFERENCE-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2907)
    write_16(fout, report_uuid)
    fout.write("\n")
    handle = handle + 1

def parseReportReference(fout, parts):
    global handle
    global total_size

    read_only_anybody_flags = property_flags['READ'];
    size = 2 + 2 + 2 + 2 + 1 + 1
    
    report_id = parts[2]
    report_type = parts[3]

    write_indent(fout)
    fout.write('// 0x%04x REPORT_REFERENCE-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2908)
    write_sequence(fout, report_id)
    write_sequence(fout, report_type)
    fout.write("\n")
    handle = handle + 1

def parseNumberOfDigitals(fout, parts):
    global handle
    global total_size

    read_only_anybody_flags = property_flags['READ'];
    size = 2 + 2 + 2 + 2 + 1

    no_of_digitals = parts[1]

    write_indent(fout)
    fout.write('// 0x%04x NUMBER_OF_DIGITALS-%s\n' % (handle, '-'.join(parts[1:])))
    write_indent(fout)
    write_16(fout, size)
    write_16(fout, read_only_anybody_flags)
    write_16(fout, handle)
    write_16(fout, 0x2909)
    write_sequence(fout, no_of_digitals)
    fout.write("\n")
    handle = handle + 1

def parseLines(fname_in, fin, fout):
    global handle
    global total_size

    line_count = 0;
    for line in fin:
        line = line.strip("\n\r ")
        line_count += 1

        if line.startswith("//"):
            fout.write("    //" + line.lstrip('/') + '\n')
            continue

        if line.startswith("#import"):
            imported_file = ''
            parts = re.match('#import\\s+<(.*)>\\w*',line)
            if parts and len(parts.groups()) == 1:
                imported_file = parts.groups()[0]
            parts = re.match('#import\\s+"(.*)"\\w*',line)
            if parts and len(parts.groups()) == 1:
                imported_file = parts.groups()[0]
            if len(imported_file) == 0:
                print('ERROR: #import in file %s - line %u neither <name.gatt> nor "name.gatt" form', (fname_in, line_count))
                continue

            imported_file = getFile( imported_file )
            print("Importing %s" % imported_file)
            try:
                imported_fin = codecs.open (imported_file, encoding='utf-8')
                fout.write('\n\n    // ' + line + ' -- BEGIN\n')
                parseLines(imported_file, imported_fin, fout)
                fout.write('    // ' + line + ' -- END\n')
            except IOError as e:
                print('ERROR: Import failed. Please check path.')

            continue

        if line.startswith("#TODO"):
            print ("WARNING: #TODO in file %s - line %u not handled, skipping declaration:" % (fname_in, line_count))
            print ("'%s'" % line)
            fout.write("// " + line + '\n')
            continue
            
        if len(line) == 0:
            continue
        
        f = io.StringIO(line)
        parts_list = csv.reader(f, delimiter=',', quotechar='"')
        
        for parts in parts_list:
            for index, object in enumerate(parts):
                parts[index] = object.strip().lstrip('"').rstrip('"')
                
            if parts[0] == 'PRIMARY_SERVICE':
                parsePrimaryService(fout, parts)
                continue

            if parts[0] == 'SECONDARY_SERVICE':
                parseSecondaryService(fout, parts)
                continue

            if parts[0] == 'INCLUDE_SERVICE':
                parseIncludeService(fout, parts)
                continue

            # 2803
            if parts[0] == 'CHARACTERISTIC':
                parseCharacteristic(fout, parts)
                continue

            # 2900 Characteristic Extended Properties

            # 2901
            if parts[0] == 'CHARACTERISTIC_USER_DESCRIPTION':
                parseGenericDynamicDescriptor(fout, parts, 0x2901, 'USER_DESCRIPTION')
                continue


            # 2902 Client Characteristic Configuration - automatically included in Characteristic if
            # notification / indication is supported
            if parts[0] == 'CLIENT_CHARACTERISTIC_CONFIGURATION':
                continue

            # 2903
            if parts[0] == 'SERVER_CHARACTERISTIC_CONFIGURATION':
                parseGenericDynamicDescriptor(fout, parts, 0x2903, 'SERVER_CONFIGURATION')
                continue

            # 2904
            if parts[0] == 'CHARACTERISTIC_FORMAT':
                parseCharacteristicFormat(fout, parts)
                continue

            # 2905
            if parts[0] == 'CHARACTERISTIC_AGGREGATE_FORMAT':
                parseCharacteristicAggregateFormat(fout, parts)
                continue

            # 2906
            if parts[0] == 'VALID_RANGE':
                parseGenericDynamicReadOnlyDescriptor(fout, parts, 0x2906, 'VALID_RANGE')
                continue

            # 2907 
            if parts[0] == 'EXTERNAL_REPORT_REFERENCE':
                parseExternalReportReference(fout, parts)
                continue

            # 2908
            if parts[0] == 'REPORT_REFERENCE':
                parseReportReference(fout, parts)
                continue

            # 2909
            if parts[0] == 'NUMBER_OF_DIGITALS':
                parseNumberOfDigitals(fout, parts)
                continue

            # 290A
            if parts[0] == 'VALUE_TRIGGER_SETTING':
                parseGenericDynamicDescriptor(fout, parts, 0x290A, 'VALUE_TRIGGER_SETTING')
                continue

            # 290B
            if parts[0] == 'ENVIRONMENTAL_SENSING_CONFIGURATION':
                parseGenericDynamicDescriptor(fout, parts, 0x290B, 'ENVIRONMENTAL_SENSING_CONFIGURATION')
                continue

            # 290C
            if parts[0] == 'ENVIRONMENTAL_SENSING_MEASUREMENT':
                parseGenericDynamicReadOnlyDescriptor(fout, parts, 0x290C, 'ENVIRONMENTAL_SENSING_MEASUREMENT')
                continue

            # 290D 
            if parts[0] == 'ENVIRONMENTAL_SENSING_TRIGGER_SETTING':
                parseGenericDynamicDescriptor(fout, parts, 0x290D, 'ENVIRONMENTAL_SENSING_TRIGGER_SETTING')
                continue

            print("WARNING: unknown token: %s\n" % (parts[0]))

def parse(fname_in, fin, fname_out, tool_path, fout):
    global handle
    global total_size
    
    fout.write(header.format(fname_out, fname_in, tool_path))
    fout.write('{\n')
    write_indent(fout)
    fout.write('// ATT DB Version\n')
    write_indent(fout)
    fout.write('1,\n')
    fout.write("\n")
 
    parseLines(fname_in, fin, fout)

    serviceDefinitionComplete(fout)
    write_indent(fout)
    fout.write("// END\n");
    write_indent(fout)
    write_16(fout,0)
    fout.write("\n")
    total_size = total_size + 2
    
    fout.write("}; // total size %u bytes \n" % total_size);

def listHandles(fout):
    fout.write('\n\n')
    fout.write('//\n')
    fout.write('// list service handle ranges\n')
    fout.write('//\n')
    for define in defines_for_services:
        fout.write(define)
        fout.write('\n')
    fout.write('\n')
    fout.write('//\n')
    fout.write('// list mapping between characteristics and handles\n')
    fout.write('//\n')
    for define in defines_for_characteristics:
        fout.write(define)
        fout.write('\n')

def getFile( fileName ):
    for d in include_paths:
        fullFile = os.path.normpath(d + os.sep + fileName) # because Windows exists
        # print("test %s" % fullFile)
        if os.path.isfile( fullFile ) == True:
            return fullFile
    print ("'{0}' not found".format( fileName ))
    print ("Include paths: %s" % ", ".join(include_paths))
    exit(-1)


btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')
default_includes = [os.path.normpath(path) for path in [ 
    btstack_root + '/src/', 
    btstack_root + '/src/ble/gatt-service/',
    btstack_root + '/src/le-audio/gatt-service/',
    btstack_root + '/src/mesh/gatt-service/'
]]

parser = argparse.ArgumentParser(description='BLE GATT configuration generator for use with BTstack')

parser.add_argument('-I', action='append', nargs=1, metavar='includes', 
        help='include search path for .gatt service files and bluetooth_gatt.h (default: %s)' % ", ".join(default_includes))
parser.add_argument('gattfile', metavar='gattfile', type=str,
        help='gatt file to be compiled')
parser.add_argument('hfile', metavar='hfile', type=str,
        help='header file to be generated')

args = parser.parse_args()

# add include path arguments
if args.I != None:
    for d in args.I:
        include_paths.append(os.path.normpath(d[0]))

# append default include paths
include_paths.extend(default_includes)

try:
    # read defines from bluetooth_gatt.h
    gen_path = getFile( 'bluetooth_gatt.h' )
    bluetooth_gatt = read_defines(gen_path)

    filename = args.hfile
    fin  = codecs.open (args.gattfile, encoding='utf-8')

    # pass 1: create temp .h file
    ftemp = tempfile.TemporaryFile(mode='w+t')
    parse(args.gattfile, fin, filename, sys.argv[0], ftemp)
    listHandles(ftemp)

    # calc GATT Database Hash
    db_hash = aes_cmac(bytearray(16), database_hash_message)
    if isinstance(db_hash, str):
        # python2
        db_hash_sequence = [('0x%02x' % ord(i)) for i in db_hash]
    elif isinstance(db_hash, bytes):
        # python3
        db_hash_sequence = [('0x%02x' % i) for i in db_hash]
    else:
        print("AES CMAC returns unexpected type %s, abort" % type(db_hash))
        sys.exit(1)
    # reverse hash to get little endian
    db_hash_sequence.reverse()
    db_hash_string = ', '.join(db_hash_sequence) + ', '

    # pass 2: insert GATT Database Hash
    fout = open (filename, 'w')
    ftemp.seek(0)
    for line in ftemp:
        fout.write(line.replace('THE-DATABASE-HASH', db_hash_string))
    fout.close()
    ftemp.close()

    print('Created %s' % filename)

except IOError as e:
    parser.print_help() 
    print(e)
    sys.exit(1)

print('Compilation successful!\n')

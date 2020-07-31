#!/usr/bin/env python3
#
# Parase and dump UUID128 in various formats

import codecs
import io
import os
import re
import string
import sys

usage = '''
Usage: ./uuid128_formats.py UUID128
'''

def twoByteLEFor(value):
    return [ (value & 0xff), (value >> 8)]

def parseUUID128(uuid):
    parts = re.match("([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})([0-9A-Fa-f]{4})", uuid)
    uuid_bytes = []
    for i in range(8, 0, -1):
        uuid_bytes = uuid_bytes + twoByteLEFor(int(parts.group(i),16))
    return uuid_bytes

if (len(sys.argv) < 2):
    print(usage)
    sys.exit(1)

uuid128 = sys.argv[1]
uuid_bytes = parseUUID128(uuid128)
print('UUID128:       %s' % uuid128)
print('little endian: %s' % ', '.join( [hex(i) for i in uuid_bytes] ))
print('big endian:    %s' % ', '.join( [hex(i) for i in reversed(uuid_bytes)] ))

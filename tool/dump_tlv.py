#!/usr/bin/env python
# BlueKitchen GmbH (c) 2017

# primitive dump for .tlv format

import re
import sys
import time
import datetime

def read_net_32(f):
    a = f.read(1)
    if a == '':
    	return -1
    b = f.read(1)
    if b == '':
    	return -1
    c = f.read(1)
    if c == '':
    	return -1
    d = f.read(1)
    if d == '':
    	return -1
    return ord(a) << 24 | ord(b) << 16 | ord(c) << 8 | ord(d)

def as_hex(data):
	str_list = []
	for byte in data:
	    str_list.append("{0:02x} ".format(ord(byte)))
	return ''.join(str_list)

if len(sys.argv) == 1:
	print 'Dump TLV file'
	print 'Copyright 2017, BlueKitchen GmbH'
	print ''
	print 'Usage: ', sys.argv[0], 'file.tlv'
	exit(0)

infile = sys.argv[1]

with open (infile, 'rb') as fin:
	pos = 0
	try:
		# check header
		magic_0 = read_net_32(fin)
		magic_1 = read_net_32(fin)
		if magic_0 != 0x42547374 or magic_1 != 0x61636b00:
			print("%x" % magic_0)
			print("%x" % magic_1)
			print ("Not a valid BTstack .tlv file\n")
			exit(0)
		pos += 8
		print("Valid .tlv file")
		while True:
			tag     = read_net_32(fin)
			if tag < 0:
				break
			pos += 4
			len     = read_net_32(fin)
			pos += 4
			packet  = fin.read(len)
			pos += len
			print('%04x: ' % tag + as_hex(packet))
		print("Done")

	except TypeError:
		print ("Error parsing tlv at offset %u (%x)." % (pos, pos))


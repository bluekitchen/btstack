#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

# primitive dump for PacketLogger format

# APPLE PacketLogger
# typedef struct {
# 	uint32_t	len;
# 	uint32_t	ts_sec;
# 	uint32_t	ts_usec;
# 	uint8_t		type;   // 0xfc for note
# }

import re
import sys
import time
import datetime

packet_types = [ "CMD =>", "EVT <=", "ACL =>", "ACL <="]

def read_net_32(f):
    a = f.read(1)
    b = f.read(1)
    c = f.read(1)
    d = f.read(1)
    return ord(a) << 24 | ord(b) << 16 | ord(c) << 8 | ord(d)

def as_hex(data):
	str_list = []
	for byte in data:
	    str_list.append("{0:02x} ".format(ord(byte)))
	return ''.join(str_list)

if len(sys.argv) == 1:
	print 'Dump PacketLogger file'
	print 'Copyright 2014, BlueKitchen GmbH'
	print ''
	print 'Usage: ', sys.argv[0], 'hci_dump.pklg'
	exit(0)

infile = sys.argv[1]

with open (infile, 'rb') as fin:
	try:
		while True:
			len     = read_net_32(fin)
			ts_sec  = read_net_32(fin)
			ts_usec = read_net_32(fin)
			type    = ord(fin.read(1))
			packet_len = len - 9;
			packet  = fin.read(packet_len)
			time    = "[%s.%03u]" % (datetime.datetime.fromtimestamp(ts_sec).strftime("%Y-%m-%d %H:%M:%S"), ts_usec / 1000)
			if type == 0xfc:
				print time, "LOG", packet
				continue
			if type <= 0x03:
				print time, packet_types[type], as_hex(packet)
	except TypeError:
		exit(0) 


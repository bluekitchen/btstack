#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2014

# primitive dump for PacketLogger format

# APPLE PacketLogger
# typedef struct {
# 	uint32_t	len;
# 	uint32_t	ts_sec;
# 	uint32_t	ts_usec;
# 	uint8_t		type;   // 0xfc for note
# }

import sys
import datetime
import struct

packet_types = [ "CMD =>", "EVT <=", "ACL =>", "ACL <="]

def read_header(f):
	bytes_read = f.read(13)
	if bytes_read:
		return struct.unpack(">IIIB", bytes_read)
	else:
		return (-1, 0, 0, 0)

def as_hex(data):
	str_list = []
	for byte in data:
	    str_list.append("{0:02x} ".format(byte))
	return ''.join(str_list)

if len(sys.argv) == 1:
	print ('Dump PacketLogger file')
	print ('Copyright 2014, BlueKitchen GmbH')
	print ('')
	print ('Usage: ', sys.argv[0], 'hci_dump.pklg')
	exit(0)

infile = sys.argv[1]

with open (infile, 'rb') as fin:
	pos = 0
	try:
		while True:
			(len, ts_sec, ts_usec, type) = read_header(fin)
			if len < 0:
				break
			packet_len = len - 9;
			if (packet_len > 66000):
				print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
				break
			packet  = fin.read(packet_len)
			pos     = pos + 4 + len
			time    = "[%s.%03u]" % (datetime.datetime.fromtimestamp(ts_sec).strftime("%Y-%m-%d %H:%M:%S"), ts_usec / 1000)
			if type == 0xfc:
				print (time, "LOG", packet.decode('ascii'))
				continue
			if type <= 0x03:
				print (time, packet_types[type], as_hex(packet))
	except TypeError:
		print ("Error parsing pklg at offset %u (%x)." % (pos, pos))

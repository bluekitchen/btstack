#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2014

# Report SM Pairing Random packets with value zero


import re
import sys
import time
import datetime

packet_types = [ "CMD =>", "EVT <=", "ACL =>", "ACL <="]

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

def check_file(infile):
	with open (infile, 'rb') as fin:
		pos = 0
		warning = True
		try:
			while True:
				len     = read_net_32(fin)
				if len < 0:
					break
				ts_sec  = read_net_32(fin)
				ts_usec = read_net_32(fin)
				type    = ord(fin.read(1))
				packet_len = len - 9;
				if (packet_len > 66000):
					print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
					break
				packet  = fin.read(packet_len)
				pos     = pos + 4 + len
				time    = "[%s.%03u]" % (datetime.datetime.fromtimestamp(ts_sec).strftime("%Y-%m-%d %H:%M:%S"), ts_usec / 1000)
				if type not in [0x02, 0x03]:
					continue
				packet_boundary_flags = (ord(packet[1]) >> 4) & 3
				if packet_boundary_flags not in [0x00, 0x02]:
					continue
				channel = ord(packet[6]) | (ord(packet[7]) << 8)
				if channel != 0x06:
					continue
				smp_command = ord(packet[8])
				if smp_command != 4:
					continue
				random = [ ord(i) for i in packet[9:25] ]
				num_zeros = random.count(0)
				if num_zeros != 16:
					continue
				if warning:
					print("%s contains SM Pairing Random command with Zeroes:" % infile)
					warning = False
				print (time, packet_types[type], as_hex(packet))

			if not warning:
				print("")

		except TypeError:
			print ("Error parsing pklg at offset %u (%x)." % (pos, pos))

if len(sys.argv) == 1:
	print ('Usage: ' + sys.argv[0] + ' hci_dump.pklg')
	exit(0)

for infile in sys.argv[2:]:
	check_file(infile)

infile = sys.argv[1]


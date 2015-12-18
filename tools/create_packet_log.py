#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

# convert log output to PacketLogger format
# can be viewed with Wireshark

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

packet_counter = 0
last_time = None

def chop(line, prefix):
	if line.startswith(prefix):
		return line[len(prefix):]
	return None

def str2hex(value):
	if value:
		return int(value, 16)
	return None

def arrayForNet32(value):
	return bytearray([value >> 24, (value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff])

def generateTimestamp(t):
	global last_time
	global packet_counter

	# use last_time if time missing for this entry
	if not t:
		t = last_time
	if t:
		last_time = t
		# handle ms
		try: 
			(t1, t2) = t.split('.')
			if t1 and t2:
				t_obj = time.strptime(t1, "%Y-%m-%d %H:%M:%S")
				tv_sec  = int(time.mktime(t_obj)) 
				tv_usec = int(t2) * 1000
				return (tv_sec, tv_usec)
		except ValueError:
			# print 'Cannot parse time', t
			pass
	packet_counter += 1
	return (packet_counter, 0)

def dumpPacket(fout, timestamp, type, data):
	length = 9 + len(data)
	(tv_sec, tv_usec) = generateTimestamp(timestamp)
	fout.write(arrayForNet32(length))
	fout.write(arrayForNet32(tv_sec))
	fout.write(arrayForNet32(tv_usec))
	fout.write(bytearray([type]))
	fout.write(data)

def handleHexPacket(fout, timestamp, type, text):
	try:
		data = bytearray(list(map(str2hex, text.strip().split())))
		dumpPacket(fout, timestamp, type, data)
	except TypeError:
		print('Cannot parse hexdump', text.strip())

if len(sys.argv) == 1:
	print('BTstack Console to PacketLogger converter')
	print('Copyright 2014, BlueKitchen GmbH')
	print('')
	print('Usage: ', sys.argv[0], 'asci-log-file.txt [hci_dump.pkgl]')
	print('Converted hci_dump.pklg can be viewed with Wireshark and OS X PacketLogger')
	exit(0)

infile = sys.argv[1]
outfile = 'hci_dump.pklg'
if len(sys.argv) > 2:
	outfile = sys.argv[2]

# with open(outfile, 'w') as fout:
with open (outfile, 'wb') as fout:
	with open (infile, 'rt') as fin:
		packet_counter = 0
		for line in fin:
			timestamp = None
			parts = parts = re.match('\[(.*)\] (.*)', line)
			if parts and len(parts.groups()) == 2:
				(timestamp, line) = parts.groups()
			rest = chop(line,'CMD => ')
			if rest:
				handleHexPacket(fout, timestamp, 0, rest)
				continue
			rest = chop(line,'EVT <= ')
			if rest:
				handleHexPacket(fout, timestamp, 1, rest)
				continue
			rest = chop(line,'ACL => ')
			if rest:
				handleHexPacket(fout, timestamp, 2, rest)
				continue
			rest = chop(line,'ACL <= ')
			if rest:
				handleHexPacket(fout, timestamp, 3, rest)
				continue
			rest = chop(line,'LOG -- ')
			if rest:
				line = rest
			dumpPacket(fout, timestamp, 0xfc, line.encode('ascii'))

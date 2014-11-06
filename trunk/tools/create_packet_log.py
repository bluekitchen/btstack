#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

# convert log output to PacketLogger format
# can be viewed with Wireshark

import re
import sys

def chop(line, prefix):
	if line.startswith(prefix):
		return line[len(prefix):]
	return None

def str2hex(value):
	if value:
		return int(value, 16)
	return None

def log(time, type, incmoing, text):
	data = map(str2hex, text.strip(" ").split(" "))
	print time, type, data

# print 'Number of arguments:', len(sys.argv), 'arguments.'
# print 'Argument List:', str(sys.argv) 

infile  = 'hci_console.txt'
if len(sys.argv) > 1:
	infile = sys.argv[1]

outfile = 'hci_dump.pklg'
if len(sys.argv) > 2:
	outfile = sys.argv[2]

# with open(outfile, 'w') as fout:
with open (infile, 'rb') as fin:
	for line in fin:
		time = None
		parts = parts = re.match('\[(.*)\] (.*)', line)
		if parts and len(parts.groups()) == 2:
			(time, line) = parts.groups()
		rest = chop(line,'CMD => ')
		if rest:
			log(time, 1, 0, rest)
		rest = chop(line,'EVT <= ')
		if rest:
			log(time, 4, 1, rest)
		rest = chop(line,'ACL => ')
		if rest:
			log(time, 4, 0, rest)
		rest = chop(line,'ACL <= ')
		if rest:
			log(time, 4, 1, rest)

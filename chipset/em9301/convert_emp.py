#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2018
import sys

usage = '''This script converts a set of configurations and patches in .emp format for EM9304
into a single C files to be used with BTstack. It also configures the controller for 8 connections 
and enables support for LE Data Length Extension.

Usage:
$ ./convert_emp.py container.emp
'''

header = '''/** 
 * BASENAME.h converted from BASENAME.emp
 */

#ifndef BASENAME_H
#define BASENAME_H

#include <stdint.h>
extern const uint8_t  container_blob_data[];
extern const uint32_t container_blob_size;
extern const char *   container_blob_name;

#endif
'''

code_start = '''/** 
 * BASENAME.c converted from BASENAME.bin
 * Size: SIZE bytes 
 *
 * BTstack: added config for 8 connections + 251 packet len
 */

#include <stdint.h>

const char *   container_blob_name = "BASENAME";

const uint8_t  container_blob_data[] = {
'''

code_end = '''
    // configure EM9304 for 8 connections, data length 251
    0x33, 0x39, 0x6d, 0x65, 0x24, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x14,
    0x11, 0x0c, 0x00, 0x00, 0x56, 0x62, 0xc3, 0xd4, 0x30, 0x02, 0x00, 0x00,
    0xfb, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x5a, 0x00, 0x0c, 0xf3

};
const uint32_t container_blob_size = sizeof(container_blob_data);
'''

def convert_emp(basename):
	emp_name = basename + '.emp'
	print ('Reading %s' % emp_name)

	with open (emp_name, 'rb') as fin:
		firm = fin.read()
		size = len(firm)

		# reduce size by 4 as it ends with four zero bytes that indicate the end of the file
		size -= 4

		# don't write .h file as we would need to store its name in btstack_chipset_em9301.c, too
		# with open(basename + '.h', 'w') as fout:
		# 	fout.write(header.replace('BASENAME',basename));

		with open(basename + '.c', 'w') as fout:
			fout.write(code_start.replace('BASENAME',basename).replace('SIZE',str(size)));
			fout.write('    ')
			for i in range(0,size):
				if i % 1000 == 0:
					print ('- Write %05u/%05u' % (i, size))
				byte = firm[i]
				fout.write("0x{0:02x}, ".format(byte))
				if (i & 0x0f) == 0x0f:
					fout.write('\n    ')
			fout.write(code_end);
			print ('Done\n') 

# check usage: 1 param
if not len(sys.argv) == 2:
    print(usage)
    sys.exit(1)

name = sys.argv[1]
basename = name.replace('.emp','')
convert_emp(basename)

#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2026
import os
import sys

usage = '''This script converts a binary file into C files to be used with BTstack, similar to xxd.

Usage:
$ ./create_c_array.py file [path_to_output_folder]

Creates BASENAME.c and BASENAME.h. The header declares the generated buffer
and its size.
'''

code_start = '''
/** 
 * BASENAME.c converted from BASENAME
 */

#include "BASENAME.h"

const uint8_t BASENAME_buffer[] = {
'''

code_end = '''
};

const int BASENAME_size = sizeof(BASENAME_buffer);

'''

header_start = '''
/**
 * BASENAME.h converted from BASENAME
 */

#ifndef HEADER_GUARD
#define HEADER_GUARD

#include <stdint.h>

extern const uint8_t BASENAME_buffer[];
extern const int BASENAME_size;

#endif
'''

def convert_bin(input_path, basename, output_path, header_path):
	with open (input_path, 'rb') as fin:
		firm = fin.read()
		size = len(firm)

		print (f'Size:     {size}')
		with open(header_path, 'w') as hout:
			hout.write(header_start
				.replace('BASENAME', basename)
				.replace('HEADER_GUARD', basename.upper() + '_H'))

		with open(output_path, 'w') as fout:
			fout.write(code_start.replace('BASENAME',basename))
			fout.write('    ')
			for i in range(0,size):
				# if i % 10000 == 0:
				#	print ('- Write %05u/%05u' % (i, size))
				byte = firm[i]
				fout.write("0x{0:02x}, ".format(byte))
				if (i & 0x0f) == 0x0f:
					fout.write('\n    ')
			fout.write(code_end.replace('BASENAME', basename))
			print ('Done')

# check usage: 1 param
if len(sys.argv) < 2:
    print(usage)
    sys.exit(1)

input_path = sys.argv[1]
basename = os.path.basename(input_path).replace('.','_')

if len(sys.argv) >= 3:
    output_path = os.path.join(sys.argv[2], basename + ".c")
else:
    output_path = basename + ".c"
header_path = os.path.splitext(output_path)[0] + ".h"

print(f"Input:    {input_path}")
print(f"Basename: {basename}")
print(f"Code:     {output_path}")
print(f"Header:   {header_path}")

convert_bin(input_path, basename, output_path, header_path)

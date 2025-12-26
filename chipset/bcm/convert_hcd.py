#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2025
import os
import sys

usage = '''This script converts the PatchRAM file in .hcd format for  Broadcom/Cypress/Infineon Controllers
into C files to be used with BTstack.

Usage:
$ ./convert_hcd.py patchram.hcd [path_to_output_folder]
'''

code_start = '''
/** 
 * BASENAME.c converted from 
 * BASENAME.hcd
 */

#include "btstack_chipset_bcm.h"

const char brcm_patch_version[] = "BASENAME";

const uint8_t  brcm_patchram_buf[] = {
'''

code_end = '''
};

const int brcm_patch_ram_length = sizeof(brcm_patchram_buf);

'''

def convert_bin(input_path, basename, output_path):
	with open (input_path, 'rb') as fin:
		firm = fin.read()
		size = len(firm)

		print (f'Size:     {size}')

		with open(output_path, 'w') as fout:
			fout.write(code_start.replace('BASENAME',basename))
			fout.write('    ')
			for i in range(0,size):
				if i % 10000 == 0:
					print ('- Write %05u/%05u' % (i, size))
				byte = firm[i]
				fout.write("0x{0:02x}, ".format(byte))
				if (i & 0x0f) == 0x0f:
					fout.write('\n    ')
			fout.write(code_end)
			print ('Done\n') 

# check usage: 1 param
if len(sys.argv) < 2:
    print(usage)
    sys.exit(1)

input_path = sys.argv[1]
basename = os.path.splitext(os.path.basename(input_path))[0]

if len(sys.argv) >= 3:
	output_path = sys.argv[2] + "/" + basename + ".c"
else:
	output_path = basename + ".c"

print(f"Input:    {input_path}")
print(f"Basename: {basename}")
print(f"Output:   {output_path}")

convert_bin(input_path, basename, output_path)

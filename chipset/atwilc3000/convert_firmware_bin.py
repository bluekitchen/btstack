#!/usr/bin/env python
# BlueKitchen GmbH (c) 2012-2014

# avr-objcopy -I ihex -O binary hci_581_active_uart.hex hci_581_active_uart.bin

# requires IntelHex package https://pypi.python.org/pypi/IntelHex
# docs: http://python-intelhex.readthedocs.io/en/latest/

from intelhex import IntelHex
import glob

usage = '''This script converts the HCI Firmware in .bin format for Atmel WILC3000
into C files to be used with BTstack.
'''

header = '''
/** 
 * BASENAME.h converted from BASENAME.bin 
 */

#ifndef __BASENAME_H
#define __BASENAME_H

#include <stdint.h>

extern const uint8_t  atwilc3000_fw_data[];
extern const uint32_t atwilc3000_fw_size;
extern const char *   atwilc3000_fw_name;

#endif
'''

code_start = '''
/** 
 * BASENAME.c converted from BASENAME.bin
 */

#include "BASENAME.h"

const char *   atwilc3000_fw_name = "BASENAME";

const uint8_t  atwilc3000_fw_data[] = {
'''

code_end = '''
};
const uint32_t atwilc3000_fw_size = sizeof(atwilc3000_fw_data);
'''

def convert_bin(basename):
	bin_name = basename + '.bin'
	print ('Reading %s' % bin_name)

	with open (bin_name, 'rb') as fin:
		firm = fin.read()
		size = len(firm)
		print ('Size %u', size)

		with open(basename + '.h', 'w') as fout:
			fout.write(header.replace('BASENAME',basename));

		with open(basename + '.c', 'w') as fout:
			fout.write(code_start.replace('BASENAME',basename));
			fout.write('    ')
			for i in range(0,size):
				if i % 10000 == 0:
					print ('- Write %05u/%05u' % (i, size))
				byte = ord(firm[i])
				fout.write("0x{0:02x}, ".format(byte))
				if (i & 0x0f) == 0x0f:
					fout.write('\n    ')
			fout.write(code_end);
			print ('Done\n') 

files =  glob.glob('*.bin')
if not files:
    print(usage)
    sys.exit(1) 

# convert each of them
for name in files:
	basename = name.replace('.bin','')
	convert_bin(basename)

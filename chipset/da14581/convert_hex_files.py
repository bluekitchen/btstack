#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2012-2014

# avr-objcopy -I ihex -O binary hci_581_active_uart.hex hci_581_active_uart.bin

# requires IntelHex package https://pypi.python.org/pypi/IntelHex
# docs: http://python-intelhex.readthedocs.io/en/latest/

from intelhex import IntelHex
import glob
import sys

usage = '''This script converts the HCI Firmware in .hex format for Dialog Semiconductor
into C files to be used with BTstack.
'''

header = '''
/** 
 * BASENAME.h converted from BASENAME.hex 
 */

#ifndef BASENAME_H
#define BASENAME_H

#include <stdint.h>

extern const uint8_t  da14581_fw_data[];
extern const uint32_t da14581_fw_size;
extern const char *   da14581_fw_name;

#endif
'''

code_start = '''
/** 
 * BASENAME.c converted from BASENAME.hex
 */

#include "BASENAME.h"

const char *   da14581_fw_name = "BASENAME";

const uint8_t  da14581_fw_data[] = {
'''

code_end = '''
};
const uint32_t da14581_fw_size = sizeof(da14581_fw_data);
'''

def convert_hex(basename):
	hex_name = basename + '.hex'
	print('Reading %s' % hex_name)
	ih = IntelHex(hex_name)
	# f = open(basename + '.txt', 'w') # open file for writing
	# ih.dump(f)                    # dump to file object
	# f.close()                     # close file
	size = 	ih.maxaddr() - ih.minaddr() + 1
	print('- Start: %x' % ih.minaddr())
	print('- End:   %x' % ih.maxaddr())

	with open(basename + '.h', 'w') as fout:
		fout.write(header.replace('BASENAME',basename));

	with open(basename + '.c', 'w') as fout:
		fout.write(code_start.replace('BASENAME',basename));
		fout.write('    ')
		for i in range(0,size):
			if i % 1000 == 0:
				print('- Write %05u/%05u' % (i, size))
			byte = ih[ih.minaddr() + i]
			fout.write("0x{0:02x}, ".format(byte))
			if (i & 0x0f) == 0x0f:
				fout.write('\n    ')
		fout.write(code_end);
		print ('Done\n') 


files =  glob.glob('*.hex')
if not files:
    print(usage)
    sys.exit(1) 

# convert each of them
for name in files:
	basename = name.replace('.hex','')
	convert_hex(basename)

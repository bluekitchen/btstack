#!/usr/bin/env python
# Matthias Ringwald (c) 2012

import glob
import re
import sys

print '''
CC256x init script conversion tool for use with BTstack, v0.1
Copyright 2012 Matthias Ringwald
'''

usage = '''This script converts init scripts for TI's
CC2560/CC2567 chipset from the provided BTS file for use with BTstack .

Please download the Service Pack for your module from http://processors.wiki.ti.com/index.php/CC256x_Downloads 
Then, unzip it and copy the *.bts file into this folder and start the script again.
'''

fartext = '''
#if defined(__GNUC__) && (__MSP430X__ > 0)
__attribute__((section (".fartext")))
#endif
'''

data_indent = '    '

def read_little_endian_16(f):
    low  = f.read(1)
    if low == "":
        return -1
    high = f.read(1)
    return ord(high) << 8 | ord(low) 


def convert_bts(bts_file):
    array_name = 'cc256x'
    c_file   = bts_file.replace('bts', 'c')    
    
    with open(c_file, 'w') as fout:
    
        with open (bts_file, 'rb') as fin:
    
            print "Converting {0:32} to {1}".format(bts_file, c_file)
            
            header = fin.read(32)
            if header[0:4] != 'BTSB':
                print 'Error', bts_file, 'is not a valid .BTS file'
                sys.exit(1)

            part_size = 0
            have_eHCILL = False
            parts = 0
            str_list = []
            part_strings = []
            part_sizes   = []

            while True:
                action_type = read_little_endian_16(fin)
                action_size = read_little_endian_16(fin)
                action_data = fin.read(action_size)
                
                if (action_type == 1):  # hci command

                    opcode = (ord(action_data[2]) << 8) | ord(action_data[1])
                    if opcode == 0xFF36:
                        continue    # skip baud rate command
                    if opcode == 0xFD0C:
                        have_eHCILL = True
                        
                    counter = 0
                    str_list.append(data_indent)
                    for byte in action_data:
                        str_list.append("0x{0:02x}, ".format(ord(byte)))
                        counter = counter + 1
                        if (counter != 15):
                            continue
                        counter = 0
                        str_list.append("\n")
                        str_list.append(data_indent)
                    str_list.append("\n\n")
                    
                    part_size = part_size + action_size

                    # 30 kB chunks
                    if part_size < 30 * 1024:
                        continue

                    part_strings.append(''.join(str_list))
                    part_sizes.append(part_size)
                    parts += 1

                    str_list = []
                    part_size = 0

                if (action_type == 6):  # comment
                    action_data = action_data.rstrip('\0')
                    str_list.append(data_indent)
                    str_list.append("// " + action_data + "\n")
                    
                if (action_type < 0):   # EOF
                    break;
                    

            if not have_eHCILL:
                str_list.append('\n')
                str_list.append(data_indent)
                str_list.append('// BTstack: added HCI_VS_Sleep_Mode_Configurations 0xFD0C template for eHCILL\n');
                str_list.append(data_indent)
                str_list.append('0x01, 0x0c, 0xfd, 9 , 1, 0, 0,  0xff, 0xff, 0xff, 0xff, 100, 0\n');
                    
            part_strings.append(''.join(str_list))
            part_sizes.append(part_size)
            parts += 1

            fout.write( '// init script created from {0}\n'.format(bts_file))
            fout.write( '#include <stdint.h>\n')

            part = 0
            size = 0
            for part_size in part_sizes:
                part += 1
                size += part_size
                print "- part", part, "size", part_size

            if not have_eHCILL:
                print "- adding eHCILL template"

            print '- total size', size

            part = 0
            for part_text in part_strings:
                part += 1
                suffix = ''
                
                if part == 1:
                    fout.write( fartext )

                if (part > 1):
                    suffix = '_{0}'.format(part)
                    fout.write('#if defined(__GNUC__) && (__MSP430X__ > 0)\n')
                    fout.write('};\n')
                    fout.write('__attribute__((section (".fartext")))\n')

                fout.write('const uint8_t {0}_init_script{1}[] = {2}\n\n'.format(array_name, suffix, '{'))

                if (part > 1):
                    fout.write('#endif\n')

                fout.write(part_text)

            
            fout.write('};\n\n')

            fout.write('const uint32_t {0}_init_script_size = {1};\n\n'.format(array_name,size));
            # fout.write('void main() {0} printf("size {1}\\n", {2}_init_script_size); {3}'.format('{', '%u', array_name,'}'));

# get list of *.bts files
files =  glob.glob('*.bts')
if not files:
    print usage
    sys.exit(1) 

# convert each of them
for name in files:
    convert_bts(name)

# done
print '\nConversion(s) successful!\n'
    



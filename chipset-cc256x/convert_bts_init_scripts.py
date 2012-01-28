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
    
        fout.write( '// init script created from {0}\n'.format(bts_file))
        fout.write( '#include <stdint.h>\n')
        fout.write( fartext )
        fout.write( 'const uint8_t {0}_init_script[] = {1}\n'.format(array_name, '{'))

        with open (bts_file, 'rb') as fin:
    
            print "Converting {0:32} to {1}".format(bts_file, c_file)
            
            header = fin.read(32)
            if header[0:4] != 'BTSB':
                print 'Error', bts_file, 'is not a valid .BTS file'
                sys.exit(1)

            second_part = False
            size = 0
            have_eHCILL = False
            
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
                    fout.write("    ")
                    for byte in action_data:
                        fout.write("0x{0:02x}, ".format(ord(byte)))
                        counter = counter + 1
                        if (counter != 15):
                            continue
                        counter = 0
                        fout.write("\n    ")
                    fout.write("\n\n")
                    
                    size = size + action_size                               
                    if second_part:
                       continue
                    if size < 20*1024:
                        continue
                        
                    fout.write ('};\n')
                    fout.write (fartext)
                    fout.write ('const uint8_t {0}_init_script_2[] =\n'.format(array_name))
                    fout.write ('{\n')
                    second_part = True

                if (action_type == 6):  # comment
                    action_data = action_data.rstrip('\0')
                    fout.write("    // " + action_data + "\n")
                    
                if (action_type < 0):   # EOF
                    break;
                    
            if not have_eHCILL:
                print "Adding eHCILL template"
                fout.write('// BTstack: added HCI_VS_Sleep_Mode_Configurations 0xFD0C template for eHCILL\n');
                fout.write('0x01, 0x0c, 0xfd, 9 , 1, 0, 0,  0xff, 0xff, 0xff, 0xff, 100, 0\n');
                    
            fout.write('};\n\n') 
            if second_part:
                fout.write('const uint32_t {0}_init_script_size = sizeof({0}_init_script) + sizeof({0}_init_script_2);\n'.format(array_name))
            else:
                fout.write('const uint32_t {0}_init_script_size = sizeof({0}_init_script);\n'.format(array_name))

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
    



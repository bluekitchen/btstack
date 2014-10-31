#!/usr/bin/env python
# BlueKitchen GmbH (c) 2012-2014

# documentation for TI Vendor Specific commands:
# http://processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands

import glob
import re
import sys

print '''
CC256x init script conversion tool for use with BTstack, v0.1
Copyright 2012-2014 BlueKitchen GmbH
'''

usage = '''This script perpares init scripts for TI's
CC256x chipsets for use with BTstack .

Please download the Service Pack for your module from http://processors.wiki.ti.com/index.php/CC256x_Downloads 
Then, unzip it and copy the *.bts file into this folder and start the script again.
'''

fartext = '''
#if defined(__GNUC__) && defined(__MSP430X__) && (__MSP430X__ > 0)
__attribute__((section (".fartext")))
#endif
#ifdef __AVR__
__attribute__((__progmem__))
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

            # assert script contains templates for configuration by BTstack
            have_eHCILL = False
            have_power_vector_gfsk = False;
            have_power_vector_edr2 = False;
            have_power_vector_edr3 = False;
            have_class2_single_power = False;

            parts = 0
            str_list = []
            part_strings = []
            part_sizes   = []
            additions = []

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
                    if opcode == 0xFD82:
                        modulation_type = ord(action_data[4])
                        if modulation_type == 0:
                            have_power_vector_gfsk = True
                        elif modulation_type == 1:
                            have_power_vector_edr2 + True
                        elif modulation_type == 2:
                            have_power_vector_edr3 = True
                    if opcode == 0xFD80:
                        # add missing power command templates
                        if not have_power_vector_gfsk:
                            additions.append("- added HCI_VS_SET_POWER_VECTOR(GFSK) template")
                            str_list.append(data_indent)
                            str_list.append('// BTstack: added HCI_VS_SET_POWER_VECTOR(GFSK) 0xFD82 template\n');
                            str_list.append(data_indent)
                            str_list.append("0x01, 0x82, 0xfd, 0x14, 0x00, 0x9c, 0x18, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xdc,\n");
                            str_list.append(data_indent)
                            str_list.append("0xe6, 0xf0, 0xfa, 0x04, 0x0e, 0x18, 0xff, 0x00, 0x00,\n");
                            have_power_vector_gfsk = True;                            
                        if not have_power_vector_edr2:
                            additions.append("- added HCI_VS_SET_POWER_VECTOR(EDR2) template")
                            str_list.append(data_indent)
                            str_list.append('// BTstack: added HCI_VS_SET_POWER_VECTOR(EDR2) 0xFD82 template\n');
                            str_list.append(data_indent)
                            str_list.append("0x01, 0x82, 0xfd, 0x14, 0x01, 0x9c, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xd8, \n");
                            str_list.append(data_indent)
                            str_list.append("0xe2, 0xec, 0xf6, 0x00, 0x0a, 0x14, 0xff, 0x00, 0x00\n");
                            have_power_vector_edr2 = True;                            
                        if not have_power_vector_edr3:
                            additions.append("- added HCI_VS_SET_POWER_VECTOR(EDR3) template")
                            str_list.append(data_indent)
                            str_list.append('// BTstack: added HCI_VS_SET_POWER_VECTOR(EDR3) 0xFD82 for EDR3 template\n');
                            str_list.append(data_indent)
                            str_list.append("0x01, 0x82, 0xfd, 0x14, 0x02, 0x9c, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xd8,\n");
                            str_list.append(data_indent)
                            str_list.append("0xe2, 0xec, 0xf6, 0x00, 0x0a, 0x14, 0xff, 0x00, 0x00,\n");
                            have_power_vector_edr3 = True;                            
                        if not have_class2_single_power:
                            additions.append("- added HCI_VS_SET_CLASS2_SINGLE_POWER template")
                            str_list.append(data_indent)
                            str_list.append('// BTstack: added HCI_VS_SET_CLASS2_SINGLE_POWER 0xFD87 template\n');
                            str_list.append(data_indent)
                            str_list.append("0x01, 0x87, 0xfd, 0x03, 0x0d, 0x0d, 0x0d, \n");
                            have_class2_single_power = True;                            

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
                additions.append("- added eHCILL template")
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

            print '- total size', size

            print "\n".join(additions)

            # raise alaram if not all power commands have been incorporated
            all_power_commands_provided = have_power_vector_gfsk and have_power_vector_edr2 and have_power_vector_edr3 and have_class2_single_power
            if not all_power_commands_provided:
                print "\nWARNING: init script doesn't contain a calibration sequence. Please report on the BTstack Dveloper mailing list\n"

            part = 0
            for part_text in part_strings:
                part += 1
                suffix = ''
                
                if part == 1:
                    fout.write( fartext )

                if (part > 1):
                    suffix = '_{0}'.format(part)
                    fout.write('#if defined(__GNUC__) && defined(__GNUC__) && (__MSP430X__ > 0)\n')
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
    



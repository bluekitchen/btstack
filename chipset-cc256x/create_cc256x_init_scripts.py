#!/usr/bin/env python
#

usage = '''This script extracts the init scripts needed to use TI's
CC2560/CC2567 with BTstack from the Stellaris Bluetopia SDK.

Please download and install the Bluetopia SDK available at
http://www.ti.com/tool/sw-dk-em2-2560b on a Windows system
and copy 'Stellarware/Bluetopia/btpsvend/BTPSVEND.c' 
into the current directory. Then start the script again.
'''

import re
import sys

print '''
CC256x init script extraction for use with BTstack, v0.1
Copyright 2011 Matthias Ringwald
'''

def extract_script(fin, patch_name, chipset, array_name):
    
    fartext = '''
#if defined(__GNUC__) && (__MSP430X__ > 0)
__attribute__((section (".fartext")))
#endif
'''

    filename = '{0}_init_script.c'.format(chipset)

    fout = open (filename,'w')
    
    fout.write( '// init script for {0} extracted from Stellaris Bluetopia SDK\n'.format(chipset))
    fout.write( '#include <stdint.h>\n')
    fout.write( fartext )
    fout.write( 'const uint8_t {1}_init_script[] =\n'.format(chipset,array_name))
    
    in_script = False
    second_part = False
    size = 0
    
    for line in f:
        line = line.strip("\n\r ")
        if not in_script:
            if line.find('{0}[]'.format(patch_name)) >= 0:
                in_script = True
        else:

            if line.find('};') >= 0 or line.find('} ;') >= 0:
                if chipset == "cc2560":
                    fout.write(",\n")
                fout.write('// BTstack: add HCI_VS_Sleep_Mode_Configurations 0xFD0C template for eHCILL\n');
                fout.write('0x01, 0x0c, 0xfd, 9 , 1, 0, 0,  0xff, 0xff, 0xff, 0xff, 100, 0\n');
                fout.write('};\n')
                break;

            if not second_part:
                size = size + 16
                if size > 20*1024:
                    fout.write ('};\n')
                    fout.write (fartext)
                    fout.write ('const uint8_t {1}_init_script_2[] =\n'.format(chipset,array_name))
                    fout.write ('{\n')
                    second_part = True

            fout.write(line + '\n')

    if second_part:
        fout.write('const uint32_t {0}_init_script_size = sizeof({0}_init_script) + sizeof({0}_init_script_2);\n'.format(array_name))
    else:
        fout.write('const uint32_t {0}_init_script_size = sizeof({0}_init_script);\n'.format(array_name))
    fout.close()
    print 'Created', filename


try:
    f = open ('BTPSVEND.c', 'r')
except IOError as e:
    print usage
    sys.exit(1)

# extract the CC2560 script first
extract_script(f, 'Patch', 'cc2560', 'cc256x')

# then the CC2567 script
extract_script(f, 'PatchXETU', 'cc2567', 'cc256x')

print '\nExtraction successful!\n'
    



#!/usr/bin/env python3
#
# Create Makefile.inc file for all source folders
# Copyright 2017 BlueKitchen GmbH

import sys
import os

makefile_inc_header = '''# Makefile to collect all C source files of {folder}

{var_name} = \\
'''

folders = [
'src',
'src/ble',
'src/ble/gatt-service',
'src/classic',
]

# get btstack root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')

def create_makefile_inc(path):
    global btstack_root

    folder_path = btstack_root + '/' + path + '/'

    # write makefile based on header and list
    with open(folder_path + "Makefile.inc", "wt") as fout:
        var_name = path.upper().replace('/','_').replace('-','_')+'_FILES'
        fout.write(makefile_inc_header.format(var_name=var_name,folder=path))

        # get all .c files in folder
        for file in sorted(os.listdir(folder_path)):
            if not file.endswith(".c"):
                continue
            fout.write('    %s \\\n' % file)

        fout.write('\n')

# create all makefile.inc
if (len(sys.argv) > 1):
    path = sys.argv[1]
    print('Creating Makefile.inc for %s' % path)
    create_makefile_inc(path)
else:
    for path in folders:
        print('Creating Makefile.inc for %s' % path)
        create_makefile_inc(path)

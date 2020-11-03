#!/usr/bin/env python3
#
# Create project files for all BTstack embedded examples in WICED/apps/btstack

import os
import re
import shutil
import subprocess
import sys

# build all template
build_all = '''
SUBDIRS =  \\
%s

all:
\techo Building all examples
\tfor dir in $(SUBDIRS); do \\
\t$(MAKE) -C $$dir || exit 1; \\
\tdone

clean:
\techo Cleaning all ports
\tfor dir in $(SUBDIRS); do \\
\t$(MAKE) -C $$dir clean; \\
\tdone
'''

# 
le_examples = [
'ancs_client_demo.c', 
'gap_le_advertisements.c', 
'gatt_battery_query.c', 
'gatt_browser.c', 
'hog_keyboard_demo.c', 
'hog_mouse_demo.c', 
'gatt_counter.c', 
'gatt_streamer.c', 
'le_streamer_client.c', 
'led_counter.c', 
'sm_pairing_central.c', 
'sm_pairing_peripheral.c', 
]

# get script path
script_path = os.path.abspath(os.path.dirname(sys.argv[0])) + '/../'

# get btstack root
btstack_root = script_path + '../../'

# path to examples
examples_embedded = btstack_root + 'example/'

# path to generated example projects
projects_path = script_path + "example/"

# path to template
template_path = script_path + 'example/template/'

print("Creating example projects:")

# iterate over btstack examples
example_files = os.listdir(examples_embedded)

examples = []

for file in example_files:
    if not file.endswith(".c"):
        continue
    if not file in le_examples:
        continue
    example = file[:-2]
    examples.append(example)

    # create folder
    project_folder = projects_path + example + "/"
    if not os.path.exists(project_folder):
        os.makedirs(project_folder)

    # check if .gatt file is present
    gatt_path = examples_embedded + example + ".gatt"
    gatt_h = ""
    if os.path.exists(gatt_path):
        gatt_h = example+'.h'

    # create Makefile
    shutil.copyfile(template_path + 'Makefile', project_folder + 'Makefile')

    # create upload.cfg
    with open(project_folder + 'upload.cfg', 'wt') as fout:
        with open(template_path + 'upload.cfg', 'rt') as fin:
            for line in fin:
                if 'flash write_image erase le_counter_flash.elf' in line:
                    fout.write('flash write_image erase %s_flash.elf\n' % example)
                    continue
                fout.write(line)

    # create Makefile
    with open(project_folder + 'Makefile', 'wt') as fout:
        with open(template_path + 'Makefile', 'rt') as fin:
            for line in fin:
                if 'le_counter.h: ${BTSTACK_ROOT}/example/le_counter.gatt' in line:
                    fout.write('%s.h: ${BTSTACK_ROOT}/example/%s.gatt\n' % (example,example))
                    continue
                if 'all: le_counter.h wilc3000_ble_firmware.h' in line:
                    if len(gatt_h):
                        fout.write("all: %s.h wilc3000_ble_firmware.h\n" % example)
                    else:
                        fout.write("all: wilc3000_ble_firmware.h\n")
                    continue
                fout.write(line)

    # create config.mk
    with open(project_folder + 'config.mk', 'wt') as fout:
        with open(template_path + 'config.mk', 'rt') as fin:
            for line in fin:
                if 'TARGET_FLASH=le_counter_flash.elf' in line:
                    fout.write('TARGET_FLASH=%s_flash.elf\n' % example)
                    continue
                if 'TARGET_SRAM=le_counter_sram.elf' in line:
                    fout.write('TARGET_SRAM=%s_sram.elf\n' % example)
                    continue
                if 'CSRCS+=${BTSTACK_ROOT_CONFIG}/example/le_counter.c' in line:
                    fout.write('CSRCS+=${BTSTACK_ROOT_CONFIG}/example/%s.c\n' % example)
                    continue
                if 'INC_PATH += ${BTSTACK_ROOT_CONFIG}/port/samv71-xplained-atwilc3000/example/template' in line:
                    fout.write('INC_PATH += ${BTSTACK_ROOT_CONFIG}/port/samv71-xplained-atwilc3000/example/%s\n' % example)
                    continue
                fout.write(line)

    print("- %s" % example)

with open(projects_path+'Makefile', 'wt') as fout:
    fout.write(build_all % ' \\\n'.join(examples))

print("Projects are ready for compile in example folder. See README for details.")

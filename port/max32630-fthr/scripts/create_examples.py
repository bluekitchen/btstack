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

# get script path
script_path = os.path.abspath(os.path.dirname(sys.argv[0])) + '/../'

# get btstack root
btstack_root = script_path + '../../'

## pick correct init script based on your hardware
# - init script for CC2564B
cc256x_init_script = 'bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c'

subprocess.call("make -f ../Makefile -C src " + cc256x_init_script, shell=True)

# fetch init script
# print("Creating init script %s" % cc256x_init_script)
# make_template = 'make -f {BTSTACK_ROOT}chipset/cc256x/Makefile.inc -C {SCRIPT_PATH}src/ {INIT_SCRIPT} BTSTACK_ROOT={BTSTACK_ROOT}'
# make_command = make_template.format(BTSTACK_ROOT=btstack_root, SCRIPT_PATH=script_path, INIT_SCRIPT=cc256x_init_script)
# print(make_command)
# subprocess.call(make_command)

# path to examples
examples_embedded = btstack_root + 'example/'

# path to generated example projects
projects_path = script_path + "example/"

# path to template
template_path = script_path + 'example/template/Makefile'

print("Creating example projects:")

# iterate over btstack examples
example_files = os.listdir(examples_embedded)

examples = []

for file in example_files:
    if not file.endswith(".c"):
        continue
    if file in ['panu_demo.c', 'sco_demo_util.c', 'ant_test.c']:
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

    # create makefile
    with open(project_folder + 'Makefile', 'wt') as fout:
        with open(template_path, 'rt') as fin:
            for line in fin:
                if 'PROJECT=spp_and_le_streamer' in line:
                    fout.write('PROJECT=%s\n' % example)
                    continue
                if 'all: spp_and_le_streamer.h' in line:
                    if len(gatt_h):
                        fout.write("all: %s\n" % gatt_h)
                    continue
                fout.write(line)

    print("- %s" % example)

with open(projects_path+'Makefile', 'wt') as fout:
    fout.write(build_all % ' \\\n'.join(examples))

print("Projects are ready for compile in example folder. See README for details.")

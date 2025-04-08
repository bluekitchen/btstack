#!/usr/bin/env python3
#
# Create project files for all BTstack embedded examples in WICED/apps/btstack

import os
import re
import shutil
import subprocess
import sys

from create_project_makefile import createProjectMakefile

# build all template
build_all = '''
SUBDIRS =  \\
%s

include template/Categories.mk

all:
\t@echo Building all examples
\tfor dir in $(SUBDIRS); do \\
\t\t$(MAKE) -C $$dir || exit 1; \\
\tdone

general:
\t@echo Building general examples
\tfor dir in $(EXAMPLES_GENERAL); do \\
\t\t$(MAKE) -C $$dir || exit 1; \\
\tdone

classic:
\t@echo Building classic examples
\tfor dir in $(EXAMPLES_CLASSIC_ONLY); do \\
\t\t$(MAKE) -C $$dir || exit 1; \\
\tdone

ble:
\t@echo Building ble examples
\tfor dir in $(EXAMPLES_LE_ONLY); do \\
\t\t$(MAKE) -C $$dir || exit 1; \\
\tdone

dual:
\t@echo Building dual examples
\tfor dir in $(EXAMPLES_DUAL_MODE); do \\
\t\t$(MAKE) -C $$dir || exit 1; \\
\tdone

clean:
\t@echo Cleaning all examples
\tfor dir in $(SUBDIRS); do \\
\t\t$(MAKE) -C $$dir clean; \\
\tdone
'''

# get script path
print(os.path.dirname(sys.argv[0]))
script_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), '..'))
print(script_path)
# get btstack root
btstack_root = os.path.abspath(os.path.join(script_path, '../..'))
print(btstack_root)
# path to examples
examples_embedded = os.path.join(btstack_root, 'example')

# path to generated example projects
projects_path = os.path.join(script_path, "example")

print("Creating example projects:")

# iterate over btstack examples
example_files = os.listdir(examples_embedded)

examples = []

for file in example_files:
    if not file.endswith(".c"):
        continue
    if file in ['panu_demo.c', 'sco_demo_util.c', 'ant_test.c', 'mesh_node_demo.c']:
        continue
    example = file[:-2]
    examples.append(example)

    # create folder
    project_folder = os.path.join(projects_path, example)
    if not os.path.exists(project_folder):
        os.makedirs(project_folder)

    # check if .gatt file is present
    gatt_path = os.path.join(examples_embedded, example + ".gatt")
    gatt_h = ""
    if os.path.exists(gatt_path):
        gatt_h = example + '.h'

    createProjectMakefile(project_folder, btstack_root, gatt_h)

    print("- %s" % example)

with open(os.path.join(projects_path, 'Makefile'), 'wt') as fout:
    fout.write(build_all % ' \\\n'.join(examples))

print("Projects are ready for compile in example folder. See README for details.")

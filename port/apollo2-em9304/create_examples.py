#!/usr/bin/env python3
#
# Create project files for all BTstack embedded examples in AmbiqSuite/boards/apollo2_evb_am_ble

import os
import shutil
import sys
import time
import subprocess

makefile_gatt_add_on = '''
$(CONFIG)/EXAMPLE.o: $(CONFIG)/EXAMPLE.h
$(CONFIG)/EXAMPLE.h: ../../../../../third_party/btstack/example/EXAMPLE.gatt
\t../../../../../third_party/btstack/tool/compile_gatt.py $^ $@
'''

# get script path
script_path = os.path.abspath(os.path.dirname(sys.argv[0]))

# validate AmbiqSuite root by reading VERSION.txt
am_root = script_path + "/../../../../"
am_version_txt = ""
try:
    with open(am_root + 'VERSION.txt', 'r') as fin:
        am_version_txt = fin.read()  # Read the contents of the file into memory.
except:
    pass
if len(am_version_txt) == 0:
    print("Cannot find AmbiqSuite root. Make sure BTstack is checked out as AmbiqSuite/third/btstack");
    sys.exit(1)

# show WICED version
print("Found AmbiqSuite SDK version: %s" % am_version_txt)

# path to examples
examples_embedded = script_path + "/../../example/"

# path to example template
example_template = script_path + "/example-template/"

# path to AmbiqSuite/boards/apollo2_evb_am_ble/examples
apps_btstack = am_root + "/boards/apollo2_evb_am_ble/examples/"

print("Creating examples in /boards/apollo2_evb_am_ble/examples:")

LE_EXAMPLES = ["ancs_client_demo", "gap_le_advertisements", "gatt_battery_query", "gatt_browser", "gatt_counter", "gatt_streamer", "le_streamer_client", "sm_pairing_peripheral", "sm_pairing_central"]

# iterate over btstack examples
for example in LE_EXAMPLES:

    # create example folder
    apps_folder = apps_btstack + "btstack_" + example + "/"
    if not os.path.exists(apps_folder):
        os.makedirs(apps_folder)

    # copy project makefile
    shutil.copyfile(example_template + "Makefile", apps_folder + "Makefile");

    # create GCC folder
    gcc_folder = apps_folder + "/gcc/"
    if not os.path.exists(gcc_folder):
        os.makedirs(gcc_folder)

    # add rule to generate .h file in src folder if .gatt is present
    gatt_path = examples_embedded + example + ".gatt"
    need_h = False
    if os.path.exists(gatt_path):
        # create src folder
        src_folder = apps_folder + "/src/"
        if not os.path.exists(src_folder):
            os.makedirs(src_folder)
        need_h = True

    # copy makefile and update project name
    with open(gcc_folder + 'Makefile', "wt") as fout:
        with open(example_template + 'gcc/Makefile', "rt") as fin:
            for line in fin:
                fout.write(line.replace('TARGET := EXAMPLE', 'TARGET := ' + example))
        if (need_h):
            fout.write(makefile_gatt_add_on.replace("EXAMPLE",example))
            fout.write("INCLUDES += -I${CONFIG}\n")

    # copy other files
    for file in ['startup_gcc.c', 'btstack_template.ld']:
        shutil.copyfile(example_template + "gcc/" + file, apps_folder + "/gcc/" + file);


    print("- %s" % example)

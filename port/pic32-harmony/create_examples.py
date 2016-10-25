#!/usr/bin/env python
#
# Create project files for all BTstack embedded examples in harmony/apps/btstack

import os
import shutil
import sys
import time
import subprocess
import re

gatt_update_template = '''#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT=$DIR/../../../framework/btstack
echo "Creating EXAMPLE.h from EXAMPLE.gatt"
$BTSTACK_ROOT/tool/compile_gatt.py $BTSTACK_ROOT/example/EXAMPLE.gatt $DIR/EXAMPLE.h
'''

# get script path
script_path = os.path.abspath(os.path.dirname(sys.argv[0]))

# validate Harmony root by reading version.txt
harmony_root = script_path + "/../../../../"
print harmony_root
harmony_version = ""
try:
    with open(harmony_root + 'config/harmony.hconfig', 'r') as fin:
        for line in fin:
            m = re.search('default \"(.*)\"', line)
            if m and len(m.groups()) == 1:
                harmony_version = m.groups(1)
                break
except:
    pass

if len(harmony_version) == 0:
    print("Cannot find Harmony root. Make sure BTstack is checked out as harmony/vx_xx/frameworks/btstack")
    sys.exit(1)

# show Harmony version
print("Found Harmony version %s" % harmony_version)

# path to examples
examples_embedded = script_path + "/../../example/"

# path to WICED/apps/btstack
apps_btstack = harmony_root + "/apps/btstack/"

print("Creating examples in apps/btstack")

# iterate over btstack examples
for file in os.listdir(examples_embedded):
    if not file.endswith(".c"):
        continue
    if file.endswith("sco_demo_util.c"):
        continue
    example = file[:-2]

    # recreate folder
    apps_folder = apps_btstack + example + "/"
    shutil.rmtree(apps_folder, ignore_errors=True)
    os.makedirs(apps_folder)

    # create update_gatt.sh if .gatt file is present
    gatt_path = examples_embedded + example + ".gatt"
    if os.path.exists(gatt_path):
        update_gatt_script = apps_folder + "update_gatt_db.sh"
        with open(update_gatt_script, "wt") as fout:
            fout.write(gatt_update_template.replace("EXAMPLE", example))        
        os.chmod(update_gatt_script, 0o755)
        subprocess.call(update_gatt_script + "> /dev/null", shell=True)
        print("- %s including compiled GATT DB" % example)
    else:
        print("- %s" % example)


    # create $example.X
    appX_folder = apps_folder + example + ".X/"
    os.makedirs(appX_folder)

    # create makefife
    shutil.copyfile(script_path + "/app.X/Makefile", appX_folder + "Makefile")

    nbproject_folder = appX_folder = appX_folder + "nbproject/"
    os.makedirs(nbproject_folder)

    template_path = script_path + "/app.X/nbproject/"
    for file in os.listdir(template_path):
        src = template_path + file
        dst = nbproject_folder + file
        # copy private folder
        if file == "private":
            shutil.copytree(src, dst)
            continue
        # replace app.X and spp_and_le_counter.c
        with open(src, 'r') as fin:
            template = fin.read()
        with open(dst, 'wt') as fout:
            # template = template.replace('app', example)
            template = template.replace("<itemPath>../../../example/spp_and_le_counter.c", "<itemPath>../../../../framework/btstack/example/" + example + ".c")
            template = template.replace(">../../../../driver", ">../../../../framework/driver")
            template = template.replace(">../../../../system", ">../../../../framework/system")
            template = template.replace(">../../../chipset",   ">../../../../framework/btstack/chipset")
            template = template.replace(">../../../platform",  ">../../../../framework/btstack/platform")
            template = template.replace(">../../../3rd-party", ">../../../../framework/btstack/3rd-party")
            template = template.replace(">../../../src",       ">../../../../framework/btstack/src")
            template = template.replace(">../src",             ">../../../../framework/btstack/port/pic32-harmony/src")
            template = template.replace("app.X", example+".X")
            template = template.replace(";../../../..", ";../../../../framework")
            template = template.replace(";../../../chipset",   ";../../../../framework/btstack/chipset")
            template = template.replace(";../../../platform",  ";../../../../framework/btstack/platform")
            template = template.replace(";../../../src",       ";../../../../framework/btstack/src")
            template = template.replace(";../../../3rd-party", ";../../../../framework/btstack/3rd-party")
            template = template.replace(";../src",             ";../../../../framework/btstack/port/pic32-harmony/src")
            template = template.replace(">../../../../../bin/framework/peripheral", ">../../../../bin/framework/peripheral")
            template = template.replace('value=".;', 'value="..;')
            fout.write(template)

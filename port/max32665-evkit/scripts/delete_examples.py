#!/usr/bin/env python3
#
# Delete project files for all BTstack embedded examples in local port/esp32 folder

import os
import shutil
import sys
import time
import subprocess

# get script path
script_path = os.path.abspath(os.path.dirname(sys.argv[0]))

# path to examples
examples_embedded = script_path + "/../../../example/"

# path to port/esp32
apps_btstack = "example/"

print("Deleting examples in local folder")

# iterate over btstack examples
for file in os.listdir(examples_embedded):
    if not file.endswith(".c"):
        continue
    example = file[:-2]
    apps_folder = apps_btstack + example + "/"
    if os.path.exists(apps_folder):
        shutil.rmtree(apps_folder)
    print("- %s" % example)


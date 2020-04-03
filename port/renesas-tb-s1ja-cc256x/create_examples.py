#!/usr/bin/env python
#
# Create project files for all BTstack embedded examples in local port/renesas-tb-s1ja-cc256x folder

import os
import shutil
import sys
import time
import subprocess

# use for CC2564B
# init_script = 'bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c'

# use for CC2564C
init_script = 'bluetooth_init_cc2564C_1.3.c'

gatt_update_bat_template = '''python.exe BTSTACK_ROOT\\tool\\compile_gatt.py BTSTACK_ROOT\\example\\EXAMPLE.gatt PROJECT_SRC\\EXAMPLE.h
'''

gatt_update_sh_template = '''#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT=$DIR/../../../..
echo "Creating EXAMPLE.h from EXAMPLE.gatt"
$BTSTACK_ROOT/tool/compile_gatt.py $BTSTACK_ROOT/example/EXAMPLE.gatt $DIR/src/EXAMPLE.h
'''

def fetch_init_script():
    # print("Fetch CC256x initscript: " + init_script)
    # subprocess.call("make -f ../../chipset/cc256x/Makefile.inc BTSTACK_ROOT=../.. " + init_script, shell=True)
    print("Using local CC256x initscript: " + init_script)

def create_folder(path):
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path)

def create_examples(port_folder, suffix):

	# btstack root
    btstack_root = os.path.abspath(port_folder + "/../..")

    # path to examples
    examples_embedded = btstack_root + "/example/"

    # path to project template
    project_template = port_folder + "/template/btstack_example/"

    # path to example projects
    example_folder = port_folder + "/example" + suffix + "/"


    print("Creating example folder")
    create_folder(example_folder)

    print("Creating example projects in example folder")

    # iterate over btstack examples
    for file in os.listdir(examples_embedded):
        if not file.endswith(".c"):
            continue
        if file in ['panu_demo.c', 'sco_demo_util.c', 'ant_test.c']:
            continue

        example = file[:-2]
        gatt_path = examples_embedded + example + ".gatt"

        # create project folder
        project_folder = example_folder + example + "/"
        create_folder(project_folder)

        # copy some folders
        for folder in ['.settings', 'script', 'synergy', 'synergy_cfg']:
            src = project_template + folder
            dst = project_folder   + folder
            shutil.copytree(src, dst)

        # copy some files
        for file in ['configuration.xml', 'synergy_cfg.txt', 'TB_S1JA.pincfg']:
            shutil.copy(project_template + file, project_folder)

        # create src folder
        src_folder = project_folder + 'src/'
        create_folder(src_folder)

        # copy files skipping example.c and gatt_streamer_server.h
        for file in ['btstack_config.h', 'hal_entry.c']:
            shutil.copy(project_template + "src/" + file, src_folder)

        # copy synergy_gen
        src = project_template + 'src/synergy_gen'
        dst = project_folder   + 'src/synergy_gen'
        shutil.copytree(src, dst)

        # copy example file
        shutil.copyfile(examples_embedded + example + '.c', src_folder + example + ".c")

        # copy init script
        shutil.copy(port_folder + "/" + init_script, src_folder)

        # add sco_demo_util.c for audio examples
        if example in ['hfp_ag_demo','hfp_hf_demo', 'hsp_ag_demo', 'hsp_hs_demo']:
            shutil.copy(examples_embedded + 'sco_demo_util.c', src_folder)
            shutil.copy(examples_embedded + 'sco_demo_util.h', src_folder)

        # update project files
        for file in ['.project', '.cproject']:
            with open(project_template + file, 'r') as fin:
                template = fin.read()
            with open(project_folder + file, 'wt') as fout:
                template = template.replace("btstack_example", example)
                fout.write(template)

        # copy jlink and launch files
        shutil.copy(project_template + 'btstack_port Debug.jlink',  project_folder + example + ' Debug.jlink' )
        shutil.copy(project_template + 'btstack_port Debug.launch', project_folder + example + ' Debug.launch')

        # generate .h from .gatt
        gatt_path = examples_embedded + example + ".gatt"
        if os.path.exists(gatt_path):
            update_gatt_script_sh  = project_folder + "update_gatt_db.sh"
            update_gatt_script_bat = project_folder + "update_gatt_db.bat"
            with open(update_gatt_script_sh, "wt") as fout:
                fout.write(gatt_update_sh_template.replace("EXAMPLE", example))
            os.chmod(update_gatt_script_sh, 0o755)
            with open(update_gatt_script_bat, "wt") as fout:
                fout.write(gatt_update_bat_template.replace("EXAMPLE", example).replace("BTSTACK_ROOT", btstack_root).replace("PROJECT_SRC", src_folder))
            if os.name == "nt":
                subprocess.run(["python.exe", port_folder + "/../../tool/compile_gatt.py", gatt_path, src_folder + example + ".h" ], shell=True, capture_output=True)
            else:
	            subprocess.run(update_gatt_script.sh, shell=True, capture_output=True)
            print("- %s - converting GATT DB" % example)
        else:
            print("- %s" % example)

if __name__ == '__main__':
    # get script path
    port_folder = os.path.abspath(os.path.dirname(sys.argv[0]))
    suffix = ''
    if len(sys.argv) > 1:
        suffix = sys.argv[1]
    fetch_init_script()
    create_examples(port_folder, suffix)



import os
import re
import shutil
import subprocess
import sys

def createProjectMakefile(project_path, btstack_root, gatt_h = ""):
    # path to template
    template_path = os.path.join(btstack_root, 'port/max32630-fthr/' ,'example/template/ProjectTemplate.mk')
    example = os.path.basename(project_path.strip("/"))
    print(f"project - {example}")
    example_files = os.listdir(project_path)
    # check if .gatt file is present
    if not len(gatt_h):
        for file in example_files:
            if not file.endswith(".gatt"):
                continue
            gatt_h = file[:-4] + "h"
            break

    if (len(gatt_h)):
        print(".. gatt - " + gatt_h)

    # create makefile
    with open(os.path.join(project_path, 'Makefile'), 'wt') as fout:
        with open(template_path, 'rt') as fin:
            for line in fin:
                if line.startswith('BTSTACK_ROOT ?='):
                    fout.write('BTSTACK_ROOT ?= %s\n' % btstack_root)
                    continue
                if 'PROJECT=inject_project' in line:
                    fout.write('PROJECT=%s\n' % example)
                    continue
                if 'all: inject_gatt_h' in line:
                    if len(gatt_h):
                        fout.write("all: %s\n" % gatt_h)
                    continue
                if 'rm-compiled-gatt-file:' in line:
                    if len(gatt_h):
                        fout.write(line)
                        fout.write("\trm -f %s\n" % gatt_h)
                        continue
                fout.write(line)

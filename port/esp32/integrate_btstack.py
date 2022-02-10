#!/usr/bin/env python3

#
# Add btstack component to esp-idf
#

import os
import sys
import shutil
import re
import create_examples

component_manager_yml = '''version: "BTSTACK_VERSION"
description: BTstack - BlueKitchen's implementation of the official Bluetooth stack.
url: https://github.com/bluekitchen/btstack
'''

if not 'IDF_PATH' in os.environ:
	print('Error: IDF_PATH not defined. Please set IDF_PATH as described here:\nhttp://esp-idf.readthedocs.io/en/latest/get-started/index.html#get-started-get-esp-idf');
	sys.exit(10)

IDF_PATH=os.environ['IDF_PATH']
print("IDF_PATH=%s" % IDF_PATH)

IDF_COMPONENTS=IDF_PATH + "/components"

if not os.path.exists(IDF_COMPONENTS):
	print("Error: No components folder at $IDF_PATH/components, please check IDF_PATH")
	sys.exit(10)

IDF_BTSTACK=IDF_COMPONENTS+"/btstack"

if os.path.exists(IDF_BTSTACK):
	print("Deleting old BTstack component %s" % IDF_BTSTACK)
	shutil.rmtree(IDF_BTSTACK)

# get local dir
local_dir = os.path.abspath(os.path.dirname(sys.argv[0]))

# create components/btstack
print("Creating BTstack component at %s" % IDF_COMPONENTS)
shutil.copytree(local_dir+'/components/btstack', IDF_BTSTACK)

dirs_to_copy = [
'src',
'3rd-party/bluedroid',
'3rd-party/hxcmod-player',
'3rd-party/lwip/dhcp-server',
'3rd-party/md5',
'3rd-party/micro-ecc',
'3rd-party/yxml',
'platform/freertos',
'platform/embedded',
'platform/lwip',
'tool'
]

for dir in dirs_to_copy:
	print('- %s' % dir)
	shutil.copytree(local_dir + '/../../' + dir, IDF_BTSTACK + '/' + dir)

# add hci dump stdout
shutil.copy(local_dir+'/../../platform/embedded/hci_dump_embedded_stdout.c', IDF_BTSTACK)

# add support for idf-component-manager
parsed_btstack_version = '0.0'
with open('../../CHANGELOG.md', 'r') as f:
	dev_version = False
	while True:
		rawline = f.readline()
		if not rawline:
			break
		matchobj = re.match(r'^##[\s]+(?:(?:Release v)|(?:))([^\n]+)[\s]*$', rawline)
		if matchobj is not None:
			if matchobj.groups()[0] == 'Unreleased':
				dev_version = True
			else:
				if re.match(r'^[\d]+(?:(?:.[\d]+)|(?:))(?:(?:.[\d]+)|(?:))(?:(?:.[\d]+)|(?:))$', matchobj.groups()[0]) is None:
					continue
				parsed_btstack_version = matchobj.groups()[0]
				if dev_version:
					ver_num = parsed_btstack_version.split('.')
					ver_num[len(ver_num)-1] = str(int(ver_num[len(ver_num)-1]) + 1)
					parsed_btstack_version = '.'.join(ver_num)
					parsed_btstack_version += '-alpha'
				print('BTstack version: ' + parsed_btstack_version)
				break
with open(IDF_BTSTACK + '/idf_component.yml', 'w') as f:
	f.write(component_manager_yml.replace('BTSTACK_VERSION', parsed_btstack_version))

# create example/btstack
create_examples.create_examples(local_dir, '')

#!/usr/bin/env python3

#
# Add btstack component to esp-idf
#

import os
import sys
import shutil
import create_examples

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
'3rd-party/lc3-google',
'3rd-party/md5',
'3rd-party/micro-ecc',
'3rd-party/yxml',
'platform/freertos',
'platform/lwip',
'tool'
]

for dir in dirs_to_copy:
	print('- %s' % dir)
	shutil.copytree(local_dir + '/../../' + dir, IDF_BTSTACK + '/' + dir)

# manually prepare platform/embedded
print('- platform/embedded')
platform_embedded_path = IDF_BTSTACK + '/platform/embedded'
os.makedirs(platform_embedded_path)
platform_embedded_files_to_copy = [
	'hal_time_ms.h',
	'hal_uart_dma.h',
	'hci_dump_embedded_stdout.h',
	'hci_dump_embedded_stdout.c',
]
for file in platform_embedded_files_to_copy:
	shutil.copy(local_dir+'/../../platform/embedded/'+file, platform_embedded_path)

# create example/btstack
create_examples.create_examples(local_dir, '')

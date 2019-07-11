#!/usr/bin/env python

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
'3rd-party/micro-ecc',
'3rd-party/md5',
'3rd-party/yxml',
'platform/freertos',
'platform/embedded',
'platform/lwip',
'tool'
]

for dir in dirs_to_copy:
	print('- %s' % dir)
	shutil.copytree(local_dir + '/../../' + dir, IDF_BTSTACK + '/' + dir)

# create example/btstack
create_examples.create_examples(local_dir, '')

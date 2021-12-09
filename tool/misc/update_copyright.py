#!/usr/bin/env python3
import os
import re
import sys

copyrightTitle = ".*(Copyright).*(BlueKitchen GmbH)"
copyrightEndString = "Please inquire about"

findAndReplace = {
	"MATTHIAS" : "BLUEKITCHEN",
	"RINGWALD" : "GMBH"
}

ignoreFolders = ["cpputest", "test", "docs", "3rd-party"]
ignoreFiles = ["ant_cmds.h", "btstack_config.h", "bluetoothdrv.h", "bluetoothdrv-stub.c", "BTstackDaemonRespawn.c"]


class State:
	SearchStartCopyright = 0
	SearchEndCopyright = 1
	CopyrightEnd = 2

def updateCopyright(dir_name, file_name):
	global copyrightTitle

	infile = dir_name + "/" + file_name
	outfile = dir_name + "/tmp_" + file_name
		
	with open(outfile, 'wt') as fout:
		bufferComment = ""
		state = State.SearchStartCopyright

		with open(infile, 'rt') as fin:
			for line in fin:
				# search Copyright start
				if state == State.SearchStartCopyright:
					fout.write(line)
					parts = re.match(copyrightTitle, line)
					if parts:
						state = State.SearchEndCopyright
						continue
					
				if state == State.SearchEndCopyright:
					# search end of Copyright
					parts = re.match('\s*(\*\/)\s*',line)
					if parts:
						state = State.CopyrightEnd
					else:
						for key, value in findAndReplace.items():
							line = line.replace(key, value)
					
					fout.write(line)
					continue

				# write rest of the file
				if state == State.CopyrightEnd:
					fout.write(line)
					
	os.rename(outfile, infile)


def requiresCopyrightUpdate(file_name):
	global copyrightTitle, copyrightEndString

	state = State.SearchStartCopyright
	with open(file_name, "rt") as fin:
		try:
			for line in fin:
				if state == State.SearchStartCopyright:
					parts = re.match(copyrightTitle, line)
					if parts:
						state = State.SearchEndCopyright
						continue
				if state == State.SearchEndCopyright:
					parts = re.match(copyrightEndString, line)
					if parts:
						return False
					return True

		except UnicodeDecodeError:
			return False

	return False


btstack_root = os.path.abspath(os.path.dirname(sys.argv[0])) + "/../../"

# file_name = btstack_root + "/panu_demo.c"
# if requiresCopyrightUpdate(file_name):
#  	print(file_name, ": update")
# 	# updateCopyright(btstack_root + "/example", "panu_demo.c")


for root, dirs, files in os.walk(btstack_root, topdown=True):
	dirs[:] = [d for d in dirs if d not in ignoreFolders]
	files[:] = [f for f in files if f not in ignoreFiles]
	for f in files:
		if f.endswith(".h") or f.endswith(".c"):
			file_name = root + "/" + f
			if requiresCopyrightUpdate(file_name):
				print(file_name)
				updateCopyright(root, f)
				
#!/usr/bin/env python
import os
import re

filetag = '#define BTSTACK_FILE__ "%s"\n'
filetag_re = '#define BTSTACK_FILE__ \"(.*)\"'

ignoreFolders = ["3rd-party", "pic32-harmony", "msp430", "cpputest", "test", "msp-exp430f5438-cc2564b", "msp430f5229lp-cc2564b", "ez430-rf2560", "ios", "chipset/cc256x", "docs", "mtk", "port"]
ignoreFiles =   ["ant_cmds.h", "rijndael.c", "btstack_config.h", "btstack_version.h", "profile.h", "bluetoothdrv.h", 
	"ancs_client_demo.h", "spp_and_le_counter.h", "bluetoothdrv-stub.c", "minimal_peripheral.c", "BTstackDaemonRespawn.c"]

class State:
	SearchStartComment = 0
	SearchCopyrighter = 1
	SearchEndComment = 2
	ProcessRest = 3

def update_filename_tag(dir_name, file_name, has_tag):
	infile = dir_name + "/" + file_name
	outfile = dir_name + "/tmp_" + file_name
	
	# print "Update copyright: ", infile
	
	with open(outfile, 'wt') as fout:

		bufferComment = ""
		state = State.SearchStartComment

		with open(infile, 'rt') as fin:
			for line in fin:
				if state == State.SearchStartComment:
					fout.write(line)
					parts = re.match('\s*(/\*).*(\*/)',line)
					if parts:
						if len(parts.groups()) == 2:
							# one line comment
							continue
						
					parts = re.match('\s*(/\*).*',line)
					if parts: 
						# beginning of comment
						state = State.SearchCopyrighter
					continue
					
				if state == State.SearchCopyrighter:
					fout.write(line)
					parts = re.match('.*(\*/)',line)
					if parts:
						# end of comment
						state = State.SearchStartComment

						# add filename tag if missing
						if not has_tag:
							fout.write('\n')
							fout.write(filetag % file_name)
						state = State.ProcessRest
					continue

				if state == State.ProcessRest:
					if has_tag:
						parts = re.match(filetag_re,line)
						if parts:
							print('have tag, found tag')
							fout.write(filetag % file_name)
							continue
					fout.write(line)

	os.rename(outfile, infile)


def get_filename_tag(file_path):
	basename = os.path.basename(file_path)
	with open(file_path, "rb") as fin:
		for line in fin:
			parts = re.match(filetag_re,line)
			if not parts:
				continue
			tag = parts.groups()[0]
			return tag
	return None

for root, dirs, files in os.walk('../', topdown=True):
	dirs[:]  = [d for d in dirs if d not in ignoreFolders]
	files[:] = [f for f in files if f not in ignoreFiles]
	for f in files:
		if not f.endswith(".c"):
			continue
		file_path = root + "/" + f
		tag = get_filename_tag(file_path)
		if tag != f:
			print('%s needs filetag' % file_path)
			update_filename_tag(root, f, tag != None)

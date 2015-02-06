#!/usr/bin/env python
import os
import re

copyright = """
/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
"""

onlyDumpDifferentCopyright = True
copyrightString = "Copyright (C) 2014 BlueKitchen GmbH"
copyrighters = ["BlueKitchen", "Matthias Ringwald"]

ignoreFolders = ["cpputest", "test", "msp-exp430f5438-cc2564b", "msp430f5229lp-cc2564b", "ez430-rf2560", "ios", "chipset-cc256x", "docs", "mtk"]
ignoreFiles = ["ant_cmds.h", "rijndael.c", "btstack-config.h", "version.h", "profile.h", "bluetoothdrv.h", 
	"ancs_client.h", "spp_and_le_counter.h", "bluetoothdrv-stub.c", "minimal_peripheral.c", "BTstackDaemonRespawn.c"]

class State:
	SearchStartComment = 0
	SearchCopyrighter = 1

def updateCopyright(file_name):
	global onlyDumpDifferentCopyright
	if not onlyDumpDifferentCopyright:
		print file_name, ": Update copyright"

def requiresCopyrightUpdate(file_name):
	global copyrightString, onlyDumpDifferentCopyright
	
	with open(file_name, "rb") as fin:
		parts = []
		allowedCopyrighters = []
		state = State.SearchStartComment

		for line in fin:
			if state == State.SearchStartComment:
				parts = re.match('\s*(/\*).*',line, re.I)
				if parts:
					state = State.SearchCopyrighter

			if state == State.SearchCopyrighter:
				parts = re.match('.*(Copyright).*',line, re.I)
				if parts:
					allowedCopyrighterFound = False
					for name in copyrighters:
						allowedCopyrighters = re.match('.*('+name+').*',line, re.I)
						if allowedCopyrighters:
							allowedCopyrighterFound = True
							return re.match(copyrightString,line)
					
					if onlyDumpDifferentCopyright:
						print file_name, ": Copyrighter not allowed > ", parts.group()
					return False
					
	print file_name, ": File has no copyright"
	return False

for root, dirs, files in os.walk('../', topdown=True):
	dirs[:] = [d for d in dirs if d not in ignoreFolders]
	files[:] = [f for f in files if f not in ignoreFiles]
	for f in files:
		if f.endswith(".h") or f.endswith(".c"):
			file_name = root + "/" + f
			if requiresCopyrightUpdate(file_name):
				updateCopyright(file_name)


    
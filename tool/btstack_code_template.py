#!/usr/bin/env python3

import glob
import re
import sys
import os
import datetime
from pathlib import Path

program_info = '''
Generate .h and .c with BTstack copyright header in BTstack tree

Usage: tool/btstack_code_template.py path/filename (without extension)
'''

copyright = """/*
 * Copyright (C) {copyright_year} BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

include_template = '''
/**
 *  @brief TODO
 */

#ifndef {include_guard}
#define {include_guard}

#if defined __cplusplus
extern "C" {{
#endif

#if defined __cplusplus
}}
#endif
#endif // {include_guard}
'''

source_template = '''
#define BTSTACK_FILE__ "{btstack_file}"

#include "{header_file}"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>
'''

btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/..')

if len(sys.argv) == 1:
	print(program_info)
	sys.exit(0)

path = sys.argv[1]
path_object = Path(path)

# include file
path_include = path + '.h'
copyright_year = datetime.datetime.now().year
include_guard = path_object.name.replace('/','_').upper()+'_H'
with open(btstack_root + '/' + path_include, 'wt') as fout:
    fout.write(copyright.format(copyright_year=copyright_year))
    fout.write(include_template.format(include_guard=include_guard))

# source file
path_source  = path + ".c"
btstack_file = path_object.name + '.c'
if path_include.startswith('src/'):
	# keep path for src/ folder
	header_file = path_include.replace('src/','')
else:
	# only use filename for everything else
	header_file = Path(path).name + '.h'
with open(btstack_root + '/' + path_source, 'wt') as fout:
    fout.write(copyright.format(copyright_year=copyright_year))
    fout.write(source_template.format(btstack_file=btstack_file,header_file=header_file))

print("Generated {file}.h and {file}.c in {path}".format(file=path_object.name,path=str(path_object.parent)))

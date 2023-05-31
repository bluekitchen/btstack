#!/usr/bin/env python3
#
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from setuptools import setup, Extension
import os, sys, glob

if len(sys.argv) <= 1:
  sys.argv = sys.argv + [
    'build', '--build-platlib', os.getcwd() + os.sep + 'build' ]

INC_DIR = '..' + os.sep + 'include'
SRC_DIR = '..' + os.sep + 'src'

sources = glob.glob('*_py.c') + \
          [ SRC_DIR + os.sep + 'tables.c',
            SRC_DIR + os.sep + 'bits.c',
            SRC_DIR + os.sep + 'plc.c' ]

depends = [ 'ctypes.h' ] + \
          glob.glob(INC_DIR + os.sep + '*.h') + \
          glob.glob(SRC_DIR + os.sep + '*.[c,h]')

includes = [ SRC_DIR, INC_DIR ]

extension = Extension('lc3',
  extra_compile_args = [ '-std=c11', '-ffast-math' ],
  define_macros = [ ('NPY_NO_DEPRECATED_API', 'NPY_1_7_API_VERSION') ],
  sources = sources,
  depends = depends,
  include_dirs = includes)

setup(name = 'LC3',
      version = '1.0',
      description = 'LC3 Test Python Module',
      ext_modules = [ extension ])

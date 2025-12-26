#  Copyright © 2022 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at:
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

project('lc3', 'c',
	version: '1.1.3',
	license: 'Apache-2.0',
	meson_version: '>= 0.48.0',
	default_options: ['buildtype=release', 'b_lto=true'])

cc = meson.get_compiler('c')

if cc.get_id() == 'msvc'
    add_project_arguments(['/wd4244', '/wd4305', '/fp:fast'], language: 'c')
else
    add_project_arguments('-ffast-math', language: 'c')
endif


m_dep = cc.find_library('m', required: false)

subdir('src')

if get_option('tools')
    subdir('tools')
endif

if get_option('python')
    subdir('python')
endif

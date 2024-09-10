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

inc = include_directories('../include')

lc3toolslib = static_library('lc3toolslib',
    ['lc3bin.c', 'wave.c'],
    link_with: lc3lib,
    include_directories: inc,
    dependencies: m_dep,
)

executable('elc3', ['elc3.c'],
    link_with : lc3toolslib,
    include_directories: inc,
    install: true,
    install_rpath: join_paths(get_option('prefix'), get_option('libdir')))

executable('dlc3', ['dlc3.c'],
    link_with : lc3toolslib,
    include_directories: inc,
    install: true,
    install_rpath: join_paths(get_option('prefix'), get_option('libdir')))

#!/bin/bash -x

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

install_newt() {
    pushd $HOME
    git clone --depth=1 https://github.com/apache/mynewt-newt
    [[ $? -ne 0 ]] && exit 1

    pushd mynewt-newt && ./build.sh
    [[ $? -ne 0 ]] && exit 1

    cp newt/newt $HOME/bin
    popd
    popd
}

shallow_clone_mynewt() {
    mkdir -p repos/apache-mynewt-core
    git clone --depth=1 https://github.com/apache/mynewt-core repos/apache-mynewt-core
    [[ $? -ne 0 ]] && exit 1
}

arm_toolchain_install() {
    TOOLCHAIN_PATH=$HOME/TOOLCHAIN

    GCC_URL=https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2017q4/gcc-arm-none-eabi-7-2017-q4-major-linux.tar.bz2
    GCC_BASE=gcc-arm-none-eabi-7-2017-q4-major

    mkdir -p $TOOLCHAIN_PATH

    if [ ! -s ${TOOLCHAIN_PATH}/$GCC_BASE/bin/arm-none-eabi-gcc ]; then
      wget -O ${TOOLCHAIN_PATH}/${GCC_BASE}.tar.bz2 $GCC_URL
      [[ $? -ne 0 ]] && exit 1

      tar xfj ${TOOLCHAIN_PATH}/${GCC_BASE}.tar.bz2 -C $TOOLCHAIN_PATH
    fi

    for i in ${TOOLCHAIN_PATH}/${GCC_BASE}/bin/arm-none-eabi-* ; do
        rm -f  $HOME/bin/${i##*/}
        ln -s $i $HOME/bin/${i##*/}
    done
}

mkdir -p $HOME/bin
export PATH=$HOME/bin:$PATH

install_newt
shallow_clone_mynewt
arm_toolchain_install

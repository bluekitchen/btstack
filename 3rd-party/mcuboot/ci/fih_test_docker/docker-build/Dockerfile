# Copyright (c) 2020 Arm Limited
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

FROM ubuntu:focal

# get dependencies for retrieving and building TF-M with MCUBoot, and QEMU.
RUN apt-get update && \
    DEBIAN_FRONTEND="noninteractive" \
    apt-get install -y \
    cmake \
    curl \
    gcc-arm-none-eabi \
    gdb-multiarch \
    git \
    libncurses5 \
    python3 \
    python3-pip \
    qemu-system-arm

RUN \
    # installing python packages
    python3 -m pip install \
        imgtool==1.7.0 \
        Jinja2==2.10 \
        PyYAML==3.12 \
        pyasn1==0.1.9

# Add tfm work directory
RUN mkdir -p /root/work/tfm

# run the command
CMD ["bash"]

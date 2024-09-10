#
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

elc3_src += \
    $(TOOLS_DIR)/elc3.c \
    $(TOOLS_DIR)/lc3bin.c \
    $(TOOLS_DIR)/wave.c

elc3_ldlibs += lc3 m
elc3_dependencies += liblc3

$(eval $(call add-bin,elc3))


dlc3_src += \
    $(TOOLS_DIR)/dlc3.c \
    $(TOOLS_DIR)/lc3bin.c \
    $(TOOLS_DIR)/wave.c

dlc3_ldlibs += lc3 m
dlc3_dependencies += liblc3

$(eval $(call add-bin,dlc3))


.PHONY: tools
tools: elc3 dlc3

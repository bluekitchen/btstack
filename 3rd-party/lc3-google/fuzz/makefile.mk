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

efuzz_src += \
    $(FUZZ_DIR)/efuzz.cc

efuzz_ldlibs += lc3 m
efuzz_dependencies += liblc3

$(eval $(call add-bin,efuzz))


dfuzz_src += \
    $(FUZZ_DIR)/dfuzz.cc

dfuzz_ldlibs += lc3 m
dfuzz_dependencies += liblc3

$(eval $(call add-bin,dfuzz))


.PHONY: fuzz dfuzz efuzz

efuzz dfuzz: CC  = clang
efuzz dfuzz: CXX = clang++
efuzz dfuzz: LD  = clang

FUZZER_SANITIZE := -fsanitize=fuzzer,address
efuzz dfuzz: CFLAGS   += $(FUZZER_SANITIZE)
efuzz dfuzz: CXXFLAGS += $(FUZZER_SANITIZE)
efuzz dfuzz: LDFLAGS  += $(FUZZER_SANITIZE)

dfuzz:
	$(V)LD_LIBRARY_PATH=$(BIN_DIR) $(dfuzz_bin) -runs=1000000

efuzz:
	$(V)LD_LIBRARY_PATH=$(BIN_DIR) $(efuzz_bin) -runs=1000000

fuzz: efuzz dfuzz

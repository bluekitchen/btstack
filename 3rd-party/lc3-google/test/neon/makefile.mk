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

test_neon_src += \
    $(TEST_DIR)/neon/test_neon.c \
    $(TEST_DIR)/neon/ltpf_neon.c \
    $(TEST_DIR)/neon/mdct_neon.c \
    $(SRC_DIR)/tables.c

test_neon_include += $(SRC_DIR)
test_neon_ldlibs += m

$(eval $(call add-bin,test_neon))

test_neon: $(test_neon_bin)
	@echo "  RUN     $(notdir $<)"
	$(V)$<

test: test_neon

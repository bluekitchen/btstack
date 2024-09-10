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

test_py:
	$(V)cd $(TEST_DIR) && python3 setup.py && PYTHONPATH=build python3 run.py

.PHONY: test test-clean

test: test_py

test-clean:
	$(V)cd $(TEST_DIR) && python3 setup.py clean > /tmp/zero

-include $(TEST_DIR)/arm/makefile.mk
-include $(TEST_DIR)/neon/makefile.mk

clean-all: test-clean

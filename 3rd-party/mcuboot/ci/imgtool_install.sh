#!/bin/bash -x

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [[ $TRAVIS == "true" ]]; then
    if [[ $TRAVIS_PULL_REQUEST != "false" || $TRAVIS_BRANCH != "master" ]]; then
        echo "Either a PR or not \"master\" branch, exiting"
        exit 0
    fi
fi

pip install setuptools twine packaging wheel
pip install --pre imgtool

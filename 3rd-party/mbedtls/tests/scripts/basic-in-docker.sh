#!/bin/bash -eu

# basic-in-docker.sh
#
# Purpose
# -------
# This runs sanity checks and library tests in a Docker container. The tests
# are run for both clang and gcc. The testing includes a full test run
# in the default configuration, partial test runs in the reference
# configurations, and some dependency tests.
#
# WARNING: the Dockerfile used by this script is no longer maintained! See
# https://github.com/Mbed-TLS/mbedtls-test/blob/master/README.md#quick-start
# for the set of Docker images we use on the CI.
#
# Notes for users
# ---------------
# See docker_env.sh for prerequisites and other information.

# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

source tests/scripts/docker_env.sh

run_in_docker tests/scripts/all.sh 'check_*'

for compiler in clang gcc; do
    run_in_docker -e CC=${compiler} cmake -D CMAKE_BUILD_TYPE:String="Check" .
    run_in_docker -e CC=${compiler} make
    run_in_docker -e CC=${compiler} make test
    run_in_docker programs/test/selftest
    run_in_docker -e OSSL_NO_DTLS=1 tests/compat.sh
    run_in_docker tests/ssl-opt.sh -e '\(DTLS\|SCSV\).*openssl'
    run_in_docker tests/scripts/test-ref-configs.pl
    run_in_docker tests/scripts/depends.py curves
    run_in_docker tests/scripts/depends.py kex
done

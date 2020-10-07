#!/usr/bin/env python3
#
# Demangle C++ function names in lcov .info reports
#
# Copyright 2020 BlueKitchen GmbH
#

import cxxfilt
import fileinput
import sys
import re

for line in fileinput.input(inplace=1):
    match = re.match('(FN|FNDA):(\d.*),(\w*)', line)
    if match:
        (key, line_no, mangled) = match.groups()
        demangled = cxxfilt.demangle(mangled)
        match = re.match('(\w+)\(.*\)', demangled)
        if (match):
            fn = match.groups()[0]
            sys.stdout.write('%s:%s,%s\n' % (key, line_no, fn))
            continue
    sys.stdout.write(line)
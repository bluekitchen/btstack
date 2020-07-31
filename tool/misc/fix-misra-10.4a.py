#!/usr/bin/env python3
import os
import sys
import re
import fileinput

# find root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../../')
print(btstack_root)

# messages
cstat_file = 'cstat.txt'

# project prefix
project_prefixes = [ 'c:\\projects\\iar\\btstack\\btstack\\',
                     'c:/users/buildbot/buildbot-worker/cstat-develop/btstack/']

def fix(path, lineno, expression):
    source_path = btstack_root + "/" + path
    print(source_path, lineno, expression)
    with open(source_path + '.cocci_res') as cocci_fd:
        cocci_lines = cocci_fd.readlines()
        for line_source, line_fix in zip(fileinput.input(source_path, inplace=True), cocci_lines):
            if fileinput.lineno() == lineno:
                sys.stdout.write(line_fix)
            else:
                sys.stdout.write(line_source)

with open(cstat_file, 'rt') as fin:
    fixed = 0
    total = 0
    for line in fin:
        chunks = line.strip().split('\t')
        if len(chunks) != 4: continue
        (msg, rule, severity, location) = chunks
        if not rule.startswith('MISRAC2012-Rule-10.4_a'): continue
        total += 1
        # remove project prefix
        for project_prefix in project_prefixes:
            location = location.replace(project_prefix, '').replace('\\','/')
        parts = location.split(':')
        (path, lineno) = parts
        match = re.match("The operands `(.+)' and `(.+)' have essential type categories (.*) and (.*), which do not match.", msg)
        # fix if operand is signed literal and cstat complains about signednesss
        if match:
            (op1, op2, t1, t2) = match.groups()
            if re.match("[(0x)0-9]+", op1) and t1.startswith('signed'):
                fix(path, int(lineno), op1)
                fixed += 1
                continue
            if re.match("[(0x)0-9]+", op2) and t2.startswith('signed'):
                fix(path, int(lineno), op2)
                fixed += 1
                continue
    print ("Fixed %u of %u messages" % (fixed, total))

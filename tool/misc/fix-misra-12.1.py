#!/usr/bin/env python3
import os
import sys
import re
import fileinput
import string

# find root
btstack_root = os.path.abspath(os.path.dirname(sys.argv[0]) + '/../../')
print(btstack_root)

# messages
cstat_file = 'cstat.txt'

# project prefix
project_prefix = 'c:\\projects\\iar\\btstack\\btstack\\'

def remove_whitespace(line):
    return line.replace(' ','')

def add_parantheses(line, expression):
    stripped_line = ''
    positions = []
    for (char,pos) in zip(line, range(len(line))):
        if char in string.whitespace:
            continue
        stripped_line += char
        positions.append(pos)
    pos = stripped_line.find(expression)
    if pos < 0:
        return line
    pos_start = positions[pos]
    pos_end   = positions[pos + len(expression)-1]
    new_line = line[0:pos_start] + '(' + line[pos_start:pos_end+1] + ")" + line[pos_end+1:]
    return new_line

def fix(path, lineno, expression):
    full_path = btstack_root + "/" + path
    print(full_path, lineno, expression)
    for line in fileinput.input(full_path, inplace=True):
        if fileinput.lineno() == lineno:
            line = add_parantheses(line, expression)
        sys.stdout.write(line)

with open(cstat_file, 'rt') as fin:
    fixed = 0
    total = 0
    for line in fin:
        chunks = line.strip().split('\t')
        if len(chunks) != 4: continue
        (msg, rule, severity, location) = chunks
        if not rule.startswith('MISRAC2012-Rule-12.1'): continue
        parts = re.match(".*Suggest parentheses.*`(.*)'", msg)
        total += 1
        expression = parts.groups()[0]
        expression = remove_whitespace(expression)
        # skip some expressions
        if expression.endswith('=='): continue
        if expression.endswith('!='): continue
        if expression.endswith('>'): continue
        if expression.endswith('<'): continue
        if expression.endswith('>='): continue
        if expression.endswith('<='): continue
        # remove windows prefix
        location = location.replace(project_prefix, '').replace('\\','/')
        parts = location.split(':')
        (path, lineno) = parts
        fix(path, int(lineno), expression)
        fixed += 1
    print ("Fixed %u of %u messsages" % (fixed, total))

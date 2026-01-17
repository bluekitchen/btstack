#!/usr/bin/env python3

import argparse, sys
import subprocess
from pathlib import Path

demangle_cpp = 'llvm-cxxfilt'
whitelist = set()

def ingest_whitelist(file):
    with open(file, 'r') as fd:
        for line in fd:
            line = line.strip()
            if line and not line.startswith('#'):
                whitelist.add(line)

def in_whitelist(name):
    for entry in whitelist:
        if name.endswith(entry):
            return True
    return False

def demangle(names):
    print( names )
    result = subprocess.run( [demangle_cpp, '-p', names], capture_output = True, text = True )

    return result.stdout.strip()

parser = argparse.ArgumentParser(prog=sys.argv[0], usage='%(prog)s [options]')
parser.add_argument('-o', '--output', help='output goes to file')
parser.add_argument('files', nargs='+', help='files to process')
parser.add_argument('-d', '--demangle-cpp', help='tool for function name demangling')
parser.add_argument('-w', '--whitelist', help='list of files included in the report')

args = parser.parse_args()

if args.demangle_cpp:
    demangle_cpp = args.demangle_cpp

if args.whitelist:
    ingest_whitelist(args.whitelist)

output = sys.stdout
if args.output:
    output = open(args.output, 'w')

test_name = ''
start_of_record = False
for file in args.files:
    with open(file, 'r') as fd:
        delete_record = False
        for line in fd:
            line = line.strip()
            if line.startswith('#'):
                continue
            command, _, value = line.partition(':')
            if whitelist:
                if not delete_record:
                    if command == 'TN':
                        test_name = f'TN:{value}'
                        continue
                    if command == 'SF':
                        if in_whitelist( value ):
                            output.write(f'{test_name}\n')
                            delete_record = False
                        else:
                            delete_record = True
                if delete_record:
                    if command == 'end_of_record':
                        delete_record = False
                    continue
            # convert function signatures
            if command == 'FN' or command =='FNDA':
                arguments = value.split(',')
                function_name = arguments[-1].split(':')
                if len(function_name) > 1:
                    file_suffix = Path(function_name[0]).suffix
                    if file_suffix == '.cpp':
                        arguments[-1] = demangle(function_name[-1])
                    else:
                        arguments[-1] = function_name[-1]
                output.write(f"{command}:{','.join(arguments)}\n")
            else:
                output.write(f"{line}\n")


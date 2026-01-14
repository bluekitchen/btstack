#!/bin/env python3

import argparse, sys
import subprocess
from pathlib import Path

def demangle(names):
    result = subprocess.run( ['c++filt', '-p', names], capture_output = True, text = True )
    return result.stdout.strip()

parser = argparse.ArgumentParser(prog=sys.argv[0], usage='%(prog)s [options]')
parser.add_argument('-o', '--output', help='output goes to file')
parser.add_argument('files', nargs='+', help='files to process')

args = parser.parse_args()
output = sys.stdout
if args.output:
    output = open(args.output, 'w')

for file in args.files:
    with open(file, 'r') as fd:
        for line in fd:
            line = line.strip()
            command, _, value = line.partition(':')
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


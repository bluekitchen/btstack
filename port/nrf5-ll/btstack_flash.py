#!/usr/bin/env python

import sys
import os
import subprocess

program_info = '''
Show BTstack RAM usage for compiled .elf file

Usage: {0} path/to/file.elf
'''

# Return a dict containing symbol_name: path/to/file/where/it/originates
# for all symbols from the .elf file. Optionnaly strips the path according
# to the passed sub-path
def load_symbols(elf_file, path_to_strip = None):
    symbols = []
    nm_out = subprocess.check_output(["arm-none-eabi-nm", elf_file, "-S", "-l", "--size-sort", "--radix=d"])
    for line in nm_out.split('\n'):
        fields = line.replace('\t', ' ').split(' ')
        # Get rid of trailing empty field
        if len(fields) == 1 and fields[0] == '':
            continue
        assert len(fields)>=4
        if len(fields)<5:
            path = ":/" + fields[3]
        else:
            path = fields[4].split(':')[0]
        if path_to_strip != None:
            if path_to_strip in path:
                path = path.replace(path_to_strip, "") + '/' + fields[3]
            else:
                path = ":/" + fields[3]
        dict = {}
        dict['offset'] = fields[0]
        dict['size']   = fields[1]
        dict['type']   = fields[2]
        dict['name']   = fields[3]
        dict['path']   = path
        symbols.append(dict)
    return symbols

if len(sys.argv) < 2:
    print (program_info.format(sys.argv[0]))
    sys.exit(0)


phoenix_symbols = [
'CSWTCH',
'cpu_sleep',
'irq_unlock',
'gc_lookup_ppm',
'hex',
'dv',
'rtc0_handler',
'rx_fc_lock',
'radio_scan_disable',
'soc_init',
'uart_tx',
'rng_handler',
'uart0_handler',
'assert_print',
'radio_tmr_start',
'power_clock_handler'
]

phoenix_files = [
    'ctrl.c',
    'ticker.c',
    'll.c',
    'radio.c',
    'clock.c',
    'cntr.c',
    'ecb.c',
    'mem.c',
    'rand.c',
    'uart.c',
    'core_cm0.h'
]
symbols = load_symbols(sys.argv[1])
ram = 0
ram_symbols = 0
for dict in symbols:
    if not dict['type'] in "tT":
      continue
    if 'path' in dict:
        path = dict['path']
    size = int(dict['size'])
    full_name = dict['name']
    name = full_name.split('.')[0]
    # if "../../src" in path:
    #     continue
    filename = path.split('/')[-1]
    if filename in phoenix_files:
        continue
    if name in phoenix_symbols:
        continue
    if name.startswith('__'):
        continue
    if name.startswith('ticker'):
        continue
    if name.startswith('mayfly'):
        continue
    # path = ''
    # print (name)
    print ("%-50s %s %5s %s" % (name, dict['type'], size, path))
    ram += size
    ram_symbols += 1

print ("")
print ("BTstack Symbols %u" % ram_symbols)
print ("BTstack ROM:    %u" % ram)


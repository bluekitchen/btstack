#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2019

import sys

usage = '''
CSR/Qualcomm PSR conversion tool for use with BTstack
Copyright 2019 BlueKitchen GmbH

Usage:
$ ./convert_prs.py file.prs

'''

msg_seqno = 0
indent = '    '

def write_verbatim(fout, text):
    fout.write(text + '\n')

def write_set_varid_cmd(fout, varid, msg_payload):
    global msg_seqno

    # msg_payload must be at least 4 uint16
    while len(msg_payload) < 4:
        msg_payload.append(0)

    # setup msg
    msg_type   = 0x0002
    msg_len    = 5 + len(msg_payload)
    msg_seqno += 1
    msg_status = 0
    msg = [msg_type, msg_len, msg_seqno, varid, msg_status] + msg_payload

    # setup hci command
    hci_payload_descriptor = 0xc2
    hci_param_len          = len(msg) * 2 + 1
    hci_cmd = [ 0x00, 0xfc, hci_param_len, hci_payload_descriptor]

    # convert list of uint16_t to sequence of uint8_t in little endian
    for value in msg:
        hci_cmd.append(value & 0xff)
        hci_cmd.append(value >> 8)

    fout.write(indent + ', '.join(["0x%02x" % val for val in hci_cmd]) + ',\n')

def write_key(fout, key, value):
    # setup ps command
    payload_len = len(value)
    stores   = 0x0008 # psram
    ps_cmd = [key, payload_len, stores] + value

    # write ps command
    write_set_varid_cmd(fout, 0x7003, ps_cmd)

def write_warm_reset(fout):
    write_verbatim(fout, indent + "// WarmReset")
    write_set_varid_cmd(fout, 0x4002, [])


# check args
if len(sys.argv) != 2:
    print(usage)
    sys.exit(1)

prs_file = sys.argv[1]
fout     = sys.stdout

with open (prs_file, 'rt') as fin:
    for line_with_nl in fin:
        line = line_with_nl.strip()
        if line.startswith('&'):
            # pskey
            parts = line.split('=')
            key = int(parts[0].strip().replace('&','0x'), 16)
            value = [int('0x'+i, 16) for i in parts[1].strip().split(' ')]
            write_key(fout, key, value)
        elif line.startswith('#'):
            #ifdef, ..
            write_verbatim(fout, line)
        else:
            # comments
            write_verbatim(fout, indent + line)

write_warm_reset(fout)

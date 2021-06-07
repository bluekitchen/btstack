#!/usr/bin/env python3
#
# Filter coverage reported by lcov
#
# Copyright 2020 BlueKitchen GmbH
#

import sys

whitelist = [
    '3rd-party/micro-ecc/uECC.c',
    '3rd-party/rijndael/rijndael.c',
    'src/ad_parser.c',
    'src/ble/att_db.c',
    'src/ble/att_db_util.c',
    'src/ble/att_dispatch.c',
    'src/ble/att_server.c',
    'src/ble/gatt_client.c',
    'src/ble/gatt-service/ancs_client.c',
    'src/ble/gatt-service/battery_service_server.c',
    'src/ble/gatt-service/battery_service_client.c',
    'src/ble/gatt-service/device_information_service_server.c',
    'src/ble/gatt-service/device_information_service_client.c',
    'src/ble/le_device_db_tlv.c',
    'src/ble/sm.c',
    'src/btstack_crypto.c',
    'src/btstack_linked_list.c',
    'src/btstack_memory.c',
    'src/btstack_memory_pool.c',
    'src/btstack_run_loop.c',
    'src/btstack_run_loop_base.c',
    'src/btstack_tlv.c',
    'src/btstack_util.c',
    'src/hci.c',
    'src/hci_cmd.c',
    'src/hci_dump.c',
    'src/hci_transport_h4.c',
    'src/l2cap.c',
    'src/l2cap_signaling.c',
    'platform/embedded/btstack_stdin_embedded.c',
    'platform/embedded/btstack_run_loop_embedded.c',
    'platform/embedded/btstack_uart_block_embedded.c',
]

def include_file(filename):
    for pattern in whitelist:
        if pattern in filename:
            print("Add " + filename)
            return True
    return False

if len(sys.argv) != 3:
    print ('lcov .info filter')
    print ('Usage: ', sys.argv[0], 'input.info output.info')
    exit(0)

infile = sys.argv[1]
outfile = sys.argv[2]

with open(infile, 'rt') as fin:
    with open(outfile, 'wt') as fout:
        mirror = False
        read_tn = False
        for line in fin:
            line = line.strip()
            if line == 'TN:':
                read_tn = True
                continue
            if line == 'end_of_record':
                if mirror:
                    fout.write(line+'\n')
                mirror = False
                continue
            parts = line.split(':')
            if len(parts) == 2 and parts[0] == 'SF':
                filename = parts[1]
                mirror = include_file(filename)
                if mirror and read_tn:
                    fout.write("TN:\n")
                    read_tn = False
            if not mirror:
                continue
            fout.write(line+"\n")

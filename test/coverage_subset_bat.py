#!/usr/bin/env python3
#
# Filter coverage reported by lcov
#
# Copyright 2020 BlueKitchen GmbH
#

import sys

blacklist = [
    '/opt/local',
    '3rd-party/yxml',
    '3rd-party/tinydir',
    'chipset/zephyr',
    'platform/embedded/btstack_audio_embedded.c',
    'platform/embedded/btstack_em9304_spi_embedded.c',
    'platform/embedded/btstack_stdin_embedded.c',
    'platform/embedded/btstack_tlv_flash_bank.c',
    'platform/embedded/btstack_uart_block_embedded.c',
    'platform/embedded/hal_flash_bank_memory.c',
    'platform/freertos/btstack_run_loop_freertos.c',
    'platform/freertos/btstack_uart_block_freertos.c',
    'platform/libusb',
    'platform/posix',
    'port/libusb',
    'src/ble/ancs_client.c',
    'src/ble/le_device_db_memory.c',
    'src/ble/gatt-service/cycling_power_service_server.c',
    'src/ble/gatt-service/cycling_speed_and_cadence_service_server.c',
    'src/ble/gatt-service/heart_rate_service_server.c',
    'src/ble/gatt-service/hids_device.c',
    'src/ble/gatt-service/nordic_spp_service_server.c',
    'src/ble/gatt-service/ublox_spp_service_server.c',
    'src/btstack_audio.c',
    'src/btstack_base64_decoder.c',
    'src/btstack_event.h',
    'src/btstack_hid_parser.c',
    'src/btstack_resample.c',
    'src/btstack_slip.c',
    'src/hci_transport_em9304_spi.c',
    'src/hci_transport_h5.c',
    'src/mesh/',
    'src/classic',
]

def include_file(filename):
    for pattern in blacklist:
        if pattern in filename:
            print("Skip " + filename)
            return False
    return True

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

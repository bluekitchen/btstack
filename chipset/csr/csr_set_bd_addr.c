/*
 * Copyright (C) 2018 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "csr_set_bd_addr.c"

// *****************************************************************************
/* EXAMPLE_START(csr_set_bd_addr): Set BD ADDR on USB CSR modules
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static bd_addr_t cmdline_addr;
static btstack_timer_source_t warm_boot_timer;
static int cmdline_addr_found;
static const char * prog_name;

static uint8_t csr_set_bd_addr[] = {
    // 0x0001: Set Bluetooth address 
    0x00, 0xFC, 0x19, 0xc2, 0x02, 0x00, 0x0A, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0xf3, 0x00, 0xf5, 0xf4, 0xf2, 0x00, 0xf2, 0xf1,
};

static uint8_t csr_warm_start[] = {
    //  WarmReset
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x03, 0x0e, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void usage(void){
    fprintf(stderr, "\nUsage: %s aa:bb:cc:dd:ee:ff\n", prog_name);
    exit(0);
}
/* @section Bluetooth Logic 
 *
 * @text The Bluetooth logic is implemented as a state machine within the packet
 * handler. In this example, the following states are passed sequentially:
 * INIT, and ACTIVE.
 */ 

static void local_version_information_handler(uint8_t * packet){
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO:
            printf("Cambridge Silicon Radio - CSR chipset.\n");
            break;
        default:
            printf("Local version information:\n");
            printf("- HCI Version    0x%04x\n", hci_version);
            printf("- HCI Revision   0x%04x\n", hci_revision);
            printf("- LMP Version    0x%04x\n", lmp_version);
            printf("- LMP Subversion 0x%04x\n", lmp_subversion);
            printf("- Manufacturer   0x%04x\n", manufacturer);
            printf("Not a CSR chipset, exit\n");
            usage();
            break;
    }
}

static int state = 0;

static void warm_boot_handler(struct btstack_timer_source *ts){
    UNUSED(ts);

    if (state != 4) return;
    printf("Done\n");
    exit(0);
} 

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                if (!cmdline_addr_found){
                    usage();
                    break;
                }
                printf("Setting BD ADDR to %s\n", bd_addr_to_str(cmdline_addr));
                state = 1;
            }
            break;
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION){
                local_version_information_handler(packet);
            }
            break;
        case HCI_EVENT_VENDOR_SPECIFIC:
            // Vendor event
            state++;
            break;
        default:
            break;
    }

    if (!hci_can_send_command_packet_now()) return;
    switch (state){
        case 0:
        case 2:
            break;
        case 1:
            state++;
            hci_send_cmd_packet(csr_set_bd_addr, sizeof(csr_set_bd_addr));
            break;
        case 3:
            state++;
            hci_send_cmd_packet(csr_warm_start, sizeof(csr_warm_start));
            // set timer
            warm_boot_timer.process = &warm_boot_handler;
            btstack_run_loop_set_timer(&warm_boot_timer, 1000);
            btstack_run_loop_add_timer(&warm_boot_timer);
            break;
        default:
            break;
    }
}


/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It registers the HCI packet handler and starts the Bluetooth stack.
 */

/* LISTING_START(MainConfiguration): Setup packet handler for GAP inquiry */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {

    prog_name = argv[0];
    if (argc > 1) {
        cmdline_addr_found = sscanf_bd_addr(argv[1], cmdline_addr);
    }

    if (cmdline_addr_found){
        // prepare set bd addr command
        csr_set_bd_addr[20] = cmdline_addr[3];
        csr_set_bd_addr[22] = cmdline_addr[5];
        csr_set_bd_addr[23] = cmdline_addr[4];
        csr_set_bd_addr[24] = cmdline_addr[2];
        csr_set_bd_addr[26] = cmdline_addr[1];
        csr_set_bd_addr[27] = cmdline_addr[0];
    }

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

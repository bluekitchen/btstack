/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "l2cap_test.c"

/*
 * l2cap_test.c 
 */ 


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_stdin.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
 
static void show_usage(void);

// static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
// static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x07, 0x32, 0xef};;
static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0xe2, 0x72};

static uint16_t handle;
static uint16_t local_cid;
static int l2cap_ertm;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t ertm_buffer[10000];
static l2cap_ertm_config_t ertm_config = {
    0,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    144,    // l2cap ertm mtu
    4,
    4,
};

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_addr;
    uint16_t psm;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack L2CAP Test Ready\n");
                        show_usage();
                    }
                    break;
                case HCI_EVENT_CONNECTION_COMPLETE:
                    handle = hci_event_connection_complete_get_connection_handle(packet);
                    break;

                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    reverse_bd_addr(&packet[3], event_addr);
                    psm = little_endian_read_16(packet, 11); 
                    local_cid = little_endian_read_16(packet, 13); 
                    handle = little_endian_read_16(packet, 9);
                    if (packet[2] == 0) {
                        printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_addr), handle, psm, local_cid,  little_endian_read_16(packet, 15));
                    } else {
                        printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_addr), packet[2]);
                    }
                    break;

                case L2CAP_EVENT_INCOMING_CONNECTION: {
                    uint16_t l2cap_cid  = little_endian_read_16(packet, 12);
                    if (l2cap_ertm){
                        printf("L2CAP Accepting incoming connection request in ERTM\n"); 
                        l2cap_ertm_accept_connection(l2cap_cid, &ertm_config, ertm_buffer, sizeof(ertm_buffer));
                    } else {
                        printf("L2CAP Accepting incoming connection request in Basic Mode\n"); 
                        l2cap_accept_connection(l2cap_cid);
                    }
                    break;
                }
                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            printf_hexdump(packet, size);
            break;

    }
    if (packet_type != HCI_EVENT_PACKET) return;


}

static void show_usage(void){
    printf("\n--- CLI for L2CAP TEST ---\n");
    printf("L2CAP Channel Mode %s\n", l2cap_ertm ? "Enhanced Retransmission" : "Basic");
    printf("c      - create connection to SDP at addr %s\n", bd_addr_to_str(remote));
    printf("s      - send some data\n");
    printf("S      - send more data\n");
    printf("p      - send echo request\n");
    printf("i      - send connectionless data\n");
    printf("e      - optional ERTM mode\n");
    printf("E      - mandatory ERTM mode\n");
    printf("b      - set channel as busy (ERTM)\n");
    printf("B      - set channel as ready (ERTM)\n");
    printf("d      - disconnect\n");
    printf("t      - terminate ACL connection\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char buffer){
    switch (buffer){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM SDP\n", bd_addr_to_str(remote));
            if (l2cap_ertm){
                l2cap_ertm_create_channel(packet_handler, remote, BLUETOOTH_PSM_SDP, &ertm_config, ertm_buffer, sizeof(ertm_buffer), &local_cid);
            } else {
                l2cap_create_channel(packet_handler, remote, BLUETOOTH_PSM_SDP, 100, &local_cid);
            }
            break;
        case 'b':
            printf("Set channel busy\n");
            l2cap_ertm_set_busy(local_cid);
            break;
        case 'B':
            printf("Set channel ready\n");
            l2cap_ertm_set_ready(local_cid);
            break;
        case 's':
            printf("Send some L2CAP Data\n");
            l2cap_send(local_cid, (uint8_t *) "0123456789", 10);
            break;
        case 'S':
            printf("Send more L2CAP Data\n");
            l2cap_send(local_cid, (uint8_t *) "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789", 100);
            break;
        case 'u':
            printf("Send connection-less L2CAP Data\n");
            l2cap_send_connectionless(handle, 0x0002, (uint8_t *) "0123456789", 10);
            break;
        case 'd':
            printf("L2CAP Channel Closed\n");
            l2cap_disconnect(local_cid);
            break;
        case 'e':
            printf("L2CAP Enhanced Retransmission Mode (ERTM) optional\n");
            l2cap_ertm = 1;
            ertm_config.ertm_mandatory = 0;
            break;
        case 'E':
            printf("L2CAP Enhanced Retransmission Mode (ERTM) mandatory\n");
            l2cap_ertm = 1;
            ertm_config.ertm_mandatory = 1;
            break;
        case 'p':
            printf("Send L2CAP ECHO Request\n");
            l2cap_send_echo_request(handle, (uint8_t *)  "Hello World!", 13);
            break;
        case 't':
            printf("Disconnect ACL handle\n");
            gap_disconnect(handle);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    UNUSED(argc);
    (void) argv;
    
    gap_set_class_of_device(0x220404);
    gap_discoverable_control(1);

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    l2cap_register_service(packet_handler, BLUETOOTH_PSM_SDP, 100, LEVEL_0);
    
    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}

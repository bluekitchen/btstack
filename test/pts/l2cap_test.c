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
 
// *****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "gap.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "stdin_support.h"
 
static void show_usage();

// static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};

static uint16_t handle;
static uint16_t local_cid;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    bd_addr_t event_addr;
    uint16_t psm;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                printf("BTstack L2CAP Test Ready\n");
                show_usage();
            }
            break;
        case L2CAP_EVENT_CHANNEL_OPENED:
            // inform about new l2cap connection
            bt_flip_addr(event_addr, &packet[3]);
            psm = READ_BT_16(packet, 11); 
            local_cid = READ_BT_16(packet, 13); 
            handle = READ_BT_16(packet, 9);
            if (packet[2] == 0) {
                printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                       bd_addr_to_str(event_addr), handle, psm, local_cid,  READ_BT_16(packet, 15));
            } else {
                printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_addr), packet[2]);
            }
            break;
        case L2CAP_EVENT_INCOMING_CONNECTION: {
            uint16_t l2cap_cid  = READ_BT_16(packet, 12);
            printf("L2CAP Accepting incoming connection request\n"); 
            l2cap_accept_connection_internal(l2cap_cid);
            break;
        }


        default:
            break;
    }
}

static void packet_handler2 (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    packet_handler(packet_type, 0, packet, size);
}

static void show_usage(void){
    printf("\n--- CLI for L2CAP TEST ---\n");
    printf("c      - create connection to SDP at addr %s\n", bd_addr_to_str(remote));
    printf("s      - send data\n");
    printf("e      - send echo request\n");
    printf("d      - disconnect\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static int stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);
    switch (buffer){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM SDP\n", bd_addr_to_str(remote));
            l2cap_create_channel_internal(NULL, packet_handler, remote, PSM_SDP, 100);
            break;
        case 's':
            printf("Send L2CAP Data\n");
            l2cap_send_internal(local_cid, (uint8_t *) "0123456789", 10);
       break;
        case 'e':
            printf("Send L2CAP ECHO Request\n");
            l2cap_send_echo_request(handle, (uint8_t *)  "Hello World!", 13);
            break;
        case 'd':
            printf("L2CAP Channel Closed\n");
            l2cap_disconnect_internal(local_cid, 0);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
    return 0;
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    hci_set_class_of_device(0x220404);
    hci_discoverable_control(1);

    l2cap_init();
    l2cap_register_packet_handler(&packet_handler2);
    l2cap_register_service_internal(NULL, packet_handler, PSM_SDP, 100, LEVEL_0);
    
    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}

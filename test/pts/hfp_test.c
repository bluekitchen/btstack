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
// Minimal setup for HFP Audio Gateway
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "hci_cmd.h"
#include "btstack_run_loop.h"

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "classic/sdp_client_rfcomm.h"

// static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static bd_addr_t remote = {0x00, 0x21, 0x3C, 0xAC, 0xF7, 0x38};
static uint8_t channel_nr = 0;

static uint16_t mtu;
static uint16_t rfcomm_cid = 0;

static char data[50];
static int send_err = 0; 

static uint8_t hfp_service_level_connection_state = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void send_str_over_rfcomm(uint16_t cid, char * command){
    printf("Send %s.\n", command);
    int err = rfcomm_send(cid, (uint8_t*) command, strlen(command));
    if (err){
        printf("rfcomm_send -> error 0X%02x", err);
    }
}

static void send_packet(void){
    switch (hfp_service_level_connection_state){
        case 1:
            send_str_over_rfcomm(rfcomm_cid, "\r\n+BRSF: 224\r\n\r\nOK\r\n");
            hfp_service_level_connection_state++;
            break;
        default:
            break;
    }
}

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
    if (packet_type == RFCOMM_DATA_PACKET){
        hfp_service_level_connection_state++;
        if (rfcomm_can_send_packet_now(rfcomm_cid)) send_packet();
        return;
    }
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];
    bd_addr_t event_addr;

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("Start SDP RFCOMM Query for UUID 0x%02x\n", BLUETOOTH_SERVICE_CLASS_HANDSFREE);
                sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, remote, BLUETOOTH_SERVICE_CLASS_HANDSFREE);
            }
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            printf("Pin code request - using '0000'\n");
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            gap_pin_code_response(event_addr, "0000");
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            printf("RFCOMM_EVENT_CHANNEL_OPENED packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
            } else {
                // data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                rfcomm_cid = little_endian_read_16(packet, 12);
                mtu = little_endian_read_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, mtu);
                break;
            }
            break;
        case RFCOMM_EVENT_CAN_SEND_NOW:
            if (!rfcomm_cid) break;
            if (rfcomm_can_send_packet_now(rfcomm_cid)) send_packet();
            break;
        default:
            break;
    }
}


static void hci_event_handler(uint8_t packet_type, uint8_t * packet, uint16_t size){
    packet_handler(packet_type, 0, packet, size);
}


static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (event->type){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            channel_nr = sdp_event_query_rfcomm_service_get_name(packet);
            printf("** Service name: '%s', RFCOMM port %u\n", sdp_event_query_rfcomm_service_get_rfcomm_channel(packet), channel_nr);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (channel_nr > 0) {
                printf("RFCOMM create channel.\n");
                rfcomm_create_channel(packet_handler, remote, channel_nr); 
                break;
            }
            printf("Service not found.\n");
            exit(0);
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    printf("Client HCI init done\r\n");
        
    /* Register for HCI events */
    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
    rfcomm_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}

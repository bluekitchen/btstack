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


#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "rfcomm.h"

#define NUM_ROWS 25
#define NUM_COLS 40

typedef enum {
    W4_SDP_RESULT,
    W4_SDP_COMPLETE,
    W4_RFCOMM_CHANNEL,
    DONE
} state_t;

#define DATA_VOLUME (1000 * 1000)

// configuration area {
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};     // address of remote device
static const char * spp_service_name_prefix = "Bluetooth-Incoming"; // default on OS X
// configuration area }

static uint8_t  test_data[NUM_ROWS * NUM_COLS];
static uint16_t test_data_len = sizeof(test_data);
static uint8_t  channel_nr = 0;
static uint16_t mtu;
static uint16_t rfcomm_cid = 0;
static uint32_t data_to_send =  DATA_VOLUME;
static state_t state = W4_SDP_RESULT;

static void create_test_data(void){
    int x,y;
    for (y=0;y<NUM_ROWS;y++){
        for (x=0;x<NUM_COLS-2;x++){
            test_data[y*NUM_COLS+x] = '0' + (x % 10);
        }
        test_data[y*NUM_COLS+NUM_COLS-2] = '\n';
        test_data[y*NUM_COLS+NUM_COLS-1] = '\r';
    }
}

static void send_packet(void){
    int err = rfcomm_send_internal(rfcomm_cid, (uint8_t*) test_data, test_data_len);
    if (err){
        printf("rfcomm_send_internal -> error 0X%02x", err);
        return;
    }
    
    if (data_to_send < test_data_len){
        rfcomm_disconnect_internal(rfcomm_cid);
        rfcomm_cid = 0;
        state = DONE;
        printf("SPP Streamer: enough data send, closing DLC\n");
        return;
    }
    data_to_send -= test_data_len;
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                sdp_query_rfcomm_channel_and_name_for_uuid(remote, SDP_PublicBrowseGroup);
            }
            break;
        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
            } else {
                // data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                rfcomm_cid = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, mtu);
                if ((test_data_len > mtu)) {
                    test_data_len = mtu;
                }
                if (rfcomm_can_send_packet_now(rfcomm_cid)) send_packet();
                break;
            }
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
        case RFCOMM_EVENT_CREDITS:
            if (rfcomm_can_send_packet_now(rfcomm_cid)) send_packet();
            break;
        default:
            break;
    }
}

static void handle_found_service(char * name, uint8_t port){
    printf("APP: Service name: '%s', RFCOMM port %u\n", name, port);

    if (strncmp(name, spp_service_name_prefix, strlen(spp_service_name_prefix)) != 0) return;

    printf("APP: matches requested SPP Service Name\n");
    channel_nr = port;
    state = W4_SDP_COMPLETE;
}

static void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
            
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            handle_found_service((char*) ve->service_name, ve->channel_nr);
            break;
        case SDP_QUERY_COMPLETE:
            if (state != W4_SDP_COMPLETE){
                printf("Requested SPP Service %s not found \n", spp_service_name_prefix);
                break;
            }
            // connect
            printf("Requested SPP Service found, creating RFCOMM channel\n");
            state = W4_RFCOMM_CHANNEL;
            rfcomm_create_channel_internal(NULL, remote, channel_nr); 
            break;
        default: 
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    create_test_data();

    printf("Client HCI init done\r\n");
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_register_packet_handler(packet_handler);

    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}

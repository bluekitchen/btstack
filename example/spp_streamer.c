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
#include "classic/rfcomm.h"
#include "btstack_event.h"

#define NUM_ROWS 25
#define NUM_COLS 40

typedef enum {
    W4_SDP_RESULT,
    W4_SDP_COMPLETE,
    W4_RFCOMM_CHANNEL,
    SENDING,
    DONE
} state_t;

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

#define DATA_VOLUME (10 * 1000 * 1000)

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
static btstack_packet_callback_registration_t hci_event_callback_registration;

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */
#define REPORT_INTERVAL_MS 3000
static uint32_t test_data_sent;
static uint32_t test_data_start;

static void test_reset(void){
    test_data_start = btstack_run_loop_get_time_ms();
    test_data_sent = 0;
}

static void test_track_sent(int bytes_sent){
    test_data_sent += test_data_len;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t time_passed = now - test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    int bytes_per_second = test_data_sent * 1000 / time_passed;
    printf("%u bytes sent-> %u.%03u kB/s\n", (int) test_data_sent, (int) bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    test_data_start = now;
    test_data_sent  = 0;
}
/* LISTING_END(tracking): Tracking throughput */


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
    rfcomm_send(rfcomm_cid, (uint8_t*) test_data, test_data_len);

    test_track_sent(test_data_len);
    if (data_to_send <= test_data_len){
        state = DONE;
        printf("SPP Streamer: enough data send, closing channel\n");
        rfcomm_disconnect(rfcomm_cid);
        rfcomm_cid = 0;
        return;
    }
    data_to_send -= test_data_len;
    rfcomm_request_can_send_now_event(rfcomm_cid);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("SDP Query for RFCOMM services on %s started\n", bd_addr_to_str(remote));
                sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, remote, SDP_PublicBrowseGroup);
            }
            break;
        case RFCOMM_EVENT_CHANNEL_OPENED:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                state = DONE;
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
            } else {
                // data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                state = SENDING;
                rfcomm_cid = little_endian_read_16(packet, 12);
                mtu = little_endian_read_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, mtu);
                if ((test_data_len > mtu)) {
                    test_data_len = mtu;
                }
                test_reset();
                rfcomm_request_can_send_now_event(rfcomm_cid);
                break;
            }
            break;
        case RFCOMM_EVENT_CAN_SEND_NOW:
            send_packet();
            break;
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            if (state != DONE) {
                printf("RFCOMM_EVENT_CHANNEL_CLOSED received before all test data was sent\n");
                state = DONE;
            }
            break;
        default:
            break;
    }
}

static void handle_found_service(const char * name, uint8_t port){
    printf("APP: Service name: '%s', RFCOMM port %u\n", name, port);

    if (strncmp(name, spp_service_name_prefix, strlen(spp_service_name_prefix)) != 0) return;

    printf("APP: matches requested SPP Service Name\n");
    channel_nr = port;
    state = W4_SDP_COMPLETE;
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){            
    switch (packet[0]){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            handle_found_service(sdp_event_query_rfcomm_service_get_name(packet), 
                                 sdp_event_query_rfcomm_service_get_rfcomm_channel(packet));
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (state != W4_SDP_COMPLETE){
                printf("Requested SPP Service %s not found \n", spp_service_name_prefix);
                break;
            }
            // connect
            printf("Requested SPP Service found, creating RFCOMM channel\n");
            state = W4_RFCOMM_CHANNEL;
            rfcomm_create_channel(packet_handler, remote, channel_nr, NULL); 
            break;
        default: 
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    create_test_data();

    printf("Client HCI init done\r\n");
    
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}

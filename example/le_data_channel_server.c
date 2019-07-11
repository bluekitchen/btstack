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

#define BTSTACK_FILE__ "le_data_channel_server.c"

// *****************************************************************************
/* EXAMPLE_START(le_data_channel): LE Data Channel Server - Receive data via L2CAP
 *
 * @text iOS 11 and newer supports LE Data Channels for fast transfer via LE
 *       [https://github.com/bluekitchen/CBL2CAPChannel-Demo](Basic iOS example on GitHub)
 */
 // *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "le_data_channel_server.h"

// #define TEST_STREAM_DATA
#define TEST_PACKET_SIZE 1000

#define REPORT_INTERVAL_MS 3000
#define MAX_NR_CONNECTIONS 3 

const uint16_t TSPX_le_psm = 0x25;

static void  packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x10, 0x09, 'L', 'E', ' ', 'D', 'a', 't', 'a', ' ', 'C', 'h', 'a', 'n', 'n', 'e', 'l', 
};
const uint8_t adv_data_len = sizeof(adv_data);

static btstack_packet_callback_registration_t hci_event_callback_registration;

// support for multiple clients
typedef struct {
    char name;
    hci_con_handle_t connection_handle;
    uint16_t cid;
    int  counter;
    char test_data[TEST_PACKET_SIZE];
    int  test_data_len;
    uint32_t test_data_sent;
    uint32_t test_data_start;
} le_data_channel_connection_t;

static le_data_channel_connection_t le_data_channel_connection;

static uint16_t initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;
static uint8_t data_channel_buffer[TEST_PACKET_SIZE];

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager, and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_data_channel.gatt$. Finally, it configures the advertisements 
 * and boots the Bluetooth stack. 
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP, SM, ATT Server, and enable advertisements */

static void le_data_channel_setup(void){

    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server: iOS disconnects if ATT MTU Exchange fails
    att_server_init(profile_data, NULL, NULL);    

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_register_packet_handler(&packet_handler);

    // le data channel setup
    l2cap_le_register_service(&packet_handler, TSPX_le_psm, LEVEL_0);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);
}
/* LISTING_END */

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */

static void test_reset(le_data_channel_connection_t * context){
    context->test_data_start = btstack_run_loop_get_time_ms();
    context->test_data_sent = 0;
}

static void test_track_data(le_data_channel_connection_t * context, int bytes_transferred){
    context->test_data_sent += bytes_transferred;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t time_passed = now - context->test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    int bytes_per_second = context->test_data_sent * 1000 / time_passed;
    printf("%c: %"PRIu32" bytes sent-> %u.%03u kB/s\n", context->name, context->test_data_sent, bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    context->test_data_start = now;
    context->test_data_sent  = 0;
}
/* LISTING_END(tracking): Tracking throughput */

#ifdef TEST_STREAM_DATA
/* LISTING_END */
/*
 * @section Streamer
 *
 * @text The streamer function checks if notifications are enabled and if a notification can be sent now.
 * It creates some test data - a single letter that gets increased every time - and tracks the data sent.
 */

 /* LISTING_START(streamer): Streaming code */
static void streamer(void){

    if (le_data_channel_connection.cid == 0) return;

    // create test data
    le_data_channel_connection.counter++;
    if (le_data_channel_connection.counter > 'Z') le_data_channel_connection.counter = 'A';
    memset(le_data_channel_connection.test_data, le_data_channel_connection.counter, le_data_channel_connection.test_data_len);

    // send
    l2cap_le_send_data(le_data_channel_connection.cid, (uint8_t *) le_data_channel_connection.test_data, le_data_channel_connection.test_data_len);

    // track
    test_track_data(&le_data_channel_connection, le_data_channel_connection.test_data_len);

    // request another packet
    l2cap_le_request_can_send_now_event(le_data_channel_connection.cid);
} 
/* LISTING_END */
#endif


/* 
 * @section Packet Handler
 *
 * @text The packet handler is used to stop the notifications and reset the MTU on connect
 * It would also be a good place to request the connection parameter update as indicated 
 * in the commented code block.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_address;
    uint16_t psm;
    uint16_t cid;
    uint16_t conn_interval;
    hci_con_handle_t handle;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                // BTstack activated, get started
                if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                    printf("To start the streaming, please run the le_data_channel_client example on other device.\n");
                } 
                break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // free connection
                    printf("%c: Disconnect, reason %02x\n", le_data_channel_connection.name, hci_event_disconnection_complete_get_reason(packet));                    
                    le_data_channel_connection.connection_handle = HCI_CON_HANDLE_INVALID;
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // print connection parameters (without using float operations)
                            conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                            printf("%c: Connection Interval: %u.%02u ms\n", le_data_channel_connection.name, conn_interval * 125 / 100, 25 * (conn_interval & 3));
                            printf("%c: Connection Latency: %u\n", le_data_channel_connection.name, hci_subevent_le_connection_complete_get_conn_latency(packet));

                            // min con interval 15 ms - supported from iOS 11 
                            gap_request_connection_parameter_update(le_data_channel_connection.connection_handle, 12, 12, 0, 0x0048);
                            printf("Connected, requesting conn param update for handle 0x%04x\n", le_data_channel_connection.connection_handle);
                            // 
                            test_reset(&le_data_channel_connection);
                            break;
                        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                            // print connection parameters (without using float operations)
                            conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                            printf("LE Connection Update:\n");
                            printf("%c: Connection Interval: %u.%02u ms\n", le_data_channel_connection.name, conn_interval * 125 / 100, 25 * (conn_interval & 3));
                            printf("%c: Connection Latency: %u\n", le_data_channel_connection.name, hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                            break;
                    }
                    break;  

                case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
                    printf("L2CAP Connection Parameter Update Complete, response: %x\n", l2cap_event_connection_parameter_update_response_get_result(packet));
                    break;

                // LE Data Channels

                case L2CAP_EVENT_LE_INCOMING_CONNECTION: 
                    psm = l2cap_event_le_incoming_connection_get_psm(packet);
                    cid = l2cap_event_le_incoming_connection_get_local_cid(packet);
                    if (psm != TSPX_le_psm) break;
                    printf("L2CAP: Accepting incoming LE connection request for 0x%02x, PSM %02x\n", cid, psm); 
                    l2cap_le_accept_connection(cid, data_channel_buffer, sizeof(data_channel_buffer), initial_credits);
                    break;

                case L2CAP_EVENT_LE_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_le_channel_opened_get_address(packet, event_address);
                    psm = l2cap_event_le_channel_opened_get_psm(packet); 
                    cid = l2cap_event_le_channel_opened_get_local_cid(packet); 
                    handle = l2cap_event_le_channel_opened_get_handle(packet);
                    if (packet[2] == 0) {
                        printf("L2CAP: LE Data Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_address), handle, psm, cid,  little_endian_read_16(packet, 15));
                        // setup new 
                        le_data_channel_connection.counter = 'A';
                        le_data_channel_connection.cid = cid;
                        le_data_channel_connection.connection_handle = handle;
                        le_data_channel_connection.test_data_len = btstack_min(l2cap_event_le_channel_opened_get_remote_mtu(packet), sizeof(le_data_channel_connection.test_data));
                        printf("Test packet size: %u\n", le_data_channel_connection.test_data_len);
                        test_reset(&le_data_channel_connection);
#ifdef TEST_STREAM_DATA
                        l2cap_le_request_can_send_now_event(le_data_channel_connection.cid);
#endif
                    } else {
                        printf("L2CAP: LE Data Channel connection to device %s failed. status code %u\n", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;

                case L2CAP_EVENT_LE_CHANNEL_CLOSED:
                    printf("L2CAP: LE Data Channel closed\n"); 
                    le_data_channel_connection.cid = 0;
                    break;

#ifdef TEST_STREAM_DATA
                case L2CAP_EVENT_LE_CAN_SEND_NOW:
                    streamer();
                    break;
#endif

                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            test_track_data(&le_data_channel_connection, size);
            break;

        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void)
{
    le_data_channel_setup();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

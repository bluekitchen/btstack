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

#define BTSTACK_FILE__ "nordic_spp_le_streamer.c"

// *****************************************************************************
/* EXAMPLE_START(nordic_spp_le_streamer): LE Nordic SPP-like Streamer Server 
 *
 * @text All newer operating systems provide GATT Client functionality.
 * This example shows how to get a maximal throughput via BLE:
 * - send whenever possible,
 * - use the max ATT MTU.
 *
 * @text In theory, we should also update the connection parameters, but we already get
 * a connection interval of 30 ms and there's no public way to use a shorter 
 * interval with iOS (if we're not implementing an HID device).
 *
 * @text Note: To start the streaming, run the example.
 * On remote device use some GATT Explorer, e.g. LightBlue, BLExplr to enable notifications.
 */
 // *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "ble/gatt-service/nordic_spp_service_server.h"

// nordic_spp_le_streamer.gatt contains the declaration of the provided GATT Services + Characteristics
// nordic_spp_le_streamer.h    contains the binary representation of nordic_spp_le_streamer.gatt
// it is generated by the build system by calling: $BTSTACK_ROOT/tool/compile_gatt.py nordic_spp_le_streamer.gatt nordic_spp_le_streamer.h
// it needs to be regenerated when the GATT Database declared in nordic_spp_le_streamer.gatt file is modified
#include "nordic_spp_le_streamer.h"

#define REPORT_INTERVAL_MS 3000
#define MAX_NR_CONNECTIONS 3 

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // Name
    8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'n', 'R', 'F',' ', 'S', 'P', 'P',
    // UUID ...
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, 0x9e, 0xca, 0xdc, 0x24, 0xe, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x1, 0x0, 0x40, 0x6e,
};
const uint8_t adv_data_len = sizeof(adv_data);

static btstack_packet_callback_registration_t hci_event_callback_registration;

// support for multiple clients
typedef struct {
    char name;
    int le_notification_enabled;
    hci_con_handle_t connection_handle;
    int  counter;
    char test_data[200];
    int  test_data_len;
    uint32_t test_data_sent;
    uint32_t test_data_start;
    btstack_context_callback_registration_t send_request;
} nordic_spp_le_streamer_connection_t;

static nordic_spp_le_streamer_connection_t nordic_spp_le_streamer_connections[MAX_NR_CONNECTIONS];

// round robin sending
static int connection_index;

static void init_connections(void){
    // track connections
    int i;
    for (i=0;i<MAX_NR_CONNECTIONS;i++){
        nordic_spp_le_streamer_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
        nordic_spp_le_streamer_connections[i].name = 'A' + i;
    }
}

static nordic_spp_le_streamer_connection_t * connection_for_conn_handle(hci_con_handle_t conn_handle){
    int i;
    for (i=0;i<MAX_NR_CONNECTIONS;i++){
        if (nordic_spp_le_streamer_connections[i].connection_handle == conn_handle) return &nordic_spp_le_streamer_connections[i];
    }
    return NULL;
}

static void next_connection_index(void){
    connection_index++;
    if (connection_index == MAX_NR_CONNECTIONS){
        connection_index = 0;
    }
}

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */

static void test_reset(nordic_spp_le_streamer_connection_t * context){
    context->test_data_start = btstack_run_loop_get_time_ms();
    context->test_data_sent = 0;
}

static void test_track_sent(nordic_spp_le_streamer_connection_t * context, int bytes_sent){
    context->test_data_sent += bytes_sent;
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

/* 
 * @section HCI Packet Handler
 *
 * @text The packet handler prints the welcome message and requests a connection paramter update for LE Connections
 */

/* LISTING_START(packetHandler): Packet Handler */
static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    uint16_t conn_interval;
    hci_con_handle_t con_handle;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                printf("To start the streaming, please run nRF Toolbox -> UART to connect.\n");
            } 
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
                        case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // print connection parameters (without using float operations)
                            con_handle    = gap_subevent_le_connection_complete_get_connection_handle(packet);
                            conn_interval = gap_subevent_le_connection_complete_get_conn_interval(packet);
                            printf("LE Connection - Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                            printf("LE Connection - Connection Latency: %u\n", gap_subevent_le_connection_complete_get_conn_latency(packet));

                            // request min con interval 15 ms for iOS 11+
                            printf("LE Connection - Request 15 ms connection interval\n");
                            gap_request_connection_parameter_update(con_handle, 12, 12, 4, 0x0048);
                            break;
                        default:
                            break;
                    }
                break;
                default:
                    break;
            }
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                    // print connection parameters (without using float operations)
                    con_handle    = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
                    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                    printf("LE Connection - Connection Param update - connection interval %u.%02u ms, latency %u\n", conn_interval * 125 / 100,
                        25 * (conn_interval & 3), hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                    break;
                default:
                    break;
            }
            break;  
        default:
            break;
    }
}
/* LISTING_END */

/* 
 * @section ATT Packet Handler
 *
 * @text The packet handler is used to setup and tear down the spp-over-gatt connection and its MTU
 */

/* LISTING_START(packetHandler): Packet Handler */
static void att_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET) return;

    int mtu;
    nordic_spp_le_streamer_connection_t * context;
    
    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED:
            // setup new 
            context = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
            if (!context) break;
            context->counter = 'A';
            context->test_data_len = ATT_DEFAULT_MTU - 4;   // -1 for nordic 0x01 packet type
            context->connection_handle = att_event_connected_get_handle(packet);
            break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            context = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            if (!context) break;
            context->test_data_len = btstack_min(mtu - 3, sizeof(context->test_data));
            printf("%c: ATT MTU = %u => use test data of len %u\n", context->name, mtu, context->test_data_len);
            break;
        case ATT_EVENT_DISCONNECTED:
            context = connection_for_conn_handle(att_event_disconnected_get_handle(packet));
            if (!context) break;
            // free connection
            printf("%c: Disconnect\n", context->name);                    
            context->le_notification_enabled = 0;
            context->connection_handle = HCI_CON_HANDLE_INVALID;
            break;
        default:
            break;
    }
}
/* LISTING_END */



/*
 * @section Streamer
 *
 * @text The streamer function checks if notifications are enabled and if a notification can be sent now.
 * It creates some test data - a single letter that gets increased every time - and tracks the data sent.
 */

 /* LISTING_START(streamer): Streaming code */
static void nordic_can_send(void * some_context){
    UNUSED(some_context);

    // find next active streaming connection
    int old_connection_index = connection_index;
    while (1){
        // active found?
        if ((nordic_spp_le_streamer_connections[connection_index].connection_handle != HCI_CON_HANDLE_INVALID) &&
            (nordic_spp_le_streamer_connections[connection_index].le_notification_enabled)) break;
        
        // check next
        next_connection_index();

        // none found
        if (connection_index == old_connection_index) return;
    }

    nordic_spp_le_streamer_connection_t * context = &nordic_spp_le_streamer_connections[connection_index];

    // create test data
    context->counter++;
    if (context->counter > 'Z') context->counter = 'A';
    memset(context->test_data, context->counter, context->test_data_len);

    // send
    nordic_spp_service_server_send(context->connection_handle, (uint8_t*) context->test_data, context->test_data_len);

    // track
    test_track_sent(context, context->test_data_len);

    // request next send event
    nordic_spp_service_server_request_can_send_now(&context->send_request, context->connection_handle);

    // check next
    next_connection_index();
} 
/* LISTING_END */

static void nordic_spp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hci_con_handle_t con_handle;
    nordic_spp_le_streamer_connection_t * context;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) break;
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED:
                    con_handle = gattservice_subevent_spp_service_connected_get_con_handle(packet);
                    context = connection_for_conn_handle(con_handle);
                    if (!context) break;
                    context->le_notification_enabled = 1;
                    test_reset(context);
                    context->send_request.callback = &nordic_can_send;
                    nordic_spp_service_server_request_can_send_now(&context->send_request, context->connection_handle);
                    break;
                case GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED:
                    con_handle = HCI_CON_HANDLE_INVALID;
                    context = connection_for_conn_handle(con_handle);
                    if (!context) break;
                    context->le_notification_enabled = 0;
                    break;
                default:
                    break;
            }
            break;
        case RFCOMM_DATA_PACKET:
            printf("RECV: ");
            printf_hexdump(packet, size);
            context = connection_for_conn_handle((hci_con_handle_t) channel);
            if (!context) break;
            test_track_sent(context, size);
            break;
        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void){
    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    
    
    // setup Nordic SPP service
    nordic_spp_service_server_init(&nordic_spp_packet_handler);

    // register for ATT events
    att_server_register_packet_handler(att_packet_handler);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // init client state
    init_connections();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

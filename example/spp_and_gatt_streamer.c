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

#define BTSTACK_FILE__ "spp_and_gatt_streamer.c"

// *****************************************************************************
/* EXAMPLE_START(spp_and_le_streamer): Dual mode example
 * 
 * @text The SPP and LE Streamer example combines the Bluetooth Classic SPP Streamer
 * and the Bluetooth LE Streamer into a single application.
 *
 * @text In this Section, we only point out the differences to the individual examples
 * and how how the stack is configured.
 *
 * @text Note: To test, please run the example, and then: 
 *    - for SPP pair from a remote device, and open the Virtual Serial Port,
 *    - for LE use some GATT Explorer, e.g. LightBlue, BLExplr, to enable notifications.
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
#include "btstack.h"
#include "spp_and_gatt_streamer.h"

int btstack_main(int argc, const char * argv[]);

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

#define TEST_COD 0x1234
#define NUM_ROWS 25
#define NUM_COLS 40
#define DATA_VOLUME (10 * 1000 * 1000)

/*
 * @section Advertisements 
 *
 * @text The Flags attribute in the Advertisement Data indicates if a device is in dual-mode or not.
 * Flag 0x06 indicates LE General Discoverable, BR/EDR not supported although we're actually using BR/EDR.
 * In the past, there have been problems with Anrdoid devices when the flag was not set.
 * Setting it should prevent the remote implementation to try to use GATT over LE/EDR, which is not 
 * implemented by BTstack. So, setting the flag seems like the safer choice (while it's technically incorrect).
 */
/* LISTING_START(advertisements): Advertisement data: Flag 0x06 indicates LE-only device */
const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x0c, 0x09, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r', 
};

static btstack_packet_callback_registration_t hci_event_callback_registration;

uint8_t adv_data_len = sizeof(adv_data);

static uint8_t  test_data[NUM_ROWS * NUM_COLS];

// SPP
static uint8_t   spp_service_buffer[150];

static uint16_t  spp_test_data_len;
static uint16_t  rfcomm_mtu;
static uint16_t  rfcomm_cid = 0;
// static uint32_t  data_to_send =  DATA_VOLUME;

// LE
static uint16_t         att_mtu;
static int              counter = 'A';
static int              le_notification_enabled;
static uint16_t         le_test_data_len;
static hci_con_handle_t le_connection_handle;

#ifdef ENABLE_GATT_OVER_CLASSIC
static uint8_t gatt_service_buffer[70];
#endif

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */
#define REPORT_INTERVAL_MS 3000
static uint32_t test_data_transferred;
static uint32_t test_data_start;

static void test_reset(void){
    test_data_start = btstack_run_loop_get_time_ms();
    test_data_transferred = 0;
}

static void test_track_transferred(int bytes_sent){
    test_data_transferred += bytes_sent;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t time_passed = now - test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    int bytes_per_second = test_data_transferred * 1000 / time_passed;
    printf("%u bytes -> %u.%03u kB/s\n", (int) test_data_transferred, (int) bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    test_data_start = now;
    test_data_transferred  = 0;
}
/* LISTING_END(tracking): Tracking throughput */


static void spp_create_test_data(void){
    int x,y;
    for (y=0;y<NUM_ROWS;y++){
        for (x=0;x<NUM_COLS-2;x++){
            test_data[y*NUM_COLS+x] = '0' + (x % 10);
        }
        test_data[y*NUM_COLS+NUM_COLS-2] = '\n';
        test_data[y*NUM_COLS+NUM_COLS-1] = '\r';
    }
}

static void spp_send_packet(void){
    rfcomm_send(rfcomm_cid, (uint8_t*) test_data, spp_test_data_len);

    test_track_transferred(spp_test_data_len);
#if 0
    if (data_to_send <= spp_test_data_len){
        printf("SPP Streamer: enough data send, closing channel\n");
        rfcomm_disconnect(rfcomm_cid);
        rfcomm_cid = 0;
        return;
    }
    data_to_send -= spp_test_data_len;
#endif
    rfcomm_request_can_send_now_event(rfcomm_cid);
}

static void le_streamer(void){
    // check if we can send
    if (!le_notification_enabled) return;

    // create test data
    counter++;
    if (counter > 'Z') counter = 'A';
    memset(test_data, counter, sizeof(test_data));

    // send
    att_server_notify(le_connection_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) test_data, le_test_data_len);

    // track
    test_track_transferred(le_test_data_len);

    // request next send event
    att_server_request_can_send_now_event(le_connection_handle);
} 

/* 
 * @section HCI Packet Handler
 * 
 * @text The packet handler of the combined example is just the combination of the individual packet handlers.
 */

static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_addr;
    uint16_t  conn_interval;
    hci_con_handle_t con_handle;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // print connection parameters (without using float operations)
                            con_handle    = hci_subevent_le_connection_complete_get_connection_handle(packet);
                            conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                            printf("LE Connection - Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                            printf("LE Connection - Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));

                            // request min con interval 15 ms for iOS 11+ 
                            printf("LE Connection - Request 15 ms connection interval\n");
                            gap_request_connection_parameter_update(con_handle, 12, 12, 0, 0x0048);
                            break;

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

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // re-enable page/inquiry scan again
                    gap_discoverable_control(1);
                    gap_connectable_control(1);
                    // re-enable advertisements
                    gap_advertisements_enable(1);
                    le_notification_enabled = 0;
                    break;

                default:
                    break;
            }
            break;
                        
        default:
            break;
    }
}

/* 
 * @section RFCOMM Packet Handler
 * 
 * @text The RFCOMM packet handler accepts incoming connection and triggers sending of RFCOMM data on RFCOMM_EVENT_CAN_SEND_NOW
 */

static void rfcomm_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr); 
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_cid);
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status %u\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, rfcomm_mtu);

                        spp_test_data_len = rfcomm_mtu;
                        if (spp_test_data_len > sizeof(test_data)){
                            spp_test_data_len = sizeof(test_data);
                        }

                        // disable page/inquiry scan to get max performance
                        gap_discoverable_control(0);
                        gap_connectable_control(0);
                        // disable advertisements
                        gap_advertisements_enable(0);

                        test_reset();
                        rfcomm_request_can_send_now_event(rfcomm_cid);
                    }
                    break;

                case RFCOMM_EVENT_CAN_SEND_NOW:
                    spp_send_packet();
                    break;

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_cid = 0;
                    break;

                default:
                    break;
            }
            break;
                        
        case RFCOMM_DATA_PACKET:
            test_track_transferred(size);
#if 0
            printf("RCV: '");
            for (i=0;i<size;i++){
                putchar(packet[i]);
            }
            printf("'\n");
#endif
            break;

        default:
            break;
    }
}

/* 
 * @section ATT Packet Handler
 *
 * @text The packet handler is used to track the ATT MTU Exchange and trigger ATT send
 */

/* LISTING_START(attPacketHandler): Packet Handler */
static void att_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED:
            le_connection_handle = att_event_connected_get_handle(packet);
            att_mtu = att_server_get_mtu(le_connection_handle);
            le_test_data_len = btstack_min(att_server_get_mtu(le_connection_handle) - 3, sizeof(test_data));
            printf("ATT MTU = %u\n", att_mtu);
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            att_mtu = att_event_mtu_exchange_complete_get_MTU(packet);
            le_test_data_len = btstack_min(att_mtu - 3, sizeof(test_data));
            printf("ATT MTU = %u\n", att_mtu);
            break;

        case ATT_EVENT_CAN_SEND_NOW:
            le_streamer();
            break;

        case ATT_EVENT_DISCONNECTED:
            le_notification_enabled = 0;
            le_connection_handle = HCI_CON_HANDLE_INVALID;
            break;
            
        default:
            break;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

// write requests
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(offset);
    UNUSED(buffer_size);

    // printf("att_write_callback att_handle %04x, transaction mode %u\n", att_handle, transaction_mode);
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE) return 0;
    switch(att_handle){
        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
            le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            printf("Notifications enabled %u\n", le_notification_enabled); 
            if (le_notification_enabled){
                att_server_request_can_send_now_event(le_connection_handle);
            }

            // disable page/inquiry scan to get max performance
            gap_discoverable_control(0);
            gap_connectable_control(0);

            test_reset();
            break;
        default:
            break;
    }
    return 0;
}

/*
 * @section Main Application Setup
 *
 * @text As with the packet and the heartbeat handlers, the combined app setup contains the code from the individual example setups.
 */


/* LISTING_START(MainConfiguration): Init L2CAP RFCOMM SDO SM ATT Server and start heartbeat timer */
int btstack_main(int argc, const char * argv[])
{
    UNUSED(argc);
    (void)argv;

    l2cap_init();

    rfcomm_init();
    rfcomm_register_service(rfcomm_packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Streamer");
    sdp_register_service(spp_service_buffer);

#ifdef ENABLE_GATT_OVER_CLASSIC
    // init SDP, create record for GATT and register with SDP
    memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
    gatt_create_sdp_record(gatt_service_buffer, 0x10001, ATT_SERVICE_GATT_SERVICE_START_HANDLE, ATT_SERVICE_GATT_SERVICE_END_HANDLE);
    sdp_register_service(gatt_service_buffer);
#endif

    gap_set_local_name("SPP and LE Streamer 00:00:00:00:00:00");
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // short-cut to find other SPP Streamer
    gap_set_class_of_device(TEST_COD);

    gap_discoverable_control(1);

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

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

    spp_create_test_data();

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

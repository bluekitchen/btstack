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

#define __BTSTACK_FILE__ "spp_and_le_streamer.c"

// *****************************************************************************
/* EXAMPLE_START(spp_and_le_counter): Dual mode example
 * 
 * @text The SPP and LE Counter example combines the Bluetooth Classic SPP Counter
 * and the Bluetooth LE Counter into a single application.
 *
 * In this Section, we only point out the differences to the individual examples
 * and how how the stack is configured.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
#include "btstack.h"
#include "spp_and_le_streamer.h"

int btstack_main(int argc, const char * argv[]);

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

#define TEST_COD 0x1234
#define NUM_ROWS 25
#define NUM_COLS 40
#define DATA_VOLUME (10 * 1000 * 1000)

// prototypes
static enum test_mode {
    TEST_NONE,
    TEST_SPP,
    TEST_LE,
} test_mode = TEST_NONE;

typedef enum {
    // SPP
    W4_PEER_COD,
    W4_SCAN_COMPLETE,
    W4_SDP_RESULT,
    W4_SDP_COMPLETE,
    W4_RFCOMM_CHANNEL,
    SENDING,
    // LE
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_SERVICE_RESULT,
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_TEST_DATA,
    // Both
    DONE
} state_t;

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

static const uint8_t eir_data[] = { 
// iAP2 UUID
0x11, 0x07, 0xff, 0xca, 0xca, 0xde, 0xaf, 0xde, 0xca, 0xde, 0xde, 0xfa, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00,
// Local Name
0x13, 0x09, 'B','T','s','t','a','c','k',' ','M','F','i',' ','D','e','v','i','c','e',
// TX Power Level
0x02, 0x0a, 0x00
}; 

static btstack_packet_callback_registration_t hci_event_callback_registration;

uint8_t adv_data_len = sizeof(adv_data);

static bd_addr_t peer_addr;

static uint8_t  test_data[NUM_ROWS * NUM_COLS];
static state_t state = W4_SDP_RESULT;

// SPP
static uint8_t   spp_service_buffer[150];

static uint16_t  spp_test_data_len = sizeof(test_data);
static uint16_t  rfcomm_mtu;
static uint16_t  rfcomm_cid = 0;
// static uint32_t  data_to_send =  DATA_VOLUME;

// LE
static uint16_t         att_mtu;
static int              counter = 'A';
static int              le_notification_enabled;
static uint16_t         le_test_data_len;
static hci_con_handle_t le_connection_handle;

// addr and type of device with correct name
static bd_addr_t      le_streamer_addr;
static bd_addr_type_t le_streamer_addr_type;
static uint8_t        le_streamer_service_uuid[16]        = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static uint8_t        le_streamer_characteristic_uuid[16] = { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static gatt_client_service_t        le_streamer_service;
static gatt_client_characteristic_t le_streamer_characteristic;
static gatt_client_notification_t   notification_registration;


/** 
 * Find remote peer by COD
 */
#define INQUIRY_INTERVAL 5
static void start_scan(void){
    printf("Starting inquiry scan..\n");
    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
    state = W4_PEER_COD;
}
static void stop_scan(void){
    printf("Stopping inquiry scan..\n");
    hci_send_cmd(&hci_inquiry_cancel);
    state = W4_SCAN_COMPLETE;
}
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
        state = DONE;
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

// returns 1 if name is found in advertisement
static int advertisement_report_contains_name(const char * name, uint8_t * advertisement_report){
    // get advertisement from report event
    const uint8_t * adv_data = gap_event_advertising_report_get_data(advertisement_report);
    uint16_t        adv_len  = gap_event_advertising_report_get_data_length(advertisement_report);
    int             name_len = strlen(name);

    // iterate over advertisement data
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t data_size    = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        int i;
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                // compare common prefix
                for (i=0; i<data_size && i<name_len;i++){
                    if (data[i] != name[i]) break;
                }
                // prefix match
                return 1;
            default:
                break;
        }
    }
    return 0;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch(state){
        case TC_W4_SERVICE_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &le_streamer_service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("SERVICE_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(le_connection_handle);
                        break;  
                    } 
                    // service query complete, look for characteristic
                    state = TC_W4_CHARACTERISTIC_RESULT;
                    printf("Search for LE Streamer test characteristic.\n");
                    gatt_client_discover_characteristics_for_service_by_uuid128(handle_gatt_client_event, le_connection_handle, &le_streamer_service, le_streamer_characteristic_uuid);
                    break;
                default:
                    break;
            }
            break;
            
        case TC_W4_CHARACTERISTIC_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &le_streamer_characteristic);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(le_connection_handle);
                        break;  
                    } 
                    // register handler for notifications
                    gatt_client_listen_for_characteristic_value_updates(&notification_registration, handle_gatt_client_event, le_connection_handle, &le_streamer_characteristic);
                    // enable notifications
                    test_reset();
                    state = TC_W4_TEST_DATA;
                    printf("Start streaming - enable notify on test characteristic.\n");
                    gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, le_connection_handle, &le_streamer_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_TEST_DATA:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_NOTIFICATION:
                    // printf("Data: ");
                    // printf_hexdump( gatt_event_notification_get_value(packet), gatt_event_notification_get_value_length(packet));
                    // just track data
                    test_track_transferred(gatt_event_notification_get_value_length(packet));
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    break;
                default:
                    printf("Unknown packet type %x\n", hci_event_packet_get_type(packet));
                    break;
            }
            break;

        default:
            printf("error\n");
            break;
    }
    
}
/* 
 * @section Packet Handler
 * 
 * @text The packet handler of the combined example is just the combination of the individual packet handlers.
 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    int i;
    int num_responses;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {

				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    le_notification_enabled = 0;
                    break;

                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            le_test_data_len = ATT_DEFAULT_MTU - 3;
                            le_connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);

                            if (state != TC_W4_CONNECT) return;
                            printf("Change connection interval to minimum 7.5 ms\n");
                            gap_update_connection_parameters(le_connection_handle, 6, 6, 4, 1000);
                            // query primary services
                            printf("Search for LE Streamer service.\n");
                            state = TC_W4_SERVICE_RESULT;
                            gatt_client_discover_primary_services_by_uuid128(handle_gatt_client_event, le_connection_handle, le_streamer_service_uuid);
                            break;
                    }
                    break;  

                case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
                    att_mtu = att_event_mtu_exchange_complete_get_MTU(packet);
                    printf("ATT MTU = %u\n", att_mtu);
                    le_test_data_len = att_mtu - 3;
                    if (le_test_data_len > sizeof(test_data)){
                        le_test_data_len = sizeof(test_data);
                    }
                    break;

                case ATT_EVENT_CAN_SEND_NOW:
                    le_streamer();
                    break;

                case GAP_EVENT_ADVERTISING_REPORT:
                    if (state != TC_W4_SCAN_RESULT) return;
                    // check name in advertisement
                    if (!advertisement_report_contains_name("LE Streamer", packet)) return;
                    // store address and type
                    gap_event_advertising_report_get_address(packet, le_streamer_addr);
                    le_streamer_addr_type = gap_event_advertising_report_get_address_type(packet);
                    // stop scanning, and connect to the device
                    state = TC_W4_CONNECT;
                    gap_stop_scan();
                    printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(le_streamer_addr));
                    gap_connect(le_streamer_addr,le_streamer_addr_type);
                    break;

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

                        // initiator also sends data
                        test_reset();
                        if (test_mode == TEST_SPP){
                            rfcomm_request_can_send_now_event(rfcomm_cid);
                        }
                    }
					break;

                case RFCOMM_EVENT_CAN_SEND_NOW:
                    if (test_mode == TEST_SPP){
                        spp_send_packet();
                    }
                    break;

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_cid = 0;
                    break;

                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    break;

                case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
                    num_responses = hci_event_inquiry_result_get_num_responses(packet);
                    for (i=0; i<num_responses;i++){
                        reverse_bd_addr(&packet[3 + i * 6], event_addr);
                        uint32_t class_of_device = little_endian_read_24(packet, 3 + num_responses*(6+1+1)     + i*3);
                        if (class_of_device == TEST_COD){
                            memcpy(peer_addr, event_addr, 6);
                            printf("Peer found: %s\n", bd_addr_to_str(peer_addr));
                            stop_scan();

                            printf("Start to connect\n");
                            state = W4_RFCOMM_CHANNEL;
                            rfcomm_create_channel(packet_handler, peer_addr, RFCOMM_SERVER_CHANNEL, NULL); 
                        } else {
                            printf("Device found: %s with COD: 0x%06x\n", bd_addr_to_str(event_addr), (int) class_of_device);
                        }
                    }
                    break;
                    
                case HCI_EVENT_INQUIRY_COMPLETE:
                    printf("Inquiry complete\n");
                    printf("Peer not found, starting scan again\n");
                    start_scan();
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

    // printf("att_write_callback att_handle %04x, transaction mode %u\n", att_handle, transaction_mode);
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE) return 0;
    switch(att_handle){
        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
            le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            printf("Notifications enabled %u\n", le_notification_enabled); 
            if (le_notification_enabled){
                att_server_request_can_send_now_event(le_connection_handle);
            }
            test_reset();
            break;
        default:
            break;
    }
    return 0;
}

#if 0
static void process_button(int button){
    port_set_button_handler(NULL);
    switch (button){
        case 0:
            test_mode = TEST_SPP;
            printf("Test started: SPP client\n");
            start_scan();
            break;
        case 1:
            test_mode = TEST_LE;
            printf("Test started: Central + GATT client\n");
            state = TC_W4_SCAN_RESULT;
            gap_set_scan_parameters(0,0x0030, 0x0030);
            gap_start_scan();
            break;
        default:
            break;
    }
    // disable other services
    gap_connectable_control(0);
    gap_discoverable_control(0);
    gap_advertisements_enable(0);
}
#endif


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


    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Streamer");
    sdp_register_service(spp_service_buffer);
    // printf("SDP service record size: %u\n", de_get_len(spp_service_buffer));

    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // short-cut to find other ODIN module
    gap_set_class_of_device(TEST_COD);

    gap_discoverable_control(1);

    // setup le device db
    le_device_db_init();

    // enabled EIR
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
    gap_set_extended_inquiry_response(eir_data);

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_server_register_packet_handler(packet_handler);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    gatt_client_init();

    spp_create_test_data();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

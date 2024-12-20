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

#define BTSTACK_FILE__ "spp_streamer_client.c"

/*
 * spp_streamer_client.c
 */

// *****************************************************************************
/* EXAMPLE_START(spp_streamer_client): Performance - Stream Data over SPP (Client)
 * 
 * @text Note: The SPP Streamer Client scans for and connects to SPP Streamer,
 * and measures the throughput.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
#include "btstack.h"

// prototypes
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint8_t rfcomm_server_channel;

#define NUM_ROWS 25
#define NUM_COLS 40

#define TEST_COD 0x1234

#define TEST_MODE_SEND      1
#define TEST_MODE_RECEIVE   2
#define TEST_MODE_DUPLEX    3

// configure test mode: send only, receive only, full duplex
#define TEST_MODE TEST_MODE_SEND

typedef enum {
    // SPP
    W4_PEER_COD,
    W4_SCAN_COMPLETE,
    W4_SDP_RESULT,
    W2_SEND_SDP_QUERY,
    W4_RFCOMM_CHANNEL,
    SENDING,
    DONE
} state_t;

static uint8_t   test_data[NUM_ROWS * NUM_COLS];
static uint16_t  spp_test_data_len;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;

static bd_addr_t peer_addr;
static state_t state;

// SPP
static uint16_t  rfcomm_mtu;
static uint16_t  rfcomm_cid = 0;
// static uint32_t  data_to_send =  DATA_VOLUME;

/**
 * RFCOMM can make use for ERTM. Due to the need to re-transmit packets,
 * a large buffer is needed to still get high throughput
 */
#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM
static uint8_t ertm_buffer[20000];
static l2cap_ertm_config_t ertm_config = {
    0,       // ertm mandatory
    8,       // max transmit
    2000,
    12000,
    1000,    // l2cap ertm mtu
    8,
    8,
    0,       // No FCS
};
static int ertm_buffer_in_use;
static void rfcomm_ertm_request_handler(rfcomm_ertm_request_t * ertm_request){
    printf("ERTM Buffer requested, buffer in use %u\n", ertm_buffer_in_use);
    if (ertm_buffer_in_use) return;
    ertm_buffer_in_use = 1;
    ertm_request->ertm_config      = &ertm_config;
    ertm_request->ertm_buffer      = ertm_buffer;
    ertm_request->ertm_buffer_size = sizeof(ertm_buffer);
}
static void rfcomm_ertm_released_handler(uint16_t ertm_id){
    printf("ERTM Buffer released, buffer in use  %u, ertm_id %x\n", ertm_buffer_in_use, ertm_id);
    ertm_buffer_in_use = 0;
}
#endif

/** 
 * Find remote peer by COD
 */
#define INQUIRY_INTERVAL 5
static void start_scan(void){
    printf("Starting inquiry scan..\n");
    state = W4_PEER_COD;
    gap_inquiry_start(INQUIRY_INTERVAL);
}
static void stop_scan(void){
    printf("Stopping inquiry scan..\n");
    state = W4_SCAN_COMPLETE;
    gap_inquiry_stop();
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

#if (TEST_MODE & TEST_MODE_SEND)
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
    rfcomm_request_can_send_now_event(rfcomm_cid);
}
#endif

/* 
 * @section SDP Query Packet Handler
 * 
 * @text Store RFCOMM Channel for SPP service and initiates RFCOMM connection
 */
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            rfcomm_server_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                printf("SDP query failed, status 0x%02x\n", sdp_event_query_complete_get_status(packet));
                break;
            } 
            if (rfcomm_server_channel == 0){
                printf("No SPP service found\n");
                break;
            }
            printf("SDP query done, channel 0x%02x.\n", rfcomm_server_channel);
            rfcomm_create_channel(packet_handler, peer_addr, rfcomm_server_channel, NULL); 
            break;
        default:
            break;
    }
}

static void handle_start_sdp_client_query(void * context){
    UNUSED(context);
    if (state != W2_SEND_SDP_QUERY) return;
    state = W4_RFCOMM_CHANNEL;
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, peer_addr, BLUETOOTH_SERVICE_CLASS_SERIAL_PORT);
}


/* 
 * @section Gerenal Packet Handler
 * 
 * @text Handles startup (BTSTACK_EVENT_STATE), inquiry, pairing, starts SDP query for SPP service, and RFCOMM connection
 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint32_t class_of_device;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {

                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    start_scan();
                    break;

                case GAP_EVENT_INQUIRY_RESULT:
                    if (state != W4_PEER_COD) break;
                    class_of_device = gap_event_inquiry_result_get_class_of_device(packet);
                    gap_event_inquiry_result_get_bd_addr(packet, event_addr);
                    if (class_of_device == TEST_COD){
                        memcpy(peer_addr, event_addr, 6);
                        printf("Peer found: %s\n", bd_addr_to_str(peer_addr));
                        stop_scan();
                    } else {
                        printf("Device found: %s with COD: 0x%06x\n", bd_addr_to_str(event_addr), (int) class_of_device);
                    }                        
                    break;
                    
                case GAP_EVENT_INQUIRY_COMPLETE:
                    switch (state){
                        case W4_PEER_COD:                        
                            printf("Inquiry complete\n");
                            printf("Peer not found, starting scan again\n");
                            start_scan();
                            break;                        
                        case W4_SCAN_COMPLETE:
                            printf("Start to connect and query for SPP service\n");
                            state = W2_SEND_SDP_QUERY;
                            handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
                            (void) sdp_client_register_query_callback(&handle_sdp_client_query_request);
                            break;
                        default:
                            break;
                    }
                    if (state == W4_PEER_COD){
                    }
                    break;

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

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel 0x%02x requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_cid);
					break;
					
				case RFCOMM_EVENT_CHANNEL_OPENED:
					if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID 0x%02x, max frame size %u\n", rfcomm_cid, rfcomm_mtu);
                        test_reset();

                        // disable page/inquiry scan to get max performance
                        gap_discoverable_control(0);
                        gap_connectable_control(0);

#if (TEST_MODE & TEST_MODE_SEND)
                        // configure test data
                        spp_test_data_len = rfcomm_mtu;
                        if (spp_test_data_len > sizeof(test_data)){
                            spp_test_data_len = sizeof(test_data);
                        }
                        spp_create_test_data();
                        state = SENDING;
                        // start sending
                        rfcomm_request_can_send_now_event(rfcomm_cid);
#endif
                    }
					break;

#if (TEST_MODE & TEST_MODE_SEND)
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    spp_send_packet();
                    break;
#endif

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_cid = 0;

                    // re-enable page/inquiry scan again
                    gap_discoverable_control(1);
                    gap_connectable_control(1);
                    break;



                default:
                    break;
			}
            break;
                        
        case RFCOMM_DATA_PACKET:
            test_track_transferred(size);
            
#if 0
            // optional: print received data as ASCII text
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
 * @section Main Application Setup
 *
 * @text As with the packet and the heartbeat handlers, the combined app setup contains the code from the individual example setups.
 */


/* LISTING_START(MainConfiguration): Init L2CAP RFCOMM SDO SM ATT Server and start heartbeat timer */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM
    // setup ERTM management
    rfcomm_enable_l2cap_ertm(&rfcomm_ertm_request_handler, &rfcomm_ertm_released_handler);
#endif

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init SDP
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

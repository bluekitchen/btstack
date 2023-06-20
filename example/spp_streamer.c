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

#define BTSTACK_FILE__ "spp_streamer.c"

/*
 * spp_streamer.c
 */

// *****************************************************************************
/* EXAMPLE_START(spp_streamer): Performance - Stream Data over SPP (Server)
 * 
 * @text After RFCOMM connections gets open, request a
 * RFCOMM_EVENT_CAN_SEND_NOW via rfcomm_request_can_send_now_event().
 * @text When we get the RFCOMM_EVENT_CAN_SEND_NOW, send data and request another one.
 *
 * @text Note: To test, run the example, pair from a remote 
 * device, and open the Virtual Serial Port.
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

int btstack_main(int argc, const char * argv[]);

#define RFCOMM_SERVER_CHANNEL 1

#define TEST_COD 0x1234
#define NUM_ROWS 25
#define NUM_COLS 40
#define DATA_VOLUME (10 * 1000 * 1000)

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t  test_data[NUM_ROWS * NUM_COLS];

// SPP
static uint8_t   spp_service_buffer[150];

static uint16_t  spp_test_data_len;
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

/* 
 * @section Packet Handler
 * 
 * @text The packet handler of the combined example is just the combination of the individual packet handlers.
 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;

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
                    printf("SSP User Confirmation Request with numeric value '%06" PRIu32 "'\n", little_endian_read_32(packet, 8));
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

                        spp_test_data_len = rfcomm_mtu;
                        if (spp_test_data_len > sizeof(test_data)){
                            spp_test_data_len = sizeof(test_data);
                        }

                        // disable page/inquiry scan to get max performance
                        gap_discoverable_control(0);
                        gap_connectable_control(0);

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


/* LISTING_START(MainConfiguration): Init L2CAP RFCOMM SDP SPP */
int btstack_main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);

#ifdef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM
    // setup ERTM management
    rfcomm_enable_l2cap_ertm(&rfcomm_ertm_request_handler, &rfcomm_ertm_released_handler);
#endif

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, sdp_create_service_record_handle(), RFCOMM_SERVER_CHANNEL, "SPP Streamer");
    btstack_assert(de_get_len( spp_service_buffer) <= sizeof(spp_service_buffer));
    sdp_register_service(spp_service_buffer);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // short-cut to find other SPP Streamer
    gap_set_class_of_device(TEST_COD);

    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("SPP Streamer 00:00:00:00:00:00");
    gap_discoverable_control(1);

    spp_create_test_data();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

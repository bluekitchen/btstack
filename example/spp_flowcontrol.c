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

#define BTSTACK_FILE__ "spp_flowcontrol.c"

// *****************************************************************************
/* EXAMPLE_START(spp_flowcontrol): SPP Server - RFCOMM Flow Control
 *
 * @text This example adds explicit flow control for incoming RFCOMM data to the 
 * SPP heartbeat counter example. We will highlight the changes compared to the 
 * SPP counter example. 
 */
 // *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#define HEARTBEAT_PERIOD_MS 500
#define TEST_COD 0x1234
#define RFCOMM_SERVER_CHANNEL 1

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t rfcomm_channel_id;
static uint8_t  rfcomm_send_credit = 0;
static uint8_t  spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* @section SPP Service Setup   
 *
 * @text Listing explicitFlowControl shows how to
 * provide one initial credit during RFCOMM service initialization. Please note
 * that providing a single credit effectively reduces the credit-based (sliding
 * window) flow control to a stop-and-wait flow control that limits the data
 * throughput substantially.  
 */ 

/* LISTING_START(explicitFlowControl): Providing one initial credit during RFCOMM service initialization */
static void spp_service_setup(void){     

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
    
    // init RFCOMM
    rfcomm_init();
    // reserved channel, mtu limited by l2cap, 1 credit
    rfcomm_register_service_with_initial_credits(&packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff, 1);  

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, 1, "SPP Counter");
    sdp_register_service(spp_service_buffer);
    printf("SDP service buffer size: %u\n\r", (uint16_t) de_get_len(spp_service_buffer));
}
/* LISTING_END */

/* @section Periodic Timer Setup  
 *  
 * @text Explicit credit management is
 * recommended when received RFCOMM data cannot be processed immediately. In this
 * example, delayed processing of received data is simulated with the help of a
 * periodic timer as follows. When the packet handler receives a data packet, it
 * does not provide a new credit, it sets a flag instead, see Listing phManual. 
 * If the flag is set, a new
 * credit will be granted by the heartbeat handler, introducing a delay of up to 1
 * second. The heartbeat handler code is shown in Listing hbhManual. 
 */ 

static btstack_timer_source_t heartbeat;

/* LISTING_START(hbhManual): Heartbeat handler with manual credit management */ 
static void  heartbeat_handler(struct btstack_timer_source *ts){
    if (rfcomm_send_credit){
        rfcomm_grant_credits(rfcomm_channel_id, 1);
        rfcomm_send_credit = 0;
    }
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
} 
/* LISTING_END */

static void one_shot_timer_setup(void){
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);
}

/* LISTING_START(phManual): Packet handler with manual credit management */
// Bluetooth logic
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
/* LISTING_PAUSE */
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;
                
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;
               
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID 0x%02x, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
            }
            break;
/* LISTING_RESUME */
        case RFCOMM_DATA_PACKET:
            for (i=0;i<size;i++){
                putchar(packet[i]);
            };
            putchar('\n');
            rfcomm_send_credit = 1;
            break;
/* LISTING_PAUSE */
        default:
            break;
    }
/* LISTING_RESUME */ 
}
/* LISTING_END */



int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    (void)argc;
    (void)argv;

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    spp_service_setup();
    one_shot_timer_setup();
    
    puts("SPP FlowControl Demo: simulates processing on received data...\n\r");
    gap_set_local_name("SPP Flowcontrol 00:00:00:00:00:00");
    gap_discoverable_control(1);

    // short-cut to find other SPP Streamer
    gap_set_class_of_device(TEST_COD);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */



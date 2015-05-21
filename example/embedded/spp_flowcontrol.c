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
/* EXAMPLE_START(spp_flowcontrol): SPP Server - Flow Control
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

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "rfcomm.h"
#include "sdp.h"
#include "btstack-config.h"

#define HEARTBEAT_PERIOD_MS 500

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id;
static uint8_t   rfcomm_send_credit = 0;
static uint32_t  spp_service_buffer[150/4];  // implicit alignment to 4-byte memory address

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
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
    
    // init RFCOMM
    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    // reserved channel, mtu limited by l2cap, 1 credit
    rfcomm_register_service_with_initial_credits_internal(NULL, rfcomm_channel_nr, 0xffff, 1);  

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, 1, "SPP Counter");
    printf("SDP service buffer size: %u\n\r", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);
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

static timer_source_t heartbeat;

/* LISTING_START(hbhManual): Heartbeat handler with manual credit management */ 
static void  heartbeat_handler(struct timer *ts){
    if (rfcomm_send_credit){
        rfcomm_grant_credits(rfcomm_channel_id, 1);
        rfcomm_send_credit = 0;
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 
/* LISTING_END */

static void one_shot_timer_setup(void){
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);
}

/* LISTING_START(phManual): Packet handler with manual credit management */
// Bluetooth logic
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
/* LISTING_PAUSE */
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                    
                case BTSTACK_EVENT_STATE:
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("BTstack is up and running\n");
                    }
                    break;
                
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                        bt_flip_addr(event_addr, &packet[6]);
                        printf("BD-ADDR: %s\n\r", bd_addr_to_str(event_addr));
                        break;
                    }
                    break;

                case HCI_EVENT_LINK_KEY_REQUEST:
                    // deny link key request
                    printf("Link key request\n\r");
                    bt_flip_addr(event_addr, &packet[2]);
                    hci_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
                    break;
                    
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n\r");
                    bt_flip_addr(event_addr, &packet[2]);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;
                
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
                    bt_flip_addr(event_addr, &packet[2]); 
                    rfcomm_channel_nr = packet[8];
                    rfcomm_channel_id = READ_BT_16(packet, 9);
                    printf("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection_internal(rfcomm_channel_id);
                    break;
                    
                case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
                    // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
                    if (packet[2]) {
                        printf("RFCOMM channel open failed, status %u\n\r", packet[2]);
                    } else {
                        rfcomm_channel_id = READ_BT_16(packet, 12);
                        mtu = READ_BT_16(packet, 14);
                        printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
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
    
    spp_service_setup();
    one_shot_timer_setup();
    
    puts("SPP FlowControl Demo: simulates processing on received data...\n\r");
    gap_set_local_name("BTstack SPP Flow Control");
    hci_discoverable_control(1);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */



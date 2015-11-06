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
/* EXAMPLE_START(spp_counter): SPP Server - Heartbeat Counter over RFCOMM
 *
 * @text The Serial port profile (SPP) is widely used as it provides a serial
 * port over Bluetooth. The SPP counter example demonstrates how to setup an SPP
 * service, and provide a periodic timer over RFCOMM.   
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "rfcomm.h"

#include "sdp.h"

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t  rfcomm_channel_id;
static uint32_t  spp_service_buffer[150/4];  // implicit alignment to 4-byte memory address


/* @section SPP Service Setup 
 *s
 * @text To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers 
 * are required. After setting up an RFCOMM service with channel nubmer
 * RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server.
 * Example code for SPP service setup is
 * provided in Listing SPPSetup. The SDP record created by function
 * sdp_create_spp_service consists of a basic SPP definition that uses the provided
 * RFCOMM channel ID and service name. For more details, please have a look at it
 * in \path{src/sdp_util.c}. 
 * The SDP record is created on the fly in RAM and is deterministic.
 * To preserve valuable RAM, the result could be stored as constant data inside the ROM.   
 */

/* LISTING_START(SPPSetup): SPP service setup */ 
static void spp_service_setup(void){
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 0xffff);  // reserved channel, mtu limited by l2cap

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
/* LISTING_PAUSE */
#ifdef EMBEDDED
/* LISTING_RESUME */
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);
/* LISTING_PAUSE */
#else
    sdp_create_spp_service( (uint8_t*) spp_service_buffer, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    printf("SDP service record size: %u\n", de_get_len((uint8_t*) spp_service_buffer));
    sdp_register_service_internal(NULL, (uint8_t*)spp_service_buffer);
#endif
/* LISTING_RESUME */
}
/* LISTING_END */

/* @section Periodic Timer Setup
 * 
 * @text The heartbeat handler increases the real counter every second, 
 * and sends a text string with the counter value, as shown in Listing PeriodicCounter. 
 */

/* LISTING_START(PeriodicCounter): Periodic Counter */ 
static timer_source_t heartbeat;

static void  heartbeat_handler(struct timer *ts){
    static int counter = 0;

    if (rfcomm_channel_id){
        char lineBuffer[30];
        sprintf(lineBuffer, "BTstack counter %04u\n", ++counter);
        printf("%s", lineBuffer);
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)) {
            int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));  
            if (err) {
                log_error("rfcomm_send_internal -> error 0X%02x", err);  
            }
        }   
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 

static void one_shot_timer_setup(void){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);
}
/* LISTING_END */


/* @section Bluetooth Logic 
 * @text The Bluetooth logic is implemented within the 
 * packet handler, see Listing SppServerPacketHandler. In this example, 
 * the following events are passed sequentially: 
 * - BTSTACK_EVENT_STATE,
 * - HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or 
 * - HCI_EVENT_USER_CONFIRMATION_REQUEST (Secure Simple Pairing),
 * - RFCOMM_EVENT_INCOMING_CONNECTION,
 * - RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE, and
 * - RFCOMM_EVENT_CHANNEL_CLOSED
 */

/* @text Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle
 * authentication. Here, we use a fixed PIN code "0000".
 *
 * When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be 
 * asked to accept the pairing request. If the IO capability is set to 
 * SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.
 *
 * The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection.
 * Here, the connection is accepted. More logic is need, if you want to handle connections
 * from multiple clients. The incoming RFCOMM connection event contains the RFCOMM
 * channel number used during the SPP setup phase and the newly assigned RFCOMM
 * channel ID that is used by all BTstack commands and events.
 *
 * If RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE event returns status greater then 0,
 * then the channel establishment has failed (rare case, e.g., client crashes).
 * On successful connection, the RFCOMM channel ID and MTU for this
 * channel are made available to the heartbeat counter. After opening the RFCOMM channel, 
 * the communication between client and the application
 * takes place. In this example, the timer handler increases the real counter every
 * second. 
 */ 

/* LISTING_START(SppServerPacketHandler): SPP Server - Heartbeat Counter over RFCOMM */
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
                    // BTstack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("BTstack is up and running\n");
                    }
                    break;
/* LISTING_RESUME */ 
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // pre-ssp: inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    bt_flip_addr(event_addr, &packet[2]);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06u'\n", READ_BT_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
                    bt_flip_addr(event_addr, &packet[2]); 
                    rfcomm_channel_nr = packet[8];
                    rfcomm_channel_id = READ_BT_16(packet, 9);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection_internal(rfcomm_channel_id);
                    break;
               
                case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
                    // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
                    if (packet[2]) {
                        printf("RFCOMM channel open failed, status %u\n", packet[2]);
                    } else {
                        rfcomm_channel_id = READ_BT_16(packet, 12);
                        mtu = READ_BT_16(packet, 14);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
/* LISTING_PAUSE */                 
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
            }
            break;

        case RFCOMM_DATA_PACKET:
            printf("RCV: '");
            for (i=0;i<size;i++){
                putchar(packet[i]);
            }
            printf("'\n");
            break;

        default:
            break;
    }
/* LISTING_RESUME */ 
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    one_shot_timer_setup();
    spp_service_setup();

    hci_discoverable_control(1);
    hci_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("BTstack SPP Counter");

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
/* EXAMPLE_END */


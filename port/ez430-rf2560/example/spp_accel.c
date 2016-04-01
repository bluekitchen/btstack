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
// accel_demo
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "btstack_chipset_cc256x.h"
#include "hal_adc.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "classic/sdp_util.h"

#include "hci.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "classic/btstack_link_key_db.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#include "btstack_config.h"

#define HEARTBEAT_PERIOD_MS 1000

#define FONT_HEIGHT     12                    // Each character has 13 lines 
#define FONT_WIDTH       8
static int row = 0;
char lineBuffer[80];

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;

// SPP description
static uint8_t accel_buffer[6];

static void  prepare_accel_packet(void){
    int16_t accl_x;
    int16_t accl_y;
    int16_t accl_z;
    
    /* read the digital accelerometer x- direction and y - direction output */
    halAccelerometerRead((int *)&accl_x, (int *)&accl_y, (int *)&accl_z);

    accel_buffer[0] = 0x01; // Start of "header"
    accel_buffer[1] = accl_x;
    accel_buffer[2] = (accl_x >> 8);
    accel_buffer[3] = accl_y;
    accel_buffer[4] = (accl_y >> 8);
    int index;
    uint8_t checksum = 0;
    for (index = 0; index < 5; index++) {
        checksum += accel_buffer[index];
    }
    accel_buffer[5] = checksum;
    
    /* start the ADC to read the next accelerometer output */
    halAdcStartRead();
    
    printf("Accel: X: %04d, Y: %04d, Z: %04d\n\r", accl_x, accl_y, accl_z);
} 

static void send_packet(void){
    int err = rfcomm_send(rfcomm_channel_id, (uint8_t *)accel_buffer, sizeof(accel_buffer));
    switch(err){
        case 0:
            prepare_accel_packet();
            break;
        case BTSTACK_ACL_BUFFERS_FULL:
            break;
        default:
            printf("rfcomm_send() -> err %d\n\r", err);
            break;
    }
}

// Bluetooth logic
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int err;
    
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                    
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack is up and running\n");
                    }
                    break;
                
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_bd_addr)){
                        reverse_bd_addr(&packet[6], event_addr);
                        printf("BD-ADDR: %s\n\r", bd_addr_to_str(event_addr));
                        break;
                    }
                    break;

                case HCI_EVENT_LINK_KEY_REQUEST:
                    // deny link key request
                    printf("Link key request\n\r");
                    hci_event_link_key_request_get_bd_addr(packet, event_addr);
                    hci_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
                    break;
                    
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n\r");
                    reverse_bd_addr(&packet[2], event_addr);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;
                
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr); 
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;
               
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status %u\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
                    
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    if (rfcomm_can_send_packet_now(rfcomm_channel_id)) send_packet();
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
            }
            break;
                        
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // Accel
    halAccelerometerInit(); 
    prepare_accel_packet();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
    
    // init RFCOMM
    rfcomm_init();
    rfcomm_register_service(rfcomm_packet_handler, rfcomm_channel_nr, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record((uint8_t*) spp_service_buffer, 1, "SPP Accel");
    printf("SDP service buffer size: %u\n\r", (uint16_t) de_get_len((uint8_t*) spp_service_buffer));
    sdp_register_service((uint8_t*) spp_service_buffer);
    
    // ready - enable irq used in h4 task
    __enable_interrupt();   
    
    // set local name
    gap_set_local_name("BTstack SPP Sensor");
    // make discoverable
    gap_discoverable_control(1);
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/*

rfcomm_send gets called before we have credits
rfcomm_send returns undefined error codes???

*/


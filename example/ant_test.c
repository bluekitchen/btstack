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

#define BTSTACK_FILE__ "ant_test.c"

// *****************************************************************************
// ANT + SPP Counter demo for TI CC2567
// - it provides a SPP port and and sends a counter every second
// - it also listens on ANT channel 33,1,1
// *****************************************************************************

#include <stdio.h>

#include "btstack_chipset_cc256x.h"

#include "btstack.h"
#include "ant_cmd.h"

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id = 0;
static uint8_t   spp_service_buffer[100];
static btstack_timer_source_t heartbeat;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// ant logic
static enum {
    ANT_IDLE,
    ANT_SEND_RESET,
    ANT_W4_RESET_COMPLETE,
    ANT_SEND_ASSIGN_CHANNEL,
    ANT_W4_ASSIGN_CHANNEL_COMPLETE,
    ANT_SEND_SET_CHANNEL_ID,
    ANT_W4_SET_CHANNEL_ID_COMPLETE,
    ANT_SEND_CHANNEL_OPEN,
    ANT_W4_CHANNEL_OPEN_COMPLETE,
    ANT_ACTIVE
} ant_state = ANT_IDLE;

static void ant_run(void){
    // check if ANT/HCI command can be sent
    if (!hci_can_send_command_packet_now()) return;

    // send next command
    switch(ant_state){
        case ANT_SEND_RESET:
            ant_state = ANT_W4_RESET_COMPLETE;
            // 1. reset
            printf("Send ANT Reset\n");
            ant_send_cmd(&ant_reset); 
            break;
        case ANT_SEND_ASSIGN_CHANNEL:
            ant_state = ANT_W4_ASSIGN_CHANNEL_COMPLETE;
            // 2. assign channel
            printf("Send Assign Channel\n");
            ant_send_cmd(&ant_assign_channel, 0, 0x00, 0);
            break;
        case ANT_SEND_SET_CHANNEL_ID:
            ant_state = ANT_W4_SET_CHANNEL_ID_COMPLETE;
            // 3. set channel ID
            printf("Send Set Channel ID\n");
            ant_send_cmd(&ant_channel_id, 0, 33, 1, 1);
            break;
        case ANT_SEND_CHANNEL_OPEN:
            ant_state = ANT_W4_CHANNEL_OPEN_COMPLETE;
            // 4. open channel
            printf("Send Channel Open\n");
            ant_send_cmd(&ant_open_channel, 0);
            break;
        default:
            break;
    }
}

// Bluetooth logic
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    
    uint8_t event_code;
	// uint8_t channel;
	uint8_t message_id;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
					
				case BTSTACK_EVENT_STATE:
					if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack is up and running\n");
                        // start ANT init
                        ant_state = ANT_SEND_RESET;
					}
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
                        printf("RFCOMM channel open failed, status 0%02x\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_channel_id = 0;
                    break;
                

                case 0xff:	// vendor specific -> ANT
                	
                	// vendor specific ant message
                	if (packet[2] != 0x00) break;
                	if (packet[3] != 0x05) break;

					event_code = packet[7];

                	printf("ANT Event: ");
                	printf_hexdump(packet, size);

					switch(event_code){
						
						case MESG_STARTUP_MESG_ID:
                            ant_state = ANT_SEND_ASSIGN_CHANNEL;
							break;

						case MESG_RESPONSE_EVENT_ID:
							// channel    = packet[8];
							message_id = packet[9];
							switch (message_id){
								case MESG_ASSIGN_CHANNEL_ID:
                                    ant_state = ANT_SEND_SET_CHANNEL_ID;
									break; 
								case MESG_CHANNEL_ID_ID:
                                    ant_state = ANT_SEND_CHANNEL_OPEN;
                                    break;
                                default:
                                    break;
							}
							break;
						default:
							break;
					}
                	break;

				default:
					break;
			} 
            break;
                        
        default:
            break;
	}

    ant_run();
}

static void  heartbeat_handler(struct btstack_timer_source *ts){

    if (rfcomm_channel_id){
        static int counter = 0;
        char lineBuffer[30];
        sprintf(lineBuffer, "BTstack counter %04u\n\r", ++counter);
        puts(lineBuffer);
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)){
            uint8_t status = rfcomm_send(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));
            if (status) {
                printf("rfcomm_send -> status 0x%02x", status);
            }
        }   
    }
    
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
} 

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // init L2CAP
    l2cap_init();
    
    // init RFCOMM
    rfcomm_init();
    rfcomm_register_service(packet_handler, rfcomm_channel_nr, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    sdp_register_service(spp_service_buffer);
        
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // set local name
    gap_set_local_name("ANT Demo");
    // make discoverable
    gap_discoverable_control(1);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

	printf("Run...\n\r");
 	// turn on!
	hci_power_control(HCI_POWER_ON);
    return 0;
}


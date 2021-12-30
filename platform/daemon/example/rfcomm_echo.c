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

#define BTSTACK_FILE__ "rfcomm_echo.c"

/*
 *  rfcomm_echo.c
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "btstack_client.h"
#include "classic/sdp_util.h"

#ifdef _WIN32
#include "btstack_run_loop_windows.h"
#else
#include "btstack_run_loop_posix.h"
#endif

// input from command line arguments
bd_addr_t addr = { };
hci_con_handle_t con_handle;
uint16_t mtu;
char pin[17];
int counter = 0;
uint16_t rfcomm_channel_nr;
uint16_t rfcomm_channel_id;

uint8_t service_buffer[150];

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	
	switch (packet_type) {
			
		case RFCOMM_DATA_PACKET:
			printf("Received RFCOMM data on channel id %u, size %u\n", channel, size);
			printf_hexdump(packet, size);
            bt_send_rfcomm(channel, packet, size);
			break;
			
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
					
				case BTSTACK_EVENT_POWERON_FAILED:
					// handle HCI init failure
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;		
                    
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        // get persistent RFCOMM channel
                        printf("HCI_STATE_WORKING\n");
                        bt_send_cmd(&rfcomm_persistent_channel_for_service_cmd, "ch.ringwald.btstack.rfcomm_echo2");
                  	}
					break;
                    
                case DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL:
                    rfcomm_channel_nr = packet[3];
                    printf("RFCOMM channel %u was assigned by BTdaemon\n", rfcomm_channel_nr);
                    bt_send_cmd(&rfcomm_register_service_cmd, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap
                    break;
                    
                case DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED:
                    printf("DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED channel: %u, status: 0x%02x\n", packet[3], packet[2]);
                    // register SDP for our SPP
                    spp_create_sdp_record((uint8_t*)service_buffer, 0x10001, rfcomm_channel_nr, "SPP ECHO");
                    bt_send_cmd(&sdp_register_service_record_cmd, service_buffer);
                    bt_send_cmd(&btstack_set_discoverable, 1);
                    break;
                
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printf("Using PIN 0000\n");
					hci_event_pin_code_request_get_bd_addr(packet, event_addr); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;
					
                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr); 
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    bt_send_cmd(&rfcomm_accept_connection_cmd, rfcomm_channel_id);
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
					
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					// connection closed -> quit test app
					printf("Basebank connection closed\n");
					break;
					
				default:
					break;
			}
			break;
		default:
			break;
	}
}


int main (int argc, const char * argv[]){
	
#ifdef _WIN32
	btstack_run_loop_init(btstack_run_loop_windows_get_instance());
#else
	btstack_run_loop_init(btstack_run_loop_posix_get_instance());
#endif
	int err = bt_open();
	if (err) {
		fprintf(stderr,"Failed to open connection to BTdaemon, err %d\n",err);
		return 1;
	}
	bt_register_packet_handler(packet_handler);
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	btstack_run_loop_execute();
	bt_close();
    return 0;
}

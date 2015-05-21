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

/*
 *  rfcomm-echo.c
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

#include <btstack/btstack.h>
#include <btstack/sdp_util.h>

#define NUM_ROWS 25
#define NUM_COLS 80

// input from command line arguments
bd_addr_t addr = { };
uint16_t con_handle;
char pin[17];
int counter = 0;
uint16_t rfcomm_channel_id = 0;
uint16_t mtu = 0;
static uint8_t   spp_service_buffer[150];
uint8_t test_data[NUM_ROWS * NUM_COLS + 1];

void create_test_data(void){
    int x,y;
    for (y=0;y<NUM_ROWS;y++){
        for (x=0;x<NUM_COLS-2;x++){
            test_data[y*NUM_COLS+x] = '0' + (x % 10);
        }
        test_data[y*NUM_COLS+NUM_COLS-2] = '\n';
        test_data[y*NUM_COLS+NUM_COLS-1] = '\r';
    }
    test_data[NUM_COLS*NUM_ROWS] = 0;
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	uint16_t rfcomm_channel_nr;
	
	switch (packet_type) {
			
		case RFCOMM_DATA_PACKET:
			printf("Received RFCOMM data on channel id %u, size %u\n", channel, size);
			printf_hexdump(packet, size);
            bt_send_rfcomm(channel, packet, size);
			break;
			
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
					
				case BTSTACK_EVENT_POWERON_FAILED:
					// handle HCI init failure
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;		
                    
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        // get persistent RFCOMM channel
                        printf("HCI_STATE_WORKING\n");
                        bt_send_cmd(&rfcomm_persistent_channel_for_service, "ch.ringwald.btstack.rfcomm-test");
                  	}
					break;
                    
                case RFCOMM_EVENT_PERSISTENT_CHANNEL:
                    rfcomm_channel_nr = packet[3];
                    printf("RFCOMM channel %u was assigned by BTdaemon\n", rfcomm_channel_nr);
                    bt_send_cmd(&rfcomm_register_service, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap
                    break;
                    
                case RFCOMM_EVENT_SERVICE_REGISTERED:
                    printf("RFCOMM_EVENT_SERVICE_REGISTERED\n");
                    rfcomm_channel_nr = packet[3];
                    // register SDP for our SPP
				    sdp_create_spp_service((uint8_t*) spp_service_buffer, rfcomm_channel_nr, "RFCOMM Test");
                    bt_send_cmd(&sdp_register_service_record, spp_service_buffer);
                    bt_send_cmd(&btstack_set_discoverable, 1);
                    break;

                case RFCOMM_EVENT_CREDITS:
                    sprintf((char*)test_data, "\n\r\n\r-> %09u <- ", counter++);
                    bt_send_rfcomm(rfcomm_channel_id, test_data, mtu);
                    break;
                    
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printf("Using PIN 0000\n");
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;
					
				case RFCOMM_EVENT_INCOMING_CONNECTION:
					// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
					bt_flip_addr(event_addr, &packet[2]); 
					rfcomm_channel_nr = packet[8];
					rfcomm_channel_id = READ_BT_16(packet, 9);
					printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
					bt_send_cmd(&rfcomm_accept_connection, rfcomm_channel_id);
					break;
					
				case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
					// data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
					if (packet[2]) {
						printf("RFCOMM channel open failed, status %u\n", packet[2]);
					} else {
                        // data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
						rfcomm_channel_id = READ_BT_16(packet, 12);
						mtu = READ_BT_16(packet, 14);
                        
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
	
    create_test_data();
    printf("created test data: \n%s\n", test_data);
    
	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		fprintf(stderr,"Failed to open connection to BTdaemon, err %d\n",err);
		return 1;
	}
	bt_register_packet_handler(packet_handler);
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
    return 0;
}

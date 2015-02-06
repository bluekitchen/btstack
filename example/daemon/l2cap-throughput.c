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
 *  l2cap-troughput.c 
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>

#define PSM_TEST 0xdead
#define PACKET_SIZE 1000

int serverMode = 1;
bd_addr_t addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
uint8_t packet[PACKET_SIZE];
uint32_t counter = 0;

timer_source_t timer;

void update_packet(void){
    net_store_32( packet, 0, counter++);
}

void prepare_packet(void){
    int i;
    counter = 0;
    net_store_32( packet, 0, 0);
    for (i=4;i<PACKET_SIZE;i++)
        packet[i] = i-4;
}
void  timer_handler(struct timer *ts){
	bt_send_cmd(&hci_read_bd_addr);
	run_loop_set_timer(&timer, 3000);
	run_loop_add_timer(&timer);
};

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	bd_addr_t event_addr;
	uint16_t handle;
	uint16_t psm;
	uint16_t local_cid;
	char pin[20];
	int i;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			// measure data rate
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {

				case BTSTACK_EVENT_POWERON_FAILED:
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
	                if (packet[2] == HCI_STATE_WORKING) {
	                   if (serverMode) {
						   printf("Waiting for incoming L2CAP connection on PSM %04x...\n", PSM_TEST);
						   timer.process = timer_handler;
						   run_loop_set_timer(&timer, 3000);
						   // run_loop_add_timer(&timer);
				        } else {
				        	bt_send_cmd(&hci_write_class_of_device, 0x38010c);
				        }
					}
					break;
                
                case HCI_EVENT_COMMAND_COMPLETE:
					// use pairing yes/no
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_class_of_device) ) {
    				    bt_send_cmd(&l2cap_create_channel_mtu, addr, PSM_TEST, PACKET_SIZE);
					}
					break;

				case L2CAP_EVENT_INCOMING_CONNECTION:
					// data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
					bt_flip_addr(event_addr, &packet[2]);
					handle     = READ_BT_16(packet, 8); 
					psm        = READ_BT_16(packet, 10); 
					local_cid  = READ_BT_16(packet, 12); 
					// remote_cid = READ_BT_16(packet, 14); 
					printf("L2CAP_EVENT_INCOMING_CONNECTION %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x\n", bd_addr_to_str(event_addr), handle, psm, local_cid);
					// accept
					bt_send_cmd(&l2cap_accept_connection, local_cid);
					break;
					
				case HCI_EVENT_LINK_KEY_REQUEST:
					// link key request
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printf("Please enter PIN here: ");
					fgets(pin, 20, stdin);
					i = strlen(pin);
					if( pin[i-1] == '\n' || pin[i-1] == '\r') { 
						pin[i-1] = '\0';
						i--;
					}
					printf("PIN (%u)= '%s'\n", i, pin);
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, i, pin);
					break;
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[3]);
					psm = READ_BT_16(packet, 11); 
					local_cid = READ_BT_16(packet, 13); 
					handle = READ_BT_16(packet, 9);
					if (packet[2] == 0) {
						printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
							   bd_addr_to_str(event_addr), handle, psm, local_cid,  READ_BT_16(packet, 15));
					} else {
						printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_addr), packet[2]);
					}
					break;
				
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					printf("Basebank connection closed\n");
					break;
					
				case L2CAP_EVENT_CREDITS:
					if (!serverMode) {
						// can send! (assuming single credits are handet out)
						update_packet();
						local_cid = READ_BT_16(packet, 2);
						bt_send_l2cap( local_cid, packet, PACKET_SIZE); 
					}
				    break;
				    	
				default:
					// other event
					break;
			}
			break;
			
		default:
			// other packet type
			break;
	}
}
int main (int argc, const char * argv[]){
    // handle remote addr
    if (argc > 1){
        if (sscan_bd_addr((uint8_t *)argv[1], addr)){
            serverMode = 0;
            prepare_packet();
        }
    }

	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&l2cap_register_service, PSM_TEST, PACKET_SIZE);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	
	// banner
	printf("L2CAP Throughput Test (compatible with Apple's Bluetooth Explorer)\n");
	if (serverMode) {
	   printf(" * Running in Server mode. For client mode, specify remote addr 11:22:33:44:55:66\n");
    }
    printf(" * MTU: 1000 bytes\n");
	
	run_loop_execute();
	bt_close();
	return 0;
}

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
 *  l2cap-server.c
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>
#include <btstack/sdp_util.h>

int l2cap_reg_fail = 0;

hci_con_handle_t con_handle;

uint16_t hid_control = 0;
uint16_t hid_interrupt = 0;

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	bd_addr_t event_addr;
	uint16_t handle;
	uint16_t psm;
	uint16_t local_cid;
	uint16_t remote_cid;
	char pin[20];
	int i;
	uint8_t status;
	
	//printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			// just dump data for now
			printf("source cid %x -- ", channel);
			printf_hexdump( packet, size );
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
						bt_send_cmd(&hci_write_class_of_device, 0x2540);
					}
					break;

				case L2CAP_EVENT_SERVICE_REGISTERED:
					status = packet[2];
					psm = READ_BT_16(packet, 3); 
					printf("L2CAP_EVENT_SERVICE_REGISTERED psm: 0x%02x, status: 0x%02x\n", psm, status);
					if (status) {
						l2cap_reg_fail = 1;
					} else {
						if (psm == PSM_HID_INTERRUPT && !l2cap_reg_fail) { // The second of the two
							bt_send_cmd(&btstack_set_discoverable, 1);
							printf("Both PSMs registered.\n");
						}
					}
					break;
					
				case L2CAP_EVENT_INCOMING_CONNECTION:
					// data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
					bt_flip_addr(event_addr, &packet[2]);
					handle     = READ_BT_16(packet, 8); 
					psm        = READ_BT_16(packet, 10); 
					local_cid  = READ_BT_16(packet, 12); 
					remote_cid = READ_BT_16(packet, 14); 
					printf("L2CAP_EVENT_INCOMING_CONNECTION %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
						bd_addr_to_str(event_addr), handle, psm, local_cid, remote_cid);

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
					i = strlen(pin)-1;
					if( pin[i] == '\n') { 
						pin[i] = '\0';
					}
					printf("PIN = '%s'\n", pin);
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, strlen(pin), pin);
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
						
						if (psm == PSM_HID_CONTROL){
							hid_control = local_cid;
						}
						if (psm == PSM_HID_INTERRUPT){
							hid_interrupt = local_cid;
						}
						if (hid_control && hid_interrupt){
							bt_send_cmd(&hci_switch_role_command, &event_addr, 0);
						}
					} else {
						printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_addr), packet[2]);
						exit(1);
					}
					break;
				
				case HCI_EVENT_ROLE_CHANGE: {
					//HID Control: 0x06 bytes - SET_FEATURE_REPORT [ 53 F4 42 03 00 00 ]
					uint8_t set_feature_report[] = { 0x53, 0xf4, 0x42, 0x03, 0x00, 0x00}; 
					bt_send_l2cap(hid_control, (uint8_t*) &set_feature_report, sizeof(set_feature_report));
					break;
				}
										
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					// connection closed -> quit tes app
					printf("Basebank connection closed\n");
					
					// exit(0);
					break;
					
				case HCI_EVENT_COMMAND_COMPLETE:
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_class_of_device) ) {
						printf("Ready\n");
					}
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
	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&l2cap_register_service, PSM_HID_CONTROL, 250);
	bt_send_cmd(&l2cap_register_service, PSM_HID_INTERRUPT, 250);
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
	return 0;
}

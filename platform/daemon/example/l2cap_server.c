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

#define BTSTACK_FILE__ "l2cap_server.c"

/*
 *  l2cap_server.c
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_client.h"
#include "hci_cmd.h"
#include "classic/sdp_util.h"
#include "bluetooth_psm.h"

#ifdef _WIN32
#include "btstack_run_loop_windows.h"
#else
#include "btstack_run_loop_posix.h"
#endif

int l2cap_reg_fail = 0;

hci_con_handle_t con_handle;

uint16_t hid_control = 0;
uint16_t hid_interrupt = 0;

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	bd_addr_t event_addr;
	hci_con_handle_t con_handle;
	uint16_t psm;
	uint16_t local_cid;
	uint16_t remote_cid;
	char pin[20];
	int i;
	uint8_t status;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			// just dump data for now
			printf("source cid %x -- ", channel);
			printf_hexdump( packet, size );
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (hci_event_packet_get_type(packet)) {

				case BTSTACK_EVENT_POWERON_FAILED:
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started 
					if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
						bt_send_cmd(&hci_write_class_of_device, 0x2540);
					}
					break;

				case DAEMON_EVENT_L2CAP_SERVICE_REGISTERED:
					status = packet[2];
					psm = little_endian_read_16(packet, 3); 
					printf("DAEMON_EVENT_L2CAP_SERVICE_REGISTERED psm: 0x%02x, status: 0x%02x\n", psm, status);
					if (status) {
						l2cap_reg_fail = 1;
					} else {
						if (psm == BLUETOOTH_PSM_HID_INTERRUPT && !l2cap_reg_fail) { // The second of the two
							bt_send_cmd(&btstack_set_discoverable, 1);
							printf("Both PSMs registered.\n");
						}
					}
					break;
					
				case L2CAP_EVENT_INCOMING_CONNECTION:
					l2cap_event_incoming_connection_get_address(packet, event_addr);
					con_handle = l2cap_event_incoming_connection_get_handle(packet); 
					psm        = l2cap_event_incoming_connection_get_psm(packet); 
					local_cid  = l2cap_event_incoming_connection_get_local_cid(packet); 
					remote_cid = l2cap_event_incoming_connection_get_remote_cid(packet); 
					printf("L2CAP_EVENT_INCOMING_CONNECTION %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
						bd_addr_to_str(event_addr), con_handle, psm, local_cid, remote_cid);

					// accept
					bt_send_cmd(&l2cap_accept_connection_cmd, local_cid);
					break;
					
				case HCI_EVENT_LINK_KEY_REQUEST:
					// link key request
					hci_event_link_key_request_get_bd_addr(packet, event_addr); 
					bt_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printf("Please enter PIN here: ");
					// avoid -Wunused-result
					char* res = fgets(pin, 20, stdin);
					UNUSED(res);
					i = strlen(pin)-1;
					if( pin[i] == '\n') { 
						pin[i] = '\0';
					}
					printf("PIN = '%s'\n", pin);
					hci_event_pin_code_request_get_bd_addr(packet, event_addr); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, strlen(pin), pin);
					break;
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					l2cap_event_channel_opened_get_address(packet, event_addr);
					
					if (l2cap_event_channel_opened_get_status(packet)){
						printf("L2CAP connection to device %s failed. status code %u\n", 
							bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
						exit(1);
					}
					psm = l2cap_event_channel_opened_get_psm(packet); 
					local_cid = l2cap_event_channel_opened_get_local_cid(packet); 
					con_handle = l2cap_event_channel_opened_get_handle(packet);

					printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
						   bd_addr_to_str(event_addr), con_handle, psm, local_cid,  l2cap_event_channel_opened_get_remote_cid(packet));
					
					if (psm == BLUETOOTH_PSM_HID_CONTROL){
						hid_control = local_cid;
					}
					if (psm == BLUETOOTH_PSM_HID_INTERRUPT){
						hid_interrupt = local_cid;
					}
					if (hid_control && hid_interrupt){
						bt_send_cmd(&hci_switch_role_command, &event_addr, 0);
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
					if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_WRITE_CLASS_OF_DEVICE){
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
#ifdef _WIN32
	btstack_run_loop_init(btstack_run_loop_windows_get_instance());
#else
	btstack_run_loop_init(btstack_run_loop_posix_get_instance());
#endif
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&l2cap_register_service_cmd, BLUETOOTH_PSM_HID_CONTROL, 250);
	bt_send_cmd(&l2cap_register_service_cmd, BLUETOOTH_PSM_HID_INTERRUPT, 250);
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	btstack_run_loop_execute();
	bt_close();
	return 0;
}

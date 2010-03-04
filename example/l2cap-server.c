/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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

hci_con_handle_t con_handle;

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	bd_addr_t event_addr;
	uint16_t handle;
	uint16_t psm;
	uint16_t source_cid;
	uint16_t dest_cid;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			// just dump data for now
			printf("source cid %x -- ", channel);
			hexdump( packet, size );
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {

				case BTSTACK_EVENT_POWERON_FAILED:
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
						bt_send_cmd(&hci_write_local_name, "BTstack-Test");
					}
					break;
					
				case L2CAP_EVENT_INCOMING_CONNECTION:
					// data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
					bt_flip_addr(event_addr, &packet[2]);
					handle     = READ_BT_16(packet, 8); 
					psm        = READ_BT_16(packet, 10); 
					local_cid  = READ_BT_16(packet, 12); 
					remote_cid = READ_BT_16(packet, 14); 
					printf("L2CAP_EVENT_INCOMING_CONNECTION ");
					print_bd_addr(event_addr);
					printf(", handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
						   handle, psm, local_cid, remote_cid);

					// accept
					bt_send_cmd(&l2cap_accept_connection, source_cid);
					break;
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[3]);
					psm = READ_BT_16(packet, 11); 
					local_cid = READ_BT_16(packet, 13); 
					handle = READ_BT_16(packet, 9);
					if (packet[2] == 0) {
						printf("Channel successfully opened: ");
						print_bd_addr(event_addr);
						printf(", handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
							   handle, psm, local_cid,  READ_BT_16(packet, 15));
					} else {
						printf("L2CAP connection to device ");
						print_bd_addr(event_addr);
						printf(" failed. status code %u\n", packet[2]);
						exit(1);
					}
					break;
					
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					// connection closed -> quit tes app
					printf("Basebank connection closed, exit.\n");
					exit(0);
					break;
					
				case HCI_EVENT_COMMAND_COMPLETE:
					// use pairing yes/no
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
						bt_send_cmd(&hci_write_authentication_enable, 0);
					}
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
						bt_send_cmd(&hci_write_class_of_device, 0x2580);
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
	bt_send_cmd(&l2cap_register_service, 0x11, 250);
	bt_send_cmd(&l2cap_register_service, 0x13, 250);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
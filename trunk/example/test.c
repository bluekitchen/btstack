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
 *  test.c
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>

// bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
// bd_addr_t addr = {0x00, 0x19, 0x1d, 0x90, 0x44, 0x68 };  // WiiMote
// bd_addr_t addr = {0x76, 0x6d, 0x62, 0xdb, 0xca, 0x73 };  // iPad
bd_addr_t addr = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};  // MBA

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	switch (packet_type) {
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {

				case BTSTACK_EVENT_POWERON_FAILED:
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (packet[2] == HCI_STATE_WORKING) {
						uint8_t des_serviceSearchPattern[5] = {0x35, 0x03, 0x19, 0x10, 0x02};
						bt_send_cmd(&sdp_client_query_rfcomm_services, addr, des_serviceSearchPattern);
					}
					break;

				case SDP_QUERY_COMPLETE:
					// data: event(8), len(8), status(8)
					printf("SDP_QUERY_COMPLETE, status %u\n", packet[2]);
					break;

				case SDP_QUERY_RFCOMM_SERVICE:
					// data: event(8), len(8), rfcomm channel(8), name(var)
					printf("SDP_QUERY_RFCOMM_SERVICE, rfcomm channel %u, name '%s'\n", packet[2], (const char*)&packet[3]);
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
	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
	return 0;
}

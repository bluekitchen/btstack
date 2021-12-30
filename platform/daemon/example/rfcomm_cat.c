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

#define BTSTACK_FILE__ "rfcomm_cat.c"

/*
 *  rfcomm.c
 * 
 *  Command line parsing and debug option
 *  added by Vladimir Vyskocil <vladimir.vyskocil@gmail.com>
 *
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
int rfcomm_channel = 1;
char pin[17];

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	uint16_t mtu;
	uint16_t rfcomm_channel_id;
	
	switch (packet_type) {
			
		case RFCOMM_DATA_PACKET:
			printf("Received RFCOMM data on channel id %u, size %u\n", channel, size);
			printf_hexdump(packet, size);
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
						bt_send_cmd(&rfcomm_create_channel_cmd, addr, rfcomm_channel);
					}
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printf("Using PIN 0000\n");
					hci_event_pin_code_request_get_bd_addr(packet, event_addr);
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;
										
				case RFCOMM_EVENT_CHANNEL_OPENED:
					// data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
					if (packet[2]) {
						printf("RFCOMM channel open failed, status %u\n", packet[2]);
					} else {
						rfcomm_channel_id = little_endian_read_16(packet, 12);
						mtu = little_endian_read_16(packet, 14);
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

void usage(const char *name){
	fprintf(stderr, "Usage : %s [-a|--address aa:bb:cc:dd:ee:ff] [-c|--channel n] [-p|--pin nnnn]\n", name);
}

int main (int argc, const char * argv[]){
	
	int arg = 1;
	
	if (argc == 1){
		usage(argv[0]);
		return 1;	}
	
	while (arg < argc) {
		if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
			arg++;
			if(arg >= argc || !sscanf_bd_addr(argv[arg], addr)){
				usage(argv[0]);
				return 1;
			}
		} else if (!strcmp(argv[arg], "-c") || !strcmp(argv[arg], "--channel")) {
			arg++;
			if(arg >= argc || !sscanf(argv[arg], "%d", &rfcomm_channel)){
				usage(argv[0]);
				return 1;
			}
		} else if (!strcmp(argv[arg], "-p") || !strcmp(argv[arg], "--pin")) {
			arg++;
			if(arg >= argc) {
				usage(argv[0]);
				return 1;
			}
			strncpy(pin, argv[arg], 16);
			pin[16] = 0;
		} else {
			usage(argv[0]);
			return 1;
		}
		arg++;
	}
		
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

	printf("Trying to connect to %s, channel %d\n", bd_addr_to_str(addr), rfcomm_channel);
			
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	btstack_run_loop_execute();
	bt_close();
    return 0;
}

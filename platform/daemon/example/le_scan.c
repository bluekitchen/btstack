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

#define BTSTACK_FILE__ "le_scan.c"

/*
 *  le_scan.c
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_client.h"

#ifdef _WIN32
#include "btstack_run_loop_windows.h"
#else
#include "btstack_run_loop_posix.h"
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	if (packet_type != HCI_EVENT_PACKET) return;

	uint8_t event = hci_event_packet_get_type(packet);

	switch (event){
		case BTSTACK_EVENT_STATE:
			// bt stack activated, get started
			printf("- btstack state %u\n", packet[2]);
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
            	bt_send_cmd(&gap_le_set_scan_parameters, 1, 0x30, 0x30);
	            bt_send_cmd(&gap_le_scan_start);
			}
			break;
		case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_ADVERTISING_REPORT: 
                	printf("\n- ADV: ");
                	printf_hexdump(packet, size);
                    break;
                }
                break;
		default:
			break;
	}
}
	
int main (int argc, const char * argv[]){
	printf("le_scan started\n");
	printf("- connecting to BTstack Daemon\n");
	// start stack
#ifdef _WIN32
	btstack_run_loop_init(btstack_run_loop_windows_get_instance());
#else
	btstack_run_loop_init(btstack_run_loop_posix_get_instance());
#endif
	int err = bt_open();
	if (err) {
		printf("-> Failed to open connection to BTstack Daemon\n");
		return err;
	}
	
	printf("- connected\n");
	printf("- send power on\n");

	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	btstack_run_loop_execute();
	bt_close();
	return 0;
}

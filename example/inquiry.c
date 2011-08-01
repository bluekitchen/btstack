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
 *  inquiry.c
 * 
 *  basic inquiry scan with remote name lookup
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>

// until next BTstack Cydia update
#include "compat-svn.c"

#define MAX_DEVICES 10
struct device {
	bd_addr_t  address;
	uint16_t   clockOffset;
	uint32_t   classOfDevice;
	uint8_t	   pageScanRepetitionMode;
	uint8_t    rssi;
	uint8_t    state; // 0 empty, 1 found, 2 remote name tried, 3 remote name found
};

#define INQUIRY_INTERVAL 5
struct device devices[MAX_DEVICES];
int deviceCount = 0;
int state = 0;

int getDeviceIndexForAddress( bd_addr_t addr){
	int j;
	for (j=0; j< deviceCount; j++){
		if (BD_ADDR_CMP(addr, devices[j].address) == 0){
			return j;
		}
	}
	return -1;
}

void next(void){
	int i;
	int found = 0;
	
	switch (state) {

		case 0:
			printf("Starting inquiry scan..\n");
			bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
			state = 1;
			break;

		case 1:
			for (i=0;i<deviceCount;i++) {
				// remote name request
				if (devices[i].state == 1){
					found = 1;
					devices[i].state = 2;
					printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
					bt_send_cmd(&hci_remote_name_request, devices[i].address,
								devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
					break;
				}
			}
			if (!found) {
				printf("Queried all devices, restart.\n");
				state = 0;
				next();
			}
			break;
			
		default:
			break;
	}
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t addr;
	int i;
	int numResponses;
	
	switch (packet_type){

		case HCI_EVENT_PACKET:
			
			switch(packet[0]){
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
						bt_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
					}
					break;

				case HCI_EVENT_COMMAND_COMPLETE:
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
						next();
					}
					break;
					
				case HCI_EVENT_INQUIRY_RESULT:
					numResponses = packet[2];
					for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
						bt_flip_addr(addr, &packet[3+i*6]);
						int index = getDeviceIndexForAddress(addr);
						if (index >= 0) continue;
						memcpy(devices[deviceCount].address, addr, 6);
						devices[deviceCount].pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						devices[deviceCount].classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1+1)   + i*3);
						devices[deviceCount].clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff;
						devices[deviceCount].rssi  = 0;
						devices[deviceCount].state = 1;
						printf("Device found: %s with COD: %x, pageScan %u, clock offset %x\n", bd_addr_to_str(addr), devices[deviceCount].classOfDevice,
				                devices[deviceCount].pageScanRepetitionMode,  devices[deviceCount].clockOffset);
						deviceCount++;
					}
					break;
					
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
					numResponses = packet[2];
					for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
						bt_flip_addr(addr, &packet[3+i*6]);
						int index = getDeviceIndexForAddress(addr);
						if (index >= 0) continue;
						memcpy(devices[deviceCount].address, addr, 6);
						devices[deviceCount].pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						devices[deviceCount].classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1)     + i*3);
						devices[deviceCount].clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+3)   + i*2) & 0x7fff;
						devices[deviceCount].rssi  =                    packet [3 + numResponses*(6+1+1+3+2) + i*1];
						devices[deviceCount].state = 1;
						printf("Device found: %s with COD: 0x%06x, pageScan %u, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
				                devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
				                devices[deviceCount].clockOffset, devices[deviceCount].rssi);
						deviceCount++;
					}
					break;
					
				case BTSTACK_EVENT_REMOTE_NAME_CACHED:
					bt_flip_addr(addr, &packet[3]);
					printf("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
					break;

				case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
					bt_flip_addr(addr, &packet[3]);
					int index = getDeviceIndexForAddress(addr);
					if (index >= 0) {
						if (packet[2] == 0) {
							printf("Name: '%s'\n", &packet[9]);
							devices[index].state = 3;
						} else {
							printf("Failed to get name: page timeout\n");
						}
					}
					next();
					break;
					
				case HCI_EVENT_INQUIRY_COMPLETE:
					printf("Inquiry scan done.\n");
					for (i=0;i<deviceCount;i++) {
						// retry remote name request
						if (devices[i].state == 2)
							devices[i].state = 1;
					}
					next();
					break;
					
				default:
					break;
			}

		default:
			break;
	}
	
	
}

int main (int argc, const char * argv[]){
	// start stack
	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	// init table
	int i; for (i=0;i<MAX_DEVICES;i++) devices[i].state = 0;
	
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
	return 0;
}
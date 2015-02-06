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
 *  inquiry.c
 * 
 *  basic inquiry scan with remote name lookup
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>

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

enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE, DEVICE_NAME, REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED} ;
enum STATE state = INIT;

int getDeviceIndexForAddress( bd_addr_t addr){
	int j;
	for (j=0; j< deviceCount; j++){
		if (BD_ADDR_CMP(addr, devices[j].address) == 0){
			return j;
		}
	}
	return -1;
}

void start_scan(void){
	printf("Starting inquiry scan..\n");
	bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
}

int has_more_remote_name_requests(void){
	int i;
	for (i=0;i<deviceCount;i++) {
		if (devices[i].state == REMOTE_NAME_REQUEST) return 1;
	}
	return 0;
}

void do_next_remote_name_request(void){
	int i;
	for (i=0;i<deviceCount;i++) {
		// remote name request
		if (devices[i].state == REMOTE_NAME_REQUEST){
			devices[i].state = REMOTE_NAME_INQUIRED;
			printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
			bt_send_cmd(&hci_remote_name_request, devices[i].address,
								devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
			return;
		}
	}
}


static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
//static void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
	bd_addr_t addr;
	int i;
	int index;
	int numResponses;
	
	// printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
	if (packet_type != HCI_EVENT_PACKET) return;

	uint8_t event = packet[0];

	switch(state){

		case INIT: 
			if (packet[2] == HCI_STATE_WORKING) {
				bt_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
				state = W4_INQUIRY_MODE_COMPLETE;
			}
			break;

		case W4_INQUIRY_MODE_COMPLETE:
			switch(event){
				case HCI_EVENT_COMMAND_COMPLETE:
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
						start_scan();
						state = ACTIVE;
					}
					break;
				case HCI_EVENT_COMMAND_STATUS:
					if ( COMMAND_STATUS_EVENT(packet, hci_write_inquiry_mode) ) {
						printf("Ignoring error (0x%x) from hci_write_inquiry_mode.\n", packet[2]);
						start_scan();
						state = ACTIVE;
					}
					break;
				default:
					break;
			}

			break;
			
		case ACTIVE:
			switch(event){
				case HCI_EVENT_INQUIRY_RESULT:
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
                    numResponses = packet[2];
                    int offset = 3;
                    for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
                        bt_flip_addr(addr, &packet[offset]);
                        offset += 6;
                        index = getDeviceIndexForAddress(addr);
                        if (index >= 0) continue;   // already in our list
                        memcpy(devices[deviceCount].address, addr, 6);

                        devices[deviceCount].pageScanRepetitionMode = packet[offset];
                        offset += 1;

                        if (event == HCI_EVENT_INQUIRY_RESULT){
                            offset += 2; // Reserved + Reserved
                            devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                            offset += 3;
                            devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                            offset += 2;
                            devices[deviceCount].rssi  = 0;
                        } else {
                            offset += 1; // Reserved
                            devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                            offset += 3;
                            devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                            offset += 2;
                            devices[deviceCount].rssi  = packet[offset];
                            offset += 1;
                        }
                        devices[deviceCount].state = REMOTE_NAME_REQUEST;
                        printf("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                                devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                                devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                        deviceCount++;
                    }

                    break;
                }
					
				case HCI_EVENT_INQUIRY_COMPLETE:
					for (i=0;i<deviceCount;i++) {
						// retry remote name request
						if (devices[i].state == REMOTE_NAME_INQUIRED)
							devices[i].state = REMOTE_NAME_REQUEST;
					}
					if (has_more_remote_name_requests()){
						do_next_remote_name_request();
						break;
					} 
					start_scan();
					break;

				case BTSTACK_EVENT_REMOTE_NAME_CACHED:
					bt_flip_addr(addr, &packet[3]);
					printf("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
					break;

				case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
					bt_flip_addr(addr, &packet[3]);
					index = getDeviceIndexForAddress(addr);
					if (index >= 0) {
						if (packet[2] == 0) {
							printf("Name: '%s'\n", &packet[9]);
							devices[index].state = REMOTE_NAME_FETCHED;
						} else {
							printf("Failed to get name: page timeout\n");
						}
					}
					if (has_more_remote_name_requests()){
						do_next_remote_name_request();
						break;
					} 
					start_scan();
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

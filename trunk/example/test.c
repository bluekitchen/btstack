/*
 *  test.c
 * 
 *  Created by Matthias Ringwald on 7/14/09.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <btstack/btstack.h>

// bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
bd_addr_t addr = {0x00, 0x19, 0x1d, 0x90, 0x44, 0x68 };  // WiiMote

hci_con_handle_t con_handle;
uint16_t source_cid_interrupt;
uint16_t source_cid_control;

void data_handler(uint8_t *packet, uint16_t size){
	// just dump data for now
	hexdump( packet, size );
	
	// HOME => disconnect
	if (packet[8] == 0xA1) {							// Status report
		if (packet[9] == 0x30 || packet[9] == 0x31) {   // type 0x30 or 0x31
			if (packet[11] & 0x080) {                   // homne button pressed
				printf("Disconnect baseband\n");
				bt_send_cmd(&hci_disconnect, con_handle, 0x13); // remote closed connection
			}
		}
	}
}

void event_handler(uint8_t *packet, uint16_t size){
	// handle HCI init failure
	if (packet[0] == BTSTACK_EVENT_POWERON_FAILED){
		printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
		exit(1);
	}

    // bt stack activated, get started - set local name
    if (packet[0] == BTSTACK_EVENT_WORKING ||
	   (packet[0] == BTSTACK_EVENT_STATE && packet[2] == HCI_STATE_WORKING)) {
        bt_send_cmd(&hci_write_local_name, "BTstack-Test");
    }
    
	// use pairing yes/no
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
        bt_send_cmd(&hci_write_authentication_enable, 0);
    }
	
	// connect to HID device (PSM 0x13) at addr
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
		bt_send_cmd(&l2cap_create_channel, addr, 0x13);
		printf("Press 1+2 on WiiMote to make it discoverable - Press HOME to disconnect later :)\n");
	}

	
	// inform about pin code request
	if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
		printf("Please enter PIN 1234 on remote device\n");
	}
	
	// inform about new l2cap connection
	if (packet[0] == L2CAP_EVENT_CHANNEL_OPENED){
		bd_addr_t addr;
		bt_flip_addr(addr, &packet[2]);
		uint16_t psm = READ_BT_16(packet, 10); 
		uint16_t source_cid = READ_BT_16(packet, 12); 
		con_handle = READ_BT_16(packet, 8);
		printf("Channel successfully opened: ");
		print_bd_addr(addr);
		printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
			   con_handle, psm, source_cid,  READ_BT_16(packet, 14));

		if (psm == 0x13) {
			source_cid_interrupt = source_cid;
			// interupt channel openedn succesfully, now open control channel, too.
			bt_send_cmd(&l2cap_create_channel, addr, 0x11);
		} else {
			source_cid_control = source_cid;
			// request acceleration data..
			uint8_t setMode31[] = { 0x52, 0x12, 0x00, 0x31 };
			l2cap_send( source_cid, setMode31, sizeof(setMode31));
		}
	}
	
	// connection closed -> quit tes app
	if (packet[0] == HCI_EVENT_DISCONNECTION_COMPLETE) {
		printf("Basebank connection closed, exit.\n");
		exit(0);
	}
}

int main (int argc, const char * argv[]){
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_event_packet_handler(event_handler);
	bt_register_data_packet_handler(data_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
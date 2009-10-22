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
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>

// bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
bd_addr_t addr = {0x00, 0x19, 0x1d, 0x90, 0x44, 0x68 };  // WiiMote
// bd_addr_t addr = {0x00, 0x19, 0x1d, 0x94, 0x7a, 0xff };  // WiiMote - iPhoneBlog.de

static void (*data_cb)(uint8_t x, uint8_t y, uint8_t z);
static void (*state_cb)(char *text);

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			if (packet[0] == 0xa1 && packet[1] == 0x31){
				(*data_cb)(packet[4], packet[5], packet[6]);
			}
			break;
			
		case HCI_EVENT_PACKET:

			switch (packet[0]){
				
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - // use pairing yes/no
					if (packet[2] == HCI_STATE_WORKING) {
						bt_send_cmd(&hci_write_authentication_enable, 0);
						(*state_cb)("BT running");
					}
					break;
				
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[3]);
					uint16_t psm = READ_BT_16(packet, 11); 
					uint16_t source_cid = READ_BT_16(packet, 13); 
					printf("Channel successfully opened: ");
					print_bd_addr(event_addr);
					printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
						   READ_BT_16(packet, 9), psm, source_cid,  READ_BT_16(packet, 15));
					if (psm == 0x13) {
						// interupt channel openedn succesfully, now open control channel, too.
						bt_send_cmd(&l2cap_create_channel, event_addr, 0x11);
					} else {
						// request acceleration data.. probably has to be sent to control channel 0x11 instead of 0x13
						uint8_t setMode31[] = { 0x52, 0x12, 0x00, 0x31 };
						bt_send_l2cap( source_cid, setMode31, sizeof(setMode31));
						uint8_t setLEDs[] = { 0x52, 0x11, 0x10 };
						bt_send_l2cap( source_cid, setLEDs, sizeof(setLEDs));
						(*state_cb)("WiiMote connected");
					}

				case HCI_EVENT_COMMAND_COMPLETE:
					// connect to HID device (PSM 0x13) at addr
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
						(*state_cb)("Connecting to WiiMote");
						bt_send_cmd(&l2cap_create_channel, addr, 0x13);
					}
					break;
					
				default:
					break;
			}
			
		default:
			break;
	}
}

void set_bt_state_cb(void (*cb)(char *text)){
	state_cb = cb;
}

void set_data_cb(void (*handler)(uint8_t x, uint8_t y, uint8_t z)){
	data_cb = handler;
}

void start_bt(){
	run_loop_init(RUN_LOOP_COCOA);
	bt_open();
	bt_register_packet_handler(packet_handler);
	(*state_cb)("BT started");
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
}
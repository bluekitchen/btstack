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
#include <btstack/utils.h>

// copy and paste from BTnut

// Control field values      bit no.       1 2 3 4 5   6 7 8
#define BT_RFCOMM_SABM       0x3F       // 1 1 1 1 P/F 1 0 0
#define BT_RFCOMM_UA         0x73       // 1 1 0 0 P/F 1 1 0
// FCS calc 
#define BT_RFCOMM_CODE_WORD         0xE0 // pol = x8+x2+x1+1
#define BT_RFCOMM_CRC_CHECK_LEN     3
#define BT_RFCOMM_UIHCRC_CHECK_LEN  2

bd_addr_t addr = {0x00,0x1c,0x4d,0x02,0x1a,0x77};  // WiiMote

hci_con_handle_t con_handle;
uint16_t source_cid;

void data_handler(uint8_t *packet, uint16_t size){
	// just dump data for now
	hexdump( packet, size );

    // 	received 1. message BT_RF_COMM_UA
    if (size == 12 && packet[9] == BT_RFCOMM_UA){
        printf("Received RFCOMM unnumbered acknowledgement - mutliplexer working.. sorry, this hacks stop here\n");
        // send SABM command on dlci 0
        _bt_rfcomm_send_sabm(source_cid, 1, 1);
        // exit(0);
    }
}

void _bt_rfcomm_send_sabm(uint16_t source_cid, uint8_t initiator, uint8_t dlci)
{
	uint8_t payload[4];
	
	// This is a command
	payload[0] = (1 << 0) | (initiator << 1) | (dlci << 2);              // EA and C/R bit set
	payload[1] = BT_RFCOMM_SABM;        // control field
	payload[2] = 1;                     // set EA bit to 1 to indicate 7 bit length field
	payload[3] = crc8_calc(payload, BT_RFCOMM_CRC_CHECK_LEN);   // calc fcs, 3 bytes to be send...
    l2cap_send( source_cid, payload, sizeof(payload));
}

void event_handler(uint8_t *packet, uint16_t size){
	// handle HCI init failure
	if (packet[0] == HCI_EVENT_POWERON_FAILED){
		printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
		exit(1);
	}

    // bt stack activated, get started - set local name
    if (packet[0] == HCI_EVENT_BTSTACK_WORKING ||
	   (packet[0] == HCI_EVENT_BTSTACK_STATE && packet[2] == HCI_STATE_WORKING)) {
        bt_send_cmd(&hci_write_local_name, "BTstack-Test");
    }
    
	// use pairing yes/no
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
        bt_send_cmd(&hci_write_authentication_enable, 1);
    }
	
	// connect to RFCOMM device (PSM 0x03) at addr
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
		bt_send_cmd(&l2cap_create_channel, addr, 0x03);
		printf("Turn on the Zeemote\n");
	}

	
	// inform about pin code request
	if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
		bt_flip_addr(addr, &packet[2]); 
		bt_send_cmd(&hci_pin_code_request_reply, &addr, 4, "0000");
		printf("Please enter PIN 0000 on remote device\n");
	}
	
	// inform about new l2cap connection
	if (packet[0] == HCI_EVENT_L2CAP_CHANNEL_OPENED){
		bd_addr_t addr;
		bt_flip_addr(addr, &packet[2]);
		uint16_t psm = READ_BT_16(packet, 10); 
		source_cid = READ_BT_16(packet, 12); 
		con_handle = READ_BT_16(packet, 8);
		printf("Channel successfully opened: ");
		print_bd_addr(addr);
		printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
			   con_handle, psm, source_cid,  READ_BT_16(packet, 14));
        
        // send SABM command on dlci 0
        _bt_rfcomm_send_sabm(source_cid, 1, 0);
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
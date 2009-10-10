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

// copy and paste from BTnut

// Control field values      bit no.       1 2 3 4 5   6 7 8
#define BT_RFCOMM_SABM       0x3F       // 1 1 1 1 P/F 1 0 0
#define BT_RFCOMM_UA         0x73       // 1 1 0 0 P/F 1 1 0
#define BT_RFCOMM_DM         0x0F       // 1 1 1 1 P/F 0 0 0
#define BT_RFCOMM_DM_PF      0x1F
#define BT_RFCOMM_DISC       0x53       // 1 1 0 0 P/F 0 1 1
#define BT_RFCOMM_UIH        0xEF       // 1 1 1 1 P/F 1 1 1
#define BT_RFCOMM_UIH_PF     0xFF

// Multiplexer message types 
#define BT_RFCOMM_PN_CMD     0x83
#define BT_RFCOMM_PN_RSP     0x81
#define BT_RFCOMM_TEST_CMD   0x23
#define BT_RFCOMM_TEST_RSP   0x21
#define BT_RFCOMM_FCON_CMD   0xA3
#define BT_RFCOMM_FCON_RSP   0xA1
#define BT_RFCOMM_FCOFF_CMD  0x63
#define BT_RFCOMM_FCOFF_RSP  0x61
#define BT_RFCOMM_MSC_CMD    0xE3
#define BT_RFCOMM_MSC_RSP    0xE1
#define BT_RFCOMM_RPN_CMD    0x93
#define BT_RFCOMM_RPN_RSP    0x91
#define BT_RFCOMM_RLS_CMD    0x53
#define BT_RFCOMM_RLS_RSP    0x51
#define BT_RFCOMM_NSC_RSP    0x11

// FCS calc 
#define BT_RFCOMM_CODE_WORD         0xE0 // pol = x8+x2+x1+1
#define BT_RFCOMM_CRC_CHECK_LEN     3
#define BT_RFCOMM_UIHCRC_CHECK_LEN  2

bd_addr_t addr = {0x00,0x1c,0x4d,0x02,0x1a,0x77};  // WiiMote

hci_con_handle_t con_handle;
uint16_t source_cid;

void _bt_rfcomm_send_sabm(uint16_t source_cid, uint8_t initiator, uint8_t channel)
{
	uint8_t payload[4];
	payload[0] = (1 << 0) | (initiator << 1) |  (initiator << 1) | (channel << 3); 
	payload[1] = BT_RFCOMM_SABM;        // control field
	payload[2] = (0 << 2) | 1;          // set EA bit to 1 to indicate 7 bit length field
	payload[3] = crc8_calc(payload, 3); // calc fcs
    bt_send_l2cap( source_cid, payload, sizeof(payload));
}

void _bt_rfcomm_send_uih_msc_cmd(uint16_t source_cid, uint8_t initiator, uint8_t channel, uint8_t signals)
{
	uint8_t payload[8];
	
	// This is a command
	payload[0] = (1 << 0) | (initiator << 1); // EA and C/R bit set - always server channel 0
	payload[1] = BT_RFCOMM_UIH;        // control field
	payload[2] = 4 << 1 | 1;		   // len
	
	payload[3] = BT_RFCOMM_MSC_CMD;
	payload[4] = 2 << 1 | 1;  // len
	payload[5] = (1 << 0) | (1 << 1) | (0 << 2) | (channel << 3); // shouldn't D = initiator = 1 ?
	payload[6] = signals;

	payload[7] = crc8_calc(payload, 2);   // calc fcs, 3 bytes to be send...
    bt_send_l2cap( source_cid, payload, sizeof(payload));
}


void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;

	static uint8_t msc_resp_send = 0;
	static uint8_t msc_resp_received = 0;
	static uint8_t credits_used = 0;
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			
			// just dump data for now
			hexdump( packet, size );
			
			// 	received 1. message BT_RF_COMM_UA
			if (size == 12 && packet[9] == BT_RFCOMM_UA && packet[8] == 0x03){
				printf("Received RFCOMM unnumbered acknowledgement for channel 0 - mutliplexer working\n");
				printf("Sending UIH Parameter Negotiation Command\n");
				// _bt_rfcomm_send_sabm(source_cid, 1, 1);
				uint8_t payload[14];
				uint8_t initiator = 1;
				uint8_t channel = 1;
				payload[0] = (1 << 0) | (initiator << 1); // EA and C/R bit set - always server channel 0
				payload[1] = BT_RFCOMM_UIH;        // control field
				payload[2] = 10 << 1 | 1;		   // len
				
				payload[3] = BT_RFCOMM_PN_CMD;
				payload[4] = 8 << 1 | 1;  // len
				
				payload[5] = channel << 1; // channel << 1
				payload[6] = 0xf0; // pre defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
				payload[7] = 0; // priority
				payload[8] = 0; // max 60 seconds ack
				payload[9] =   0x96; // max framesize low
				payload[10] =  0x06; // max framesize high
				payload[11] =  0x00; // number of retransmissions
				payload[12] =  0x00; // unused error recovery window
				
				payload[13] = crc8_calc(payload, 2 );   // calc fcs, 3 bytes to be send...
				bt_send_l2cap( source_cid, payload, sizeof(payload));
				return;
			}
		
			//  received UIH Parameter Negotiation Response
			if (size == 22 && packet[9] == BT_RFCOMM_UIH && packet[11] == BT_RFCOMM_PN_RSP){
				printf("UIH Parameter Negotiation Response\n");
				printf("Sending SABM #1\n");
				_bt_rfcomm_send_sabm(source_cid, 1, 1);
			}
			
			
			// 	received 2. message BT_RF_COMM_UA
			if (size == 12 && packet[9] == BT_RFCOMM_UA && packet[8] == 0x0b ){
				printf("Received RFCOMM unnumbered acknowledgement for channel 1 - channel opened\n");
				printf("Sending MSC  'I'm ready'\n");
				_bt_rfcomm_send_uih_msc_cmd(source_cid, 1, 1, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
			}
			
			// received BT_RFCOMM_MSC_CMD
			if (size == 16 && packet[9] == BT_RFCOMM_UIH && packet[11] == BT_RFCOMM_MSC_CMD){
				printf("Received BT_RFCOMM_MSC_CMD\n");
				printf("Responding to 'I'm ready'\n");
				// fine with this
				uint8_t * payload = &packet[8];
				payload[0] |= 2; // response
				payload[3]  = BT_RFCOMM_MSC_RSP;
				payload[7] = crc8_calc(payload, 2); 
				bt_send_l2cap( source_cid, payload, 8);
				// 
				msc_resp_send = 1;
			}
			
			// received BT_RFCOMM_MSC_RSP
			if (size == 16 && packet[9] == BT_RFCOMM_UIH && packet[11] == BT_RFCOMM_MSC_RSP){
				msc_resp_received = 1;
			}

			if (packet[9] == BT_RFCOMM_UIH && packet[8] == 9){
				credits_used++;
			}
			
			if (credits_used > 40 || (msc_resp_send && msc_resp_received)) {
				msc_resp_send = msc_resp_received = credits_used = 0;
				uint8_t initiator = 1;
				uint8_t channel = 1;
				uint8_t payload[5];
				payload[0] = (1 << 0) | (initiator << 1) |  (initiator << 1) | (channel << 3); 
				payload[1] = BT_RFCOMM_UIH_PF;      // control field
				payload[2] = (0<<1) | 1;            // len
				payload[3] = 0x30;					 // credits
				payload[4] = crc8_calc(payload, 2); // calc fcs
				bt_send_l2cap( source_cid, payload, sizeof(payload));
			}
			
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {
					
				case BTSTACK_EVENT_POWERON_FAILED:
					// handle HCI init failure
					printf("HCI Init failed - make sure you have turned off Bluetooth in the System Settings\n");
					exit(1);
					break;		
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
						bt_send_cmd(&hci_write_local_name, "BTstack-Test");
					}
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					printf("Please enter PIN 0000 on remote device\n");
					break;
			
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[2]);
					uint16_t psm = READ_BT_16(packet, 10); 
					source_cid = READ_BT_16(packet, 12); 
					con_handle = READ_BT_16(packet, 8);
					printf("Channel successfully opened: ");
					print_bd_addr(event_addr);
					printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
						   con_handle, psm, source_cid,  READ_BT_16(packet, 14));
					
					// send SABM command on dlci 0
					printf("Sending SABM #0\n");
					_bt_rfcomm_send_sabm(source_cid, 1, 0);
					break;
			
				case HCI_EVENT_DISCONNECTION_COMPLETE:
					// connection closed -> quit test app
					printf("Basebank connection closed, exit.\n");
					exit(0);
					break;
				
				case HCI_EVENT_COMMAND_COMPLETE:
					// use pairing yes/no
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
						bt_send_cmd(&hci_write_authentication_enable, 1);
					}
			
					// connect to RFCOMM device (PSM 0x03) at addr
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
						bt_send_cmd(&l2cap_create_channel, addr, 0x03);
						printf("Turn on the Zeemote\n");
					}
					break;
					
				default:
					// unhandled event
					break;
			}
		default:
			// unhandled packet type
			break;
	}
}

int main (int argc, const char * argv[]){
	run_loop_init(RUN_LOOP_POSIX);
	int err = bt_open();
	if (err) {
		printf("Failed to open connection to BTdaemon\n");
		return err;
	}
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	run_loop_execute();
	bt_close();
}
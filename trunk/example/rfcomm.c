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
 *  test.c
 * 
 *  Command line parsing and debug option
 *  added by Vladimir Vyskocil <vladimir.vyskocil@gmail.com>
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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

bd_addr_t addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
int RFCOMM_CHANNEL_ID = 1;
char PIN[] = "0000";

int DEBUG = 0;

hci_con_handle_t con_handle;
uint16_t source_cid;
int fifo_fd;

// used to assemble rfcomm packets
uint8_t rfcomm_out_buffer[1000];

/**
 * @param credits - only used for RFCOMM flow control in UIH wiht P/F = 1
 */
void rfcomm_send_packet(uint16_t source_cid, uint8_t address, uint8_t control, uint8_t credits, uint8_t *data, uint16_t len){
	
	uint16_t pos = 0;
	uint8_t crc_fields = 3;
	
	rfcomm_out_buffer[pos++] = address;
	rfcomm_out_buffer[pos++] = control;
	
	// length field can be 1 or 2 octets
	if (len < 128){
		rfcomm_out_buffer[pos++] = (len << 1)| 1;     // bits 0-6
	} else {
		rfcomm_out_buffer[pos++] = (len & 0x7f) << 1; // bits 0-6
		rfcomm_out_buffer[pos++] = len >> 7;          // bits 7-14
		crc_fields++;
	}
	
	// add credits for UIH frames when PF bit is set
	if (control == BT_RFCOMM_UIH_PF){
		rfcomm_out_buffer[pos++] = credits;
	}
	
	// copy actual data
	memcpy(&rfcomm_out_buffer[pos], data, len);
	pos += len;
	
	// UIH frames only calc FCS over address + control (5.1.1)
	if ((control & 0xef) == BT_RFCOMM_UIH){
		crc_fields = 2;
	}
	rfcomm_out_buffer[pos++] =  crc8_calc(rfcomm_out_buffer, crc_fields); // calc fcs
    bt_send_l2cap( source_cid, rfcomm_out_buffer, pos);
}

void _bt_rfcomm_send_sabm(uint16_t source_cid, uint8_t initiator, uint8_t channel)
{
	uint8_t address = (1 << 0) | (initiator << 1) |  (initiator << 1) | (channel << 3); 
	rfcomm_send_packet(source_cid, address, BT_RFCOMM_SABM, 0, NULL, 0);
}

void _bt_rfcomm_send_uih_data(uint16_t source_cid, uint8_t initiator, uint8_t channel, uint8_t *data, uint16_t len) {
	uint8_t address = (1 << 0) | (initiator << 1) |  (initiator << 1) | (channel << 3); 
	rfcomm_send_packet(source_cid, address, BT_RFCOMM_UIH, 0, data, len);
}	

void _bt_rfcomm_send_uih_msc_cmd(uint16_t source_cid, uint8_t initiator, uint8_t channel, uint8_t signals)
{
	uint8_t address = (1 << 0) | (initiator << 1); // EA and C/R bit set - always server channel 0
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_CMD;
	payload[pos++] = 2 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (0 << 2) | (channel << 3); // shouldn't D = initiator = 1 ?
	payload[pos++] = signals;
	rfcomm_send_packet(source_cid, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

void _bt_rfcomm_send_uih_pn_command(uint16_t source_cid, uint8_t initiator, uint8_t channel, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (initiator << 1); // EA and C/R bit set - always server channel 0
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_CMD;
	payload[pos++] = 8 << 1 | 1;  // len
	payload[pos++] = channel << 1;
	payload[pos++] = 0xf0; // pre defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
	payload[pos++] = 0; // priority
	payload[pos++] = 0; // max 60 seconds ack
	payload[pos++] = max_frame_size & 0xff; // max framesize low
	payload[pos++] = max_frame_size >> 8;   // max framesize high
	payload[pos++] = 0x00; // number of retransmissions
	payload[pos++] = 0x00; // unused error recovery window
	rfcomm_send_packet(source_cid, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	
	static uint8_t msc_resp_send = 0;
	static uint8_t msc_resp_received = 0;
	static uint8_t credits_used = 0;
	static uint8_t credits_free = 0;
	uint8_t packet_processed = 0;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			// rfcomm: data[8] = addr
			// rfcomm: data[9] = command
			
			// 	received 1. message BT_RF_COMM_UA
			if (size == 4 && packet[1] == BT_RFCOMM_UA && packet[0] == 0x03){
				packet_processed++;
				printf("Received RFCOMM unnumbered acknowledgement for channel 0 - multiplexer working\n");
				printf("Sending UIH Parameter Negotiation Command\n");
				_bt_rfcomm_send_uih_pn_command(source_cid, 1, RFCOMM_CHANNEL_ID, 100);
			}
			
			//  received UIH Parameter Negotiation Response
			if (size == 14 && packet[1] == BT_RFCOMM_UIH && packet[3] == BT_RFCOMM_PN_RSP){
				packet_processed++;
				printf("UIH Parameter Negotiation Response\n");
				printf("Sending SABM #1\n");
				_bt_rfcomm_send_sabm(source_cid, 1, 1);
			}
			
			// 	received 2. message BT_RF_COMM_UA
			if (size == 4 && packet[1] == BT_RFCOMM_UA && packet[0] == ((RFCOMM_CHANNEL_ID << 3) | 3) ){
				packet_processed++;
				printf("Received RFCOMM unnumbered acknowledgement for channel 1 - channel opened\n");
				printf("Sending MSC  'I'm ready'\n");
				_bt_rfcomm_send_uih_msc_cmd(source_cid, 1, 1, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
			}
			
			// received BT_RFCOMM_MSC_CMD
			if (size == 8 && packet[1] == BT_RFCOMM_UIH && packet[3] == BT_RFCOMM_MSC_CMD){
				packet_processed++;
				printf("Received BT_RFCOMM_MSC_CMD\n");
				printf("Responding to 'I'm ready'\n");
				// fine with this
				uint8_t address = packet[0] | 2; // set response 
				packet[3]  = BT_RFCOMM_MSC_RSP;  //  "      "
				rfcomm_send_packet(source_cid, address, BT_RFCOMM_UIH, 0x30, (uint8_t*)&packet[3], 4);
				msc_resp_send = 1;
			}
			
			// received BT_RFCOMM_MSC_RSP
			if (size == 8 && packet[1] == BT_RFCOMM_UIH && packet[3] == BT_RFCOMM_MSC_RSP){
				packet_processed++;
				msc_resp_received = 1;
			}
			
			if (packet[1] == BT_RFCOMM_UIH && packet[0] == ((RFCOMM_CHANNEL_ID<<3)|1)){
				packet_processed++;
				credits_used++;
				if(DEBUG){
					printf("RX: address %02x, control %02x: ", packet[0], packet[1]);
					hexdump( (uint8_t*) &packet[3], size-4);
				}
				int written = 0;
				int length = size-4;
				int start_of_data = 3;
				//write data to fifo
				while (length) {
					if ((written = write(fifo_fd, &packet[start_of_data], length)) == -1) {
						printf("Error writing to FIFO\n");
					} else {
						length -= written;
					}
				}
			}
			
			if (packet[1] == BT_RFCOMM_UIH_PF && packet[0] == ((RFCOMM_CHANNEL_ID<<3)|1)){
				packet_processed++;
				credits_used++;
				if (!credits_free) {
					printf("Got %u credits, can send!\n", packet[2]);
				}
				credits_free = packet[2];
				if(DEBUG){
					printf("RX: address %02x, control %02x: ", packet[0], packet[1]);
					hexdump( (uint8_t *) &packet[4], size-5);				
				}
			}
			
			uint8_t send_credits_packet = 0;
			
			
			if (credits_used > 40 ) {
				send_credits_packet = 1;
				credits_used = 0;
			}
			
			if (msc_resp_send && msc_resp_received) {
				send_credits_packet = 1;
				msc_resp_send = msc_resp_received = 0;
				
				printf("RFCOMM up and running!\n");
			}
			
			if (send_credits_packet) {
				// send 0x30 credits
				uint8_t initiator = 1;
				uint8_t address = (1 << 0) | (initiator << 1) |  (initiator << 1) | (RFCOMM_CHANNEL_ID << 3); 
				rfcomm_send_packet(source_cid, address, BT_RFCOMM_UIH_PF, 0x30, NULL, 0);
			}
			
			if (!packet_processed){
				// just dump data for now
				printf("??: address %02x, control %02x: ", packet[0], packet[1]);
				hexdump( packet, size );
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
						bt_send_cmd(&hci_write_local_name, "BTstack");
					}
					break;
					
				case HCI_EVENT_LINK_KEY_REQUEST:
					// link key request
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					bt_flip_addr(event_addr, &packet[2]); 
					bt_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, PIN);
					printf("Please enter PIN %s on remote device\n", PIN);
					break;
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[3]);
					uint16_t psm = READ_BT_16(packet, 11); 
					source_cid = READ_BT_16(packet, 13); 
					con_handle = READ_BT_16(packet, 9);
					if (packet[2] == 0) {
						printf("Channel successfully opened: ");
						print_bd_addr(event_addr);
						printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
							   con_handle, psm, source_cid,  READ_BT_16(packet, 15));
						
						// send SABM command on dlci 0
						printf("Sending SABM #0\n");
						_bt_rfcomm_send_sabm(source_cid, 1, 0);
					} else {
						printf("L2CAP connection to device ");
						print_bd_addr(event_addr);
						printf(" failed. status code %u\n", packet[2]);
						exit(1);
					}
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
					}
					break;
					
				default:
					// unhandled event
					if(DEBUG) printf("unhandled event : %02x\n", packet[0]);
					break;
			}
			break;
		default:
			// unhandled packet type
			if(DEBUG) printf("unhandled packet type : %02x\n", packet_type);
			break;
	}
}

void usage(const char *name){
	fprintf(stderr, "Usage : %s [-a|--address aa:bb:cc:dd:ee:ff] [-c|--channel n] [-p|--pin nnnn]\n", name);
}

#define FIFO_NAME "/tmp/rfcomm0"
int main (int argc, const char * argv[]){
	
	int arg = 1;
	
	if (argc == 1){
		usage(argv[0]);
		return 1;	}
	
	while (arg < argc) {
		if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
			arg++;
			if(arg >= argc || !sscan_bd_addr((uint8_t *)argv[arg], addr)){
				usage(argv[0]);
				return 1;
			}
		} else if (!strcmp(argv[arg], "-c") || !strcmp(argv[arg], "--channel")) {
			arg++;
			if(arg >= argc || !sscanf(argv[arg], "%d", &RFCOMM_CHANNEL_ID)){
				usage(argv[0]);
				return 1;
			}
		} else if (!strcmp(argv[arg], "-p") || !strcmp(argv[arg], "--pin")) {
			arg++;
			int pin1,pin2,pin3,pin4;
			if(arg >= argc || sscanf(argv[arg], "%1d%1d%1d%1d", &pin1, &pin2, &pin3, &pin4) != 4){
				usage(argv[0]);
				return 1;
			}
			snprintf(PIN, 5, "%01d%01d%01d%01d", pin1, pin2, pin3, pin4);
		} else {
			usage(argv[0]);
			return 1;
		}
		arg++;
	}
	printf("Waiting for client to open %s...\n", FIFO_NAME);
	int err = mknod(FIFO_NAME, S_IFIFO | 0666, 0);
	if(err >= 0 || errno == EEXIST){
		fifo_fd = open(FIFO_NAME, O_WRONLY);
		run_loop_init(RUN_LOOP_POSIX);
		err = bt_open();
		if (err) {
			fprintf(stderr,"Failed to open connection to BTdaemon, err %d\n",err);
			return 1;
		}
		printf("Trying connection to ");
		print_bd_addr(addr);
		printf(" channel %d\n", RFCOMM_CHANNEL_ID);
		bt_register_packet_handler(packet_handler);
		bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
		run_loop_execute();
		bt_close();
	} else {
		fprintf(stderr, "Failed mknod %s, errno %d\n", FIFO_NAME, errno);
		return 1;
	}

    return 0;
}

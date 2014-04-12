#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include "att.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "rijndael.h"


static btstack_packet_handler_t le_data_handler;
static void (*event_packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static const uint16_t max_mtu = 23;
static uint8_t  l2cap_stack_buffer[max_mtu];

static uint8_t aes128_cyphertext[16];

void aes128_calc_cyphertext(uint8_t key[16], uint8_t plaintext[16], uint8_t cyphertext[16]){
	uint32_t rk[RKLENGTH(KEYBITS)];
	int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
	rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

void mock_simulate_hci_event(uint8_t * packet, uint16_t size){
	hci_dump_packet(HCI_EVENT_PACKET, 1, packet, size);
	event_packet_handler(NULL, HCI_EVENT_PACKET, NULL, packet, size);
}

void aes128_report_result(){
	uint8_t le_enc_result[22];
	uint8_t enc1_data[] = { 0x0e, 0x14, 0x01, 0x17, 0x20, 0x00 };
	memcpy (le_enc_result, enc1_data, 6);
	swap128(aes128_cyphertext, &le_enc_result[6]);
	mock_simulate_hci_event(&le_enc_result[0], sizeof(le_enc_result));
}

void mock_simulate_sm_data_packet(uint8_t * packet, uint16_t len){

	uint16_t handle = 0x40;
	uint16_t cid = 0x06;

	uint8_t acl_buffer[len + 8];

	// 0 - Connection handle : PB=10 : BC=00 
    bt_store_16(acl_buffer, 0, handle | (2 << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, cid);  

	memcpy(&acl_buffer[8], packet, len);
	hci_dump_packet(HCI_ACL_DATA_PACKET, 1, &acl_buffer[0], len + 8);

	le_data_handler(SM_DATA_PACKET, handle, packet, len);
}

void mock_simulate_command_complete(const hci_cmd_t *cmd){
	uint8_t packet[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, cmd->opcode & 0xff, cmd->opcode >> 8, 0};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_hci_state_working(){
	uint8_t packet[] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_connected(){
    uint8_t packet[] = { 0x3e, 0x13, 0x01, 0x00, 0x40, 0x00, 0x01, 0x01, 0x18, 0x12, 0x5e, 0x68, 0xc9, 0x73, 0x18, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
}

void att_init_connection(att_connection_t * att_connection){
    att_connection->mtu = 23;
    att_connection->encryption_key_size = 0;
    att_connection->authenticated = 0;
	att_connection->authorized = 0;
}

int hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type){
	return 1;
}

// get addr type and address used in advertisement packets
void hci_le_advertisement_address(uint8_t * addr_type, bd_addr_t * addr){
    *addr_type = 0;
    uint8_t dummy[] = { 0x00, 0x1b, 0xdc, 0x07, 0x32, 0xef };
    memcpy(addr, dummy, 6);
}

int  l2cap_can_send_connectionless_packet_now(void){
	return 1;	
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	uint8_t cmd_buffer[256];
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(cmd_buffer, cmd, argptr);
    va_end(argptr);
	hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, cmd_buffer, len);

	// track le encrypt and le rand
	if (cmd->opcode ==  hci_le_encrypt.opcode){
	    uint8_t * key_flipped = &cmd_buffer[3];
	    uint8_t * plaintext_flipped = &cmd_buffer[19];
	    uint8_t key[16];
	    uint8_t plaintext[16];
		swap128(key_flipped, key);
 		swap128(plaintext_flipped, plaintext);
	    printf("le_encrypt key ");
	    hexdump(key, 16);
	    printf("le_encrypt txt ");
	    hexdump(plaintext, 16);
	    aes128_calc_cyphertext(key, plaintext, aes128_cyphertext);
	    printf("le_encrypt res ");
	    hexdump(aes128_cyphertext, 16);
	}
	return 0;
}


uint8_t *l2cap_get_outgoing_buffer(void){
	printf("l2cap_get_outgoing_buffer\n");
	return (uint8_t *)&l2cap_stack_buffer; // 8 bytes
}

uint16_t l2cap_max_mtu(void){
	printf("l2cap_max_mtu\n");
    return max_mtu;
}


void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
	le_data_handler = packet_handler;
}

void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
	event_packet_handler = handler;
}

int l2cap_reserve_packet_buffer(void){
	printf("l2cap_reserve_packet_buffer\n");
	return 1;
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
	printf("l2cap_send_prepared_connectionless\n");
	return 0;
}

int l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t * buffer, uint16_t len){
	printf("l2cap_send_connectionless\n");

	uint8_t acl_buffer[len + 8];

	// 0 - Connection handle : PB=10 : BC=00 
    bt_store_16(acl_buffer, 0, handle | (2 << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, cid);  

	memcpy(&acl_buffer[8], buffer, len);
	hci_dump_packet(HCI_ACL_DATA_PACKET, 0, &acl_buffer[0], len + 8);

	return 0;
}

void hci_disconnect_security_block(hci_con_handle_t con_handle){
	printf("hci_disconnect_security_block \n");	
}

void l2cap_run(void){
}

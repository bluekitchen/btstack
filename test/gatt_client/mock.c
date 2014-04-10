#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "ble_client.h"

static btstack_packet_handler_t att_packet_handler;
static void (*gatt_central_packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static uint8_t  l2cap_stack_buffer[20];
static uint16_t max_l2cap_data_packet_length = 20;

void mock_command_complete(const hci_cmd_t *cmd){
	uint8_t packet[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, cmd->opcode & 0xff, cmd->opcode >> 8, 0};
		gatt_central_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_scan_response(){
	uint8_t packet[] = { 0x3E, 0x0F, 0x02, 0x01, 0x00, 0x01, 0x71, 0xA1, 0xE6, 0x80, 0x48, 0x61, 0x03, 0x02, 0x01, 0x1A, 0xB7};
	gatt_central_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, sizeof(packet));
}

int hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type){
	return 1;
}

int  l2cap_can_send_connectionless_packet_now(void){
	return 1;	
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	printf("hci_send_cmd \n");	
	
	if (cmd == &hci_le_set_scan_enable){
		mock_command_complete(cmd);
		mock_simulate_scan_response();
	}
	return 0;
}


uint8_t *l2cap_get_outgoing_buffer(void){
	printf("l2cap_get_outgoing_buffer \n");
	return (uint8_t *)&l2cap_stack_buffer; // 8 bytes
}


uint16_t l2cap_max_mtu(void){
    printf("l2cap_max_mtu \n");
    return max_l2cap_data_packet_length;
}


void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
	att_packet_handler = packet_handler;
}

void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
	gatt_central_packet_handler = handler;
	uint8_t packet[3] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	gatt_central_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, 3);
}

int hci_reserve_packet_buffer(void){
	printf("hci_reserve_packet_buffer \n");
	return 1;
}

int l2cap_reserve_packet_buffer(void){
	printf("l2cap_reserve_packet_buffer \n");
    return hci_reserve_packet_buffer();
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
	printf("l2cap_send_prepared_connectionless \n");
	return 0;
}


void hci_disconnect_security_block(hci_con_handle_t con_handle){
	printf("hci_disconnect_security_block \n");	
}

void hci_dump_log(const char * format, ...){
	printf("hci_disconnect_security_block \n");	
}

void l2cap_run(void){
}

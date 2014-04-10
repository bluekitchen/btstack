#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"

int hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type){
	printf("hci_can_send_packet_now_using_packet_buffer \n");
	return 1;
}

void hci_disconnect_security_block(hci_con_handle_t con_handle){
	printf("hci_disconnect_security_block \n");	
}

void hci_dump_log(const char * format, ...){
	printf("hci_disconnect_security_block \n");	
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	printf("hci_send_cmd \n");	
	return 0;
}


int  l2cap_can_send_connectionless_packet_now(void){
	printf("l2cap_can_send_connectionless_packet_now \n");	
	return 1;	
}

static uint8_t  l2cap_stack_buffer[20];
static uint16_t max_l2cap_data_packet_length = 20;


uint8_t *l2cap_get_outgoing_buffer(void){
	printf("l2cap_get_outgoing_buffer \n");
	return (uint8_t *)&l2cap_stack_buffer; // 8 bytes
}



uint16_t l2cap_max_mtu(void){
    printf("l2cap_max_mtu \n");
    return max_l2cap_data_packet_length;
}


void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
	printf("l2cap_register_fixed_channel \n");
}

void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
	printf("l2cap_register_packet_handler \n");
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


void l2cap_run(void){
}

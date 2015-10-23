#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include "att.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "gatt_client.h"
#include "sm.h"

static btstack_packet_handler_t att_packet_handler;
static void (*registered_l2cap_packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static linked_list_t     connections;
static const uint16_t max_mtu = 23;
static uint8_t  l2cap_stack_buffer[max_mtu];
uint16_t gatt_client_handle = 0x40;

uint16_t get_gatt_client_handle(void){
	return gatt_client_handle;
}

void mock_simulate_command_complete(const hci_cmd_t *cmd){
	uint8_t packet[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, cmd->opcode & 0xff, cmd->opcode >> 8, 0};
	registered_l2cap_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_hci_state_working(void){
	uint8_t packet[3] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	registered_l2cap_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, 3);
}

void mock_simulate_connected(void){
	uint8_t packet[] = {0x3E, 0x13, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00, 0x9B, 0x77, 0xD1, 0xF7, 0xB1, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD0, 0x07, 0x05};
	registered_l2cap_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_scan_response(void){
	uint8_t packet[] = {0xE2, 0x13, 0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B, 0xCC, 0x09, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	registered_l2cap_packet_handler(NULL, HCI_EVENT_PACKET, NULL, (uint8_t *)&packet, sizeof(packet));
}

le_command_status_t le_central_start_scan(void){
	return BLE_PERIPHERAL_OK; 
}
le_command_status_t le_central_stop_scan(void){
	return BLE_PERIPHERAL_OK;
}
le_command_status_t le_central_connect(bd_addr_t addr, bd_addr_type_t addr_type){
	return BLE_PERIPHERAL_OK;
}
void le_central_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window){
}

static void att_init_connection(att_connection_t * att_connection){
    att_connection->mtu = 23;
    att_connection->max_mtu = 23;
    att_connection->encryption_key_size = 0;
    att_connection->authenticated = 0;
	att_connection->authorized = 0;
}

int  l2cap_can_send_connectionless_packet_now(void){
	return 1;	
}


uint8_t *l2cap_get_outgoing_buffer(void){
	// printf("l2cap_get_outgoing_buffer\n");
	return (uint8_t *)&l2cap_stack_buffer; // 8 bytes
}

uint16_t l2cap_max_mtu(void){
	// printf("l2cap_max_mtu\n");
    return max_mtu;
}

uint16_t l2cap_max_le_mtu(void){
	// printf("l2cap_max_mtu\n");
    return max_mtu;
}

void l2cap_init(){}

void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    att_packet_handler = packet_handler;
}

void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
	registered_l2cap_packet_handler = handler;
}


int l2cap_reserve_packet_buffer(void){
	return 1;
}

int l2cap_can_send_fixed_channel_packet_now(uint16_t handle){
	return 1;
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
	att_connection_t att_connection;
	att_init_connection(&att_connection);
	uint8_t response[max_mtu];
	uint16_t response_len = att_handle_request(&att_connection, l2cap_get_outgoing_buffer(), len, &response[0]);
	if (response_len){
		att_packet_handler(ATT_DATA_PACKET, gatt_client_handle, &response[0], response_len);
	}
	return 0;
}

int  sm_cmac_ready(void){
	return 1;
}
void sm_cmac_start(sm_key_t k, uint8_t opcode, uint16_t attribute_handle, uint16_t message_len, uint8_t * message, uint32_t sign_counter, void (*done_handler)(uint8_t hash[8])){
	//sm_notify_client(SM_IDENTITY_RESOLVING_SUCCEEDED, sm_central_device_addr_type, sm_central_device_address, 0, sm_central_device_matched);      
}
int sm_le_device_index(uint16_t handle ){
	return -1;
}

void run_loop_set_timer(timer_source_t *a, uint32_t timeout_in_ms){
}

// Set callback that will be executed when timer expires.
void run_loop_set_timer_handler(timer_source_t *ts, void (*process)(timer_source_t *_ts)){
}

// Add/Remove timer source.
void run_loop_add_timer(timer_source_t *timer){
}

int  run_loop_remove_timer(timer_source_t *timer){
	return 1;
}

// todo:
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t addr, bd_addr_type_t addr_type){
	printf("hci_connection_for_bd_addr_and_type not implemented in mock backend\n");
	return NULL;
}
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
	printf("hci_connection_for_handle not implemented in mock backend\n");
	return NULL;
}
void hci_connections_get_iterator(linked_list_iterator_t *it){
	// printf("hci_connections_get_iterator not implemented in mock backend\n");
    linked_list_iterator_init(it, &connections);
}

// int hci_send_cmd(const hci_cmd_t *cmd, ...){
// //	printf("hci_send_cmd opcode 0x%02x\n", cmd->opcode);	
// 	return 0;
// }

// int hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type){
// 	return 1;
// }

// void hci_disconnect_security_block(hci_con_handle_t con_handle){
// 	printf("hci_disconnect_security_block \n");	
// }

// void hci_dump_log(const char * format, ...){
// 	printf("hci_disconnect_security_block \n");	
// }

void l2cap_run(void){
}

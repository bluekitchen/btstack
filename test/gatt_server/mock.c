#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"

#include "ble/att_db.h"
#include "ble/gatt_client.h"
#include "ble/sm.h"

#include "btstack_debug.h"

static btstack_packet_handler_t att_packet_handler;
static void (*registered_hci_event_handler) (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static btstack_linked_list_t     connections;
static const uint16_t max_mtu = 23;
static uint8_t  l2cap_stack_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 8 + max_mtu];	// pre buffer + HCI Header + L2CAP header
static uint16_t gatt_client_handle = 0x40;
static hci_connection_t hci_connection;

uint16_t get_gatt_client_handle(void){
	return gatt_client_handle;
}

void mock_simulate_command_complete(const hci_cmd_t *cmd){
	uint8_t packet[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, (uint8_t) (cmd->opcode & 0xff), (uint8_t) (cmd->opcode >> 8), 0};
	registered_hci_event_handler(HCI_EVENT_PACKET, 0, (uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_hci_state_working(void){
	uint8_t packet[3] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	registered_hci_event_handler(HCI_EVENT_PACKET, 0, (uint8_t *)&packet, 3);
}

void mock_simulate_connected(void){
	uint8_t packet[] = {0x3E, 0x13, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00, 0x9B, 0x77, 0xD1, 0xF7, 0xB1, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD0, 0x07, 0x05};
	registered_hci_event_handler(HCI_EVENT_PACKET, 0, (uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_scan_response(void){
	uint8_t packet[] = {0xE2, 0x13, 0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B, 0xCC, 0x09, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	registered_hci_event_handler(HCI_EVENT_PACKET, 0, (uint8_t *)&packet, sizeof(packet));
}

void gap_start_scan(void){
}
void gap_stop_scan(void){
}
uint8_t gap_connect(bd_addr_t addr, bd_addr_type_t addr_type){
	return 0;
}
void gap_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window){
}

int gap_reconnect_security_setup_active(hci_con_handle_t con_handle){
	UNUSED(con_handle);
	return 0;
}

static void att_init_connection(att_connection_t * att_connection){
    att_connection->mtu = 23;
    att_connection->max_mtu = 23;
    att_connection->encryption_key_size = 0;
    att_connection->authenticated = 0;
	att_connection->authorized = 0;
}

int hci_can_send_acl_le_packet_now(void){
	return 1;
}

int hci_can_send_command_packet_now(void){
	return 1;
}

HCI_STATE hci_get_state(void){
	return HCI_STATE_WORKING;
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	btstack_assert(false);
	return 0;
}

void hci_halting_defer(void){
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

void l2cap_init(void){}

void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    att_packet_handler = packet_handler;
}

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
	registered_hci_event_handler = callback_handler->callback;
}

int l2cap_reserve_packet_buffer(void){
	return 1;
}

void l2cap_release_packet_buffer(void){
}

static uint8_t l2cap_can_send_fixed_channel_packet_now_status = 1;

void l2cap_can_send_fixed_channel_packet_now_set_status(uint8_t status){
	l2cap_can_send_fixed_channel_packet_now_status = status;
}

int l2cap_can_send_fixed_channel_packet_now(uint16_t handle, uint16_t channel_id){
	return l2cap_can_send_fixed_channel_packet_now_status;
}

void l2cap_request_can_send_fix_channel_now_event(uint16_t handle, uint16_t channel_id){
	uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 1, 0};
	att_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*)event, sizeof(event));
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

void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
}

int  sm_cmac_ready(void){
	return 1;
}
void sm_cmac_signed_write_start(const sm_key_t key, uint8_t opcode, uint16_t attribute_handle, uint16_t message_len, const uint8_t * message, uint32_t sign_counter, void (*done_callback)(uint8_t * hash)){
	//sm_notify_client(SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED, sm_central_device_addr_type, sm_central_device_address, 0, sm_central_device_matched);      
}
int sm_le_device_index(uint16_t handle ){
	return -1;
}

irk_lookup_state_t sm_identity_resolving_state(hci_con_handle_t con_handle){
	return IRK_LOOKUP_SUCCEEDED;
}

void sm_request_pairing(hci_con_handle_t con_handle){
}

void btstack_run_loop_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
}

// Set callback that will be executed when timer expires.
void btstack_run_loop_set_timer_handler(btstack_timer_source_t *ts, void (*process)(btstack_timer_source_t *_ts)){
}

// Add/Remove timer source.
void btstack_run_loop_add_timer(btstack_timer_source_t *timer){
}

int  btstack_run_loop_remove_timer(btstack_timer_source_t *timer){
	return 1;
}

void * btstack_run_loop_get_timer_context(btstack_timer_source_t *ts){
    return ts->context;
}

// todo:
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t addr, bd_addr_type_t addr_type){
	printf("hci_connection_for_bd_addr_and_type not implemented in mock backend\n");
	return NULL;
}
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
	if (con_handle != 0) return NULL;
	return &hci_connection;
}
void hci_connections_get_iterator(btstack_linked_list_iterator_t *it){
	// printf("hci_connections_get_iterator not implemented in mock backend\n");
    btstack_linked_list_iterator_init(it, &connections);
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

void l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
}

int gap_authenticated(hci_con_handle_t con_handle){
	return 0;
}
authorization_state_t gap_authorization_state(hci_con_handle_t con_handle){
	return AUTHORIZATION_UNKNOWN;
}
int gap_encryption_key_size(hci_con_handle_t con_handle){
	return 0;
}
int gap_secure_connection(hci_con_handle_t con_handle){
	return 0;
}
gap_connection_type_t gap_get_connection_type(hci_con_handle_t connection_handle){
	return GAP_CONNECTION_INVALID;
}
int gap_request_connection_parameter_update(hci_con_handle_t con_handle, uint16_t conn_interval_min,
	uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout){
	return 0;	
}

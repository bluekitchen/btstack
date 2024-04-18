#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci.h"
#include "gap.h"
#include "hci_dump.h"
#include "l2cap.h"

#include "ble/att_db.h"
#include "ble/att_dispatch.h"
#include "ble/sm.h"

#include "btstack_debug.h"

static btstack_packet_handler_t att_server_packet_handler;
static void (*registered_hci_event_handler) (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static btstack_linked_list_t     connections;
static uint16_t max_mtu = 23;
static uint8_t  l2cap_stack_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 8 + ATT_DEFAULT_MTU];	// pre buffer + HCI Header + L2CAP header
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

static void hci_create_gap_connection_complete_event(const uint8_t * hci_event, uint8_t * gap_event) {
    gap_event[0] = HCI_EVENT_META_GAP;
    gap_event[1] = 36 - 2;
    gap_event[2] = GAP_SUBEVENT_LE_CONNECTION_COMPLETE;
    switch (hci_event[2]){
        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            memcpy(&gap_event[3], &hci_event[3], 11);
            memset(&gap_event[14], 0, 12);
            memcpy(&gap_event[26], &hci_event[14], 7);
            memset(&gap_event[33], 0xff, 3);
            break;
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V1:
            memcpy(&gap_event[3], &hci_event[3], 30);
            memset(&gap_event[33], 0xff, 3);
            break;
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V2:
            memcpy(&gap_event[3], &hci_event[3], 33);
            break;
        default:
            btstack_unreachable();
            break;
    }
}

void mock_simulate_connected(void){
	uint8_t packet[] = {0x3E, 0x13, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00, 0x9B, 0x77, 0xD1, 0xF7, 0xB1, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD0, 0x07, 0x05};
    uint8_t gap_event[36];
    hci_create_gap_connection_complete_event(packet, gap_event);
    registered_hci_event_handler(HCI_EVENT_PACKET, 0, gap_event, sizeof(gap_event));
}

void mock_simulate_scan_response(void){
	uint8_t packet[] = {0xE2, 0x13, 0xE2, 0x01, 0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B, 0xCC, 0x09, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	registered_hci_event_handler(HCI_EVENT_PACKET, 0, (uint8_t *)&packet, sizeof(packet));
}

void gap_start_scan(void){
}
void gap_stop_scan(void){
}
uint8_t gap_connect(const bd_addr_t addr, bd_addr_type_t addr_type){
	return 0;
}
void gap_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window){
}

bool gap_reconnect_security_setup_active(hci_con_handle_t con_handle){
    UNUSED(con_handle);
    return false;
}

static void hci_setup_connection(uint16_t con_handle, bd_addr_type_t type){
    hci_connection.att_connection.mtu = 23;
    hci_connection.att_connection.con_handle = con_handle;
    hci_connection.att_connection.max_mtu = 23;
    hci_connection.att_connection.encryption_key_size = 0;
    hci_connection.att_connection.authenticated = 0;
    hci_connection.att_connection.authorized = 0;

    hci_connection.att_server.ir_le_device_db_index = 0;

    hci_connection.con_handle = con_handle;
    hci_connection.address_type = type;

    if (btstack_linked_list_empty(&connections)){
        btstack_linked_list_add(&connections, (btstack_linked_item_t *)&hci_connection);
    }
}

void hci_setup_le_connection(uint16_t con_handle){
    hci_setup_connection(con_handle, BD_ADDR_TYPE_LE_PUBLIC);
}

void hci_setup_classic_connection(uint16_t con_handle){
    hci_setup_connection(con_handle, BD_ADDR_TYPE_ACL);
}


void mock_l2cap_set_max_mtu(uint16_t mtu){
    max_mtu = mtu;
}

void hci_deinit(void){
    hci_connection.att_connection.mtu = 0;
    hci_connection.att_connection.con_handle = HCI_CON_HANDLE_INVALID;
    hci_connection.att_connection.max_mtu = 0;
    hci_connection.att_connection.encryption_key_size = 0;
    hci_connection.att_connection.authenticated = 0;
    hci_connection.att_connection.authorized = 0;
    hci_connection.att_server.ir_le_device_db_index = 0;
    hci_connection.att_server.notification_requests = NULL;
    hci_connection.att_server.indication_requests = NULL;
    connections = NULL;
}

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    registered_hci_event_handler = callback_handler->callback;
}

bool hci_can_send_command_packet_now(void){
	return true;
}

HCI_STATE hci_get_state(void){
	return HCI_STATE_WORKING;
}

uint8_t hci_send_cmd(const hci_cmd_t *cmd, ...){
	btstack_assert(false);
	return ERROR_CODE_SUCCESS;
}

void hci_halting_defer(void){
}

bool l2cap_can_send_connectionless_packet_now(void){
	return 1;	
}

void l2cap_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    UNUSED(callback_handler);
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
	return max_mtu;
}

void l2cap_init(void){}

void l2cap_reserve_packet_buffer(void){}

void l2cap_release_packet_buffer(void){}

static uint8_t l2cap_can_send_fixed_channel_packet_now_status = 1;

void l2cap_can_send_fixed_channel_packet_now_set_status(uint8_t status){
	l2cap_can_send_fixed_channel_packet_now_status = status;
}

bool l2cap_can_send_fixed_channel_packet_now(uint16_t handle, uint16_t channel_id){
	return l2cap_can_send_fixed_channel_packet_now_status;
}

void l2cap_request_can_send_fix_channel_now_event(uint16_t handle, uint16_t channel_id){
	uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 1, 0};
    att_server_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*)event, sizeof(event));
}

uint8_t l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
	att_connection_t att_connection;
    hci_setup_le_connection(handle);
	uint8_t response[max_mtu];
	uint16_t response_len = att_handle_request(&att_connection, l2cap_get_outgoing_buffer(), len, &response[0]);
	if (response_len){
        att_server_packet_handler(ATT_DATA_PACKET, gatt_client_handle, &response[0], response_len);
	}
	return ERROR_CODE_SUCCESS;
}

static int cmac_ready = 1;
void set_cmac_ready(int ready){
    cmac_ready = ready;
}

void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
}

int  sm_cmac_ready(void){
	return cmac_ready;
}

void sm_cmac_signed_write_start(const sm_key_t key, uint8_t opcode, uint16_t attribute_handle, uint16_t message_len, const uint8_t * message, uint32_t sign_counter, void (*done_callback)(uint8_t * hash)){
	// sm_notify_client(SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED, sm_central_device_addr_type, sm_central_device_address, 0, sm_central_device_matched);      
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
hci_connection_t * hci_connection_for_bd_addr_and_type(const bd_addr_t addr, bd_addr_type_t addr_type){
	printf("hci_connection_for_bd_addr_and_type not implemented in mock backend\n");
	return NULL;
}
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
    if (hci_connection.con_handle != con_handle){
        return NULL;
    }
    return &hci_connection;
}

void hci_connections_get_iterator(btstack_linked_list_iterator_t *it){
	// printf("hci_connections_get_iterator not implemented in mock backend\n");
    btstack_linked_list_iterator_init(it, &connections);
}


void l2cap_run(void){
}

void l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
}

bool gap_authenticated(hci_con_handle_t con_handle){
	return false;
}
authorization_state_t gap_authorization_state(hci_con_handle_t con_handle){
	return AUTHORIZATION_UNKNOWN;
}
uint8_t gap_encryption_key_size(hci_con_handle_t con_handle){
	return 0;
}
bool gap_secure_connection(hci_con_handle_t con_handle){
	return false;
}
gap_connection_type_t gap_get_connection_type(hci_con_handle_t con_handle){
    if (hci_connection.con_handle != con_handle){
        return GAP_CONNECTION_INVALID;
    }
    switch (hci_connection.address_type){
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
            return GAP_CONNECTION_LE;
        case BD_ADDR_TYPE_SCO:
            return GAP_CONNECTION_SCO;
        case BD_ADDR_TYPE_ACL:
            return GAP_CONNECTION_ACL;
        default:
            return GAP_CONNECTION_INVALID;
    }
}

int gap_request_connection_parameter_update(hci_con_handle_t con_handle, uint16_t conn_interval_min,
	uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout){
	return 0;	
}

void mock_call_att_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    (*att_server_packet_handler)(packet_type, channel, packet, size);
}

void mock_call_att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    (*registered_hci_event_handler)(packet_type, channel, packet, size);
}

void att_dispatch_register_server(btstack_packet_handler_t packet_handler){
    att_server_packet_handler = packet_handler;
}

bool att_dispatch_server_can_send_now(hci_con_handle_t con_handle){
    UNUSED(con_handle);
    return l2cap_can_send_fixed_channel_packet_now_status;
}

void att_dispatch_server_mtu_exchanged(hci_con_handle_t con_handle, uint16_t new_mtu){
    UNUSED(con_handle);
    UNUSED(new_mtu);
}

void att_dispatch_server_request_can_send_now_event(hci_con_handle_t con_handle){
    if (l2cap_can_send_fixed_channel_packet_now_status){
        uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 1, 0};
        att_server_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*)event, sizeof(event));
    }
}


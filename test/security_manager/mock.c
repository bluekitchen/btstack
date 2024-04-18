#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble/att_db.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "rijndael.h"
#include "btstack_linked_list.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_debug.h"

static btstack_packet_handler_t le_data_handler;

static uint8_t packet_buffer[256];
static uint16_t packet_buffer_len = 0;

static uint8_t aes128_cyphertext[16];

static hci_connection_t  the_connection;
static btstack_linked_list_t     connections;
static btstack_linked_list_t     event_packet_handlers;

void mock_init(void){
	the_connection.item.next = NULL;
	connections = (btstack_linked_item_t*) &the_connection;
}

uint8_t * mock_packet_buffer(void){
	return packet_buffer;
}

void mock_clear_packet_buffer(void){
	packet_buffer_len = 0;
	memset(packet_buffer, 0, sizeof(packet_buffer));
}

static void dump_packet(int packet_type, uint8_t * buffer, uint16_t size){
#if 0
	static int packet_counter = 1;
	char var_name[80];
	sprintf(var_name, "test_%s_packet_%02u", packet_type == HCI_COMMAND_DATA_PACKET ? "command" : "acl", packet_counter);
	printf("uint8_t %s[] = { ", var_name);
	for (int i = 0; i < size ; i++){
		if ((i % 16) == 0) printf("\n    ");
		printf ("0x%02x, ", buffer[i]);
	}
	printf("};\n");
	packet_counter++;
#endif
}

void aes128_calc_cyphertext(uint8_t key[16], uint8_t plaintext[16], uint8_t cyphertext[16]){
	uint32_t rk[RKLENGTH(KEYBITS)];
	int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
	rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

void mock_simulate_hci_event(uint8_t * packet, uint16_t size){
	hci_dump_packet(HCI_EVENT_PACKET, 1, packet, size);
	btstack_linked_list_iterator_t  it;
	btstack_linked_list_iterator_init(&it ,&event_packet_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
       	btstack_packet_callback_registration_t * item = (btstack_packet_callback_registration_t *) btstack_linked_list_iterator_next(&it);
		item->callback(HCI_EVENT_PACKET, 0, packet, size);
    }
	if (le_data_handler){
		le_data_handler(HCI_EVENT_PACKET, 0, packet, size);
	}
}

void aes128_report_result(void){
	uint8_t le_enc_result[22];
	uint8_t enc1_data[] = { 0x0e, 0x14, 0x01, 0x17, 0x20, 0x00 };
	memcpy (le_enc_result, enc1_data, 6);
	reverse_128(aes128_cyphertext, &le_enc_result[6]);
	mock_simulate_hci_event(&le_enc_result[0], sizeof(le_enc_result));
}

void mock_simulate_sm_data_packet(uint8_t * packet, uint16_t len){

	uint16_t handle = 0x40;
	uint16_t cid = 0x06;

	uint8_t acl_buffer[len + 8];

	// 0 - Connection handle : PB=10 : BC=00 
    little_endian_store_16(acl_buffer, 0, handle | (0 << 12) | (0 << 14));
    // 2 - ACL length
    little_endian_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    little_endian_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    little_endian_store_16(acl_buffer, 6, cid);  

	memcpy(&acl_buffer[8], packet, len);
	hci_dump_packet(HCI_ACL_DATA_PACKET, 1, &acl_buffer[0], len + 8);

	le_data_handler(SM_DATA_PACKET, handle, packet, len);

	// process queued callbacks, might become obsolete after queued callback integration in run loop
	btstack_run_loop_embedded_execute_once();
}

void mock_simulate_command_complete(const hci_cmd_t *cmd){
	uint8_t packet[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, (uint8_t) cmd->opcode & 0xff, (uint8_t) cmd->opcode >> 8, 0};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
}

void mock_simulate_hci_state_working(void){
	uint8_t packet[] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
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
    uint8_t packet[] = { 0x3e, 0x13, 0x01, 0x00, 0x40, 0x00, 0x01, 0x01, 0x18, 0x12, 0x5e, 0x68, 0xc9, 0x73, 0x18, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05};
	uint8_t gap_event[36];
	hci_create_gap_connection_complete_event(packet, gap_event);
	mock_simulate_hci_event(gap_event, sizeof(gap_event));
}

void att_init_connection(att_connection_t * att_connection){
    att_connection->mtu = 23;
    att_connection->encryption_key_size = 0;
    att_connection->authenticated = 0;
	att_connection->authorized = 0;
}

void gap_local_bd_addr(bd_addr_t address_buffer){
	int i;
	for (i=0;i<6;i++) {
		address_buffer[i] = 0x11 * (i+1);
	}
}

void hci_halting_defer(void){
}

bool hci_can_send_command_packet_now(void){
	return true;
}
bool hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type){
	return true;
}

hci_connection_t * hci_connection_for_bd_addr_and_type(const bd_addr_t addr, bd_addr_type_t addr_type){
	return &the_connection;
}
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
	return &the_connection;
}
void hci_connections_get_iterator(btstack_linked_list_iterator_t *it){
    btstack_linked_list_iterator_init(it, &connections);
}

// get addr type and address used in different contexts
void gap_le_get_own_address(uint8_t * addr_type, bd_addr_t addr){
    static uint8_t dummy[] = { 0x00, 0x1b, 0xdc, 0x07, 0x32, 0xef };
    *addr_type = 0;
    memcpy(addr, dummy, 6);
}
void gap_le_get_own_advertisements_address(uint8_t * addr_type, bd_addr_t addr){
    gap_le_get_own_address(addr_type, addr);
}
void gap_le_get_own_connection_address(uint8_t * addr_type, bd_addr_t addr){
    gap_le_get_own_address(addr_type, addr);
}

void hci_le_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy) {
 }

uint16_t hci_get_manufacturer(void){
 return 0xffff;
};

void hci_le_set_own_address_type(uint8_t own_address){
}

void hci_le_random_address_set(const bd_addr_t addr){
}

bool hci_is_le_identity_address_type(bd_addr_type_t address_type) {
    switch (address_type) {
        case BD_ADDR_TYPE_LE_PUBLIC_IDENTITY:
        case BD_ADDR_TYPE_LE_RANDOM_IDENTITY:
            return true;
        default:
            return false;
    }
}

void l2cap_request_can_send_fix_channel_now_event(hci_con_handle_t con_handle, uint16_t cid){
	if (packet_buffer_len) return;
    uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 0, 0};
    little_endian_store_16(event, 2, cid);
    le_data_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

bool l2cap_can_send_connectionless_packet_now(void){
	return packet_buffer_len == 0;
}

bool l2cap_can_send_fixed_channel_packet_now(uint16_t handle, uint16_t channel_id){
	return packet_buffer_len == 0;
}

uint8_t hci_send_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_cmd_create_from_template(packet_buffer, cmd, argptr);
    va_end(argptr);
	hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet_buffer, len);
	dump_packet(HCI_COMMAND_DATA_PACKET, packet_buffer, len);
	packet_buffer_len = len;

	// track le encrypt and le rand
	if (cmd->opcode ==  hci_le_encrypt.opcode){
	    uint8_t * key_flipped = &packet_buffer[3];
	    uint8_t key[16];
		reverse_128(key_flipped, key);
	    // printf("le_encrypt key ");
	    // hexdump(key, 16);
	    uint8_t * plaintext_flipped = &packet_buffer[19];
	    uint8_t plaintext[16];
 		reverse_128(plaintext_flipped, plaintext);
	    // printf("le_encrypt txt ");
	    // hexdump(plaintext, 16);
	    aes128_calc_cyphertext(key, plaintext, aes128_cyphertext);
	    // printf("le_encrypt res ");
	    // hexdump(aes128_cyphertext, 16);
	}
	return ERROR_CODE_SUCCESS;
}

void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
	le_data_handler = packet_handler;
}

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
	btstack_linked_list_add(&event_packet_handlers, (btstack_linked_item_t *) callback_handler);
}

void l2cap_reserve_packet_buffer(void){
	printf("l2cap_reserve_packet_buffer\n");
}

uint8_t l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
	printf("l2cap_send_prepared_connectionless\n");
	return 0;
}

uint8_t l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t * buffer, uint16_t len){
	// printf("l2cap_send_connectionless\n");

    int pb = hci_non_flushable_packet_boundary_flag_supported() ? 0x00 : 0x02;

	// 0 - Connection handle : PB=pb : BC=00 
    little_endian_store_16(packet_buffer, 0, handle | (pb << 12) | (0 << 14));
    // 2 - ACL length
    little_endian_store_16(packet_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    little_endian_store_16(packet_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    little_endian_store_16(packet_buffer, 6, cid);  

	memcpy(&packet_buffer[8], buffer, len);
	hci_dump_packet(HCI_ACL_DATA_PACKET, 0, &packet_buffer[0], len + 8);

	dump_packet(HCI_ACL_DATA_PACKET, packet_buffer, len + 8);
	packet_buffer_len = len + 8;

	return ERROR_CODE_SUCCESS;
}

void hci_disconnect_security_block(hci_con_handle_t con_handle){
	printf("hci_disconnect_security_block \n");	
}

bool hci_non_flushable_packet_boundary_flag_supported(void){
	return true;
}

void l2cap_run(void){
}

HCI_STATE hci_get_state(void){
	return HCI_STATE_WORKING;
}

#include "hal_cpu.h"
void hal_cpu_disable_irqs(void){}
void hal_cpu_enable_irqs(void){}
void hal_cpu_enable_irqs_and_sleep(void){}

#include "hal_time_ms.h"
static uint32_t time_ms;
uint32_t hal_time_ms(void){
	return time_ms++;
}

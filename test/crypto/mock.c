#include "btstack_util.h"
#include "hci.h"
#include "aes_cmac.h"
#include "hci_dump.h"
#include <stdio.h>

static btstack_linked_list_t  event_packet_handlers;
static uint8_t packet_buffer[256];
static uint16_t packet_buffer_len;

static uint8_t aes128_cyphertext[16];
void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
	btstack_linked_list_add(&event_packet_handlers, (btstack_linked_item_t *) callback_handler);
}

bool hci_can_send_command_packet_now(void){
	return true;
}

HCI_STATE hci_get_state(void){
	return HCI_STATE_WORKING;
}

static void mock_simulate_hci_event(uint8_t * packet, uint16_t size){
	static int level = 0;
	// hci_dump_packet(HCI_EVENT_PACKET, 1, packet, size);
	btstack_linked_list_iterator_t  it;
	btstack_linked_list_iterator_init(&it ,&event_packet_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
       	btstack_packet_callback_registration_t * item = (btstack_packet_callback_registration_t *) btstack_linked_list_iterator_next(&it);
		item->callback(HCI_EVENT_PACKET, 0, packet, size);
    }
}

static void aes128_report_result(void){
	uint8_t le_enc_result[22];
	uint8_t enc1_data[] = { 0x0e, 0x14, 0x01, 0x17, 0x20, 0x00 };
	memcpy (le_enc_result, enc1_data, 6);
	reverse_128(aes128_cyphertext, &le_enc_result[6]);
	mock_simulate_hci_event(&le_enc_result[0], sizeof(le_enc_result));
}

uint8_t hci_send_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_cmd_create_from_template(packet_buffer, cmd, argptr);
    va_end(argptr);
	hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet_buffer, len);
	// dump_packet(HCI_COMMAND_DATA_PACKET, packet_buffer, len);
	packet_buffer_len = len;
	if (cmd->opcode ==  hci_le_encrypt.opcode){
	    uint8_t * key_flipped = &packet_buffer[3];
	    uint8_t key[16];
		reverse_128(key_flipped, key);
	    uint8_t * plaintext_flipped = &packet_buffer[19];
	    uint8_t plaintext[16];
 		reverse_128(plaintext_flipped, plaintext);
	    aes128_calc_cyphertext(key, plaintext, aes128_cyphertext);
	    aes128_report_result();
	}
	return ERROR_CODE_SUCCESS;
}

void hci_halting_defer(void){
}

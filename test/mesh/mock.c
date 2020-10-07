
#include "mock.h"

#include <stdint.h>
#include <string.h>

#include "ble/att_db.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "rijndael.h"


static btstack_linked_list_t event_packet_handlers;

static uint8_t  packet_buffer[256];
static uint16_t packet_buffer_len = 0;

static uint8_t aes128_cyphertext[16];

static int report_aes128;
static int report_random;

static uint32_t lfsr_random;
// #define ENABLE_PACKET_LOGGER
// #define ENABLE_AES128_LOGGER

/* taps: 32 31 29 1; characteristic polynomial: x^32 + x^31 + x^29 + x + 1 */
#define LFSR(a) ((a >> 1) ^ (uint32_t)((0 - (a & 1u)) & 0xd0000001u))

void mock_init(void){
    lfsr_random = 0x12345678;
}

uint8_t * mock_packet_buffer(void){
	return packet_buffer;
}

void mock_clear_packet_buffer(void){
	packet_buffer_len = 0;
	memset(packet_buffer, 0, sizeof(packet_buffer));
}

static void dump_packet(int packet_type, uint8_t * buffer, uint16_t size){
#ifdef ENABLE_PACKET_LOGGER
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
#else
UNUSED(packet_type);
UNUSED(buffer);
UNUSED(size);
#endif
}

static void aes128_calc_cyphertext(uint8_t key[16], uint8_t plaintext[16], uint8_t cyphertext[16]){
	uint32_t rk[RKLENGTH(KEYBITS)];
	int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
	rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

void mock_simulate_hci_event(uint8_t * packet, uint16_t size){
	dump_packet(HCI_EVENT_PACKET, packet, size);
	hci_dump_packet(HCI_EVENT_PACKET, 1, packet, size);
    // dispatch to all event handlers
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &event_packet_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_packet_callback_registration_t * entry = (btstack_packet_callback_registration_t*) btstack_linked_list_iterator_next(&it);
        entry->callback(HCI_EVENT_PACKET, 0, packet, size);
    }
}

static void aes128_report_result(void){
	uint8_t le_enc_result[22];
	uint8_t enc1_data[] = { 0x0e, 0x14, 0x01, 0x17, 0x20, 0x00 };
	memcpy (le_enc_result, enc1_data, 6);
	reverse_128(aes128_cyphertext, &le_enc_result[6]);
	mock_simulate_hci_event(le_enc_result, sizeof(le_enc_result));
}

static void random_report_result(void){
	uint8_t le_rand_result[14];
	uint8_t rand_complete[] = { 0x0e, 0x0c, 0x01, 0x18, 0x20, 0x00 };
	memcpy (le_rand_result, rand_complete, 6);
	int i;
	for (i=0;i<8;i++){
        lfsr_random = LFSR(lfsr_random);
		le_rand_result[6+i]= lfsr_random;
	}
	mock_simulate_hci_event(le_rand_result, sizeof(le_rand_result));
}

int mock_process_hci_cmd(void){
	if (report_aes128){
		report_aes128 = 0;
		aes128_report_result();
		return 1;
	}
	if (report_random){
		report_random = 0;
		random_report_result();
		return 1;
	}
	return 0;
}

void mock_simulate_hci_state_working(void){
	uint8_t packet[] = {BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING};
	mock_simulate_hci_event((uint8_t *)&packet, sizeof(packet));
}

int hci_can_send_command_packet_now(void){
	return 1;
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
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
	    uint8_t * plaintext_flipped = &packet_buffer[19];
	    uint8_t plaintext[16];
 		reverse_128(plaintext_flipped, plaintext);
	    aes128_calc_cyphertext(key, plaintext, aes128_cyphertext);
	    report_aes128 = 1;
#ifdef ENABLE_AES128_LOGGER
	    printf("AES128 Operation\n");
	    printf("Key:    "); printf_hexdump(key, 16);
	    printf("Plain:  "); printf_hexdump(plaintext, 16);
	    printf("Cipher: "); printf_hexdump(aes128_cyphertext, 16);
#endif
	}
	if (cmd->opcode == hci_le_rand.opcode){
		report_random = 1;
	}
	return 0;
}

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_add_tail(&event_packet_handlers, (btstack_linked_item_t*) callback_handler);
}

HCI_STATE hci_get_state(void){
	return HCI_STATE_WORKING;
}

void btstack_run_loop_add_timer(btstack_timer_source_t * ts){
    UNUSED(ts);
}
int btstack_run_loop_remove_timer(btstack_timer_source_t * ts){
    UNUSED(ts);
	return 0;
}
void btstack_run_loop_set_timer(btstack_timer_source_t * ts, uint32_t timeout){
    UNUSED(ts);
    UNUSED(timeout);
}
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * ts, void (*fn)(btstack_timer_source_t * ts)){
    UNUSED(ts);
    UNUSED(fn);
}
static void * timer_context;
void btstack_run_loop_set_timer_context(btstack_timer_source_t * ts, void * context){
    UNUSED(ts);
	timer_context = context;
}
void * btstack_run_loop_get_timer_context(btstack_timer_source_t * ts){
    UNUSED(ts);
	return timer_context;
}
void hci_halting_defer(void){
}



// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_run_loop_posix.h"

#include "hci_cmd.h"
#include "btstack_util.h"

#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "ble/sm.h"

// test data

uint8_t test_command_packet_01[] = {
    0x17, 0x20, 0x20, 0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93,
    0x92, 0x91, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, };
uint8_t test_command_packet_02[] = {
    0x17, 0x20, 0x20, 0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93,
    0x92, 0x91, 0x90, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, };
uint8_t test_acl_packet_03[] = {
    0x40, 0x00, 0x0b, 0x00, 0x07, 0x00, 0x06, 0x00, 0x02, 0x03, 0x00, 0x01, 0x10, 0x07, 0x07, };
uint8_t test_command_packet_04[] = {
    0x18, 0x20, 0x00, };
uint8_t test_command_packet_05[] = {
    0x18, 0x20, 0x00, };
uint8_t test_command_packet_06[] = {
    0x17, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7b, 0x05, 0xd4, 0xef, 0x0e, 0x26, 0x2f, 0x4f, 0x94, 0x9e, 0x45, 0x72, 0x85,
    0x92, 0x03, 0x28, };
uint8_t test_command_packet_07[] = {
    0x17, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x87, 0xd0, 0x7b, 0x7f, 0xad, 0x4d, 0x47, 0xa6, 0x84, 0x2c, 0x85, 0x79, 0x58,
    0xd3, 0xfe, 0xcd, };
uint8_t test_acl_packet_08[] = {
    0x40, 0x00, 0x15, 0x00, 0x11, 0x00, 0x06, 0x00, 0x03, 0xa9, 0xe9, 0x8d, 0x9a, 0xc3, 0x96, 0xe3,
    0x52, 0xa1, 0x17, 0xc0, 0xb2, 0xbe, 0x76, 0x42, 0xda, };
uint8_t test_command_packet_09[] = {
    0x17, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xfc, 0xd4, 0x07, 0x41, 0x0f, 0x1f, 0xcc, 0x83, 0xd2, 0x41, 0xaf, 0xf7, 0x5f,
    0xd0, 0x31, 0x2e, };
uint8_t test_command_packet_10[] = {
    0x17, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x71, 0xd8, 0xbe, 0xe4, 0xb5, 0x48, 0x4d, 0x35, 0x80, 0x5e, 0x95, 0xf9, 0xb9,
    0xaf, 0xc2, 0x55, };
uint8_t test_acl_packet_11[] = {
    0x40, 0x00, 0x15, 0x00, 0x11, 0x00, 0x06, 0x00, 0x04, 0x7a, 0x05, 0xd5, 0xeb, 0x0e, 0x27, 0x3f,
    0x48, 0x93, 0x9c, 0x46, 0x72, 0x84, 0x82, 0x04, 0x2f, };
uint8_t test_command_packet_12[] = {
    0x17, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xfd, 0xd4, 0x06, 0x45, 0x0f, 0x1e, 0xdc, 0x84, 0x7a, 0x05, 0xd5, 0xeb, 0x0e,
    0x27, 0x3f, 0x48, };
uint8_t test_command_packet_13[] = {
    0x1a, 0x20, 0x12, 0x40, 0x00, 0xd1, 0x36, 0xa2, 0x5f, 0x8d, 0x5f, 0xd4, 0xd1, 0x70, 0x56, 0x1c,
    0x12, 0x99, 0x98, 0x95, 0x30, };
uint8_t test_command_packet_14[] = {
    0x18, 0x20, 0x00, };
uint8_t test_command_packet_15[] = {
    0x18, 0x20, 0x00, };
uint8_t test_command_packet_16[] = {
    0x17, 0x20, 0x20, 0xc7, 0x29, 0x17, 0x92, 0x59, 0xb9, 0x8f, 0xaa, 0xa2, 0x0a, 0x32, 0x2a, 0x73,
    0xc6, 0x4f, 0xb5, 0xcf, 0x10, 0x70, 0x5f, 0x3c, 0x2d, 0xe3, 0xb3, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, };
uint8_t test_command_packet_17[] = {
    0x17, 0x20, 0x20, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33,
    0x32, 0x31, 0x30, 0xe2, 0xf1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, };
uint8_t test_command_packet_18[] = {
    0x17, 0x20, 0x20, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33,
    0x32, 0x31, 0x30, 0xe2, 0xf1, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, };

uint8_t test_acl_packet_18[] = {
    0x40, 0x00, 0x15, 0x00, 0x11, 0x00, 0x06, 0x00, 0x06, 0xef, 0x2f, 0xd9, 0x0b, 0x04, 0x7a, 0xe9,
    0xa1, 0xcf, 0xc3, 0xa3, 0x83, 0xb7, 0x38, 0x30, 0x73, };
uint8_t test_acl_packet_19[] = {
    0x40, 0x00, 0x0f, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x07, 0x5b, 0xb4, 0xcf, 0x10, 0x70, 0x5f, 0x3c,
    0x2d, 0xe3, 0xb3, };
uint8_t test_acl_packet_20[] = {
    0x40, 0x00, 0x15, 0x00, 0x11, 0x00, 0x06, 0x00, 0x08, 0xe6, 0xea, 0xee, 0x60, 0x31, 0x7b, 0xfc,
    0xa2, 0x3f, 0xa5, 0x79, 0x59, 0xe7, 0x41, 0xcf, 0xc7, };
uint8_t test_acl_packet_21[] = {
    0x40, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x06, 0x00, 0x09, 0x00, 0xef, 0x32, 0x07, 0xdc, 0x1b, 0x00, };
uint8_t test_acl_packet_22[] = {
    0x40, 0x00, 0x15, 0x00, 0x11, 0x00, 0x06, 0x00, 0x0a, 0x1d, 0x06, 0xba, 0xf4, 0x0c, 0x49, 0x55,
    0x5b, 0x93, 0x93, 0xc1, 0x8b, 0x09, 0xd0, 0xb8, 0x80, };

bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};

static btstack_packet_callback_registration_t sm_event_callback_registration;

void mock_init(void);
void mock_simulate_hci_state_working(void);
void mock_simulate_hci_event(uint8_t * packet, uint16_t size);
void aes128_report_result(void);
void mock_simulate_sm_data_packet(uint8_t * packet, uint16_t size);
void mock_simulate_command_complete(const hci_cmd_t *cmd);
void mock_simulate_connected(void);
uint8_t * mock_packet_buffer(void);
uint16_t mock_packet_buffer_len(void);
void mock_clear_packet_buffer(void);

void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint16_t aHandle;
    bd_addr_t event_address;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case SM_EVENT_PASSKEY_INPUT_NUMBER: 
                    // store peer address for input
                    printf("\nGAP Bonding: Enter 6 digit passkey: '");
                    fflush(stdout);
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    printf("\nGAP Bonding: Display Passkey '%06u\n", little_endian_read_32(packet, 11));
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding: Display cancel\n");
                    break;

                case SM_EVENT_JUST_WORKS_REQUEST:
                    // auto-authorize connection if requested
                    sm_just_works_confirm(little_endian_read_16(packet, 2));
                    printf("Just Works request confirmed\n");
                    break;

                case SM_EVENT_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_authorization_grant(little_endian_read_16(packet, 2));
                    break;

                default:
                    break;
            }
    }
}

void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		if (expected[i] != actual[i]) {
			printf("offset %u wrong\n", i);
			printf("expected: "); printf_hexdump(expected, size);
			printf("actual:   "); printf_hexdump(actual, size);
		}
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

#define CHECK_HCI_COMMAND(packet) { printf("check " #packet "\n") ; CHECK_EQUAL_ARRAY(packet, mock_packet_buffer(), sizeof(packet)); mock_clear_packet_buffer(); }
#define CHECK_ACL_PACKET(packet)  { printf("check " #packet "\n") ; CHECK_EQUAL_ARRAY(packet, mock_packet_buffer(), sizeof(packet)); mock_clear_packet_buffer(); }

static int parse_hex(uint8_t * buffer, const char * hex_string){
    int len = 0;
    while (*hex_string){
        if (*hex_string == ' '){
            hex_string++;
            continue;
        }
        int high_nibble = nibble_for_char(*hex_string++);
        int low_nibble = nibble_for_char(*hex_string++);
        *buffer++ = (high_nibble << 4) | low_nibble;
        len++;
    }
    return len;
}

static const char * key_string = "2b7e1516 28aed2a6 abf71588 09cf4f3c";

static const char * m0_string        = "";
static const char * cmac_m0_string   = "bb1d6929 e9593728 7fa37d12 9b756746";
static const char * m16_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a";
static const char * cmac_m16_string  = "070a16b4 6b4d4144 f79bdd9d d04a287c";
static const char * m40_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a ae2d8a57 1e03ac9c 9eb76fac 45af8e51 30c81c46 a35ce411";
static const char * cmac_m40_string  = "dfa66747 de9ae630 30ca3261 1497c827";
static const char * m64_string       = "6bc1bee2 2e409f96 e93d7e11 7393172a ae2d8a57 1e03ac9c 9eb76fac 45af8e51 30c81c46 a35ce411 e5fbc119 1a0a52ef f69f2445 df4f9b17 ad2b417b e66c3710";
static const char * cmac_m64_string  = "51f0bebf 7e3b9d92 fc497417 79363cfe";

static uint8_t cmac_hash[16];
static int cmac_hash_received;
static void cmac_done(uint8_t * hash){
    memcpy(cmac_hash, hash, 16);
    printf("cmac hash: ");
    printf_hexdump(hash, 16);
    cmac_hash_received = 1;
}

static uint8_t m[128];

#if 0
// CMAC calculation has been moved to btstack_crypto
static uint8_t get_byte(uint16_t offset){
    // printf ("get byte %02u -> %02x\n", offset, m[offset]);
    return m[offset];
}
static void validate_message(const char * name, const char * message_string, const char * cmac_string){

    mock_clear_packet_buffer();
    int len = parse_hex(m, message_string);

    // expected result
    sm_key_t cmac;
    parse_hex(cmac, cmac_string);

    printf("-- verify key %s message %s, len %u:\nm:    %s\ncmac: %s\n", key_string, name, len, message_string, cmac_string);

    sm_key_t key;
    parse_hex(key, key_string);
    // printf_hexdump(key, 16);

    cmac_hash_received = 0;
    sm_cmac_general_start(key, len, &get_byte, &cmac_done);
    while (!cmac_hash_received){
        aes128_report_result();
    }
    CHECK_EQUAL_ARRAY(cmac, cmac_hash, 16);
}
#endif

#define VALIDATE_MESSAGE(NAME) validate_message(#NAME, NAME##_string, cmac_##NAME##_string)

TEST_GROUP(SecurityManager){
	void setup(void){
        static int first = 1;
        if (first){
            first = 0;
            btstack_memory_init();
            btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        }
	    sm_init();
	    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
	    sm_set_authentication_requirements( SM_AUTHREQ_BONDING ); 
        sm_event_callback_registration.callback = &app_packet_handler;
        sm_add_event_handler(&sm_event_callback_registration);	
    }
};

#if 0
TEST(SecurityManager, CMACTest){

    mock_init();
    mock_simulate_hci_state_working();

    // expect le encrypt commmand
    CHECK_HCI_COMMAND(test_command_packet_01);

    aes128_report_result();

    // expect le encrypt commmand
    CHECK_HCI_COMMAND(test_command_packet_02);

    aes128_report_result();
    mock_clear_packet_buffer();

    // additional test: cmac signing
    // aes cmac tests
    sm_key_t key;
    parse_hex(key, key_string);
    uint8_t message [] = "hallo";
    cmac_hash_received = 0;
    sm_cmac_signed_write_start(key, 0x11, 0x1234, sizeof(message), message, 1, &cmac_done);
    while (!cmac_hash_received){
        aes128_report_result();
    }
    uint8_t expected_hash[] = { 0x40, 0x4E, 0xDC, 0x0F, 0x6E, 0x0F, 0xF9, 0x5C};
    CHECK_EQUAL_ARRAY(expected_hash, cmac_hash, 8);

    // generic aes cmac tests
    VALIDATE_MESSAGE(m0);
    VALIDATE_MESSAGE(m16);
    VALIDATE_MESSAGE(m40);
    VALIDATE_MESSAGE(m64);
}
#endif

TEST(SecurityManager, MainTest){

    mock_init();
    mock_simulate_hci_state_working();

    // expect le encrypt commmand
    CHECK_HCI_COMMAND(test_command_packet_01);
    aes128_report_result();

    // expect le encrypt commmand
    CHECK_HCI_COMMAND(test_command_packet_02);

    aes128_report_result();
	mock_clear_packet_buffer();

    mock_simulate_connected();

    uint8_t test_pairing_request_command[] = { 0x01, 0x04, 0x00, 0x01, 0x10, 0x07, 0x07 };
    mock_simulate_sm_data_packet(&test_pairing_request_command[0], sizeof(test_pairing_request_command));

    // expect send pairing response command
    CHECK_ACL_PACKET(test_acl_packet_03);

    uint8_t test_pairing_confirm_command[] = { 0x03, 0x84, 0x5a, 0x87, 0x9a, 0x0f, 0xa9, 0x42, 0xba, 0x48, 0xc5, 0x79, 0xa0, 0x70, 0x70, 0xa9, 0xc8 };
    mock_simulate_sm_data_packet(&test_pairing_confirm_command[0], sizeof(test_pairing_confirm_command));

    // expect le random command
    CHECK_HCI_COMMAND(test_command_packet_04);

    uint8_t rand1_data_event[] = { 0x0e, 0x0c, 0x01, 0x18, 0x20, 0x00, 0x2f, 0x04, 0x82, 0x84, 0x72, 0x46, 0x9c, 0x93 };
	mock_simulate_hci_event(&rand1_data_event[0], sizeof(rand1_data_event));

    // expect le random command
    CHECK_HCI_COMMAND(test_command_packet_05);

    uint8_t rand2_data_event[] = { 0x0e, 0x0c,0x01, 0x18,0x20, 0x00,0x48, 0x3f,0x27, 0x0e,0xeb, 0xd5,0x05, 0x7a };
	mock_simulate_hci_event(&rand2_data_event[0], sizeof(rand2_data_event));

	// expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_06);

    aes128_report_result();

    // expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_07);

    aes128_report_result();

    // expect send paring confirm command
    CHECK_ACL_PACKET(test_acl_packet_08);

    uint8_t test_pairing_random_command[] ={0x04, 0xfd, 0xd4, 0x06, 0x45, 0x0f, 0x1e, 0xdc, 0x84, 0xd5, 0x43, 0xac, 0xf7, 0x5e, 0xc0, 0x36, 0x29};
    mock_simulate_sm_data_packet(&test_pairing_random_command[0], sizeof(test_pairing_random_command));

    // expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_09);

    aes128_report_result();

    // expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_10);

    aes128_report_result();

    // expect send pairing random command
    CHECK_ACL_PACKET(test_acl_packet_11);

    // NOTE: SM also triggered for wrong handle

	uint8_t test_le_ltk_request[] = { 0x3e, 0x0d, 0x05, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 00 };
	mock_simulate_hci_event(&test_le_ltk_request[0], sizeof(test_le_ltk_request));

	// expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_12);

    aes128_report_result();

	// expect le ltk reply
    CHECK_HCI_COMMAND(test_command_packet_13);

	uint8_t test_ecnryption_change_event[] = { 0x08, 0x04, 0x00, 0x40, 0x00, 0x01 };
	mock_simulate_hci_event(&test_ecnryption_change_event[0], sizeof(test_ecnryption_change_event));

    // expect le random command
    CHECK_HCI_COMMAND(test_command_packet_14);

    uint8_t rand3_data_event[] = { 0x0e, 0x0c, 0x01, 0x18, 0x20, 0x00, 0xc0, 0x10, 0x70, 0x5f, 0x3c, 0x2d, 0xe3, 0xb3 };
	mock_simulate_hci_event(&rand3_data_event[0], sizeof(rand3_data_event));

    // expect le random command
    CHECK_HCI_COMMAND(test_command_packet_15);

    uint8_t rand4_data_event[] = { 0x0e, 0x0c, 0x01, 0x18, 0x20, 0x00, 0xf1, 0xe2, 0xbf, 0x7d, 0x84, 0x19, 0x32, 0x8b };
	mock_simulate_hci_event(&rand4_data_event[0], sizeof(rand4_data_event));

	// expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_16);

    aes128_report_result();

	// expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_17);

    aes128_report_result();

	// expect le encrypt command
    CHECK_HCI_COMMAND(test_command_packet_18);

    aes128_report_result();

    //
	uint8_t num_completed_packets_event[] = { 0x13, 0x05, 0x01, 0x4a, 0x00, 0x01, 00 };

    // expect send LE SMP Encryption Information Command
    CHECK_ACL_PACKET(test_acl_packet_18);
	
	mock_simulate_hci_event(&num_completed_packets_event[0], sizeof(num_completed_packets_event));

    // expect send LE SMP Master Identification Command 
    CHECK_ACL_PACKET(test_acl_packet_19);
	
	mock_simulate_hci_event(&num_completed_packets_event[0], sizeof(num_completed_packets_event));

	// expect send LE SMP Identity Information Command
    CHECK_ACL_PACKET(test_acl_packet_20);
	
	mock_simulate_hci_event(&num_completed_packets_event[0], sizeof(num_completed_packets_event));
	
	// expect send LE SMP Identity Address Information Command 
    CHECK_ACL_PACKET(test_acl_packet_21);

	mock_simulate_hci_event(&num_completed_packets_event[0], sizeof(num_completed_packets_event));

	// expect send LE SMP Code Signing Information Command
    CHECK_ACL_PACKET(test_acl_packet_22);
}

int main (int argc, const char * argv[]){
    // hci_dump_open("hci_dump.pklg", HCI_DUMP_STDOUT); // HCI_DUMP_PACKETLOGGER
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

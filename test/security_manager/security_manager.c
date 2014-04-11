
//*****************************************************************************
//
// test rfcomm query tests
//
//*****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sm.h"

/*
SM: sending security request
SMP: generation method 0
p1     07 07 10 01 00 03 02 07 07 10 01 00 04 01 00 01
r      2F 04 82 84 72 46 9C 93 48 3F 27 0E EB D5 05 7A
t1     28 03 92 85 72 45 9E 94 4F 2F 26 0E EF D4 05 7B
p2     00 00 00 00 73 C9 68 5E 12 18 00 1B DC 07 32 EF
t3     CD FE D3 58 79 85 2C 84 A6 47 4D AD 7F 7B D0 87
c1!    DA 42 76 BE B2 C0 17 A1 52 E3 96 C3 9A 8D E9 A9
p1     07 07 10 01 00 03 02 07 07 10 01 00 04 01 00 01
r      29 36 C0 5E F7 AC 43 D5 84 DC 1E 0F 45 06 D4 FD
t1     2E 31 D0 5F F7 AF 41 D2 83 CC 1F 0F 41 07 D4 FC
p2     00 00 00 00 73 C9 68 5E 12 18 00 1B DC 07 32 EF
t3     55 C2 AF B9 F9 95 5E 80 35 4D 48 B5 E4 BE D8 71
c1!    C8 A9 70 70 A0 79 C5 48 BA 42 A9 0F 9A 87 5A 84
r1     2F 04 82 84 72 46 9C 93 48 3F 27 0E EB D5 05 7A
r2     29 36 C0 5E F7 AC 43 D5 84 DC 1E 0F 45 06 D4 FD
stk    30 95 98 99 12 1C 56 70 D1 D4 5F 8D 5F A2 36 D1
div    0xf1e2
y      0x45b9
ediv   0xb45b
ltk    73 30 38 B7 83 A3 C3 CF A1 E9 7A 04 0B D9 2F EF
Central Device DB adding type 1 - 73:C9:68:5E:12:18
irk    95 78 40 80 59 66 48 90 94 7F BB 92 53 91 7B C6
csrk   02 91 78 72 29 23 C5 F5 5F B1 99 94 42 5F 02 8D
*/

bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};
 
void mock_simulate_hci_state_working();
void mock_simulate_hci_event(uint8_t * packet, uint16_t size);
void aes128_report_result();
void mock_simulate_sm_data_packet(uint8_t * packet, uint16_t size);
void mock_simulate_command_complete(const hci_cmd_t *cmd);
void mock_simulate_connected();

void hexdump2(void const *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}
TEST_GROUP(GATTClient){
	void setup(){
	    btstack_memory_init();
	    run_loop_init(RUN_LOOP_POSIX);
	    sm_init();
	    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
	    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 

	    mock_simulate_hci_state_working();

	    aes128_report_result();
	    aes128_report_result();

	    mock_simulate_connected();
	    
	    uint8_t test_pairing_request_command[] = { 0x01, 0x04, 0x00, 0x01, 0x10, 0x07, 0x07 };
	    mock_simulate_sm_data_packet(&test_pairing_request_command[0], sizeof(test_pairing_request_command));

	    uint8_t test_pairing_confirm_command[] = { 0x03, 0x84, 0x5a, 0x87, 0x9a, 0x0f, 0xa9, 0x42, 0xba, 0x48, 0xc5, 0x79, 0xa0, 0x70, 0x70, 0xa9, 0xc8 };
	    mock_simulate_sm_data_packet(&test_pairing_confirm_command[0], sizeof(test_pairing_confirm_command));

	    uint8_t rand1_data_event[] = { 0x0e, 0x0c, 0x01, 0x18, 0x20, 0x00, 0x2f, 0x04, 0x82, 0x84, 0x72, 0x46, 0x9c, 0x93 };
		mock_simulate_hci_event(&rand1_data_event[0], sizeof(rand1_data_event));

	    uint8_t rand2_data_event[] = { 0x0e, 0x0c,0x01, 0x18,0x20, 0x00,0x48, 0x3f,0x27, 0x0e,0xeb, 0xd5,0x05, 0x7a };
		mock_simulate_hci_event(&rand2_data_event[0], sizeof(rand2_data_event));

	    aes128_report_result();
	    aes128_report_result();

	    uint8_t test_pairing_random_command[] ={0x04, 0xfd, 0xd4, 0x06, 0x45, 0x0f, 0x1e, 0xdc, 0x84, 0xd5, 0x43, 0xac, 0xf7, 0x5e, 0xc0, 0x36, 0x29};
	    mock_simulate_sm_data_packet(&test_pairing_random_command[0], sizeof(test_pairing_random_command));

	    aes128_report_result();
	    aes128_report_result();
	}
};

TEST(GATTClient, TestScanning){
}

int main (int argc, const char * argv[]){
    hci_dump_open("hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

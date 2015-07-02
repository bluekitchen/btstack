
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
#include "CppUTestExt/MockSupport.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "gatt_client.h"
 
static uint8_t advertisement_received;
static uint8_t connected;
static uint8_t advertisement_packet[150];

void mock_simulate_hci_state_working();
void mock_simulate_command_complete(const hci_cmd_t *cmd);
void mock_simulate_scan_response();
void mock_simulate_connected();


void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
	for (int i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

// -----------------------------------------------------

static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    
    bd_addr_t address;
    uint8_t event = packet[0];
    switch (event) {
        case BTSTACK_EVENT_STATE:
            if (packet[2] != HCI_STATE_WORKING) break;
            le_central_set_scan_parameters(0,0x0030, 0x0030);
            le_central_start_scan();
            break;
            
        case GAP_LE_ADVERTISING_REPORT:{
            advertisement_received = 1;
            memcpy(advertisement_packet, packet, size);
            
            bt_flip_addr(address, &packet[4]);
            le_central_connect(address, (bd_addr_type_t)packet[3]);
            break;
        }
        case HCI_EVENT_LE_META:
            // wait for connection complete
            if (packet[2] !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            connected = 1;
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            exit(0);
            break;
        default:
            break;
    }
}


TEST_GROUP(LECentral){
	void setup(void){
		advertisement_received = 0;
		connected = 0;
		l2cap_init();
        l2cap_register_packet_handler(&handle_hci_event);
		
		mock().expectOneCall("hci_can_send_packet_now_using_packet_buffer").andReturnValue(1);
		mock_simulate_hci_state_working();
	}
	
	void teardown(void){
        mock().clear();
    }
};



TEST(LECentral, TestScanningAndConnect){
    uint8_t expected_bt_addr[] = {0x34, 0xB1, 0xF7, 0xD1, 0x77, 0x9B};
    
    mock_simulate_command_complete(&hci_le_set_scan_enable);
    mock_simulate_scan_response();
	
    CHECK(advertisement_received);
    CHECK_EQUAL(0xE2, advertisement_packet[2]);                   // event type
    CHECK_EQUAL(0x01, advertisement_packet[3]);                   // address type
    CHECK_EQUAL_ARRAY(expected_bt_addr, &advertisement_packet[4], 6);
    CHECK_EQUAL(0xCC, advertisement_packet[10]);      // rssi
    CHECK_EQUAL(0x09, advertisement_packet[11]);      // data size
    
    for (int i=0; i<0x09; i++){         // data
        CHECK_EQUAL(i, advertisement_packet[12+i]);
    }
    mock_simulate_connected();
    CHECK(connected);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

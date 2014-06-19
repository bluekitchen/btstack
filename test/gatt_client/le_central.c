
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
#include "CppUTestExt/MockSupport.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "ble_client.h"

static bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};
static le_central_t test_le_central_context;
 
static uint8_t advertisement_received;
static uint8_t connected;

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

static void verify_advertisement(ad_event_t * e){
    CHECK_EQUAL(0, e->event_type);
 	CHECK_EQUAL(0, e->address_type);
	CHECK_EQUAL(188, e->rssi);
	CHECK_EQUAL(3, e->length);
	CHECK_EQUAL_ARRAY((uint8_t *)test_device_addr, (uint8_t *)e->address, 6);
}


static void handle_le_client_event(le_event_t * event){
	switch(event->type){
		case GAP_LE_ADVERTISING_REPORT:
			advertisement_received = 1;
			verify_advertisement((ad_event_t *) event);
			break;
		// TODO handle hci le connection complete event instead of this
		// case GATT_CONNECTION_COMPLETE: {
		// 	connected = 1;
		// 	break;
        }
		default:
			printf("le_event_t");
			break;
	}
}

TEST_GROUP(LECentral){
	void connect(){
		le_central_connect(&test_le_central_context, 1, test_device_addr);
		mock_simulate_connected();
		CHECK(connected);
	}

	void setup(){
		advertisement_received = 0;
		connected = 0;
		
		
		// mock().expectOneCall("productionCode").andReturnValue(10);

		// ble_client_init();
		// le_central_register_handler(handle_le_client_event);
		// le_central_register_connection_handler(handle_le_client_event);

		// mock().expectOneCall("hci_can_send_packet_now_using_packet_buffer").andReturnValue(1);
		// mock_simulate_hci_state_working();
		// connect();
	}
	
	void teardown(){
        mock().clear();
    }
};



// TEST(LECentral, TestScanning){
// 	le_central_start_scan();
// 	mock_simulate_command_complete(&hci_le_set_scan_enable);
// 	mock_simulate_scan_response();
// 	CHECK(advertisement_received);
// }

int productionCode(){
	printf("productionCode 20\n");
	mock().actualCall("productionCode");
    return 20;
}

TEST(LECentral, SimpleScenario){
    mock().expectOneCall("productionCode").andReturnValue(10);
    printf("productionCode %d\n", productionCode());
    mock().checkExpectations();
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

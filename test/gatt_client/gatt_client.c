
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
#include "ble_client.h"
#include "att.h"
#include "profile.h"

static bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b};
static le_peripheral_t test_device;

static int result_index;
static le_service_t services[50];
 
static uint8_t advertisement_received;
static uint8_t connected;
static uint8_t primary_services_found;

void mock_simulate_hci_state_working();
void mock_simulate_command_complete(const hci_cmd_t *cmd);
void mock_simulate_scan_response();
void mock_simulate_connected();
void mock_simulate_exchange_mtu();
void mock_simulate_discover_primary_services_response();


void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

static void verify_advertisement(ad_event_t * e){
    CHECK_EQUAL(0, e->event_type);
 	CHECK_EQUAL(0, e->address_type);
	CHECK_EQUAL(188, e->rssi);
	CHECK_EQUAL(3, e->length);
	CHECK_EQUAL_ARRAY((uint8_t *)test_device_addr, (uint8_t *)e->address, 6);
}

static void verify_primary_services(){
	CHECK_EQUAL(6, result_index);

	uint8_t uuids[6][16] = {
		{0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0x18, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb},
		{0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}
	};

	int i;
	for (i=0; i<result_index; i++){
		CHECK_EQUAL_ARRAY(uuids[i], services[i].uuid128, 16);
	}
}

static void handle_le_central_event(le_central_event_t * event){
	switch(event->type){
		case GATT_ADVERTISEMENT:
			advertisement_received = 1;
			verify_advertisement((ad_event_t *) event);
			break;
		case GATT_CONNECTION_COMPLETE:
			printf("GATT_CONNECTION_COMPLETE\n");
			connected = 1;
			break;
		case GATT_SERVICE_QUERY_RESULT:
            services[result_index++] = ((le_service_event_t *) event)->service;
            break;
        case GATT_SERVICE_QUERY_COMPLETE:
            verify_primary_services();
            primary_services_found = 1;
            break;
		default:
			printf("handle_le_central_event");
			break;
	}
}

TEST_GROUP(GATTClient){
	int acl_buffer_size;
    uint8_t acl_buffer[27];

	void setup(){
		advertisement_received = 0;
		connected = 0;
		primary_services_found = 0;
		result_index = 0;
		le_central_init();
		le_central_register_handler(handle_le_central_event);
		mock_simulate_hci_state_working();
		att_set_db(profile_data);
	}
};

// TEST(GATTClient, TestScanning){
// 	le_central_start_scan();
// 	mock_simulate_command_complete(&hci_le_set_scan_enable);
// 	mock_simulate_scan_response();
// 	CHECK(advertisement_received);
// }

// TEST(GATTClient, TestConnecting){
// 	le_central_connect(&test_device, 1, test_device_addr);
// 	mock_simulate_connected();
// 	mock_simulate_exchange_mtu();
// 	CHECK(connected);
// }

TEST(GATTClient, TestDiscoverPrimaryServices){
	le_central_connect(&test_device, 1, test_device_addr);
	mock_simulate_connected();
	mock_simulate_exchange_mtu();
	CHECK(connected);

	le_central_discover_primary_services(&test_device);
	CHECK(primary_services_found);
}





int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

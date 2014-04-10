
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

static bd_addr_t test_device_addr = {0x61, 0x48, 0x80, 0xe6, 0xa1, 0x71};

void mock_simulate_hci_state_working();

void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
	int i;
	for (i=0; i<size; i++){
		BYTES_EQUAL(expected[i], actual[i]);
	}
}

static void verify_advertisement(ad_event_t * e){
    CHECK_EQUAL(0, e->event_type);
 	CHECK_EQUAL(1, e->address_type);
	CHECK_EQUAL(183, e->rssi);
	CHECK_EQUAL(3, e->length);
	CHECK_EQUAL_ARRAY((uint8_t *)test_device_addr, (uint8_t *)e->address, 6);
}

uint8_t advertisement_received;

static void handle_le_central_event(le_central_event_t * event){
	switch(event->type){
		case GATT_ADVERTISEMENT:
			advertisement_received = 1;
			verify_advertisement((ad_event_t *) event);
			break;
		default:
			break;
	}
}


TEST_GROUP(GATTClient){
	void setup(){
		advertisement_received = 0;
		le_central_init();
		le_central_register_handler(handle_le_central_event);
		mock_simulate_hci_state_working();
	}
};


TEST(GATTClient, TestScanning){
	le_central_start_scan();
	CHECK(advertisement_received);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}


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


static void handle_scan_and_connect(le_central_event_t * event){
    if (event->type != GATT_ADVERTISEMENT) return;
    printf("test client - SCAN ACTIVE\n");
    ad_event_t * e = (ad_event_t*) event;
    printf("    *** adv. event *** evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type, 
                            e->address_type, bd_addr_to_str(e->address), e->rssi, e->length); 
    // hexdump2( e->data, e->length);   

    // dump_ad_event(ad_event);
}

static void handle_le_central_event(le_central_event_t * event){
	handle_scan_and_connect(event);
}

TEST_GROUP(GATTClient){
	uint8_t advertisement_received;

	void setup(){
		advertisement_received = 0;
		le_central_init();
		le_central_register_handler(handle_le_central_event);
	}
};


TEST(GATTClient, TestScanning){
	le_central_start_scan();
	CHECK(advertisement_received);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

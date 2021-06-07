
// *****************************************************************************
//
// test Battery Service Client
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include "hci.h"
#include "btstack_util.h"
#include "bluetooth.h"
#include "bluetooth_gatt.h"

#include "ble/gatt-service/battery_service_client.h"
#include "mock_gatt_client.h"

static const hci_con_handle_t con_handle = 0x01;

// simulate  btstack_memory_battery_service_client_get

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

}

TEST_GROUP(BATTERY_SERVICE_CLIENT){ 
    uint16_t battery_service_cid;

    void setup(void){
        uint32_t poll_interval_ms = 2000;
        battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);

    }

    void teardown(){
        // mock().clear();
    }
};

TEST(BATTERY_SERVICE_CLIENT, lookup_attribute_handles){
    
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

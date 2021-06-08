
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
static bool connected;
// temp btstack run loop mock

static btstack_timer_source_t * btstack_timer = NULL;

void btstack_run_lopo_deinit(void){
        btstack_timer = NULL;
}

void btstack_run_loop_add_timer(btstack_timer_source_t * timer){
    btstack_timer = timer;
}

void * btstack_run_loop_get_timer_context(btstack_timer_source_t * timer){
    return btstack_timer;
}

int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){
    btstack_timer = NULL;
    return 1;
}

void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t timeout_in_ms){
}

void btstack_run_loop_set_timer_context(btstack_timer_source_t * timer, void * context){
}

void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*process)(btstack_timer_source_t * _timer)){
}

// simulate  btstack_memory_battery_service_client_get

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    // handle GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED
    // set flags
}

TEST_GROUP(BATTERY_SERVICE_CLIENT){ 
    uint16_t battery_service_cid;
    uint32_t poll_interval_ms;
        
    void setup(void){
        battery_service_cid = 1;
        connected = false;
        poll_interval_ms = 2000;
        
        mock_gatt_client_reset();
        battery_service_client_init();
    }

    void setup_service(bool add_characteristics, bool add_descriptors){
        mock_gatt_client_add_primary_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        if (!add_characteristics) return;

        mock_gatt_client_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
        
        if (!add_descriptors) return;
        mock_gatt_client_add_characteristic_descriptor_uuid16(ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION);
    }

    void teardown(void){
        battery_service_client_deinit();
    }
};

TEST(BATTERY_SERVICE_CLIENT, connect_no_service){
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    mock_gatt_client_run();
    CHECK_EQUAL(false, connected);
}


TEST(BATTERY_SERVICE_CLIENT, connect_with_service){
    setup_service(true, true);
    
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    mock_gatt_client_run();

    CHECK_EQUAL(false, connected);
}


#if 0
TEST(BATTERY_SERVICE_CLIENT, disconnect){
    uint8_t status;

    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
    
    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = battery_service_client_disconnect(battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(BATTERY_SERVICE_CLIENT, read_battery_level){
    uint8_t status = battery_service_client_connect(con_handle, &gatt_client_event_handler, poll_interval_ms, &battery_service_cid);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = battery_service_client_read_battery_level(battery_service_cid + 1, 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}
#endif

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

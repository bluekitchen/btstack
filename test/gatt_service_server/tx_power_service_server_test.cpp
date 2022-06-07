
// *****************************************************************************
//
// test battery service
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

#include "ble/gatt-service/tx_power_service_server.h"
#include "tx_power_service_server_test.h"
#include "mock_att_server.h"

static int8_t tx_power_level = 100;

TEST_GROUP(TX_POWER_SERVICE_SERVER){ 
    att_service_handler_t * service; 
    uint16_t con_handle;
    uint16_t tx_power_level_value_handle;

    void setup(void){
        // setup database
        att_set_db(profile_data);
        tx_power_level_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_TX_POWER_LEVEL);

        // setup tx power service
        tx_power_service_server_init(tx_power_level);

        service = mock_att_server_get_service();
        con_handle = 0x00;
    }

    void teardown(){
        mock_deinit();
    }
};

TEST(TX_POWER_SERVICE_SERVER, lookup_attribute_handles){
    CHECK(tx_power_level_value_handle != 0);
}


TEST(TX_POWER_SERVICE_SERVER, read_tx_power_level_value){
    uint8_t response[1];
    uint16_t response_len;

    // invalid attribute handle
    response_len = mock_att_service_read_callback(con_handle, 0xffff, 0xffff, response, sizeof(response));
    CHECK_EQUAL(0, response_len);

    response_len = mock_att_service_read_callback(con_handle, tx_power_level_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(1, response_len);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}


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

#include "ble/gatt-service/battery_service_server.h"
#include "battery_service_server_test.h"
#include "mock_att_server.h"

static uint8_t battery_level = 100;

TEST_GROUP(BATTERY_SERVICE_SERVER){ 
    att_service_handler_t * service; 
    uint16_t con_handle;
    uint16_t battery_value_handle_value;
    uint16_t battery_value_handle_client_configuration;

    void setup(void){
        // setup database
        att_set_db(profile_data);
        battery_value_handle_value = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
        battery_value_handle_client_configuration = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);

        // setup battery service
        battery_service_server_init(battery_level);

        service = mock_att_server_get_service();
        con_handle = 0x00;
    }

    void teardown(){
        mock_deinit();
    }
};

TEST(BATTERY_SERVICE_SERVER, set_battery_value_trigger_can_send_now){
    // enable notifications
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    mock_att_service_write_callback(con_handle, battery_value_handle_client_configuration, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));

    // set battery to trigger notification
    mock().expectOneCall("att_server_register_can_send_now_callback");
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();
    mock().expectOneCall("att_server_notify");
    mock_att_service_trigger_can_send_now();
    mock().checkExpectations();
}

TEST(BATTERY_SERVICE_SERVER, lookup_attribute_handles){
    CHECK(battery_value_handle_value != 0);
    CHECK(battery_value_handle_client_configuration != 0);
}


TEST(BATTERY_SERVICE_SERVER, set_battery_value){
    // battery_value_handle_client_configuration not set
    mock().expectNCalls(con_handle, "att_server_register_can_send_now_callback");
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();

    // battery_value_handle_client_configuration set
    mock().expectOneCall("att_server_register_can_send_now_callback");
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    mock_att_service_write_callback(con_handle, battery_value_handle_client_configuration, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();
}

TEST(BATTERY_SERVICE_SERVER, set_wrong_handle_battery_value){
    // battery_value_handle_client_configuration not set
    mock().expectNCalls(con_handle, "att_server_register_can_send_now_callback");
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();

    // battery_value_handle_client_configuration set
    const uint8_t enable_notify[]= { 0x1, 0x0 };
    
    mock().expectNoCall("att_server_register_can_send_now_callback");
    mock_att_service_write_callback(con_handle, battery_value_handle_client_configuration + 10, ATT_TRANSACTION_MODE_NONE, 0, enable_notify, sizeof(enable_notify));
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();

    mock().expectNoCall("att_server_register_can_send_now_callback");
    mock_att_service_write_callback(con_handle, battery_value_handle_client_configuration + 10, ATT_TRANSACTION_MODE_VALIDATE, 0, enable_notify, sizeof(enable_notify));
    battery_service_server_set_battery_value(60);
    mock().checkExpectations();
}



TEST(BATTERY_SERVICE_SERVER, read_battery_value){
    uint8_t response[2];
    uint16_t response_len;

    // invalid attribute handle
    response_len = mock_att_service_read_callback(con_handle, 0xffff, 0xffff, response, sizeof(response));
    CHECK_EQUAL(0, response_len);

    response_len = mock_att_service_read_callback(con_handle, battery_value_handle_value, 0, response, sizeof(response));
    CHECK_EQUAL(1, response_len);

    response_len = mock_att_service_read_callback(con_handle, battery_value_handle_client_configuration, 0, response, sizeof(response));
    CHECK_EQUAL(2, response_len);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

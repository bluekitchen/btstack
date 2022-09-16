
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

#include "ble/gatt-service/heart_rate_service_server.h"
#include "heart_rate_service_server_test.h"
#include "mock_att_server.h"


TEST_GROUP(HEART_RATE_SERVER){ 
    att_service_handler_t * service; 
    uint16_t con_handle;
    heart_rate_service_body_sensor_location_t location;
    int energy_expended_supported;
    uint16_t body_sensor_location_value_handle;
    uint16_t heart_rate_measurement_configuration_handle;

    void setup(void){
        // setup database
        att_set_db(profile_data);
        
        body_sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xFFFF, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
        heart_rate_measurement_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xFFFF, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT);

        energy_expended_supported = 1;
        location = HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_OTHER;

        // setup heart rate service
        heart_rate_service_server_init(location, energy_expended_supported);

        service = mock_att_server_get_service();
        con_handle = 0x00;
    }

    void teardown(){
        mock_deinit();
    }
};


TEST(HEART_RATE_SERVER, read_callback_test){
    uint8_t response[2];
    uint16_t response_len;

    // invalid attribute handle
    response_len = mock_att_service_read_callback(con_handle, 0xffff, 0xffff, response, sizeof(response));
    CHECK_EQUAL(0, response_len);

    // get location value length (1 byte length)
    response_len = mock_att_service_read_callback(con_handle, body_sensor_location_value_handle, 0, NULL, 0);
    CHECK_EQUAL(1, response_len);

    // get location value (1 byte length)
    response_len = mock_att_service_read_callback(con_handle, body_sensor_location_value_handle, 0, response, sizeof(response));
    CHECK_EQUAL(1, response_len);
    
    // get rate measurement configuration length (2 byte length)
    response_len = mock_att_service_read_callback(con_handle, heart_rate_measurement_configuration_handle, 0, NULL, 0);
    CHECK_EQUAL(2, response_len);

    // get rate measurement configuration (2 byte length)
    response_len = mock_att_service_read_callback(con_handle, heart_rate_measurement_configuration_handle, 0, response, sizeof(response));
    CHECK_EQUAL(2, response_len);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

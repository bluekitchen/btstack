
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

#include "hci.h"
#include "ble/att_db.h"
#include "ble/att_db_util.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth.h"

#include "bluetooth_gatt.h"

static uint8_t battery_level = 100;

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);

    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    
    return 0;
}

TEST_GROUP(ATT_SERVER){

    void setup(void){
                // init att db util and add a service and characteristic
        att_db_util_init();
        // 0x180F
        att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        // 0x2A19
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,       ATT_PROPERTY_WRITE | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A1B
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATE, ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A1A 
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
        // 0x2A49 
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_FEATURE, ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A35
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_MEASUREMENT, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
        // 0x2A38
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);

        const uint8_t uuid128[] = {0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
        att_db_util_add_characteristic_uuid128(uuid128, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        
        // 0x2AA7
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_MEASUREMENT, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
        
        // 0x2AAB
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_RUN_TIME, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);

        // setup ATT server
        att_server_init(att_db_util_get_address(), att_read_callback, att_write_callback);
    }
};

TEST(ATT_SERVER, test){
    
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}


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
#include "btstack_tlv.h"
#include "mock_btstack_tlv.h"

#include "bluetooth_gatt.h"

static uint8_t battery_level = 100;
static const uint8_t uuid128_with_bluetooth_base[] = { 0x00, 0x00, 0xBB, 0xBB, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static const uint8_t uuid128_no_bluetooth_base[] =   { 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAA, 0xAA, 0x00, 0x00 };

extern "C" void l2cap_can_send_fixed_channel_packet_now_set_status(uint8_t status);
extern "C" void mock_call_att_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
extern "C" void att_init_connection(uint16_t con_handle);

static uint8_t att_request[255];
static uint16_t att_write_request(uint16_t request_type, uint16_t attribute_handle, uint16_t value_length, const uint8_t * value){
    att_request[0] = request_type;
    little_endian_store_16(att_request, 1, attribute_handle);
    (void)memcpy(&att_request[3], value, value_length);
    return 3 + value_length;
}

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

static void att_client_indication_callback(void * context){
}

TEST_GROUP(ATT_SERVER){ 
    uint16_t att_con_handle;
    mock_btstack_tlv_t tlv_context;
    const btstack_tlv_t * tlv_impl;

    void setup(void){
        att_con_handle = 0x00;

        att_init_connection(att_con_handle);
        tlv_impl = mock_btstack_tlv_init_instance(&tlv_context);
        btstack_tlv_set_instance(tlv_impl, &tlv_context);

        l2cap_can_send_fixed_channel_packet_now_set_status(1);

        // init att db util and add a service and characteristic
        att_db_util_init();
        // 0x180F
        att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        // 0x2A19
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,       ATT_PROPERTY_WRITE | ATT_PROPERTY_READ | ATT_PROPERTY_INDICATE, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A1B
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATE, ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A1A 
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
        // 0x2A49 
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_FEATURE, ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A35
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_MEASUREMENT, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_AUTHENTICATED, ATT_SECURITY_AUTHENTICATED, &battery_level, 1);
        
        
        att_db_util_add_characteristic_uuid128(uuid128_no_bluetooth_base, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A38btstack_tlv_set_instance
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);

        
        att_db_util_add_characteristic_uuid128(uuid128_with_bluetooth_base, ATT_PROPERTY_WRITE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2AAB
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_RUN_TIME, ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // 0x2A5C
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE, ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE | ATT_PROPERTY_DYNAMIC, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        // setup ATT server
        att_server_init(att_db_util_get_address(), att_read_callback, att_write_callback);
    }

    void teardown(void) {
        mock_btstack_tlv_deinit(&tlv_context);
    }
};


TEST(ATT_SERVER, gatt_server_get_value_handle_for_characteristic_with_uuid16){
    // att_dump_attributes();
    uint16_t value_handle;

    // start handle > value handle
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0xf000, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    CHECK_EQUAL(0, value_handle);

    // end handle < value handle
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0x02, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    CHECK_EQUAL(0, value_handle);

    // search value handle for unknown UUID
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, 0xffff);
    CHECK_EQUAL(0, value_handle);
 
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, 0);
    CHECK_EQUAL(0, value_handle);

    // search value handle after one with uuid128_no_bluetooth_base
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
    CHECK_EQUAL(0x0014, value_handle);

    // search value handle after one with uuid128_with_bluetooth_base
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_RUN_TIME);
    CHECK_EQUAL(0x001a, value_handle);

    // search value handle registered with bluetooth_base
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, 0xbbbb);
    CHECK_EQUAL(0x0017, value_handle);

    // correct read
    value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    CHECK_EQUAL(0x03, value_handle);
}


TEST(ATT_SERVER, att_server_indicate){
    static uint8_t value[] = {0x55};
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t status;

    // invalid connection handle
    status = att_server_indicate(0x50, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    // L2CAP cannot send
    l2cap_can_send_fixed_channel_packet_now_set_status(0);
    status = att_server_indicate(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(BTSTACK_ACL_BUFFERS_FULL, status);
    l2cap_can_send_fixed_channel_packet_now_set_status(1);

    // correct command
    status = att_server_indicate(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    // already in progress
    status = att_server_indicate(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS, status);
}

TEST(ATT_SERVER, att_server_notify){
    static uint8_t value[] = {0x55};
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t status;

    // invalid connection handle
    status = att_server_notify(0x50, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    // L2CAP cannot send
    l2cap_can_send_fixed_channel_packet_now_set_status(0);
    status = att_server_notify(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(BTSTACK_ACL_BUFFERS_FULL, status);
    l2cap_can_send_fixed_channel_packet_now_set_status(1);

    // correct command
    status = att_server_notify(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);
}

TEST(ATT_SERVER, att_server_get_mtu){
    // invalid connection handle
    uint8_t mtu = att_server_get_mtu(0x50);
    CHECK_EQUAL(0, mtu);

    mtu = att_server_get_mtu(att_con_handle);
    CHECK_EQUAL(23, mtu);
}

TEST(ATT_SERVER, att_server_request_can_send_now_event){
    att_server_request_can_send_now_event(att_con_handle);

}

TEST(ATT_SERVER, att_server_can_send_packet_now){
    int status = att_server_can_send_packet_now(att_con_handle);
    CHECK_EQUAL(1, status);

    status = att_server_can_send_packet_now(0x50);
    CHECK_EQUAL(0, status);
}

static btstack_context_callback_registration_t indication_callback;

TEST(ATT_SERVER, att_server_request_to_send_indication){

    indication_callback.callback = &att_client_indication_callback;
    int status = att_server_request_to_send_indication(&indication_callback, 0x55);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    l2cap_can_send_fixed_channel_packet_now_set_status(0);

    status = att_server_request_to_send_indication(&indication_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = att_server_request_to_send_indication(&indication_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);
}

TEST(ATT_SERVER, opcode_ATT_WRITE_REQUEST){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t buffer[] = {1, 0};
    uint16_t att_request_len = att_write_request(ATT_WRITE_REQUEST, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

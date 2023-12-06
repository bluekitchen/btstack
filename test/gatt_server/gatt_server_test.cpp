
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
extern "C" void hci_setup_le_connection(uint16_t con_handle);
extern "C" void mock_call_att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
extern "C" void mock_l2cap_set_max_mtu(uint16_t mtu);
extern "C" void hci_setup_classic_connection(uint16_t con_handle);
extern "C" void set_cmac_ready(int ready);

static uint8_t att_request[255];
static uint16_t att_write_request(uint16_t request_type, uint16_t attribute_handle, uint16_t value_length, const uint8_t * value){
    att_request[0] = request_type;
    little_endian_store_16(att_request, 1, attribute_handle);
    (void)memcpy(&att_request[3], value, value_length);
    return 3 + value_length;
}

static uint16_t att_read_request(uint16_t request_type, uint16_t attribute_handle){
    att_request[0] = request_type;
    little_endian_store_16(att_request, 1, attribute_handle);
    return 3;
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
static void att_client_notification_callback(void * context){
}
static void att_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
}


TEST_GROUP(ATT_SERVER){ 
    uint16_t att_con_handle;
    mock_btstack_tlv_t tlv_context;
    const btstack_tlv_t * tlv_impl;
    btstack_context_callback_registration_t indication_callback;
    btstack_context_callback_registration_t notification_callback;

    void setup(void){
        att_con_handle = 0x01;

        hci_setup_le_connection(att_con_handle);

        tlv_impl = mock_btstack_tlv_init_instance(&tlv_context);
        btstack_tlv_set_instance(tlv_impl, &tlv_context);

        l2cap_can_send_fixed_channel_packet_now_set_status(1);
        indication_callback.callback = &att_client_indication_callback;
        notification_callback.callback = &att_client_notification_callback;

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
        att_server_deinit();
        hci_deinit();
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

TEST(ATT_SERVER, att_server_request_to_send_indication){
    int status = att_server_request_to_send_indication(&indication_callback, 0x55);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    l2cap_can_send_fixed_channel_packet_now_set_status(0);

    status = att_server_request_to_send_indication(&indication_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = att_server_request_to_send_indication(&indication_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);
}

TEST(ATT_SERVER, att_server_request_to_send_notification){
    int status = att_server_request_to_send_notification(&notification_callback, 0x55);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    l2cap_can_send_fixed_channel_packet_now_set_status(0);

    status = att_server_request_to_send_notification(&notification_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    status = att_server_request_to_send_notification(&notification_callback, att_con_handle);
    CHECK_EQUAL(ERROR_CODE_COMMAND_DISALLOWED, status);
}

TEST(ATT_SERVER, att_server_register_can_send_now_callback){
    int status = att_server_register_can_send_now_callback(&notification_callback, 0x55);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);

    att_server_register_packet_handler(&att_event_packet_handler);
}

TEST(ATT_SERVER, att_packet_handler_ATT_DATA_PACKET){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t buffer[] = {1, 0};

    uint16_t att_request_len = att_write_request(ATT_WRITE_REQUEST, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_ATT_DATA_PACKET1){
    uint16_t value_handle;
    uint16_t att_request_len;

    value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    att_request_len = att_read_request(ATT_READ_REQUEST, value_handle);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_invalid_opcode){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE);
    uint8_t  buffer[] = {1, 0};

    uint16_t att_request_len = att_write_request(0xFF, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_ATT_HANDLE_VALUE_CONFIRMATION){
    hci_setup_le_connection(att_con_handle);
    static uint8_t value[] = {0x55};
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    l2cap_can_send_fixed_channel_packet_now_set_status(1);
    // correct command
    uint8_t status = att_server_indicate(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    static uint8_t buffer[0];
    uint16_t att_request_len = att_write_request(ATT_HANDLE_VALUE_CONFIRMATION, value_handle, 0, buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_ATT_WRITE_COMMAND){
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    static uint8_t buffer[] = {0x55};
    uint16_t att_request_len = att_write_request(ATT_WRITE_COMMAND, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_ATT_WRITE_COMMAND_wrong_buffer_size){
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    static uint8_t buffer[] = {0x55};
    att_write_request(ATT_SIGNED_WRITE_COMMAND, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], ATT_REQUEST_BUFFER_SIZE + 100);
}

TEST(ATT_SERVER, att_packet_handler_ATT_WRITE_COMMAND_signed_write_confirmation){
    hci_setup_le_connection(att_con_handle);

    uint8_t signed_write_value[] = {0x12};
    uint8_t hash[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint16_t signed_write_characteristic_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE);
    
    uint8_t att_request[20];
    int value_length = sizeof(signed_write_value);
    att_request[0] = ATT_SIGNED_WRITE_COMMAND;
    little_endian_store_16(att_request, 1, signed_write_characteristic_handle);
    memcpy(&att_request[3], signed_write_value, value_length);
    little_endian_store_32(att_request, 3 + value_length, 0);
    reverse_64(hash, &att_request[3 + value_length + 4]);

    // wrong size
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], value_length + 12u);

    set_cmac_ready(0);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], value_length + 12u + 3u);

    set_cmac_ready(1);
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], value_length + 12u + 3u);

    // set ir_lookup_active = 1
    uint8_t buffer[20];
    buffer[0] = SM_EVENT_IDENTITY_RESOLVING_STARTED;
    buffer[1] = 9;
    little_endian_store_16(buffer, 2, att_con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], 11);
    
    mock_call_att_server_packet_handler(ATT_DATA_PACKET, att_con_handle, &att_request[0], value_length + 12u + 3u);

    buffer[0] = SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED;
    buffer[1] = 18; // H1B1B2
    little_endian_store_16(buffer, 18, 0);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], 11);
}


TEST(ATT_SERVER, att_packet_handler_ATT_UNKNOWN_PACKET){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t buffer[] = {1, 0};

    uint16_t att_request_len = att_write_request(ATT_WRITE_REQUEST, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(0xFF, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_HCI_EVENT_PACKET_L2CAP_EVENT_CAN_SEND_NOW){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    // local cid
    uint8_t buffer[] = {1, 0};

    uint16_t att_request_len = att_write_request(L2CAP_EVENT_CAN_SEND_NOW, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(HCI_EVENT_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_HCI_EVENT_PACKET_ATT_EVENT_MTU_EXCHANGE_COMPLETE){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t buffer[4];
    
    // invalid con handle
    memset(buffer, 0, sizeof(buffer));
    little_endian_store_16(buffer, 2, 256);
    
    uint16_t att_request_len = att_write_request(ATT_EVENT_MTU_EXCHANGE_COMPLETE, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(HCI_EVENT_PACKET, 0, &att_request[0], att_request_len);

    little_endian_store_16(buffer, 0, att_con_handle);
    att_request_len = att_write_request(ATT_EVENT_MTU_EXCHANGE_COMPLETE, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(HCI_EVENT_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, att_packet_handler_HCI_EVENT_PACKET_ATT_EVENT_UNKNOWN){
    uint16_t value_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    uint8_t buffer[] = {1, 0};

    // unknown subevent
    uint16_t att_request_len = att_write_request(0x7bu, value_handle, sizeof(buffer), buffer);
    mock_call_att_server_packet_handler(HCI_EVENT_PACKET, att_con_handle, &att_request[0], att_request_len);
}

TEST(ATT_SERVER, connection_complete_event){
    uint8_t buffer[21];
    buffer[0] = HCI_EVENT_LE_META;
    buffer[1] = 19;
    buffer[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;

    // call with wrong con_handle
    little_endian_store_16(buffer,4,HCI_CON_HANDLE_INVALID);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));

    // call with correct con_handle
    little_endian_store_16(buffer,4,att_con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));

    // use max MTU > ATT_REQUEST_BUFFER_SIZE
    mock_l2cap_set_max_mtu(ATT_REQUEST_BUFFER_SIZE + 10);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));

    // wrong subevent
    buffer[2] = 0xFF;
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));
}

TEST(ATT_SERVER, unknown_packet_type){
    uint8_t buffer[21];
    buffer[0] = HCI_EVENT_LE_META;
    buffer[1] = 1;
    buffer[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;

    mock_call_att_packet_handler(0xFF, 0, &buffer[0], sizeof(buffer));
}


TEST(ATT_SERVER, connection_disconnect_complete_event) {
    uint8_t buffer[6];
    buffer[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    buffer[1] = 19;
    buffer[2] = 0;

    // call with wrong con_handle
    hci_setup_le_connection(att_con_handle);
    little_endian_store_16(buffer, 3, HCI_CON_HANDLE_INVALID);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));

    // call with correct con_handle, classic connection
    hci_setup_classic_connection(att_con_handle);
    little_endian_store_16(buffer, 3, att_con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));

    // call with correct con_handle, le connection
    hci_setup_le_connection(att_con_handle);
    little_endian_store_16(buffer, 3, att_con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));


    hci_setup_le_connection(att_con_handle);
    static uint8_t value[] = {0x55};
    uint16_t value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    l2cap_can_send_fixed_channel_packet_now_set_status(1);
    // correct command
    uint8_t status = att_server_indicate(att_con_handle, value_handle, &value[0], 0);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], sizeof(buffer));
}

static void test_hci_event_encryption_events(hci_con_handle_t con_handle, uint8_t con_handle_index, uint8_t * buffer, uint16_t buffer_size){
    // call with wrong con_handle
    hci_setup_le_connection(con_handle);
    little_endian_store_16(buffer, con_handle_index, HCI_CON_HANDLE_INVALID);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], buffer_size);

    // call with correct con_handle, classic connection
    hci_setup_classic_connection(con_handle);
    little_endian_store_16(buffer, con_handle_index, con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], buffer_size);

    // call with correct con_handle, le connection
    hci_setup_le_connection(con_handle);
    little_endian_store_16(buffer, con_handle_index, con_handle);
    mock_call_att_packet_handler(HCI_EVENT_PACKET, 0, &buffer[0], buffer_size);
}

TEST(ATT_SERVER, att_server_multiple_notify) {

    const uint16_t attribute_handles[] = {gatt_server_get_value_handle_for_characteristic_with_uuid16(0, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL)};
    const uint16_t value_lens[] = { 1};
    static uint8_t battery_value[] = {0x55};
    const uint8_t * value_data[]  = {battery_value };
    uint8_t num_attributes = 1;

    hci_setup_le_connection(att_con_handle);
    // correct command
    uint8_t status = att_server_multiple_notify(att_con_handle, 1, attribute_handles, value_data, value_lens);
    CHECK_EQUAL(ERROR_CODE_SUCCESS, status);

    // invalid con handle
    status = att_server_multiple_notify(HCI_CON_HANDLE_INVALID, 1, attribute_handles, value_data, value_lens);
    CHECK_EQUAL(ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, status);
}

TEST(ATT_SERVER, hci_event_encryption_key_refresh_complete_event) {
    uint8_t buffer[5];
    buffer[0] = HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE;
    buffer[1] = 3;
    buffer[2] = 0;
    // con_handle (2)
    test_hci_event_encryption_events(att_con_handle, 3, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, hci_event_encryption_change_v2_event) {
    uint8_t buffer[7];
    buffer[0] = HCI_EVENT_ENCRYPTION_CHANGE_V2;
    buffer[1] = 5;
    buffer[2] = 0;
    // con_handle (2)
    // encryption_key_size
    buffer[6] = 1;
    // encryption_enabled (1)
    buffer[5] = 0;
    test_hci_event_encryption_events(att_con_handle, 3, buffer, sizeof(buffer));

    // encryption_enabled (1)
    buffer[5] = 1;
    test_hci_event_encryption_events(att_con_handle, 3, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, hci_event_encryption_change_event) {
    uint8_t buffer[6];
    buffer[0] = HCI_EVENT_ENCRYPTION_CHANGE;
    buffer[1] = 4;
    buffer[2] = 0;
    // con_handle (2)att_packet_handler_ATT_UNKNOWN_PACKET

    // encryption_enabled (1)
    buffer[5] = 0;
    test_hci_event_encryption_events(att_con_handle, 3, buffer, sizeof(buffer));

    // encryption_enabled (1)
    buffer[5] = 1;
    test_hci_event_encryption_events(att_con_handle, 3, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_identity_resolving_started_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_IDENTITY_RESOLVING_STARTED;
    buffer[1] = 9;

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_identity_resolving_succeeded_event) {
    uint8_t buffer[20];
    buffer[0] = SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED;
    buffer[1] = 18; // H1B1B2
    little_endian_store_16(buffer, 18, 0);
    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_identity_resolving_failed_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_IDENTITY_RESOLVING_FAILED;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

// 

TEST(ATT_SERVER, sm_event_just_works_request_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_JUST_WORKS_REQUEST;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_passkey_display_number_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_PASSKEY_DISPLAY_NUMBER;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_passkey_input_number_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_PASSKEY_INPUT_NUMBER;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_numeric_comparison_request_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_NUMERIC_COMPARISON_REQUEST;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_identity_created_event) {
    uint8_t buffer[20];
    buffer[0] = SM_EVENT_IDENTITY_CREATED;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_pairing_complete_event) {
    uint8_t buffer[11];
    buffer[0] = SM_EVENT_PAIRING_COMPLETE;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, sm_event_authorization_result_event) {
    uint8_t buffer[12];
    buffer[0] = SM_EVENT_AUTHORIZATION_RESULT;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, unknown_event) {
    uint8_t buffer[11];
    buffer[0] = 0xFF;
    buffer[1] = 9; 

    test_hci_event_encryption_events(att_con_handle, 2, buffer, sizeof(buffer));
}

TEST(ATT_SERVER, att_server_register_service_handler) {
    att_service_handler_t test_service;
    test_service.read_callback = att_read_callback;
    test_service.write_callback = att_write_callback;

    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE, &start_handle, &end_handle);
    CHECK_EQUAL(service_found, 1);
    
    test_service.start_handle   = start_handle;
    test_service.end_handle     = end_handle;
    att_server_register_service_handler(&test_service);

    test_service.start_handle   = start_handle;
    test_service.end_handle     = 0;
    att_server_register_service_handler(&test_service);

    test_service.start_handle   = 0;
    test_service.end_handle     = end_handle;
    att_server_register_service_handler(&test_service);

    test_service.start_handle   = 0;
    test_service.end_handle     = 0;
    att_server_register_service_handler(&test_service);
}   

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

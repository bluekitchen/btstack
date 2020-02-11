#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "ble/att_db.h"
#include "ble/att_db_util.h"
#include "bluetooth_gatt.h"

static uint8_t battery_level = 100;

static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    return 0;
}  

static int att_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (initialized == 0){
        initialized = 1;
        // setup empty db
        att_db_util_init();
        // setup att_db
        att_db_util_add_service_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
        att_db_util_add_characteristic_uuid16(ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, ATT_PROPERTY_READ | ATT_PROPERTY_NOTIFY, ATT_SECURITY_NONE, ATT_SECURITY_NONE, &battery_level, 1);
        att_set_read_callback(&att_read_callback);
        att_set_write_callback(&att_write_callback); 
        uint8_t * att_db = att_db_util_get_address();
        att_set_db(att_db);
    }

    // setup att_connection
    att_connection_t att_connection = { 0 };
    att_connection.max_mtu = 1000;
    att_connection.mtu = ATT_DEFAULT_MTU;
    uint8_t att_response[1000];
    uint16_t att_request_len = size;
    const uint8_t * att_request = data;
    uint16_t att_respnose_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
    return 0;
}

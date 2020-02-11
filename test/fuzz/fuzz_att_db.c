#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <ble/att_db.h>
#include <ble/att_db_util.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (initialized == 0){
        initialized = 1;
        // setup empty db
        att_db_util_init();
        uint8_t * att_db = att_db_util_get_address();
        // setup att_db
        att_set_db(att_db);
    }

    // TODO: setup att_connection
    att_connection_t att_connection = { 0 };
    uint8_t att_response[1000];
    uint16_t att_request_len = size;
    const uint8_t * att_request = data;
    uint16_t att_respnose_len = att_handle_request(&att_connection, (uint8_t *) att_request, att_request_len, att_response);
    return 0;
}

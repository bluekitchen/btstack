#include <assert.h>
// Include the implementation to exercise the ATT callback and prepared-write state.
#include "ble/gatt-service/object_transfer_service_server.c"

uint8_t att_server_register_can_send_now_callback(btstack_context_callback_registration_t * registration, hci_con_handle_t handle){
    UNUSED(registration);
    UNUSED(handle);
    return ERROR_CODE_SUCCESS;
}
uint8_t att_server_indicate(hci_con_handle_t handle, uint16_t attribute, const uint8_t * value, uint16_t length){
    UNUSED(handle); UNUSED(attribute); UNUSED(value); UNUSED(length);
    assert(false);
    return ERROR_CODE_SUCCESS;
}
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){
    UNUSED(timer);
    return 0;
}
bool gatt_server_get_client_configuration_value(const uint8_t * buffer, uint16_t size, uint16_t * value){
    UNUSED(buffer); UNUSED(size); UNUSED(value);
    assert(false);
    return false;
}
uint8_t l2cap_request_can_send_now_event(uint16_t cid){
    UNUSED(cid);
    assert(false);
    return ERROR_CODE_SUCCESS;
}
int main(int argc, char ** argv){
    const char * test = argc > 1 ? argv[1] : "all";
    ots_server_connection_t connection = {0};
    ots_object_t object = {0};
    uint8_t data[128] = {0};
    connection.con_handle = 1;
    connection.current_object = &object;
    ots_connections = (btstack_linked_item_t *) &connection;
    for (unsigned i = 0; i < OTS_CHARACTERISTICS_NUM; i++){
        ots_characteristics[i].value_handle = 10 + i;
        ots_characteristics[i].client_configuration_handle = 100 + i;
    }
    uint16_t name = ots_characteristics[OTS_OBJECT_NAME_INDEX].value_handle;
    if ((strcmp(test, "all") == 0) || (strcmp(test, "name") == 0)){
        // A valid initial fragment makes subsequent remote offsets reachable.
        assert(ots_server_write_callback(1, name, ATT_TRANSACTION_MODE_ACTIVE, 0, data, 1) == 0);
        assert(ots_server_write_callback(1, name, ATT_TRANSACTION_MODE_ACTIVE, UINT16_MAX, data, 2) == ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED);
        assert(ots_server_write_callback(1, name, ATT_TRANSACTION_MODE_ACTIVE, MAX_SIZE_OTS_STRING - 1, data, 1) == ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED);
        data[0] = 'x';
        assert(ots_server_write_callback(1, name, ATT_TRANSACTION_MODE_ACTIVE, MAX_SIZE_OTS_STRING - 2, data, 1) == 0);
        assert(connection.long_write_data[MAX_SIZE_OTS_STRING - 2] == 'x');
        assert(connection.long_write_data[MAX_SIZE_OTS_STRING - 1] == 0);
    }
    if ((strcmp(test, "all") == 0) || (strcmp(test, "metadata") == 0)){
        connection.current_object = NULL;
        unsigned indices[] = {OTS_OBJECT_NAME_INDEX, OTS_OBJECT_PROPERTIES_INDEX, OTS_OBJECT_FIRST_CREATED_INDEX, OTS_OBJECT_LAST_MODIFIED_INDEX};
        unsigned lengths[] = {1,4,7,7};
        memset(data, 0, sizeof(data));
        for(unsigned i = 0; i < 4; i++){
            assert(ots_server_write_callback(1, ots_characteristics[indices[i]].value_handle, ATT_TRANSACTION_MODE_NONE, 0, data, lengths[i]) == ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED);
        }
        connection.current_object = &object;
        data[0] = 1;
        for(unsigned i = 1; i < 4; i++){
            assert(ots_server_write_callback(1, ots_characteristics[indices[i]].value_handle, ATT_TRANSACTION_MODE_NONE, 0, data, lengths[i]) == ATT_ERROR_SUCCESS);
        }
        assert(object.properties == 1);
        assert(object.first_created.year == 1);
        assert(object.last_modified.year == 1);
    }
    if ((strcmp(test, "all") == 0) || (strcmp(test, "filter") == 0)){
        assert(ots_server_filter_buffer_valid_write(data, 2, UINT16_MAX, &connection) == ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
        assert(ots_server_filter_buffer_valid_write(data, 1, MAX_SIZE_OTS_STRING, &connection) == ATT_ERROR_SUCCESS);
        assert(ots_server_filter_buffer_valid_write(data, 2, MAX_SIZE_OTS_STRING, &connection) == ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
    }
    return 0;
}

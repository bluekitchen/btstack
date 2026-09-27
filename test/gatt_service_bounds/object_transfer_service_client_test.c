#include <assert.h>
#include "ble/gatt-service/object_transfer_service_client.c"

static ots_client_connection_t connection;
static ots_client_connection_t other;
static btstack_context_callback_registration_t * queued_query;
static uint8_t query_status, write_status;
static bool timer_active, try_from_callback;
static uint8_t callback_status;
static uint8_t data[8];
static unsigned sends, sent, requests, responses, chunks, disconnects;

uint16_t gatt_service_client_get_connection_id(const gatt_service_client_connection_t * client){ return client->cid; }
uint8_t gatt_service_client_characteristic_index_for_value_handle(const gatt_service_client_connection_t * client, uint16_t handle){
    UNUSED(client);
    return handle - 1;
}
uint16_t gatt_service_client_characteristic_uuid16_for_index(const gatt_service_client_t * client, uint8_t index){
    UNUSED(client);
    return index == OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT ?
           ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT : ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT;
}
uint8_t gatt_client_request_to_send_gatt_query(btstack_context_callback_registration_t * request, hci_con_handle_t handle){
    UNUSED(handle);
    if (query_status == ERROR_CODE_SUCCESS){
        assert(queued_query == NULL);
        queued_query = request;
    }
    return query_status;
}
uint8_t gatt_client_remove_gatt_query(btstack_context_callback_registration_t * request, hci_con_handle_t handle){
    UNUSED(handle);
    if (queued_query == request) queued_query = NULL;
    return ERROR_CODE_SUCCESS;
}
uint8_t gatt_client_write_value_of_characteristic_with_context(btstack_packet_handler_t callback, hci_con_handle_t handle,
        uint16_t value_handle, uint16_t length, uint8_t * value, uint16_t service_id, uint16_t connection_id){
    UNUSED(callback); UNUSED(handle); UNUSED(value_handle); UNUSED(service_id); UNUSED(connection_id);
    assert(length > 0);
    assert(value[0] == connection.pending_control_point_opcode);
    return write_status;
}
uint8_t gatt_client_att_status_to_error_code(uint8_t status){ return status; }
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*process)(btstack_timer_source_t *)){ timer->process = process; }
void btstack_run_loop_set_timer_context(btstack_timer_source_t * timer, void * context){ timer->context = context; }
void * btstack_run_loop_get_timer_context(btstack_timer_source_t * timer){ return timer->context; }
void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t timeout){ UNUSED(timer); assert(timeout == 30000); }
void btstack_run_loop_add_timer(btstack_timer_source_t * timer){ UNUSED(timer); assert(!timer_active); timer_active = true; }
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){ UNUSED(timer); timer_active = false; return 1; }
uint8_t l2cap_disconnect(uint16_t cid){ assert(cid == 0x40); disconnects++; return ERROR_CODE_SUCCESS; }
uint8_t l2cap_send(uint16_t cid, const uint8_t * buffer, uint16_t length){
    assert(cid == 0x40);
    assert(buffer == data + sent);
    assert(length <= sizeof(data) - sent);
    for (unsigned i = 0; i < length; i++) assert(buffer[i] == sent + i);
    sends++;
    sent += length;
    return ERROR_CODE_SUCCESS;
}
uint8_t l2cap_request_can_send_now_event(uint16_t cid){ assert(cid == 0x40); requests++; return ERROR_CODE_SUCCESS; }

static void event_handler(uint8_t type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(channel); UNUSED(size);
    assert(type == HCI_EVENT_PACKET);
    if ((packet[2] == LEAUDIO_SUBEVENT_OTS_CLIENT_OACP_RESPONSE) || (packet[2] == LEAUDIO_SUBEVENT_OTS_CLIENT_OLCP_RESPONSE)) responses++;
    if (packet[2] == LEAUDIO_SUBEVENT_OTS_CLIENT_DATA_CHUNK) chunks++;
    if (try_from_callback){
        try_from_callback = false;
        callback_status = object_transfer_service_client_calculate_checksum(&connection, 0, 8);
    }
}
static void setup(void){
    memset(&connection, 0, sizeof(connection));
    memset(&other, 0, sizeof(other));
    ots_connections = NULL;
    connection.basic_connection.cid = 1;
    connection.basic_connection.con_handle = 0x10;
    connection.basic_connection.characteristics = connection.characteristics_storage;
    for (unsigned i = 0; i < OTS_CLIENT_CHARACTERISTICS_COUNT; i++) connection.characteristics_storage[i].value_handle = i + 1;
    connection.le_cbm_connection.cid = 0x40;
    connection.le_cbm_connection.connection_handle = 0x10;
    connection.le_cbm_connection.mtu = 4;
    connection.state = OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY;
    connection.packet_handler = event_handler;
    connection.gatt_query_can_send_now.callback = ots_client_run_for_connection;
    ots_client_add_connection(&connection);
    other.basic_connection.cid = 2;
    other.le_cbm_connection.cid = 0x41;
    ots_client_add_connection(&other);
    // Populate the generic list as in production; its nodes are embedded structs.
    ots_client.connections = &other.basic_connection.item;
    other.basic_connection.item.next = &connection.basic_connection.item;
    queued_query = NULL;
    query_status = write_status = ERROR_CODE_SUCCESS;
    timer_active = try_from_callback = false;
    sends = sent = requests = responses = chunks = disconnects = 0;
    for (unsigned i = 0; i < sizeof(data); i++) data[i] = i;
}
static void dispatch(void){
    assert(queued_query != NULL);
    btstack_context_callback_registration_t * request = queued_query;
    queued_query = NULL;
    request->callback(request->context);
}
static void ack(uint8_t status){
    uint8_t event[] = {GATT_EVENT_QUERY_COMPLETE, 7, 0x10, 0, 0, 0, 1, 0, status};
    ots_client_handle_gatt_client_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void notify(uint8_t opcode, uint8_t result, bool olcp){
    uint8_t response[] = {olcp ? OLCP_OPCODE_RESPONSE_CODE : OACP_OPCODE_RESPONSE_CODE, opcode, result, 1, 2, 3, 4};
    uint8_t index = olcp ? OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_LIST_CONTROL_POINT : OTS_CLIENT_CHARACTERISTIC_INDEX_OBJECT_ACTION_CONTROL_POINT;
    uint16_t length = (!olcp && opcode == OACP_OPCODE_CALCULATE_CHECKSUM) ? 7 : 3;
    ots_client_emit_notify_event(&connection, index + 1, ATT_ERROR_SUCCESS, response, length);
}
static void send_event(void){
    uint8_t event[] = {L2CAP_EVENT_CAN_SEND_NOW, 2, 0x40, 0};
    ots_client_l2cap_cbm_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void close_event(void){
    uint8_t event[] = {L2CAP_EVENT_CHANNEL_CLOSED, 2, 0x40, 0};
    ots_client_l2cap_cbm_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void assert_busy(void){
    uint32_t length = connection.cbm_data_chunk_length;
    uint32_t transferred = connection.cbm_data_chunk_bytes_transferred;
    assert(object_transfer_service_client_read(&connection, 99, 55) == ERROR_CODE_CONTROLLER_BUSY);
    assert(object_transfer_service_client_write(&connection, false, 99, data, 7) == ERROR_CODE_CONTROLLER_BUSY);
    assert(object_transfer_service_client_calculate_checksum(&connection, 99, 55) == ERROR_CODE_CONTROLLER_BUSY);
    assert(object_transfer_service_client_command_first(&connection) == ERROR_CODE_CONTROLLER_BUSY);
    assert(length == connection.cbm_data_chunk_length && transferred == connection.cbm_data_chunk_bytes_transferred);
}
static void assert_ready(void){
    assert(connection.state == OBJECT_TRANSFER_SERVICE_CLIENT_STATE_READY);
    assert(connection.pending_control_point_opcode == 0);
    assert(!connection.current_object_read_transfer_in_progress && !connection.current_object_write_transfer_in_progress);
    assert(connection.cbm_data_chunk_length == 0 && connection.cbm_data_chunk_bytes_transferred == 0);
    assert(!timer_active);
}
static void test_lookup(void){
    setup();
    assert(ots_client_get_connection_for_cbm_local_cid(0x40) == &connection);
    assert(ots_client_get_connection_for_cbm_local_cid(0x41) == &other);
    assert(ots_client_get_connection_for_cbm_local_cid(0x42) == NULL);
}
static void test_checksum_and_olcp(void){
    setup();
    assert(object_transfer_service_client_calculate_checksum(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    assert_busy();
    dispatch();
    assert_busy();
    try_from_callback = true;
    ack(ATT_ERROR_SUCCESS);
    assert(callback_status == ERROR_CODE_CONTROLLER_BUSY);
    assert(connection.state == OBJECT_TRANSFER_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_RESPONSE);
    assert_busy();
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    assert(responses == 0);
    assert_busy();
    notify(OACP_OPCODE_CALCULATE_CHECKSUM, OACP_RESULT_CODE_SUCCESS, false);
    assert_ready();
    assert(object_transfer_service_client_command_first(&connection) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    assert_busy();
    notify(OLCP_OPCODE_FIRST, OLCP_RESULT_CODE_SUCCESS, true);
    assert_ready();
}
static void test_read_and_abort(void){
    setup();
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    assert_busy();
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    assert_busy();
    send_event();
    assert(sends == 0);
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 4);
    assert(chunks == 1 && connection.cbm_data_chunk_bytes_transferred == 4);
    assert(object_transfer_service_client_abort(&connection) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    assert_busy();
    notify(OACP_OPCODE_ABORT, OACP_RESULT_CODE_OPERATION_FAILED, false);
    assert_busy();
    assert(connection.current_object_read_transfer_in_progress);
    assert(connection.cbm_data_chunk_bytes_transferred == 4);
    assert(object_transfer_service_client_abort(&connection) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_ABORT, OACP_RESULT_CODE_SUCCESS, false);
    assert_ready();
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 4);
    assert(chunks == 1);
}
static void test_write_and_read_completion(void){
    setup();
    assert(object_transfer_service_client_write(&connection, false, 0, data, sizeof(data)) == ERROR_CODE_SUCCESS);
    send_event();
    assert(sends == 0);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    assert_busy();
    notify(OACP_OPCODE_WRITE, OACP_RESULT_CODE_SUCCESS, false);
    send_event(); assert_busy();
    send_event(); assert_busy();
    send_event(); assert_ready();
    assert(sent == 8 && sends == 2);
    send_event(); assert(sends == 2);
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    try_from_callback = true;
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 8);
    assert(callback_status == ERROR_CODE_SUCCESS);
    assert_busy(); // The callback's new checksum remains pending.
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_CALCULATE_CHECKSUM, OACP_RESULT_CODE_SUCCESS, false);
    assert_ready();
}
static void test_early_response_and_failures(void){
    setup();
    assert(object_transfer_service_client_calculate_checksum(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch();
    notify(OACP_OPCODE_CALCULATE_CHECKSUM, OACP_RESULT_CODE_SUCCESS, false);
    assert_busy(); // ATT completion is still outstanding.
    ack(ATT_ERROR_SUCCESS); assert_ready();
    query_status = ERROR_CODE_COMMAND_DISALLOWED;
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_COMMAND_DISALLOWED);
    assert_ready();
    query_status = ERROR_CODE_SUCCESS;
    write_status = ERROR_CODE_COMMAND_DISALLOWED;
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); assert_ready();
    write_status = ERROR_CODE_SUCCESS;
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_UNLIKELY_ERROR); assert_ready();
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_OPERATION_FAILED, false); assert_ready();
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    close_event(); assert_ready();
    send_event();
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 8);
    assert(sends == 0 && chunks == 0);
}
static void test_timeout(void){
    setup();
    assert(object_transfer_service_client_calculate_checksum(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    timer_active = false; // Run loop removes an expired timer before dispatching it.
    connection.operation_timer.process(&connection.operation_timer);
    assert(disconnects == 1);
    assert(connection.pending_control_point_opcode == 0);
    notify(OACP_OPCODE_CALCULATE_CHECKSUM, OACP_RESULT_CODE_SUCCESS, false);
    assert(responses == 0);
    close_event(); assert_ready();

    setup();
    assert(object_transfer_service_client_calculate_checksum(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch();
    timer_active = false;
    connection.operation_timer.process(&connection.operation_timer);
    close_event();
    assert_busy();
    ack(ATT_ERROR_SUCCESS); assert_ready();
}
static void test_transfer_edges(void){
    setup();
    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch();
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 8);
    assert_busy(); // An early response and complete transfer still cannot release ATT.
    ack(ATT_ERROR_SUCCESS); assert_ready();

    assert(object_transfer_service_client_read(&connection, 0, 8) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    assert(object_transfer_service_client_abort(&connection) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_UNLIKELY_ERROR);
    assert_busy();
    assert(connection.current_object_read_transfer_in_progress);
    assert(object_transfer_service_client_abort(&connection) == ERROR_CODE_SUCCESS);
    close_event();
    assert(queued_query == NULL);
    assert_ready();

    setup();
    assert(object_transfer_service_client_write(&connection, false, 0, data, sizeof(data)) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_WRITE, OACP_RESULT_CODE_SUCCESS, false);
    connection.cbm_data_chunk_bytes_transferred = UINT32_MAX;
    send_event();
    assert(sends == 0); assert_ready();

    assert(object_transfer_service_client_read(&connection, 0, 4) == ERROR_CODE_SUCCESS);
    dispatch(); ack(ATT_ERROR_SUCCESS);
    notify(OACP_OPCODE_READ, OACP_RESULT_CODE_SUCCESS, false);
    ots_client_l2cap_cbm_packet_handler(L2CAP_DATA_PACKET, 0x40, data, 8);
    assert(chunks == 0 && disconnects == 1);
    assert_busy();
    close_event(); assert_ready();
}
int main(void){
    test_lookup();
    test_checksum_and_olcp();
    test_read_and_abort();
    test_write_and_read_completion();
    test_early_response_and_failures();
    test_timeout();
    test_transfer_edges();
    return 0;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle_with_context(btstack_packet_handler_t callback,
                                                                                 hci_con_handle_t con_handle,
                                                                                 uint16_t value_handle,
                                                                                 uint16_t service_id,
                                                                                 uint16_t connection_id){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
    UNUSED(service_id);
    UNUSED(connection_id);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle_with_context(btstack_packet_handler_t callback,
                                                                                      hci_con_handle_t con_handle, uint16_t value_handle,
                                                                                      uint16_t service_id, uint16_t connection_id){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
    UNUSED(service_id);
    UNUSED(connection_id);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_value_of_characteristic_without_response(hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    UNUSED(con_handle);
    UNUSED(value_handle);
    UNUSED(value_length);
    UNUSED(value);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_long_value_of_characteristic_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle,
                                                                    uint16_t value_length, uint8_t * value, uint16_t service_id, uint16_t connection_id){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
    UNUSED(value_length);
    UNUSED(value);
    UNUSED(service_id);
    UNUSED(connection_id);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

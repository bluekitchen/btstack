#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ble/gatt-service/hids_host.c"

// Keep GATT asynchronous: starting a read records it; tests deliver its events.
static uint8_t read_status;
static unsigned read_count;
static unsigned free_count;
static uint8_t connection_status;
static bool defer_queries;
static btstack_context_callback_registration_t * pending_queries[4];
static unsigned pending_count;

uint8_t gatt_client_request_to_send_gatt_query(btstack_context_callback_registration_t * request, hci_con_handle_t con_handle){
    UNUSED(con_handle);
    if (defer_queries){
        for (unsigned i = 0; i < pending_count; i++){
            if (pending_queries[i] == request) return ERROR_CODE_COMMAND_DISALLOWED;
        }
        assert(pending_count < 4);
        pending_queries[pending_count++] = request;
        return ERROR_CODE_SUCCESS;
    }
    request->callback(request->context);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_remove_gatt_query(btstack_context_callback_registration_t * request, hci_con_handle_t con_handle){
    UNUSED(con_handle);
    for (unsigned i = 0; i < pending_count; i++){
        if (pending_queries[i] != request) continue;
        pending_count--;
        memmove(&pending_queries[i], &pending_queries[i + 1], (pending_count - i) * sizeof(pending_queries[0]));
        break;
    }
    return ERROR_CODE_SUCCESS;
}

static void dispatch_query(void){
    assert(pending_count != 0);
    btstack_context_callback_registration_t * request = pending_queries[0];
    gatt_client_remove_gatt_query(request, 0);
    request->callback(request->context);
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
    read_count++;
    return read_status;
}

uint8_t gatt_client_att_status_to_error_code(uint8_t status){
    return status;
}

void btstack_memory_hids_host_free(hids_host_t * client){
    UNUSED(client);
    free_count++;
}

uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(uuid16);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(service);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(characteristic);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
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

uint8_t gatt_client_write_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(value_handle);
    UNUSED(value_length);
    UNUSED(value);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(descriptor_handle);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration){
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(characteristic);
    UNUSED(configuration);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    UNUSED(notification);
    UNUSED(callback);
    UNUSED(con_handle);
    UNUSED(characteristic);
    assert(false);
}

void gatt_client_stop_listening_for_characteristic_value_updates(gatt_client_notification_t * notification){
    UNUSED(notification);
}

uint8_t gatt_client_request_to_write_without_response(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    UNUSED(callback_registration);
    UNUSED(con_handle);
    assert(false);
    return ERROR_CODE_SUCCESS;
}

void gatt_client_deserialize_service(const uint8_t * packet, int offset, gatt_client_service_t * service){
    memset(service, 0, sizeof(*service));
    service->start_group_handle = little_endian_read_16(packet, offset);
    service->end_group_handle = little_endian_read_16(packet, offset + 2);
}

void gatt_client_deserialize_characteristic(const uint8_t * packet, int offset, gatt_client_characteristic_t * characteristic){
    UNUSED(packet);
    UNUSED(offset);
    UNUSED(characteristic);
    assert(false);
}

void gatt_client_deserialize_characteristic_descriptor(const uint8_t * packet, int offset, gatt_client_characteristic_descriptor_t * descriptor){
    UNUSED(packet);
    UNUSED(offset);
    UNUSED(descriptor);
    assert(false);
}

static uint8_t storage[514];
static hids_host_t a, b;

static void client_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    if (packet[2] == GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED){
        connection_status = packet[5];
    }
}

static void setup(uint16_t capacity, uint8_t a_services, uint8_t b_services){
    memset(storage, 0xa5, sizeof(storage));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    clients = NULL;
    a.con_handle = 1;
    b.con_handle = 2;
    a.num_instances = a_services;
    b.num_instances = b_services;
    a.client_handler = client_event;
    b.client_handler = client_event;
    btstack_linked_list_add_tail(&clients, &a.item);
    btstack_linked_list_add_tail(&clients, &b.item);
    hids_host_descriptor_storage = &storage[1];
    hids_host_descriptor_storage_len = capacity;
    hids_host_descriptor_storage_owner = NULL;
    read_status = ERROR_CODE_SUCCESS;
    read_count = 0;
    free_count = 0;
    connection_status = ERROR_CODE_SUCCESS;
    defer_queries = false;
    pending_count = 0;
}

static void start(hids_host_t * client, uint8_t service_index){
    client->service_index = service_index;
    client->state = HIDS_HOST_STATE_W2_READ_REPORT_MAP_HID_DESCRIPTOR;
    hids_host_request_to_send_next_query(client);
}

static void fragment(hids_host_t * client, uint16_t length, uint8_t value){
    uint8_t event[14 + 512] = { GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT };
    assert(length <= 512);
    little_endian_store_16(event, 2, client->con_handle);
    little_endian_store_16(event, 12, length);
    memset(&event[14], value, length);
    handle_gatt_client_event(HCI_EVENT_PACKET, 0, event, 14 + length);
}

static void complete(hids_host_t * client, uint8_t status){
    uint8_t event[9] = { GATT_EVENT_QUERY_COMPLETE, 7 };
    little_endian_store_16(event, 2, client->con_handle);
    event[8] = status;
    handle_gatt_client_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void descriptor(hids_host_t * client, uint8_t index, uint16_t length, uint8_t value){
    start(client, index);
    fragment(client, length, value);
    complete(client, ERROR_CODE_SUCCESS);
}

static void check_bytes(hids_host_t * client, uint8_t index, uint16_t length, uint8_t value){
    hid_service_t * service = &client->services[index];
    assert(service->hid_descriptor_len == length);
    assert((uint32_t) service->hid_descriptor_offset + length <= hids_host_descriptor_storage_len);
    for (uint16_t i = 0; i < length; i++){
        assert(hids_host_descriptor_storage[service->hid_descriptor_offset + i] == value);
    }
    assert(storage[0] == 0xa5);
    assert(storage[1 + hids_host_descriptor_storage_len] == 0xa5);
}

static void test_capacity(void){
    setup(100, 2, 0);
    descriptor(&a, 0, 60, 0x11);
    start(&a, 1);
    assert(a.services[1].hid_descriptor_offset == 60);
    assert(a.services[1].hid_descriptor_max_len == 40);
    fragment(&a, 60, 0x22);
    assert(a.services[1].hid_descriptor_status == ERROR_CODE_MEMORY_CAPACITY_EXCEEDED);
    complete(&a, ERROR_CODE_SUCCESS);
    check_bytes(&a, 0, 60, 0x11);
    check_bytes(&a, 1, 40, 0x22);
    hids_host_finalize(&a);
    uint16_t used;
    assert(hids_host_descriptor_storage_get_used_space(&used) && used == 0);
}

static void test_discovery_does_not_reserve(void){
    setup(100, 0, 0);
    a.state = HIDS_HOST_STATE_W4_SERVICE_RESULT;
    uint8_t event[28] = { GATT_EVENT_SERVICE_QUERY_RESULT, 26, 1, 0 };
    handle_gatt_client_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
    handle_gatt_client_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
    assert(a.num_instances == 2);
    assert(a.services[0].hid_descriptor_max_len == 0);
    assert(a.services[1].hid_descriptor_max_len == 0);
    assert(hids_host_descriptor_storage_owner == NULL);
    descriptor(&a, 0, 60, 0x11);
    descriptor(&a, 1, 60, 0x22);
    check_bytes(&a, 0, 60, 0x11);
    check_bytes(&a, 1, 40, 0x22);
}

static void test_deferred_queries(void){
    setup(100, 1, 1);
    defer_queries = true;
    start(&a, 0);
    start(&b, 0);
    assert(pending_count == 2);
    dispatch_query();
    assert(hids_host_descriptor_storage_owner == &a);
    dispatch_query();
    assert(read_count == 1);
    assert(b.state == HIDS_HOST_STATE_W2_READ_REPORT_MAP_HID_DESCRIPTOR);
    fragment(&a, 20, 0x11);
    assert(pending_count == 1);
    hids_host_finalize(&a);
    // A's pending callback was removed; the next callback starts B's read.
    assert(pending_count == 1);
    assert(pending_queries[0] == &b.gatt_query_request);
    dispatch_query();
    assert(hids_host_descriptor_storage_owner == &b);
    fragment(&b, 100, 0x22);
    complete(&b, ERROR_CODE_SUCCESS);
    check_bytes(&b, 0, 100, 0x22);
    dispatch_query();
    assert(pending_count == 0);
}

static void test_interleaved_clients(void){
    setup(100, 2, 3);
    start(&a, 0);
    fragment(&a, 10, 0x11);
    start(&b, 0);
    assert(read_count == 1);
    assert(b.services[0].hid_descriptor_max_len == 0);
    complete(&a, ERROR_CODE_SUCCESS);
    assert(read_count == 2);
    assert(hids_host_descriptor_storage_owner == &b);
    fragment(&b, 15, 0x22);
    complete(&b, ERROR_CODE_SUCCESS);
    descriptor(&a, 1, 20, 0x33);
    descriptor(&b, 1, 5, 0x44);
    start(&b, 2);
    fragment(&b, 10, 0x55);

    // Remove A's two noncontiguous reservations while B's third read is active.
    hids_host_finalize(&a);
    assert(b.services[0].hid_descriptor_offset == 0);
    assert(b.services[1].hid_descriptor_offset == 15);
    assert(b.services[2].hid_descriptor_offset == 20);
    fragment(&b, 5, 0x55);
    complete(&b, ERROR_CODE_SUCCESS);
    check_bytes(&b, 0, 15, 0x22);
    check_bytes(&b, 1, 5, 0x44);
    check_bytes(&b, 2, 15, 0x55);
    uint16_t used;
    assert(hids_host_descriptor_storage_get_used_space(&used) && used == 35);
    hids_host_finalize(&b);
    assert(hids_host_descriptor_storage_get_used_space(&used) && used == 0);
}

static void test_disconnect_and_errors(void){
    setup(100, 1, 1);
    start(&a, 0);
    fragment(&a, 20, 0x11);
    start(&b, 0);
    uint8_t disconnect[] = { HCI_EVENT_DISCONNECTION_COMPLETE, 4, 0, 1, 0, 0 };
    handle_hci_event(HCI_EVENT_PACKET, 0, disconnect, sizeof(disconnect));
    assert(free_count == 1);
    assert(hids_host_descriptor_storage_owner == &b);
    assert(b.services[0].hid_descriptor_offset == 0);
    fragment(&b, 100, 0x22);
    complete(&b, ERROR_CODE_SUCCESS);
    check_bytes(&b, 0, 100, 0x22);

    setup(100, 1, 1);
    start(&a, 0);
    fragment(&a, 20, 0x11);
    start(&b, 0);
    complete(&a, ATT_ERROR_UNLIKELY_ERROR);
    assert(free_count == 1);
    assert(connection_status == ATT_ERROR_UNLIKELY_ERROR);
    assert(hids_host_descriptor_storage_owner == &b);
    assert(b.services[0].hid_descriptor_max_len == 100);

    setup(100, 1, 1);
    start(&a, 0);
    start(&b, 0);
    read_status = ERROR_CODE_COMMAND_DISALLOWED;
    complete(&a, ERROR_CODE_SUCCESS);
    assert(free_count == 1);
    assert(connection_status == ERROR_CODE_COMMAND_DISALLOWED);
    assert(hids_host_descriptor_storage_owner == NULL);
    read_status = ERROR_CODE_SUCCESS;
    start(&a, 0);
    assert(hids_host_descriptor_storage_owner == &a);
}

static void test_empty_and_large(void){
    setup(0, 1, 1);
    descriptor(&a, 0, 1, 0x11);
    assert(a.services[0].hid_descriptor_status == ERROR_CODE_MEMORY_CAPACITY_EXCEEDED);
    check_bytes(&a, 0, 0, 0);
    hids_host_finalize(&a);

    setup(512, 1, 1);
    descriptor(&a, 0, 512, 0x11);
    check_bytes(&a, 0, 512, 0x11);
    descriptor(&b, 0, 1, 0x22);
    assert(b.services[0].hid_descriptor_status == ERROR_CODE_MEMORY_CAPACITY_EXCEEDED);
    hids_host_finalize(&a);
    hids_host_finalize(&b);

    setup(100, 2, 1);
    descriptor(&a, 0, 0, 0);
    descriptor(&b, 0, 10, 0x22);
    descriptor(&a, 1, 0, 0);
    hids_host_finalize(&a);
    check_bytes(&b, 0, 10, 0x22);
}

static void test_storage_bounds(void){
    // Reject aggregate capacities that exceed storage, including a 16-bit wrap.
    const uint16_t capacities[] = {60, UINT16_MAX};
    for (unsigned i = 0; i < sizeof(capacities) / sizeof(capacities[0]); i++){
        setup(100, 1, 1);
        a.services[0].hid_descriptor_max_len = capacities[i];
        b.services[0].hid_descriptor_max_len = capacities[i];
        uint16_t used;
        assert(!hids_host_descriptor_storage_get_used_space(&used));
        hids_host_descriptor_storage_delete(&a);
        for (unsigned j = 0; j < sizeof(storage); j++) assert(storage[j] == 0xa5);
    }

    // Compaction checks each removed range before subtracting its end.
    const uint16_t offsets[] = {1, 100, UINT16_MAX};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++){
        setup(100, 1, 0);
        a.services[0].hid_descriptor_offset = offsets[i];
        a.services[0].hid_descriptor_max_len = 20;
        hids_host_descriptor_storage_delete(&a);
        for (unsigned j = 0; j < sizeof(storage); j++) assert(storage[j] == 0xa5);
    }

    setup(100, 1, 1);
    start(&a, 0);
    a.services[0].hid_descriptor_offset = 100;
    assert(!hids_host_descriptor_storage_store(&a, 0, 0x11));
    a.services[0].hid_descriptor_offset = UINT16_MAX;
    assert(!hids_host_descriptor_storage_store(&a, 0, 0x11));
    hids_host_descriptor_storage_delete(&a);
}

int main(void){
    test_capacity();
    test_discovery_does_not_reserve();
    test_deferred_queries();
    test_interleaved_clients();
    test_disconnect_and_errors();
    test_empty_and_large();
    test_storage_bounds();
    return 0;
}

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_debug.h"
#include "mock_gatt_client.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

static enum {
    MOCK_QUERY_IDLE = 0,
    MOCK_QUERY_DISCOVER_PRIMARY_SERVICES,
} mock_gatt_client_state;

static uint8_t mock_gatt_client_att_error;
static uint16_t mock_gatt_client_uuid;
static gatt_client_t gatt_client;

void mock_gatt_client_reset(void){
    mock_gatt_client_att_error = 0;
    mock_gatt_client_state = MOCK_QUERY_IDLE;
}

void mock_gatt_client_add_primary_service(uint16_t service_uuid){
}

void mock_gatt_client_add_characteristic(uint16_t characteristic_uuid){
}

void mock_gatt_client_add_characteristic_descriptor(uint16_t descriptor_uuid){
}

// simulate erro
void mock_gatt_client_simulate_att_error(uint8_t att_error){
    mock_gatt_client_att_error = att_error;
}

uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16){
    printf("gatt_client_discover_primary_services_by_uuid16 callback %p\n", callback);

    mock_gatt_client_state = MOCK_QUERY_DISCOVER_PRIMARY_SERVICES;
    mock_gatt_client_uuid = uuid16;

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    btstack_assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    btstack_assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    btstack_assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    btstack_assert(false);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration){
    btstack_assert(false);
    return ERROR_CODE_SUCCESS;
}

void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    btstack_assert(false);
}

void gatt_client_stop_listening_for_characteristic_value_updates(gatt_client_notification_t * notification){
    btstack_assert(false);
}

/**
 * copied from gatt_client.c - START
 */
void gatt_client_deserialize_service(const uint8_t *packet, int offset, gatt_client_service_t * service){
    service->start_group_handle = little_endian_read_16(packet, offset);
    service->end_group_handle = little_endian_read_16(packet, offset + 2);
    reverse_128(&packet[offset + 4], service->uuid128);
    if (uuid_has_bluetooth_prefix(service->uuid128)){
        service->uuid16 = big_endian_read_32(service->uuid128, 0);
    } else {
        service->uuid16 = 0;
    }
}

void gatt_client_deserialize_characteristic(const uint8_t * packet, int offset, gatt_client_characteristic_t * characteristic){
    characteristic->start_handle = little_endian_read_16(packet, offset);
    characteristic->value_handle = little_endian_read_16(packet, offset + 2);
    characteristic->end_handle = little_endian_read_16(packet, offset + 4);
    characteristic->properties = little_endian_read_16(packet, offset + 6);
    reverse_128(&packet[offset+8], characteristic->uuid128);
    if (uuid_has_bluetooth_prefix(characteristic->uuid128)){
        characteristic->uuid16 = big_endian_read_32(characteristic->uuid128, 0);
    } else {
        characteristic->uuid16 = 0;
    }
}

void gatt_client_deserialize_characteristic_descriptor(const uint8_t * packet, int offset, gatt_client_characteristic_descriptor_t * descriptor){
    descriptor->handle = little_endian_read_16(packet, offset);
    reverse_128(&packet[offset+2], descriptor->uuid128);
    if (uuid_has_bluetooth_prefix(descriptor->uuid128)){
        descriptor->uuid16 = big_endian_read_32(descriptor->uuid128, 0);
    } else {
        descriptor->uuid16 = 0;
    }
}

static void emit_event_new(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size){
    if (!callback) return;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void emit_gatt_complete_event(gatt_client_t * gatt_client, uint8_t att_status){
    // @format H1
    uint8_t packet[5];
    packet[0] = GATT_EVENT_QUERY_COMPLETE;
    packet[1] = 3;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    packet[4] = att_status;
    emit_event_new(gatt_client->callback, packet, sizeof(packet));
}

/**
 * copied from gatt_client.c - END
 */

// magic

void mock_gatt_client_run(void){
    btstack_assert(mock_gatt_client_state != MOCK_QUERY_IDLE);
    switch (mock_gatt_client_state){
        case MOCK_QUERY_DISCOVER_PRIMARY_SERVICES:
            // emit GATT_EVENT_SERVICE_QUERY_RESULT for each matching service
            // TODO:
            // emit GATT_EVENT_QUERY_COMPLETE with status mock_gatt_client_att_error
            emit_gatt_complete_event(&gatt_client, ERROR_CODE_SUCCESS);
            // reset status
            mock_gatt_client_att_error = ERROR_CODE_SUCCESS;
            break;
        default:
            btstack_assert(false);
            break;
    }    
    mock_gatt_client_state = MOCK_QUERY_IDLE;
}
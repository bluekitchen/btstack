#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_debug.h"
#include "bluetooth_gatt.h"
#include "mock_gatt_client.h"

//#include "CppUTest/TestHarness.h"
//#include "CppUTestExt/MockSupport.h"

static enum {
    MOCK_QUERY_IDLE = 0,
    MOCK_QUERY_DISCOVER_PRIMARY_SERVICES_BY_UUID,
    MOCK_QUERY_DISCOVER_CHARACTERISTICS_BY_UUID,
    MOCK_QUERY_DISCOVER_ALL_CHARACTERISTICS,
    MOCK_QUERY_DISCOVER_CHARACTERISTIC_DESCRIPTORS,
    MOCK_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION,
    MOCK_READ_VALUE_OF_CHARACTERISTIC_USING_VALUE_HANDLE,
    MOCK_READ_VALUE_OF_CHARACTERISTIC_DESCRIPTOR_USING_VALUE_HANDLE
} mock_gatt_client_state;

static uint16_t mock_gatt_client_att_handle_generator;

static uint8_t mock_gatt_client_att_error;
static uint8_t  mock_gatt_client_uuid128[16];
static uint16_t mock_gatt_client_value_handle;
static uint16_t mock_gatt_client_start_handle;
static uint16_t mock_gatt_client_end_handle;

static gatt_client_t gatt_client;

static btstack_linked_list_t mock_gatt_client_services;

static mock_gatt_client_service_t * mock_gatt_client_last_service;
static mock_gatt_client_characteristic_t * mock_gatt_client_last_characteristic;

static uint8_t moc_att_error_code_discover_services;
static uint8_t moc_att_error_code_discover_characteristics;
static uint8_t moc_att_error_code_discover_characteristic_descriptors;
static uint8_t moc_att_error_code_read_value_characteristics;

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

static void emit_event(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size){
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
    emit_event(gatt_client->callback, packet, sizeof(packet));
}

static void emit_gatt_service_query_result_event(gatt_client_t * gatt_client, uint16_t start_group_handle, uint16_t end_group_handle, const uint8_t * uuid128){
    // @format HX
    uint8_t packet[24];
    packet[0] = GATT_EVENT_SERVICE_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2u;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    ///
    little_endian_store_16(packet, 4, start_group_handle);
    little_endian_store_16(packet, 6, end_group_handle);
    reverse_128(uuid128, &packet[8]);
    emit_event(gatt_client->callback, packet, sizeof(packet));
}

static void emit_gatt_characteristic_query_result_event(gatt_client_t * gatt_client, uint16_t start_handle, uint16_t value_handle, uint16_t end_handle,
                                                        uint16_t properties, const uint8_t * uuid128){
    // @format HY
    uint8_t packet[28];
    packet[0] = GATT_EVENT_CHARACTERISTIC_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2u;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    ///
    little_endian_store_16(packet, 4,  start_handle);
    little_endian_store_16(packet, 6,  value_handle);
    little_endian_store_16(packet, 8,  end_handle);
    little_endian_store_16(packet, 10, properties);
    reverse_128(uuid128, &packet[12]);
    emit_event(gatt_client->callback, packet, sizeof(packet));
}

static void emit_gatt_all_characteristic_descriptors_result_event(
        gatt_client_t * gatt_client, uint16_t descriptor_handle, const uint8_t * uuid128){
    // @format HZ
    uint8_t packet[22];
    packet[0] = GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2u;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    ///
    little_endian_store_16(packet, 4,  descriptor_handle);
    reverse_128(uuid128, &packet[6]);
    emit_event(gatt_client->callback, packet, sizeof(packet));
}

static uint8_t event_packet[255];
static uint8_t * setup_characteristic_value_packet(uint8_t type, hci_con_handle_t con_handle, uint16_t attribute_handle, uint8_t * value, uint16_t length){
    // before the value inside the ATT PDU
    event_packet[0] = type;
    event_packet[1] = 6 + length;
    little_endian_store_16(event_packet, 2, con_handle);
    little_endian_store_16(event_packet, 4, attribute_handle);
    little_endian_store_16(event_packet, 6, length);
    memcpy(&event_packet[8], value, length);
    return &event_packet[0];
}

void mock_gatt_client_send_notification_with_handle(mock_gatt_client_characteristic_t * characteristic, uint16_t value_handle, const uint8_t * value_buffer, uint16_t value_len){
    btstack_assert(characteristic != NULL);
    btstack_assert(characteristic->notification != NULL);
    btstack_assert(characteristic->notification->callback != NULL);
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_NOTIFICATION, gatt_client.con_handle, value_handle, (uint8_t *) value_buffer, value_len);
    emit_event(characteristic->notification->callback, packet, 2 + packet[1]);
}

void mock_gatt_client_send_notification(mock_gatt_client_characteristic_t * characteristic, const uint8_t * value_buffer, uint16_t value_len){
    mock_gatt_client_send_notification_with_handle(characteristic, characteristic->value_handle, value_buffer, value_len);
}

void mock_gatt_client_send_indication_with_handle(mock_gatt_client_characteristic_t * characteristic, uint16_t value_handle, const uint8_t * value_buffer, uint16_t value_len){
    btstack_assert(characteristic != NULL);
    btstack_assert(characteristic->notification != NULL);
    btstack_assert(characteristic->notification->callback != NULL);
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_INDICATION, gatt_client.con_handle, value_handle, (uint8_t *) value_buffer, value_len);
    emit_event(characteristic->notification->callback, packet, 2 + packet[1]);
}

void mock_gatt_client_send_indication(mock_gatt_client_characteristic_t * characteristic, const uint8_t * value_buffer, uint16_t value_len){
    mock_gatt_client_send_indication_with_handle(characteristic, characteristic->value_handle, value_buffer, value_len);
}

static void mock_gatt_client_send_characteristic_value(gatt_client_t * gatt_client, mock_gatt_client_characteristic_t * characteristic){
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT, gatt_client->con_handle, characteristic->value_handle, characteristic->value_buffer, characteristic->value_len);
    emit_event(gatt_client->callback, packet, 2 + packet[1]);
}

/**
 * copied from gatt_client.c - END
 */


static mock_gatt_client_characteristic_t * mock_gatt_client_get_characteristic_for_value_handle(uint16_t value_handle){
    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_t characteristic_it;

    btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);

        btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
        while (btstack_linked_list_iterator_has_next(&characteristic_it)){
            mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);
            if (characteristic->value_handle != value_handle) continue;
            return characteristic;
        }
    }
    return NULL;
}

static mock_gatt_client_characteristic_t * mock_gatt_client_get_characteristic_for_uuid16(uint16_t uuid16){
    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_t characteristic_it;

    btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);

        btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
        while (btstack_linked_list_iterator_has_next(&characteristic_it)){
            mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);
            if (characteristic->uuid16 != uuid16) continue;
            return characteristic;
        }
    }
    return NULL;
}

static mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_get_characteristic_descriptor_for_handle(uint16_t handle){
    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_t characteristic_it;

    btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);

        btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
        while (btstack_linked_list_iterator_has_next(&characteristic_it)){
            mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);

            btstack_linked_list_iterator_t desc_it;
            btstack_linked_list_iterator_init(&desc_it, &characteristic->descriptors);
            while (btstack_linked_list_iterator_has_next(&desc_it)){
                mock_gatt_client_characteristic_descriptor_t * desc = (mock_gatt_client_characteristic_descriptor_t *) btstack_linked_list_iterator_next(&desc_it);
                if (desc->handle != handle) continue;
                return desc;
            }
        }
    }
    return NULL;
}

uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16){
    mock_gatt_client_state = MOCK_QUERY_DISCOVER_PRIMARY_SERVICES_BY_UUID;
    uuid_add_bluetooth_prefix(mock_gatt_client_uuid128, uuid16);
    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_primary_services_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, const uint8_t * uuid128){
    mock_gatt_client_state = MOCK_QUERY_DISCOVER_PRIMARY_SERVICES_BY_UUID;
    memcpy(mock_gatt_client_uuid128, uuid128, 16);
    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    mock_gatt_client_state = MOCK_QUERY_DISCOVER_CHARACTERISTICS_BY_UUID;
    uuid_add_bluetooth_prefix(mock_gatt_client_uuid128, uuid16);
    mock_gatt_client_start_handle = start_handle;
    mock_gatt_client_end_handle = end_handle;

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service){
    mock_gatt_client_state = MOCK_QUERY_DISCOVER_ALL_CHARACTERISTICS;
    mock_gatt_client_start_handle = service->start_group_handle;
    mock_gatt_client_end_handle = service->end_group_handle;
    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    mock_gatt_client_state = MOCK_QUERY_DISCOVER_CHARACTERISTIC_DESCRIPTORS;
    mock_gatt_client_value_handle = characteristic->value_handle;

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    btstack_assert(notification != NULL);
    btstack_assert(&callback != NULL);

    notification->callback = callback;
    notification->con_handle = con_handle;
    

    if (characteristic == NULL){
        // 'all characteristics': not used yet
        btstack_assert(false);
    } else {
        mock_gatt_client_characteristic_t * mock_characteristic  = mock_gatt_client_get_characteristic_for_value_handle(characteristic->value_handle);
        btstack_assert(mock_characteristic != NULL);

        notification->attribute_handle = characteristic->value_handle;
        mock_characteristic->notification = notification;
    }
}

uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration){
    mock_gatt_client_characteristic_t * mock_characteristic  = mock_gatt_client_get_characteristic_for_value_handle(characteristic->value_handle);
    btstack_assert(mock_characteristic != NULL);

    if (mock_characteristic->notification_status_code != ERROR_CODE_SUCCESS){
        return mock_characteristic->notification_status_code;
    }

    mock_gatt_client_state = MOCK_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristics_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    mock_gatt_client_state = MOCK_READ_VALUE_OF_CHARACTERISTIC_USING_VALUE_HANDLE;
    
    mock_gatt_client_characteristic_t * mock_characteristic = mock_gatt_client_get_characteristic_for_uuid16(uuid16);
    if (mock_characteristic != NULL){
        mock_gatt_client_value_handle = mock_characteristic->value_handle;
        gatt_client.callback = callback;
        gatt_client.con_handle = con_handle;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    mock_gatt_client_state = MOCK_READ_VALUE_OF_CHARACTERISTIC_USING_VALUE_HANDLE;
    
    mock_gatt_client_characteristic_t * mock_characteristic = mock_gatt_client_get_characteristic_for_value_handle(value_handle);
    btstack_assert(mock_characteristic != NULL);
    
    mock_gatt_client_value_handle = mock_characteristic->value_handle;
    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    btstack_assert(characteristic != NULL);
    mock_gatt_client_state = MOCK_READ_VALUE_OF_CHARACTERISTIC_USING_VALUE_HANDLE;
    
    mock_gatt_client_characteristic_t * mock_characteristic = mock_gatt_client_get_characteristic_for_value_handle(characteristic->value_handle);
    btstack_assert(mock_characteristic != NULL);

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    mock_gatt_client_value_handle = mock_characteristic->value_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    mock_gatt_client_state = MOCK_READ_VALUE_OF_CHARACTERISTIC_DESCRIPTOR_USING_VALUE_HANDLE;
    mock_gatt_client_value_handle = descriptor_handle;
    return ERROR_CODE_SUCCESS;
}

void gatt_client_stop_listening_for_characteristic_value_updates(gatt_client_notification_t * notification){
}

uint8_t gatt_client_write_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    mock_gatt_client_characteristic_t * mock_characteristic  = mock_gatt_client_get_characteristic_for_value_handle(value_handle);
    btstack_assert(mock_characteristic != NULL);

    if (mock_characteristic->notification_status_code != ERROR_CODE_SUCCESS){
        return mock_characteristic->notification_status_code;
    }

    mock_gatt_client_state = MOCK_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;

    gatt_client.callback = callback;
    gatt_client.con_handle = con_handle;
    return ERROR_CODE_SUCCESS;
}

void mock_gatt_client_emit_complete(uint8_t status){
    mock_gatt_client_state = MOCK_QUERY_IDLE;
    emit_gatt_complete_event(&gatt_client, status);
}

void mock_gatt_client_run_once(void){
    btstack_assert(mock_gatt_client_state != MOCK_QUERY_IDLE);
    mock_gatt_client_characteristic_t * characteristic;
    mock_gatt_client_characteristic_descriptor_t * descriptor;

    btstack_linked_list_iterator_t service_it; 
    btstack_linked_list_iterator_t characteristic_it; 
    btstack_linked_list_iterator_t descriptor_it; 
        
    switch (mock_gatt_client_state){
        case MOCK_QUERY_DISCOVER_PRIMARY_SERVICES_BY_UUID:
            // emit GATT_EVENT_SERVICE_QUERY_RESULT for each matching service
            if (moc_att_error_code_discover_services != ATT_ERROR_SUCCESS){
                mock_gatt_client_emit_complete(moc_att_error_code_discover_services);
                return;
            }

            btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
            while (btstack_linked_list_iterator_has_next(&service_it)){
                mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);
                if (memcmp(service->uuid128, mock_gatt_client_uuid128, 16) != 0) continue;
                mock_gatt_client_last_service = service;
                emit_gatt_service_query_result_event(&gatt_client, service->start_group_handle, service->end_group_handle, service->uuid128);
            }
            // emit GATT_EVENT_QUERY_COMPLETE
            mock_gatt_client_emit_complete(ATT_ERROR_SUCCESS);
            break;
        
        case MOCK_QUERY_DISCOVER_CHARACTERISTICS_BY_UUID:
        case MOCK_QUERY_DISCOVER_ALL_CHARACTERISTICS:
            // emit GATT_EVENT_CHARACTERISTIC_QUERY_RESULT for each matching characteristic
            if (moc_att_error_code_discover_characteristics != ATT_ERROR_SUCCESS){
                mock_gatt_client_emit_complete(moc_att_error_code_discover_characteristics);
                return;
            }

            btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
            while (btstack_linked_list_iterator_has_next(&service_it)){
                mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);
                
                btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
                while (btstack_linked_list_iterator_has_next(&characteristic_it)){
                    mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);
                    if (characteristic->start_handle < mock_gatt_client_start_handle) continue;
                    if (characteristic->end_handle > mock_gatt_client_end_handle) continue;
                    if ((mock_gatt_client_state == MOCK_QUERY_DISCOVER_CHARACTERISTICS_BY_UUID) && (memcmp(characteristic->uuid128, mock_gatt_client_uuid128, 16) != 0)) continue;

                    emit_gatt_characteristic_query_result_event(&gatt_client,
                        characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, 
                        characteristic->properties, characteristic->uuid128);
                }
            }

            // emit GATT_EVENT_QUERY_COMPLETE
            mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
            break;

        case MOCK_QUERY_DISCOVER_CHARACTERISTIC_DESCRIPTORS:
            characteristic = mock_gatt_client_get_characteristic_for_value_handle(mock_gatt_client_value_handle);
            btstack_assert(characteristic != NULL);

            if (moc_att_error_code_discover_characteristic_descriptors != ATT_ERROR_SUCCESS){
                mock_gatt_client_emit_complete(moc_att_error_code_discover_characteristic_descriptors);
                return;
            }

            btstack_linked_list_iterator_init(&descriptor_it, &characteristic->descriptors);
            while (btstack_linked_list_iterator_has_next(&descriptor_it)){
                mock_gatt_client_characteristic_descriptor_t * desc = (mock_gatt_client_characteristic_descriptor_t *) btstack_linked_list_iterator_next(&descriptor_it);

                emit_gatt_all_characteristic_descriptors_result_event(&gatt_client, desc->handle, desc->uuid128);
            }

            // emit GATT_EVENT_QUERY_COMPLETE
            mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
            break;

        case MOCK_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:
            mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
            break;
        
        case MOCK_READ_VALUE_OF_CHARACTERISTIC_USING_VALUE_HANDLE:  
            characteristic = mock_gatt_client_get_characteristic_for_value_handle(mock_gatt_client_value_handle);
            if (moc_att_error_code_read_value_characteristics != ATT_ERROR_SUCCESS){
                mock_gatt_client_emit_complete(moc_att_error_code_read_value_characteristics);
                break;
            }
            if (characteristic != NULL){
                mock_gatt_client_send_characteristic_value(&gatt_client, characteristic);
            }
            mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
            break;

        case MOCK_READ_VALUE_OF_CHARACTERISTIC_DESCRIPTOR_USING_VALUE_HANDLE:
            descriptor = mock_gatt_client_get_characteristic_descriptor_for_handle(mock_gatt_client_value_handle);
            btstack_assert(descriptor != NULL);
            btstack_assert(descriptor->value_buffer != NULL);
            
            mock_gatt_client_emit_complete(ERROR_CODE_SUCCESS);
            break;
        
        default:
            btstack_assert(false);
            break;
    }
}

void mock_gatt_client_run(void){
    btstack_assert(mock_gatt_client_state != MOCK_QUERY_IDLE);
    while (mock_gatt_client_state != MOCK_QUERY_IDLE){
        mock_gatt_client_run_once();
    }
    mock_gatt_client_att_error = ERROR_CODE_SUCCESS;
    mock_gatt_client_state = MOCK_QUERY_IDLE;
}

void mock_gatt_client_emit_dummy_event(void){
        // @format H1
    uint8_t packet[2];
    packet[0] = 0;
    packet[1] = 0;
    emit_event(gatt_client.callback, packet, sizeof(packet));
}

static void mock_gatt_client_reset_errors(void){
    moc_att_error_code_discover_services = ATT_ERROR_SUCCESS;
    moc_att_error_code_discover_characteristics = ATT_ERROR_SUCCESS;
    moc_att_error_code_discover_characteristic_descriptors = ATT_ERROR_SUCCESS;
    moc_att_error_code_read_value_characteristics = ATT_ERROR_SUCCESS;
}

void mock_gatt_client_reset(void){
    mock_gatt_client_att_error = 0;
    mock_gatt_client_state = MOCK_QUERY_IDLE;
    mock_gatt_client_att_handle_generator = 0;
    mock_gatt_client_last_service = NULL;

    mock_gatt_client_reset_errors();

    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);
        btstack_linked_list_remove(&mock_gatt_client_services, (btstack_linked_item_t *) service);

        btstack_linked_list_iterator_t characteristic_it;
        btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
        while (btstack_linked_list_iterator_has_next(&characteristic_it)){
            mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);
            btstack_linked_list_remove(&service->characteristics, (btstack_linked_item_t *) characteristic);

            btstack_linked_list_iterator_t desc_it;
            btstack_linked_list_iterator_init(&desc_it, &characteristic->descriptors);
            while (btstack_linked_list_iterator_has_next(&desc_it)){
                mock_gatt_client_characteristic_descriptor_t * desc = (mock_gatt_client_characteristic_descriptor_t *) btstack_linked_list_iterator_next(&desc_it);
                btstack_linked_list_remove(&characteristic->descriptors, (btstack_linked_item_t *) desc);
                free(desc);
            }
            free(characteristic);
        }
        free(service);
    }
}

void mock_gatt_client_set_att_error_discover_primary_services(void){
    moc_att_error_code_discover_services = ATT_ERROR_REQUEST_NOT_SUPPORTED;
}
void mock_gatt_client_set_att_error_discover_characteristics(void){
    moc_att_error_code_discover_characteristics = ATT_ERROR_REQUEST_NOT_SUPPORTED;
}
void mock_gatt_client_set_att_error_discover_characteristic_descriptors(void){
    moc_att_error_code_discover_characteristic_descriptors = ATT_ERROR_REQUEST_NOT_SUPPORTED;
}
void mock_gatt_client_set_att_error_read_value_characteristics(void){
    moc_att_error_code_read_value_characteristics = ATT_ERROR_REQUEST_NOT_SUPPORTED;
}

void mock_gatt_client_enable_notification(mock_gatt_client_characteristic_t * characteristic, bool command_allowed){
    if (command_allowed){
        characteristic->notification_status_code = ERROR_CODE_SUCCESS;
    } else {
        characteristic->notification_status_code = ERROR_CODE_COMMAND_DISALLOWED;
    }
}

void mock_gatt_client_dump_services(void){
    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_init(&service_it, &mock_gatt_client_services);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        mock_gatt_client_service_t * service = (mock_gatt_client_service_t *) btstack_linked_list_iterator_next(&service_it);
        printf("0x%02x: START SERVICE ", service->start_group_handle);
        if (service->uuid16) {
            printf("%04x\n", service->uuid16);
        } else {
            printf("%s\n", uuid128_to_str(service->uuid128));
        }

        btstack_linked_list_iterator_t characteristic_it;
        btstack_linked_list_iterator_init(&characteristic_it, &service->characteristics);
        while (btstack_linked_list_iterator_has_next(&characteristic_it)){
            mock_gatt_client_characteristic_t * characteristic = (mock_gatt_client_characteristic_t *) btstack_linked_list_iterator_next(&characteristic_it);
            printf("0x%02x:   START CHR ", characteristic->start_handle);
            if (characteristic->uuid16) {
                printf("%04x\n", characteristic->uuid16);
            } else {
                printf("%s\n", uuid128_to_str(characteristic->uuid128));
            }
            printf("0x%02x:   VALUE HANDLE CHR\n", characteristic->value_handle);

            btstack_linked_list_iterator_t desc_it;
            btstack_linked_list_iterator_init(&desc_it, &characteristic->descriptors);
            while (btstack_linked_list_iterator_has_next(&desc_it)){
                mock_gatt_client_characteristic_descriptor_t * desc = (mock_gatt_client_characteristic_descriptor_t *) btstack_linked_list_iterator_next(&desc_it);
                printf("0x%02x:     DESC 0x%02x\n", desc->handle, desc->uuid16);
            }

            printf("0x%02x:   END CHR 0%02x\n", characteristic->end_handle, characteristic->uuid16);
        }
        printf("0x%02x: END SERVICE 0%02x\n", service->end_group_handle, service->uuid16);

    }
}

static void mock_gatt_client_add_primary_service(void){
    // set lsat group handle
    // create new service
    mock_gatt_client_last_service = (mock_gatt_client_service_t * )malloc(sizeof(mock_gatt_client_service_t));
    memset(mock_gatt_client_last_service, 0, sizeof(mock_gatt_client_service_t));
    mock_gatt_client_last_service->start_group_handle = mock_gatt_client_att_handle_generator++;
    mock_gatt_client_last_service->end_group_handle = mock_gatt_client_last_service->start_group_handle;
    btstack_linked_list_add_tail(&mock_gatt_client_services, (btstack_linked_item_t*)mock_gatt_client_last_service);
    mock_gatt_client_last_characteristic = NULL;
}

mock_gatt_client_service_t * mock_gatt_client_add_primary_service_uuid16(uint16_t service_uuid){
    mock_gatt_client_add_primary_service();
    mock_gatt_client_last_service->uuid16 = service_uuid;
    uuid_add_bluetooth_prefix(mock_gatt_client_last_service->uuid128, service_uuid);
    return mock_gatt_client_last_service;
}
mock_gatt_client_service_t * mock_gatt_client_add_primary_service_uuid128(const uint8_t * service_uuid){
    mock_gatt_client_add_primary_service();
    (void) memcpy(mock_gatt_client_last_service->uuid128, service_uuid, 16);
    return mock_gatt_client_last_service;
}

static void mock_gatt_client_add_characteristic(uint16_t properties){
    btstack_assert(mock_gatt_client_last_service != NULL);
    mock_gatt_client_last_characteristic = (mock_gatt_client_characteristic_t * )malloc(sizeof(mock_gatt_client_characteristic_t));
    memset(mock_gatt_client_last_characteristic, 0, sizeof(mock_gatt_client_characteristic_t));
    mock_gatt_client_last_characteristic->properties = properties;
    mock_gatt_client_last_characteristic->start_handle = mock_gatt_client_att_handle_generator++;
    mock_gatt_client_last_characteristic->value_handle = mock_gatt_client_att_handle_generator++;
    mock_gatt_client_last_characteristic->end_handle = mock_gatt_client_last_characteristic->value_handle;
    btstack_linked_list_add_tail(&mock_gatt_client_last_service->characteristics, (btstack_linked_item_t*)mock_gatt_client_last_characteristic);
    mock_gatt_client_last_service->end_group_handle = mock_gatt_client_att_handle_generator - 1;
}

mock_gatt_client_characteristic_t * mock_gatt_client_add_characteristic_uuid16(uint16_t characteristic_uuid, uint16_t properties){
    mock_gatt_client_add_characteristic(properties);
    mock_gatt_client_last_characteristic->uuid16 = characteristic_uuid;
    uuid_add_bluetooth_prefix(mock_gatt_client_last_characteristic->uuid128, characteristic_uuid);
    return mock_gatt_client_last_characteristic;
}

mock_gatt_client_characteristic_t * mock_gatt_client_add_characteristic_uuid128(const uint8_t * characteristic_uuid, uint16_t properties){
    mock_gatt_client_add_characteristic(properties);
    (void) memcpy(mock_gatt_client_last_characteristic->uuid128, characteristic_uuid, 16);
    return mock_gatt_client_last_characteristic;
}

static mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_add_characteristic_descriptor(void){
    btstack_assert(mock_gatt_client_last_characteristic != NULL);
    mock_gatt_client_characteristic_descriptor_t * desc = (mock_gatt_client_characteristic_descriptor_t * )malloc(sizeof(mock_gatt_client_characteristic_descriptor_t));
    memset(desc, 0, sizeof(mock_gatt_client_characteristic_descriptor_t));
    desc->handle = mock_gatt_client_att_handle_generator++;
    btstack_linked_list_add_tail(&mock_gatt_client_last_characteristic->descriptors, (btstack_linked_item_t*)desc);
    mock_gatt_client_last_characteristic->end_handle = mock_gatt_client_att_handle_generator - 1;
    mock_gatt_client_last_service->end_group_handle = mock_gatt_client_att_handle_generator - 1;
    return desc;
}

mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_add_characteristic_descriptor_uuid16(uint16_t descriptor_uuid){
    mock_gatt_client_characteristic_descriptor_t * desc = mock_gatt_client_add_characteristic_descriptor();
    desc->uuid16 = descriptor_uuid;
    uuid_add_bluetooth_prefix(desc->uuid128, descriptor_uuid);
    return desc;
}

mock_gatt_client_characteristic_descriptor_t * mock_gatt_client_add_characteristic_descriptor_uuid128(const uint8_t * descriptor_uuid){
    mock_gatt_client_characteristic_descriptor_t * desc = mock_gatt_client_add_characteristic_descriptor();
    (void) memcpy(desc->uuid128, descriptor_uuid, 16);
    return desc;
}

void mock_gatt_client_set_descriptor_characteristic_value(mock_gatt_client_characteristic_descriptor_t * descriptor, uint8_t * value_buffer, uint16_t value_len){
    btstack_assert(descriptor != NULL);
    descriptor->value_buffer = value_buffer;
    descriptor->value_len = value_len;
}

void mock_gatt_client_set_characteristic_value(mock_gatt_client_characteristic_t * characteristic, uint8_t * value_buffer, uint16_t value_len){
    btstack_assert(characteristic != NULL);
    characteristic->value_buffer = value_buffer;
    characteristic->value_len = value_len;
}

// simulate error
void mock_gatt_client_simulate_att_error(uint8_t att_error){
    mock_gatt_client_att_error = att_error;
}

/*
 * Copyright (C) 2023 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "gatt_service_client_helper.c"
#include <inttypes.h>

#include "btstack_tlv.h"
#include "sm.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include <stdint.h>
#include <string.h>

#include "ble/gatt_service_client.h"

#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"

// MCS uses 22 Characteristics
#ifndef MAX_NUM_GATT_SERVICE_CLIENT_CHARACTERISTICS
#define MAX_NUM_GATT_SERVICE_CLIENT_CHARACTERISTICS 32
#endif

static void gatt_service_client_gatt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static bool gatt_service_client_intitialized = false;
static uint16_t gatt_service_client_service_cid;
static btstack_linked_list_t gatt_service_clients;
static btstack_packet_callback_registration_t gatt_service_client_hci_callback_registration;

#ifdef ENABLE_GATT_SERVICE_CLIENT_CACHING
static uint32_t gatt_service_client_tag_for_cache(uint8_t device_index, uint8_t cache_index){
    return (((uint8_t)'G') << 24u) | (((uint8_t)'C') << 16u) | (device_index << 8u) | cache_index;
}
#endif

// LE Audio Service Client helper functions
static void gatt_service_client_finalize_connection(gatt_service_client_t * client, gatt_service_client_connection_t * connection){
    btstack_assert(client != NULL);
    gatt_client_stop_listening_for_service_characteristic_value_updates(&connection->notification_listener);
    btstack_linked_list_remove(&client->connections, (btstack_linked_item_t*) connection);
}

static gatt_service_client_t * gatt_service_client_get_service_client_for_id(uint16_t service_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it,  &gatt_service_clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_service_client_t * service_client = (gatt_service_client_t *)btstack_linked_list_iterator_next(&it);
        if (service_client->service_id == service_cid) {
            return service_client;
        };
    }
    return NULL;
}

static gatt_service_client_connection_t * gatt_service_client_get_connection_for_con_handle_and_service_index(const gatt_service_client_t * client, hci_con_handle_t con_handle, uint8_t service_index){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        if (connection->service_index != service_index) continue;
        return connection;
    }
    return NULL;
}

static gatt_service_client_connection_t * gatt_service_client_get_connection_for_con_handle_and_attribute_handle(const gatt_service_client_t * client, hci_con_handle_t con_handle, uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        if (attribute_handle < connection->start_handle) continue;
        if (attribute_handle > connection->end_handle) continue;
        return connection;
    }
    return NULL;
}

gatt_service_client_connection_t * gatt_service_client_get_connection_for_cid(
        const gatt_service_client_t *client, uint16_t connection_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != connection_cid) continue;
        return connection;
    }
    return NULL;
}

uint16_t gatt_service_client_get_mtu(const gatt_service_client_connection_t *connection) {
    uint16_t mtu = 0;
    gatt_client_get_mtu(connection->con_handle, &mtu);
    return mtu;
}

uint16_t gatt_service_client_get_connection_id(const gatt_service_client_connection_t * connection){
    return connection->cid;

}

hci_con_handle_t gatt_service_client_get_con_handle(const gatt_service_client_connection_t * connection){
    return connection->con_handle;
}

uint8_t gatt_service_client_get_service_index(const gatt_service_client_connection_t * connection){
    return connection->service_index;
}

uint16_t gatt_service_client_characteristic_uuid16_for_index(const gatt_service_client_t * client, uint8_t characteristic_index){
    if (characteristic_index < client->characteristics_desc_num){
        return client->characteristics_desc16[characteristic_index];
    } else {
        return 0;
    }
}

const uuid128_t * gatt_service_client_characteristic_uuid128_for_index(const gatt_service_client_t * client, uint8_t characteristic_index){
    if (characteristic_index < client->characteristics_desc_num){
        return &client->characteristics_desc128[characteristic_index];
    } else {
        return NULL;
    }
}

uint16_t gatt_service_client_characteristic_value_handle_for_index(const gatt_service_client_connection_t *connection,
                                                                   uint8_t characteristic_index) {
    return connection->characteristics[characteristic_index].value_handle;
}

uint8_t gatt_service_client_characteristic_index_for_value_handle(const gatt_service_client_connection_t *connection,
                                                                  uint16_t value_handle) {
    for (int i = 0; i < connection->client->characteristics_desc_num; i++){
        if (connection->characteristics[i].value_handle == value_handle) {
            return i;
        }
    }
    return GATT_SERVICE_CLIENT_INVALID_INDEX;
}

static void gatt_service_client_emit_connected(btstack_packet_handler_t event_callback, hci_con_handle_t con_handle, uint16_t cid, uint8_t status){
    btstack_assert(event_callback != NULL);

    log_info("GATT Client Helper, connected 0x%04x, status 0x%02x", con_handle, status);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = 0; // num included services
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void gatt_service_client_emit_disconnected(btstack_packet_handler_t event_callback, hci_con_handle_t con_handle, uint16_t cid){
    btstack_assert(event_callback != NULL);

    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t gatt_service_client_get_next_cid(gatt_service_client_t * client){
    client->cid_counter = btstack_next_cid_ignoring_zero(client->cid_counter);
    return client->cid_counter;
}

static void gatt_service_client_handle_connected(const gatt_service_client_t * client, gatt_service_client_connection_t * connection) {
#ifdef ENABLE_GATT_SERVICE_CLIENT_CACHING
    if (connection->device_index >= 0) {
        // Look for cached characteristics: 4 bytes hash + 1 bytes num characteristics + 3 bytes reserved + value handles
        uint8_t cached_characteristics_data[8 + MAX_NUM_GATT_SERVICE_CLIENT_CHARACTERISTICS * 2];
        const btstack_tlv_t * tlv_impl = NULL;
        void * tlv_context;
        btstack_tlv_get_instance(&tlv_impl, &tlv_context);
        btstack_assert(tlv_impl != NULL);
        uint32_t tag = gatt_service_client_tag_for_cache(connection->device_index, connection->cache_id );
        // store characteristics
        little_endian_store_32(cached_characteristics_data, 0, connection->request_hash);
        little_endian_store_32(cached_characteristics_data, 0, 0);
        cached_characteristics_data[4] = client->characteristics_desc_num;
        for (int i=0;i<client->characteristics_desc_num;i++) {
            little_endian_store_16(cached_characteristics_data, 8 + 2 * i, connection->characteristics[i].value_handle);
        }
        tlv_impl->store_tag(tlv_context, tag, cached_characteristics_data, 8 + 2 * client->characteristics_desc_num);
    }
#endif

    connection->characteristic_index = 0;
    connection->state = GATT_SERVICE_CLIENT_STATE_CONNECTED;
    gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, ERROR_CODE_SUCCESS);
}

static bool gatt_service_client_more_descriptor_queries(const gatt_service_client_t * client, gatt_service_client_connection_t * connection) {
    bool next_query_found = false;
    while (!next_query_found && (connection->characteristic_index < client->characteristics_desc_num)) {
        uint16_t notify_or_indicate = ATT_PROPERTY_NOTIFY | ATT_PROPERTY_INDICATE;
        if ((connection->characteristics[connection->characteristic_index].properties & notify_or_indicate) != 0u){
            next_query_found = true;
            break;
        }
        connection->characteristic_index++;
    }
    return next_query_found;
}

static bool gatt_service_client_have_more_notifications_to_enable(const gatt_service_client_t * client, gatt_service_client_connection_t * connection) {
    bool next_query_found = false;
    while (!next_query_found && (connection->characteristic_index < client->characteristics_desc_num)) {
        if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0) {
            next_query_found = true;
            break;
        }
        connection->characteristic_index++;
    }
    return next_query_found;
}

static uint8_t gatt_service_client_register_notification(gatt_service_client_t * client,
                                                         gatt_service_client_connection_t *connection) {
    gatt_client_characteristic_t characteristic;
    uint8_t status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    
    if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0){
        characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
        characteristic.end_handle = connection->characteristics[connection->characteristic_index].end_handle;
        characteristic.properties = connection->characteristics[connection->characteristic_index].properties;

        if ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_INDICATE) != 0u){
            status = gatt_client_write_client_characteristic_configuration_with_context(
                    &gatt_service_client_gatt_packet_handler,
                    connection->con_handle,
                    &characteristic,
                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION,
                    client->service_id,
                    connection->cid);
        } else if ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_NOTIFY) != 0u){
            status = gatt_client_write_client_characteristic_configuration_with_context(
                    &gatt_service_client_gatt_packet_handler,
                    connection->con_handle,
                    &characteristic,
                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION,
                    client->service_id,
                    connection->cid);
        }
    }
    return status;
}

static uint8_t gatt_service_client_request_send_gatt_query(gatt_service_client_t * client, gatt_service_client_connection_t * connection){
    uint8_t status = gatt_client_request_to_send_gatt_query(&connection->can_send_query_registration, connection->con_handle);
    if (status != ERROR_CODE_SUCCESS){
        gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, gatt_client_att_status_to_error_code(status));
        gatt_service_client_finalize_connection(client, connection);
    }
    return status;
}

static void gatt_service_client_send_next_query(void * context) {
    gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)context;
    gatt_service_client_t * client = connection->client;

    uint8_t status = ATT_ERROR_SUCCESS;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;

    switch (connection->state){
        case GATT_SERVICE_CLIENT_STATE_W2_QUERY_PRIMARY_SERVICE:
            connection->state = GATT_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            if (connection->service_uuid16 == 0){
                status = gatt_client_discover_primary_services_by_uuid128_with_context(&gatt_service_client_gatt_packet_handler,
                                                                          connection->con_handle,
                                                                          (const uint8_t *)connection->service_uuid128,
                                                                          client->service_id,
                                                                          connection->cid);
            } else {
                status = gatt_client_discover_primary_services_by_uuid16_with_context(
                        &gatt_service_client_gatt_packet_handler,
                        connection->con_handle,
                        connection->service_uuid16,
                        client->service_id,
                        connection->cid);
            }
            break;

        case GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read characteristics [service 0x%04x]:\n", connection->service_uuid16);
#endif
            connection->state = GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;

            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = connection->service_uuid16;

            status = gatt_client_discover_characteristics_for_service_with_context(
                    &gatt_service_client_gatt_packet_handler,
                    connection->con_handle,
                    &service,
                    client->service_id,
                    connection->cid);
            break;

        case GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors for characteristic[%u, uuid16 0x%04x, value_handle 0x%04x]:\n",
                connection->characteristic_index, 
                client->characteristics_desc16[connection->characteristic_index],
                connection->characteristics[connection->characteristic_index].value_handle);
#endif
            connection->state = GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
            characteristic.properties   = connection->characteristics[connection->characteristic_index].properties;
            characteristic.end_handle   = connection->characteristics[connection->characteristic_index].end_handle;

            (void) gatt_client_discover_characteristic_descriptors_with_context(
                    &gatt_service_client_gatt_packet_handler,
                    connection->con_handle,
                    &characteristic,
                    client->service_id,
                    connection->cid);
            break;

        case GATT_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
#ifdef ENABLE_TESTING_SUPPORT
            if ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_NOTIFY) != 0u){
                printf("Register notification for characteristic");
            } else {
                printf("Register indication for characteristic");
            }

            printf("[%u, uuid16 0x%04x, ccd handle 0x%04x]\n",
                connection->characteristic_index, 
                client->characteristics_desc16[connection->characteristic_index],
                connection->characteristics[connection->characteristic_index].client_configuration_handle);
#endif
            connection->state = GATT_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
            status = gatt_service_client_register_notification(client, connection);
            connection->characteristic_index++;
    
#ifdef ENABLE_TESTING_SUPPORT
            if (status != ERROR_CODE_SUCCESS) {
                printf("Notification not supported, status 0%02X\n.", status);
            }
#else
            UNUSED(status);
#endif
            return;

        case GATT_SERVICE_CLIENT_STATE_CONNECTED:
            // TODO
            break;
        default:
            break;
    }

    if (status != ATT_ERROR_SUCCESS){
        gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, gatt_client_att_status_to_error_code(status));
        gatt_service_client_finalize_connection(client, connection);
    }
}

// @return true if client valid / run function should be called
static bool gatt_service_client_handle_query_complete(gatt_service_client_t *client,
                                                      gatt_service_client_connection_t *connection,
                                                      uint8_t status) {
    btstack_assert(client != NULL);
    btstack_assert(connection != NULL);

    if (status != ATT_ERROR_SUCCESS){
        switch (connection->state){
            case GATT_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            case GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            case GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            case GATT_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
                gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, gatt_client_att_status_to_error_code(status));
                gatt_service_client_finalize_connection(client, connection);
                return false;
            case GATT_SERVICE_CLIENT_STATE_CONNECTED:
                break;
            default:
                btstack_unreachable();
                break;
        }
    }

    switch (connection->state){
        case GATT_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (connection->service_instances_num == 0){
                gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                gatt_service_client_finalize_connection(client, connection);
                return false;
            }
            connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            connection->characteristic_index = 0;
            break;

        case GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            connection->characteristic_index = 0;
            break;

        case GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (gatt_service_client_more_descriptor_queries(client, connection)){
                connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }

            connection->characteristic_index = 0;
            if (gatt_service_client_have_more_notifications_to_enable(client, connection)){
                connection->state = GATT_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            } else {
                gatt_service_client_handle_connected(client, connection);
            }
            break;

        case GATT_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (gatt_service_client_have_more_notifications_to_enable(client, connection)){
                connection->state = GATT_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            } else {
                // all notifications registered, start listening
                gatt_client_service_t service;
                service.start_group_handle = connection->start_handle;
                service.end_group_handle = connection->end_handle;

                gatt_client_listen_for_service_characteristic_value_updates(&connection->notification_listener, client->packet_handler,
                                                                            connection->con_handle, &service, client->service_id, connection->cid);
                gatt_service_client_handle_connected(client, connection);
            }

            break;

        default:
            break;

    }
    // TODO run_for_client
    return true;
}

static uint8_t gatt_service_client_get_uninitialized_characteristic_index(
        gatt_service_client_t * client,
        gatt_service_client_connection_t * connection,
        gatt_client_characteristic_t * characteristic){

        uint8_t index = 0xff;
        uint8_t i;

        for (i = 0; i < client->characteristics_desc_num; i++){
            bool characteristic_found = false;

            if (client->characteristics_desc16 != NULL) {
                characteristic_found = client->characteristics_desc16[i] == characteristic->uuid16;
            } else {
                characteristic_found = memcmp(client->characteristics_desc128[i], characteristic->uuid128, 16) == 0;
            }

            if (characteristic_found) {
                if (connection->characteristics[i].value_handle != 0) {
                    continue;
                }
                index = i;
                break;
            }
        }
        return index;
}

static void gatt_service_client_handle_disconnect(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t service_it;
    btstack_linked_list_iterator_init(&service_it, &gatt_service_clients);
    while (btstack_linked_list_iterator_has_next(&service_it)){
        gatt_service_client_t * client = (gatt_service_client_t *)btstack_linked_list_iterator_next(&service_it);
        btstack_linked_list_iterator_t connection_it;
        btstack_linked_list_iterator_init(&connection_it, &client->connections);
        while (btstack_linked_list_iterator_has_next(&connection_it)){
            gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)btstack_linked_list_iterator_next(&connection_it);
            if (connection->con_handle == con_handle) {
                gatt_service_client_emit_disconnected(client->packet_handler, connection->con_handle, connection->cid);
                gatt_service_client_finalize_connection(client, connection);
            }
        }
    }
}

static void
gatt_service_client_gatt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    gatt_service_client_t * client;
    gatt_service_client_connection_t * connection;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
    uint8_t characteristic_index;

    bool request_to_send_next_query = false;
    switch (hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = gatt_service_client_get_service_client_for_id(gatt_event_service_query_result_get_service_id(packet));
            btstack_assert(client != NULL);
            connection = gatt_service_client_get_connection_for_cid(client, gatt_event_service_query_result_get_connection_id(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one Service instance.");
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = gatt_service_client_get_service_client_for_id(gatt_event_characteristic_query_result_get_service_id(packet));
            btstack_assert(client != NULL);
            connection = gatt_service_client_get_connection_for_cid(client, gatt_event_characteristic_query_result_get_connection_id(packet));
            btstack_assert(connection != NULL);
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
      
            characteristic_index = gatt_service_client_get_uninitialized_characteristic_index(client, connection, &characteristic);
            if (characteristic_index < client->characteristics_desc_num){
                connection->characteristics[characteristic_index].value_handle = characteristic.value_handle;
                connection->characteristics[characteristic_index].properties = characteristic.properties;
                connection->characteristics[characteristic_index].end_handle = characteristic.end_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("    [%u] Attribute Handle 0x%04X, Properties 0x%02X, Value Handle 0x%04X, UUID 0x%04X\n",
                       characteristic_index, characteristic.start_handle,
                       characteristic.properties, characteristic.value_handle, characteristic.uuid16);
#endif
            }
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            client = gatt_service_client_get_service_client_for_id(gatt_event_all_characteristic_descriptors_query_result_get_service_id(packet));
            btstack_assert(client != NULL);
            connection = gatt_service_client_get_connection_for_cid(client, gatt_event_all_characteristic_descriptors_query_result_get_connection_id(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 != ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                break;
            }
            btstack_assert(connection->state == GATT_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT);

            if ( ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_NOTIFY)   != 0u) ||
                 ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_INDICATE) != 0u)
               ){
                connection->characteristics[connection->characteristic_index].client_configuration_handle = characteristic_descriptor.handle;
            } else {
                connection->characteristics[connection->characteristic_index].client_configuration_handle = 0;
            } 
            connection->characteristic_index++;

#ifdef ENABLE_TESTING_SUPPORT
            printf("    Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                characteristic_descriptor.handle,
                characteristic_descriptor.uuid16);
#endif
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            client = gatt_service_client_get_service_client_for_id(gatt_event_query_complete_get_service_id(packet));
            btstack_assert(client != NULL);
            connection = gatt_service_client_get_connection_for_cid(client, gatt_event_query_complete_get_connection_id(packet));
            btstack_assert(connection != NULL);
            request_to_send_next_query = gatt_service_client_handle_query_complete(client, connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (request_to_send_next_query){
        gatt_service_client_request_send_gatt_query(client, connection);
    }
}

#ifdef ENABLE_GATT_SERVICE_CLIENT_CACHING
static void gatt_service_client_handle_deleted_bonding(int device_index) {
    const btstack_tlv_t * tlv_impl = NULL;
    void * tlv_context;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context);
    if (!tlv_impl) return;
    for (uint16_t cache_index = 0 ; cache_index <= 255 ; cache_index++) {
        uint32_t tag = gatt_service_client_tag_for_cache(device_index, cache_index);
        int len = tlv_impl->get_tag(tlv_context, tag, NULL, 0);
        if (len == 0) break;
        tlv_impl->delete_tag(tlv_context, tag);
    }
}

// @returns true if characteristic info was restored
static bool gatt_service_client_caching_restore(gatt_service_client_t *client, gatt_service_client_connection_t *connection) {
    // requirements:
    // - device index <-> bonded
    // - cache id
    // - database hash
    // - TLV
    connection->device_index = sm_le_device_index(connection->con_handle);
    connection->cache_id = gatt_client_get_next_cache_id(connection->con_handle);
    const uint8_t * database_hash = gatt_client_get_database_hash(connection->con_handle);
    if (database_hash == NULL) {
        connection->device_index = -1;
    }
    const btstack_tlv_t * tlv_impl = NULL;
    void * tlv_context;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context);
    if (tlv_impl == NULL) {
        connection->device_index = -1;
    }
    if (connection->device_index == -1) {
        return false;
    }

    // calc hash
    uint32_t crc = btstack_crc32_init();
    crc = btstack_crc32_update(crc, database_hash, 16);
    crc = btstack_crc32_update(crc, &client->characteristics_desc_num, 1);
    crc = btstack_crc32_update(crc, (const uint8_t*) &connection->service_uuid16, 2);
    crc = btstack_crc32_update(crc, (const uint8_t*) &connection->service_uuid128, 16);
    crc = btstack_crc32_update(crc, &connection->service_index, 1);
    crc = btstack_crc32_update(crc, (const uint8_t*) &connection->start_handle, 2);
    crc = btstack_crc32_update(crc, (const uint8_t*) &connection->end_handle, 2);
    connection->request_hash = crc;
    log_info("Cache ID %u, request hash %08" PRIX32, connection->cache_id, connection->request_hash);

    // Look for cached characteristics: 4 bytes hash + 1 bytes num characteristics + 3 bytes reserved + value handles
    uint8_t cached_characteristics_data[8 + MAX_NUM_GATT_SERVICE_CLIENT_CHARACTERISTICS * 2];
    uint32_t tag = gatt_service_client_tag_for_cache(connection->device_index, connection->cache_id );
    int len = tlv_impl->get_tag(tlv_context, tag, cached_characteristics_data, sizeof(cached_characteristics_data));
    if (len <= 8) {
        return false;
    }

    // verify hash
    uint32_t stored_request_hash = little_endian_read_32(cached_characteristics_data, 0);
    if (stored_request_hash != connection->request_hash) {
        log_info("Request hash does not match")
        return false;
    }

    log_info("Request hash matches")
    // load cached characteristics
    uint8_t num_cached_characteristics = cached_characteristics_data[4];
    btstack_assert(num_cached_characteristics <= MAX_NUM_GATT_SERVICE_CLIENT_CHARACTERISTICS);
    for (int i=0;i<num_cached_characteristics;i++) {
        connection->characteristics[i].value_handle = little_endian_read_32(cached_characteristics_data, 8 + 2 * i);
    }

    return true;
}
#endif

static void gatt_service_client_hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t con_handle;
    bd_addr_t address;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            gatt_service_client_handle_disconnect(con_handle);
            break;
#ifdef ENABLE_GATT_SERVICE_CLIENT_CACHING
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_BONDING_DELETED:
                    // clear cached data
                    gap_subevent_bonding_deleted_get_address(packet, address);
                    log_info("Clear stored data of remote %s, le device id %d", bd_addr_to_str(address),
                        gap_subevent_bonding_deleted_get_index(packet));
                    gatt_service_client_handle_deleted_bonding(gap_subevent_bonding_deleted_get_index(packet));
                    break;
            default:
                break;
            }
            break;
#endif
        default:
            break;
    }
}

static void gatt_service_client_start_connect(gatt_service_client_t *client, gatt_service_client_connection_t *connection, hci_con_handle_t con_handle, gatt_service_client_characteristic_t
                                              * characteristics) {

    connection->con_handle          = con_handle;
    connection->cid                 = gatt_service_client_get_next_cid(client);
    connection->client              = client;
    connection->characteristics     = characteristics;
    connection->can_send_query_registration.callback = &gatt_service_client_send_next_query;
    connection->can_send_query_registration.context = connection;
    btstack_linked_list_add(&client->connections, (btstack_linked_item_t *) connection);

    bool start_query = true;

#ifdef ENABLE_GATT_SERVICE_CLIENT_CACHING
    bool restore_ok = gatt_service_client_caching_restore(client, connection);
    log_info("Caching: restore %u", restore_ok);
    if (restore_ok) {
        start_query = false;

        // enter connected state
        connection->characteristic_index = 0;
        connection->state = GATT_SERVICE_CLIENT_STATE_CONNECTED;
        gatt_service_client_emit_connected(client->packet_handler, connection->con_handle, connection->cid, ERROR_CODE_SUCCESS);
    }
#endif

    if (start_query) {
        gatt_service_client_request_send_gatt_query(client, connection);
    }
}

/* API */

void gatt_service_client_init(void){
    if (false == gatt_service_client_intitialized){
        gatt_service_client_hci_callback_registration.callback = gatt_service_client_hci_event_handler;
        hci_add_event_handler(&gatt_service_client_hci_callback_registration);
        gatt_service_client_intitialized = true;
    }
}

void gatt_service_client_register_client_with_uuid16s(gatt_service_client_t *client, btstack_packet_handler_t packet_handler,
                                         const uint16_t *characteristic_uuid16s, uint16_t characteristic_uuid16s_num) {

    btstack_assert(gatt_service_client_intitialized);

    gatt_service_client_service_cid = btstack_next_cid_ignoring_zero(gatt_service_client_service_cid);
    client->service_id =gatt_service_client_service_cid;
    client->cid_counter = 0;
    client->packet_handler = packet_handler;
    client->characteristics_desc16 = characteristic_uuid16s;
    client->characteristics_desc_num = characteristic_uuid16s_num;
    client->characteristics_desc128 = NULL;
    btstack_linked_list_add(&gatt_service_clients, &client->item);
}

void gatt_service_client_register_client_with_uuid128s(gatt_service_client_t *client, btstack_packet_handler_t packet_handler,
                                                 const uuid128_t *characteristic_uuid128s, uint16_t characteristic_uuid128s_num){

    btstack_assert(gatt_service_client_intitialized);

    gatt_service_client_service_cid = btstack_next_cid_ignoring_zero(gatt_service_client_service_cid);
    client->service_id = gatt_service_client_service_cid;
    client->cid_counter = 0;
    client->packet_handler = packet_handler;
    client->characteristics_desc16 = NULL;
    client->characteristics_desc128 = characteristic_uuid128s;
    client->characteristics_desc_num = characteristic_uuid128s_num;
    btstack_linked_list_add(&gatt_service_clients, &client->item);
}

uint8_t
gatt_service_client_connect_primary_service_with_uuid16(hci_con_handle_t con_handle, gatt_service_client_t *client,
                                                        gatt_service_client_connection_t *connection,
                                                        uint16_t service_uuid16,
                                                        gatt_service_client_characteristic_t *characteristics,
                                                        uint8_t characteristics_num) {
    
    btstack_assert(client          != NULL);
    btstack_assert(connection      != NULL);
    btstack_assert(characteristics != NULL);
    
    if (gatt_service_client_get_connection_for_con_handle_and_service_index(client, con_handle, 0) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (characteristics_num < client->characteristics_desc_num){
        log_info("At least %u characteristics needed", client->characteristics_desc_num);
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_PRIMARY_SERVICE;
    memset(connection->service_uuid128, 0, 16);
    connection->service_uuid16      = service_uuid16;
    connection->service_index       = 0;
    connection->start_handle        = 0;
    connection->end_handle          = 0xffff;

    gatt_service_client_start_connect(client, connection, con_handle, characteristics);

    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_service_client_connect_primary_service_with_uuid128(hci_con_handle_t con_handle, gatt_service_client_t *client,
                                                                 gatt_service_client_connection_t *connection,
                                                                 const uuid128_t * service_uuid128,
                                                                 gatt_service_client_characteristic_t *characteristics,
                                                                 uint8_t characteristics_num){
    btstack_assert(client          != NULL);
    btstack_assert(connection      != NULL);
    btstack_assert(characteristics != NULL);
    btstack_assert(service_uuid128 != NULL);

    if (gatt_service_client_get_connection_for_con_handle_and_service_index(client, con_handle, 0) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (characteristics_num < client->characteristics_desc_num){
        log_info("At least %u characteristics needed", client->characteristics_desc_num);
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_PRIMARY_SERVICE;
    memcpy(connection->service_uuid128, service_uuid128, 16);
    connection->service_uuid16      = 0;
    connection->service_index       = 0;
    connection->start_handle        = 0;
    connection->end_handle          = 0xffff;

    gatt_service_client_start_connect(client, connection, con_handle, characteristics);

    return ERROR_CODE_SUCCESS;
}

uint8_t
gatt_service_client_connect_secondary_service_with_uuid16(hci_con_handle_t con_handle, gatt_service_client_t *client,
                                                          gatt_service_client_connection_t *connection,
                                                          uint16_t service_uuid16, uint8_t service_index,
                                                          uint16_t service_start_handle, uint16_t service_end_handle,
                                                          gatt_service_client_characteristic_t *characteristics,
                                                          uint8_t characteristics_num) {

    btstack_assert(client != NULL);
    btstack_assert(connection != NULL);
    btstack_assert(characteristics != NULL);
    btstack_assert(characteristics_num >= client->characteristics_desc_num);

    if (gatt_service_client_get_connection_for_con_handle_and_attribute_handle(client, con_handle, service_start_handle) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (gatt_service_client_get_connection_for_con_handle_and_attribute_handle(client, con_handle, service_end_handle) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = GATT_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
    memset(connection->service_uuid128, 0, 16);
    connection->service_uuid16      = service_uuid16;
    connection->service_index       = service_index;
    connection->start_handle        = service_start_handle;
    connection->end_handle          = service_end_handle;

    gatt_service_client_start_connect(client, connection, con_handle, characteristics);

    return ERROR_CODE_SUCCESS;
}

uint8_t
gatt_service_client_can_query_characteristic(const gatt_service_client_connection_t *connection,
                                             uint8_t characteristic_index) {
    if (connection->state != GATT_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (connection->characteristics[characteristic_index].value_handle == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_service_client_disconnect(gatt_service_client_connection_t *connection) {
    // finalize connections
    gatt_service_client_emit_disconnected(connection->client->packet_handler, connection->con_handle, connection->cid);
    gatt_service_client_finalize_connection(connection->client, connection);
    return ERROR_CODE_SUCCESS;
}

void gatt_service_client_unregister_client(gatt_service_client_t * client){
    btstack_assert(client != NULL);

    client->packet_handler = NULL;

    client->cid_counter = 0;
    client->characteristics_desc_num = 0;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_service_client_connection_t * connection = (gatt_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        gatt_service_client_finalize_connection(client, connection);
    }

    btstack_linked_list_remove(&gatt_service_clients, &client->item);
}

void gatt_service_client_dump_characteristic_value_handles(const gatt_service_client_connection_t *connection,
                                                           const char **characteristic_names) {
#ifdef ENABLE_TESTING_SUPPORT
    uint8_t i;
    for (i = 0; i < connection->client->characteristics_desc_num; i++) {
        printf("0x%04X %s\n", connection->characteristics[i].value_handle, characteristic_names[i]);
    }
#else
    UNUSED(connection);
    UNUSED(characteristic_names);
#endif
}

void gatt_service_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    UNUSED(size);
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

void gatt_service_client_deinit(void){
    gatt_service_client_service_cid = 0;
    gatt_service_clients = NULL;
    gatt_service_client_intitialized = false;
}


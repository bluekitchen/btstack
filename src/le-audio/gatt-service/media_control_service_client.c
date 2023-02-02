/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "media_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>


#include "le-audio/gatt-service/media_control_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

// active gatt client query
static le_audio_service_client_t * le_audio_active_client;

static void le_audio_service_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// LE Audio Service Client helper functions
static void le_audio_service_client_finalize_connection(le_audio_service_client_t * client, le_audio_service_client_connection_t * connection){
    if (client == NULL){
        return;
    }
    btstack_linked_list_remove(&client->connections, (btstack_linked_item_t*) connection);
    le_audio_active_client = NULL;
}

static le_audio_service_client_connection_t * le_audio_service_client_get_connection_for_con_handle(le_audio_service_client_t * client, hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_service_client_connection_t * connection = (le_audio_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static le_audio_service_client_connection_t * le_audio_service_client_get_connection_for_cid(le_audio_service_client_t * client, uint16_t connection_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_service_client_connection_t * connection = (le_audio_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != connection_cid) continue;
        return connection;
    }
    return NULL;
}

static void le_audio_service_client_emit_connected(btstack_packet_handler_t event_callback, hci_con_handle_t con_handle, uint16_t cid, uint8_t subevent, uint8_t status){
    btstack_assert(event_callback != NULL);

    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = status;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void le_audio_service_client_emit_disconnected(btstack_packet_handler_t event_callback, uint16_t cid, uint8_t subevent){
    btstack_assert(event_callback != NULL);

    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    (*event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t le_audio_service_client_get_next_cid(le_audio_service_client_t * client){
    client->cid_counter = btstack_next_cid_ignoring_zero(client->cid_counter);
    return client->cid_counter;
}

void le_audio_service_client_hci_event_handler(le_audio_service_client_t * client, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    hci_con_handle_t con_handle;
    le_audio_service_client_connection_t * connection;
    
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = le_audio_service_client_get_connection_for_con_handle(client, con_handle);
            if (connection != NULL){
                le_audio_service_client_emit_disconnected(connection->event_callback, connection->cid, GATTSERVICE_SUBEVENT_LE_AUDIO_CLIENT_DISCONNECTED);
                le_audio_service_client_finalize_connection(client, connection);
            }
            break;
        default:
            break;
    }
}

static bool le_audio_service_client_next_index_for_descriptor_query(le_audio_service_client_t * client, le_audio_service_client_connection_t * connection) {
    bool next_query_found = false;
    while (!next_query_found && (connection->characteristic_index < connection->characteristics_num)) {
        if ((connection->characteristics[connection->characteristic_index].properties & ATT_PROPERTY_NOTIFY) == 1u){
            next_query_found = true;
            break;
        }
        connection->characteristic_index++;
    }
    return next_query_found;
}

static bool le_audio_service_client_next_index_for_notification_query(le_audio_service_client_t * client, le_audio_service_client_connection_t * connection) {
    bool next_query_found = false;
    while (!next_query_found && (connection->characteristic_index < connection->characteristics_num)) {
        if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0) {
            next_query_found = true;
            break;
        }
        connection->characteristic_index++;
    }
    return next_query_found;
}

static uint8_t le_audio_service_client_register_notification(le_audio_service_client_connection_t * connection){
    gatt_client_characteristic_t characteristic;
    uint8_t status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    
    if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0){
        characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
        characteristic.end_handle = connection->characteristics[connection->characteristic_index].end_handle;
        characteristic.properties = connection->characteristics[connection->characteristic_index].properties;

        status = gatt_client_write_client_characteristic_configuration(
                    &le_audio_service_client_handle_gatt_client_event, 
                    connection->con_handle, 
                    &characteristic, 
                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
      
        // notification supported, register for value updates
        if (status == ERROR_CODE_SUCCESS){
            gatt_client_listen_for_characteristic_value_updates(
                &connection->characteristics[connection->characteristic_index].notification_listener, 
                connection->handle_gatt_server_notification,
                connection->con_handle, &characteristic);
        }
    }
    return status;
}


static void le_audio_service_client_run_for_client(le_audio_service_client_t * client, le_audio_service_client_connection_t * connection){
    uint8_t status = ATT_ERROR_SUCCESS;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;

    switch (connection->state){
        case LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(
                &le_audio_service_client_handle_gatt_client_event, 
                connection->con_handle, 
                client->service_uuid16);
            break;

        case LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read characteristics [service 0x%04x]:\n", client->service_uuid16);
#endif
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;

            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = client->service_uuid16;

            status = gatt_client_discover_characteristics_for_service(
                &le_audio_service_client_handle_gatt_client_event, 
                connection->con_handle, 
                &service);
            break;

        case LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors for characteristic[%u, uuid16 0x%04x, value_handle 0x%04x]:\n", 
                connection->characteristic_index, 
                client->characteristics_desc[connection->characteristic_index].uuid16,
                connection->characteristics[connection->characteristic_index].value_handle);
#endif
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
            characteristic.properties   = connection->characteristics[connection->characteristic_index].properties;
            characteristic.end_handle   = connection->characteristics[connection->characteristic_index].end_handle;

            (void) gatt_client_discover_characteristic_descriptors(
                &le_audio_service_client_handle_gatt_client_event, 
                connection->con_handle, 
                &characteristic);
            break;

        case LE_AUDIO_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Register notification for characteristic[%u, uuid16 0x%04x, ccd handle 0x%04x]:\n", 
                connection->characteristic_index, 
                client->characteristics_desc[connection->characteristic_index].uuid16,
                connection->characteristics[connection->characteristic_index].client_configuration_handle);
#endif
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
            status = le_audio_service_client_register_notification(connection);
            connection->characteristic_index++;
    
#ifdef ENABLE_TESTING_SUPPORT
            if (status != ERROR_CODE_SUCCESS) {
                    printf("Notification not supported, status 0%02X\n.", status);
            }
#endif
            return;

        case LE_AUDIO_SERVICE_CLIENT_STATE_CONNECTED:
            // TODO
            break;
        default:
            break;
    }

    if (status != ATT_ERROR_SUCCESS){
        le_audio_service_client_emit_connected(connection->event_callback, connection->con_handle, connection->cid, client->connect_subevent, status);
        le_audio_service_client_finalize_connection(client, connection);
    }
}

// @return true if client valid / run function should be called
static bool le_audio_service_client_handle_query_complete(le_audio_service_client_connection_t * connection, uint8_t status){
    btstack_assert(le_audio_active_client != NULL);

    if (status != ATT_ERROR_SUCCESS){
        switch (connection->state){
            case LE_AUDIO_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            case LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
                le_audio_service_client_emit_connected(connection->event_callback, connection->con_handle, connection->cid, le_audio_active_client->connect_subevent, status);
                le_audio_service_client_finalize_connection(le_audio_active_client, connection);
                return false;
            default:
                break;
        }
    }

    switch (connection->state){
        case LE_AUDIO_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (connection->services_num == 0){
                le_audio_service_client_emit_connected(connection->event_callback, connection->con_handle, connection->cid, le_audio_active_client->connect_subevent, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                le_audio_service_client_finalize_connection(le_audio_active_client, connection);
                return false;
            }
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;
        
        case LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            connection->characteristic_index = 0;
            break;

        case LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (le_audio_service_client_next_index_for_descriptor_query(le_audio_active_client, connection)){
                connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }

            connection->characteristic_index = 0;
            if (le_audio_service_client_next_index_for_notification_query(le_audio_active_client, connection)){
                connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            } else {
                connection->characteristic_index = 0;
                connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_CONNECTED;
                le_audio_service_client_emit_connected(connection->event_callback, connection->con_handle, connection->cid, le_audio_active_client->connect_subevent, ERROR_CODE_SUCCESS);
            }
            break;

        case LE_AUDIO_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (le_audio_service_client_next_index_for_notification_query(le_audio_active_client, connection)){
                connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }

            connection->characteristic_index = 0;
            connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_CONNECTED;
            le_audio_service_client_emit_connected(connection->event_callback, connection->con_handle, connection->cid, le_audio_active_client->connect_subevent, ERROR_CODE_SUCCESS);
            break;

        default:
            break;

    }
    // TODO run_for_client
    return true;
}

static uint8_t le_audio_service_client_get_characteristic_index_for_uuid16(
    le_audio_service_client_t * client,
    uint16_t uuid16){
        uint8_t index = 0xff;

        uint8_t i;
        for (i = 0; i < client->characteristics_desc_num; i++){
            if (client->characteristics_desc[i].uuid16 == uuid16){
                index = i;
                break;
            }
        }
        return index;
}

static void le_audio_service_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);
    
    btstack_assert(le_audio_active_client != NULL);

    le_audio_service_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
    bool call_run = true;
    uint8_t characteristic_index;

    switch (hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = le_audio_service_client_get_connection_for_con_handle(le_audio_active_client, gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = le_audio_service_client_get_connection_for_con_handle(le_audio_active_client, gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->services_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->services_num++;
            } else {
                log_info("Found more then one Service instance.");
            }
            break;
 
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = le_audio_service_client_get_connection_for_con_handle(le_audio_active_client, gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
      
            characteristic_index = le_audio_service_client_get_characteristic_index_for_uuid16(le_audio_active_client, characteristic.uuid16);
            if (characteristic_index < le_audio_active_client->characteristics_desc_num){
                connection->characteristics[characteristic_index].value_handle = characteristic.value_handle;
                connection->characteristics[characteristic_index].properties = characteristic.properties;
                connection->characteristics[characteristic_index].end_handle = characteristic.end_handle;
                connection->characteristics[characteristic_index].uuid16     = characteristic.uuid16;
                connection->characteristics_num++;

#ifdef ENABLE_TESTING_SUPPORT
                printf("    Found Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X\n",
                    characteristic.start_handle, 
                    characteristic.properties, 
                    characteristic.value_handle, characteristic.uuid16);
#endif
            }
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = le_audio_service_client_get_connection_for_con_handle(le_audio_active_client, gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 != ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                break;
            }
            btstack_assert(connection->state == LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT);

            connection->characteristics[connection->characteristic_index].client_configuration_handle = characteristic_descriptor.handle;
            connection->characteristic_index++;

#ifdef ENABLE_TESTING_SUPPORT
            printf("    Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                characteristic_descriptor.handle,
                characteristic_descriptor.uuid16);
#endif
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = le_audio_service_client_get_connection_for_con_handle(le_audio_active_client, gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = le_audio_service_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        le_audio_service_client_run_for_client(le_audio_active_client, connection);
    }
}

static void le_audio_service_client_init(le_audio_service_client_t * client,
    void (*hci_event_handler_trampoline)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
    client->cid_counter = 0;
    client->characteristics_desc_num = 0;
    client->hci_event_callback_registration.callback = hci_event_handler_trampoline;
    hci_add_event_handler(&client->hci_event_callback_registration);
}

static uint8_t le_audio_service_client_connect(
    le_audio_service_client_t * client, le_audio_service_client_connection_t * connection, hci_con_handle_t con_handle, 
    btstack_packet_handler_t packet_handler, uint16_t * connection_cid){
    
    btstack_assert(client         != NULL);
    btstack_assert(connection     != NULL);
    btstack_assert(packet_handler != NULL);
    
    if (le_audio_service_client_get_connection_for_con_handle(client, con_handle) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = le_audio_service_client_get_next_cid(client);
    if (connection_cid != NULL) {
        *connection_cid = cid;
    }
    
    connection->state = LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    connection->cid = *connection_cid;
    connection->con_handle = con_handle;
    connection->event_callback = packet_handler; 
    btstack_linked_list_add(&client->connections, (btstack_linked_item_t *) connection);

    le_audio_active_client = client;

    le_audio_service_client_run_for_client(client, connection);

    return ERROR_CODE_SUCCESS;
}

static void le_audio_service_client_add_characteristic(le_audio_service_client_t * client, uint16_t characteristic_uuid16, bool notify){
    btstack_assert(client != NULL);
    if (client->characteristics_desc_num < LE_AUDIO_SERVICE_CHARACTERISTICS_MAX_NUM){
        client->characteristics_desc[client->characteristics_desc_num].uuid16 = characteristic_uuid16;
        client->characteristics_desc_num++;
    }
}

static uint8_t le_audio_service_client_disconnect(le_audio_service_client_t * client, uint16_t connection_cid){
    btstack_assert(client != NULL);

    le_audio_service_client_connection_t * connection = le_audio_service_client_get_connection_for_cid(client, connection_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connections
    le_audio_service_client_emit_disconnected(connection->event_callback, connection->cid, GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED);
    le_audio_service_client_finalize_connection(client, connection);
    return ERROR_CODE_SUCCESS;
}

static void le_audio_service_client_deinit(le_audio_service_client_t * client){
    btstack_assert(client != NULL);

    client->cid_counter = 0;
    client->characteristics_desc_num = 0;
    
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &client->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_service_client_connection_t * connection = (le_audio_service_client_connection_t *)btstack_linked_list_iterator_next(&it);
        le_audio_service_client_finalize_connection(client, connection);
    }
}

// MSC Client
static le_audio_service_client_t msc_service;

static void mcs_client_packet_handler_trampoline(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    le_audio_service_client_hci_event_handler(&msc_service, packet_type, channel, packet, size);
}

uint8_t media_control_service_client_connect(hci_con_handle_t con_handle, mcs_client_connection_t * connection, btstack_packet_handler_t packet_handler, uint16_t * mcs_cid){
    return le_audio_service_client_connect(&msc_service, &connection->basic_connection, con_handle, packet_handler, mcs_cid);
}

uint8_t media_control_service_client_disconnect(uint16_t mcs_cid){
    return le_audio_service_client_disconnect(&msc_service, mcs_cid);
}

void media_control_service_client_init(void){
    le_audio_service_client_init(&msc_service, &mcs_client_packet_handler_trampoline);
    
    msc_service.disconnect_subevent = GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED;
    msc_service.connect_subevent    = GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED;
    msc_service.service_uuid16        = ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE;

    // TODO: read from file
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID,
                                               false);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL, false);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED, true);
    le_audio_service_client_add_characteristic(&msc_service,
                                               ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID, false);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED,
                                               false);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT, true);
    le_audio_service_client_add_characteristic(&msc_service,
                                               ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID,
                                               true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT, true);
    le_audio_service_client_add_characteristic(&msc_service, ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID, false);
}

void media_control_service_client_deinit(void){
    le_audio_service_client_deinit(&msc_service);
}


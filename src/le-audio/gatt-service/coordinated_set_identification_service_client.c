/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "audio_stream_control_service_client.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

typedef enum {
    CSIS_CHARACTERISTIC_INDEX_SIRK = 0,
    CSIS_CHARACTERISTIC_INDEX_SIZE, 
    CSIS_CHARACTERISTIC_INDEX_LOCK, 
    CSIS_CHARACTERISTIC_INDEX_RANK,
    CSIS_CHARACTERISTIC_INDEX_RFU                   
} csis_characteristic_index_t;

static char * csis_characteristic_index_name[] = {
    "SIRK",
    "SIZE", 
    "LOCK", 
    "RANK",
    "RFU"
};

static btstack_packet_handler_t csis_event_callback;
static btstack_linked_list_t    csis_connections;
static uint16_t                 csis_client_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t csis_client_hci_event_callback_registration;

static char * characteristic_index2name(csis_characteristic_index_t index){
    if (index >= CSIS_CHARACTERISTIC_INDEX_RFU){
        return csis_characteristic_index_name[4];
    }
    return csis_characteristic_index_name[(uint8_t) index];
}

static uint16_t csis_client_get_next_cid(void){
    csis_client_cid_counter = btstack_next_cid_ignoring_zero(csis_client_cid_counter);
    return csis_client_cid_counter;
}

static csis_client_connection_t * csis_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static csis_client_connection_t * csis_get_client_connection_for_cid(uint16_t csis_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != csis_cid) continue;
        return connection;
    }
    return NULL;
}

static void csis_client_finalize_connection(csis_client_connection_t * connection){
    btstack_linked_list_remove(&csis_connections, (btstack_linked_item_t*) connection);
}

static void csis_client_emit_connection_established(csis_client_connection_t * connection, uint8_t status){
    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_SERVER_CONNECTED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_disconnect(uint16_t cid){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_SERVER_DISCONNECTED;
    little_endian_store_16(event, pos, cid);
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_write_lock_complete(csis_client_connection_t * connection, uint8_t status){
    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_WRITE_LOCK_COMPLETE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_lock(csis_client_connection_t * connection, uint8_t status, const uint8_t * data){
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_LOCK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (data == NULL) ? 0 : data[0];
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_coordinated_set_size(csis_client_connection_t * connection, uint8_t status, const uint8_t * data){
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_COORDINATED_SET_SIZE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (data == NULL) ? 0 : data[0];
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_rank(csis_client_connection_t * connection, uint8_t status, const uint8_t * data){
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_RANK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (data == NULL) ? 0 : data[0];
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_remote_ris(csis_client_connection_t * connection, uint8_t status, const uint8_t * data){
    uint8_t event[12];
    memset(event, 0, sizeof(event));

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_CSIS_REMOTE_RANK;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    if (data != NULL){
        reverse_48(data, &event[pos]);
    }
    (*csis_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void csis_client_emit_read_event(csis_client_connection_t * connection, csis_characteristic_index_t index, uint8_t status, const uint8_t * data, uint16_t data_size){
    switch (index){
        case CSIS_CHARACTERISTIC_INDEX_SIRK:
            if (status != ERROR_CODE_SUCCESS){
                csis_client_emit_read_remote_ris(connection, status, NULL);
                break;
            }
            if (data_size != 6){
                csis_client_emit_read_remote_ris(connection, ATT_ERROR_VALUE_NOT_ALLOWED, NULL);
                break;
            }
            csis_client_emit_read_remote_ris(connection, ATT_ERROR_SUCCESS, data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_SIZE:
            if (status != ERROR_CODE_SUCCESS){
                csis_client_emit_read_remote_coordinated_set_size(connection, status, NULL);
                break;
            }
            if (data_size != 1){
                csis_client_emit_read_remote_coordinated_set_size(connection, ATT_ERROR_VALUE_NOT_ALLOWED, NULL);
                break;
            }
            csis_client_emit_read_remote_coordinated_set_size(connection, ATT_ERROR_SUCCESS, data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_LOCK:
            if (status != ERROR_CODE_SUCCESS){
                csis_client_emit_read_remote_lock(connection, status, NULL);
                break;
            }
            if (data_size != 1){
                csis_client_emit_read_remote_lock(connection, ATT_ERROR_VALUE_NOT_ALLOWED, NULL);
                break;
            }
            csis_client_emit_read_remote_lock(connection, ATT_ERROR_SUCCESS, data);
            break;
        case CSIS_CHARACTERISTIC_INDEX_RANK:
            if (status != ERROR_CODE_SUCCESS){
                csis_client_emit_read_remote_rank(connection, status, NULL);
                break;
            }
            if (data_size != 1){
                csis_client_emit_read_remote_rank(connection, ATT_ERROR_VALUE_NOT_ALLOWED, NULL);
                break;
            }
            csis_client_emit_read_remote_rank(connection, ATT_ERROR_SUCCESS, data);
            break;
        default:
            break;
    }
}
static void handle_gatt_server_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    csis_client_connection_t * connection = csis_client_get_connection_for_con_handle(gatt_event_notification_get_handle(packet));
    if (connection == NULL){
        return;
    }

    uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);

    // TODO send SIRK, LOCk, SIZE
}

static bool csis_client_register_notification(csis_client_connection_t * connection){
    gatt_client_characteristic_t characteristic;

    characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
    characteristic.end_handle = connection->characteristics[connection->characteristic_index].end_handle;
    characteristic.properties = connection->characteristics[connection->characteristic_index].properties;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
                &handle_gatt_client_event, 
                connection->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &connection->characteristics[connection->characteristic_index].notification_listener, 
            &handle_gatt_server_notification, 
            connection->con_handle, &characteristic);
    } 
    return ERROR_CODE_SUCCESS;
}

static void csis_client_run_for_connection(csis_client_connection_t * connection){
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;
    uint8_t value[0];

    switch (connection->state){
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(&handle_gatt_client_event, connection->con_handle, ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE;

            gatt_client_discover_characteristics_for_service(
                &handle_gatt_client_event, 
                connection->con_handle, 
                &service);

            break;
        
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors [index %d]:\n", connection->characteristic_index);
#endif
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->characteristics[connection->characteristic_index].value_handle;
            characteristic.properties   = connection->characteristics[connection->characteristic_index].properties;
            characteristic.end_handle   = connection->characteristics[connection->characteristic_index].end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, connection->con_handle, &characteristic);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic value [index %d, handle 0x%04X]:\n", connection->characteristic_index, connection->characteristics[connection->characteristic_index].client_configuration_handle);
#endif
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                &handle_gatt_client_event,
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].client_configuration_handle);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            status = csis_client_register_notification(connection);
            if (status == ERROR_CODE_SUCCESS){
#ifdef ENABLE_TESTING_SUPPORT
                printf("Register notification [index %d, handle 0x%04X]: %s\n", 
                    connection->characteristic_index, 
                    connection->characteristics[connection->characteristic_index].client_configuration_handle,
                    characteristic_index2name((csis_characteristic_index_t)connection->characteristic_index));
#endif          
                return;  
            } 
            
            if (connection->characteristics[connection->characteristic_index].client_configuration_handle != 0){
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
            
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED;
            csis_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &handle_gatt_client_event, 
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].value_handle);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_WRITTEN;
            value[0] = (uint8_t)connection->coordinator_lock;

            (void)gatt_client_write_value_of_characteristic(
                &handle_gatt_client_event,
                connection->con_handle, 
                connection->characteristics[connection->characteristic_index].value_handle,
                1, 
                value); 
            break;

        default:
            break;
    }
}

static bool csis_client_handle_query_complete(csis_client_connection_t * connection, uint8_t status){
    if (status != ATT_ERROR_SUCCESS){
        switch (connection->state){
            case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                csis_client_emit_connection_established(connection, status);
                csis_client_finalize_connection(connection);
                return false;
            default:
                break;
        }
    }

    switch (connection->state){
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (connection->service_instances_num == 0){
                csis_client_emit_connection_established(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                csis_client_finalize_connection(connection);
                return false;
            }
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;
        
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            connection->characteristic_index = 0;
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (connection->characteristic_index < 3){
                connection->characteristic_index++;
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }

            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            connection->characteristic_index = 0;
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (connection->characteristic_index < 2){
                connection->characteristic_index++;
                connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }

            connection->characteristic_index = 0;
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED;
            csis_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;
      
        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED;
            csis_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_WRITTEN:
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED;
            csis_client_emit_write_lock_complete(connection, status);
            break;

        case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ:
            if (status != ERROR_CODE_SUCCESS){
                csis_client_emit_read_event(connection, connection->characteristic_index, status, NULL, 0);
            }
            connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED;
            break;

        default:    
            break;

    }
    return true;
}

static uint8_t csis_get_index_of_characteristic_with_uuid16(uint16_t uuid16){
    switch (uuid16){
        case ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC:
            return (uint8_t)CSIS_CHARACTERISTIC_INDEX_SIRK;

        case ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC:
            return (uint8_t)CSIS_CHARACTERISTIC_INDEX_SIZE;

        case ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC:
            return (uint8_t)CSIS_CHARACTERISTIC_INDEX_LOCK;

        case ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC:
            return (uint8_t)CSIS_CHARACTERISTIC_INDEX_RANK;

        default:
            return (uint8_t)CSIS_CHARACTERISTIC_INDEX_RFU;
    }
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    csis_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
    uint8_t index;

    bool call_run = true;
    uint16_t bytes_read;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = csis_client_get_connection_for_con_handle(gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("CSIS Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one CSIS Service instance. ");
            }
            break;
 
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            
            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC:
                case ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC:
                    index = csis_get_index_of_characteristic_with_uuid16(characteristic.uuid16);
                    break;

                default:
                    btstack_assert(false);
                    return;
            }

            connection->characteristics[index].value_handle = characteristic.value_handle;
            connection->characteristics[index].properties = characteristic.properties;
            connection->characteristics[index].end_handle = characteristic.end_handle;
            connection->characteristics[index].uuid16     = characteristic.uuid16;
            

#ifdef ENABLE_TESTING_SUPPORT
            printf("CSIS Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, %s", 
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16, characteristic_index2name((csis_characteristic_index_t)index));
            
#endif
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 != ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                break;
            }
            
            switch (connection->state){
                case COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    connection->characteristics[connection->characteristic_index].client_configuration_handle = characteristic_descriptor.handle;
                    break;

                default:
                    btstack_assert(false);
                    break;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("    CSIS Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                characteristic_descriptor.handle,
                characteristic_descriptor.uuid16);
#endif
            break;

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection = csis_client_get_connection_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(connection != NULL);                

            if (connection->state == COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
#ifdef ENABLE_TESTING_SUPPORT
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
#endif  
                break;
            }

            csis_client_emit_read_event(connection, connection->characteristic_index, ATT_ERROR_SUCCESS, 
                gatt_event_characteristic_value_query_result_get_value(packet), 
                gatt_event_characteristic_value_query_result_get_value_length(packet));
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = csis_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = csis_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        csis_client_run_for_connection(connection);
    }
}       

uint8_t coordinated_set_identification_service_client_connect(csis_client_connection_t * connection, 
    hci_con_handle_t con_handle, uint16_t * csis_cid){
    
    if (csis_client_get_connection_for_con_handle(con_handle) != NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint16_t cid = csis_client_get_next_cid();
    if (csis_cid != NULL) {
        *csis_cid = cid;
    }

    memset(connection, 0, sizeof(csis_client_connection_t));
    connection->cid = cid;
    connection->con_handle = con_handle;
    connection->mtu = ATT_DEFAULT_MTU;

    connection->service_instances_num = 0;
    
    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    btstack_linked_list_add(&csis_connections, (btstack_linked_item_t *) connection);

    csis_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}


static void csis_client_reset(csis_client_connection_t * connection){
    if (connection == NULL){
        return;
    }

    // uint8_t i;
    // for (i = 0; i < connection->characteristics_instances_num; i++){
    //     gatt_client_stop_listening_for_characteristic_value_updates(&connection->characteristics[i].notification_listener);
    // }
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    hci_con_handle_t con_handle;
    csis_client_connection_t * connection;
    
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = csis_client_get_connection_for_con_handle(con_handle);
            if (connection != NULL){
                btstack_linked_list_remove(&csis_connections, (btstack_linked_item_t *)connection);
                
                uint16_t cid = connection->cid;
                csis_client_reset(connection);
                csis_client_emit_disconnect(cid);
            }
            break;
        default:
            break;
    }
}

static uint8_t coordinated_set_identification_service_client_read_characteristics_value(uint16_t ascs_cid, csis_characteristic_index_t index){
    csis_client_connection_t * connection = csis_get_client_connection_for_cid(ascs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (connection->characteristics[index].value_handle == 0){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE;
    connection->characteristic_index = index;
    return ERROR_CODE_SUCCESS;
}


uint8_t coordinated_set_identification_service_client_read_sirk(uint16_t ascs_cid){
    return coordinated_set_identification_service_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_SIRK);
}

uint8_t coordinated_set_identification_service_client_read_coordinated_set_size(uint16_t ascs_cid){
    return coordinated_set_identification_service_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_SIZE);
}

uint8_t coordinated_set_identification_service_client_read_coordinator_rank(uint16_t ascs_cid){
    return coordinated_set_identification_service_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_RANK);
}

uint8_t coordinated_set_identification_service_client_read_lock(uint16_t ascs_cid){
    return coordinated_set_identification_service_client_read_characteristics_value(ascs_cid, CSIS_CHARACTERISTIC_INDEX_LOCK);
}

uint8_t coordinated_set_identification_service_client_write_lock(uint16_t ascs_cid, csis_member_lock_t lock){
    csis_client_connection_t * connection = csis_get_client_connection_for_cid(ascs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (connection->characteristics[CSIS_CHARACTERISTIC_INDEX_LOCK].value_handle == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    connection->state = COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE;
    connection->characteristic_index = CSIS_CHARACTERISTIC_INDEX_LOCK;
    connection->coordinator_lock = lock;
    return ERROR_CODE_SUCCESS;
}


void coordinated_set_identification_service_client_init(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    csis_event_callback = packet_handler;

    csis_client_hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&csis_client_hci_event_callback_registration);
}

void coordinated_set_identification_service_client_deinit(void){
    csis_event_callback = NULL;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &csis_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        csis_client_connection_t * connection = (csis_client_connection_t *)btstack_linked_list_iterator_next(&it);
        btstack_linked_list_remove(&csis_connections, (btstack_linked_item_t *)connection);
    }
}


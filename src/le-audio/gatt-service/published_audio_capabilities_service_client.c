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

#define BTSTACK_FILE__ "published_audio_capabilities_service_client.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

static btstack_linked_list_t pacs_connections;

static uint16_t pacs_client_cid_counter = 0;
static btstack_packet_handler_t pacs_client_event_callback;

static void pacs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// type(1) + field_lenght
static uint8_t le_audio_specific_codec_capability_type_payload_length[] = {
    0, // LE_AUDIO_CODEC_CAPABILITY_TYPE_UNDEFINED
    3, // LE_AUDIO_CODEC_CAPABILITY_TYPE_SAMPLING_FREQUENCY,
    2, // LE_AUDIO_CODEC_CAPABILITY_TYPE_FRAME_DURATION
    2, // LE_AUDIO_CODEC_CAPABILITY_TYPE_SUPPORTED_AUDIO_CHANNEL_COUNTS
    5, // LE_AUDIO_CODEC_CAPABILITY_TYPE_OCTETS_PER_CODEC_FRAME
    2, // LE_AUDIO_CODEC_CAPABILITY_TYPE_CODEC_FRAME_BLOCKS_PER_SDU
    0  // LE_AUDIO_CODEC_CAPABILITY_TYPE_RFU
};
 
static uint16_t pacs_client_get_next_cid(void){
    pacs_client_cid_counter = btstack_next_cid_ignoring_zero(pacs_client_cid_counter);
    return pacs_client_cid_counter;
}

static void pacs_client_finalize_connection(pacs_client_connection_t * connection){
    btstack_linked_list_remove(&pacs_connections, (btstack_linked_item_t*) connection);
}

static pacs_client_connection_t * pacs_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &pacs_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        pacs_client_connection_t * connection = (pacs_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static pacs_client_connection_t * pacs_client_get_connection_for_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &pacs_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        pacs_client_connection_t * connection = (pacs_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != cid) continue;
        return connection;
    }
    return NULL;
}

static void pacs_client_emit_connection_established(pacs_client_connection_t * connection, uint8_t status){
    btstack_assert(pacs_client_event_callback != NULL);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_PACS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pacs_client_emit_operation_done(pacs_client_connection_t * connection, uint8_t status){
    btstack_assert(pacs_client_event_callback != NULL);

    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_PACS_CLIENT_OPERATION_DONE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pacs_client_emit_audio_locations(pacs_client_connection_t * connection, uint8_t status, le_audio_role_t role, const uint8_t * value, uint8_t value_len){
    btstack_assert(pacs_client_event_callback != NULL);

    uint8_t  event[11];
    memset(event, 0, sizeof(event));
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_PACS_CLIENT_AUDIO_LOCATIONS;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)role;
    memcpy(&event[pos], value, value_len);
    (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pacs_client_handle_audio_locations(pacs_client_connection_t * connection, le_audio_role_t role, const uint8_t * value, uint8_t value_len){
    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t bytes_to_copy = value_len;
    
    if (bytes_to_copy != 4){
        bytes_to_copy = 0;
        status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    } 
    pacs_client_emit_audio_locations(connection, status, role, value, bytes_to_copy);
}

static void pacs_client_emit_audio_contexts(pacs_client_connection_t * connection, uint8_t subevent, uint8_t status, const uint8_t * value, uint8_t value_len){
    btstack_assert(pacs_client_event_callback != NULL);

    uint8_t event[10];
    memset(event, 0, sizeof(event));
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    memcpy(&event[pos], value, value_len);
    (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pacs_client_handle_audio_contexts(pacs_client_connection_t * connection, uint8_t subevent, const uint8_t * value, uint8_t value_len){
    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t bytes_to_copy = value_len;
    
    if (bytes_to_copy != 4){
        bytes_to_copy = 0;
        status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    } 
    pacs_client_emit_audio_contexts(connection, subevent, status, value, bytes_to_copy);
}


static uint16_t le_audio_codec_capabilities_parse_tlv(uint8_t * buffer, uint8_t buffer_size, pacs_codec_capability_t * codec_capability){
    btstack_assert(buffer_size >= 1);
    // parse config to get sampling frequency and frame duration
    uint8_t pos = 0;
    uint8_t codec_capability_length = buffer[pos++];
    
    if ( (codec_capability_length == 0) || (codec_capability_length > (buffer_size - pos)) ){
        return pos;
    }

    uint16_t remaining_bytes = codec_capability_length;
    codec_capability->codec_capability_mask = 0;
    
    while (remaining_bytes > 1) {
        uint8_t payload_length = buffer[pos++];
        remaining_bytes -= 1;

        if (payload_length > remaining_bytes){
            return (pos - 1);
        }

        le_audio_codec_capability_type_t codec_capability_type = (le_audio_codec_capability_type_t)buffer[pos++];
        if ((codec_capability_type == LE_AUDIO_CODEC_CAPABILITY_TYPE_UNDEFINED) || (codec_capability_type >= LE_AUDIO_CODEC_CAPABILITY_TYPE_RFU)){
            return (pos - 2);
        }
        if (payload_length != le_audio_specific_codec_capability_type_payload_length[(uint8_t)codec_capability_type]){
            return (pos - 2);
        }

        codec_capability->codec_capability_mask |= (1 << codec_capability_type);
        switch (codec_capability_type){
            case LE_AUDIO_CODEC_CAPABILITY_TYPE_SAMPLING_FREQUENCY:
                codec_capability->sampling_frequency_mask = little_endian_read_16(buffer, pos);
                break;
            case LE_AUDIO_CODEC_CAPABILITY_TYPE_FRAME_DURATION:
                codec_capability->supported_frame_durations_mask = buffer[pos];
                break;
            case LE_AUDIO_CODEC_CAPABILITY_TYPE_SUPPORTED_AUDIO_CHANNEL_COUNTS:
                codec_capability->supported_audio_channel_counts_mask = buffer[pos];
                break;
            case LE_AUDIO_CODEC_CAPABILITY_TYPE_OCTETS_PER_CODEC_FRAME:
                codec_capability->octets_per_frame_min_num = little_endian_read_24(buffer, pos);
                codec_capability->octets_per_frame_max_num = little_endian_read_24(buffer, pos + 3);
                break;
            case LE_AUDIO_CODEC_CAPABILITY_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
                codec_capability->frame_blocks_per_sdu_max_num = buffer[pos];
                break;
            
            default:
                break;
        }
        pos += payload_length;
        remaining_bytes -= payload_length;
    }
    return pos;
}

static uint16_t le_audio_codec_capabilities_copy_to_event_buffer(pacs_codec_capability_t * codec_capability, uint8_t * event, uint16_t event_size){
    if (event_size < 10){
        return 0;
    }

    uint8_t pos = 0;
    event[pos++] = codec_capability->codec_capability_mask;
    
    little_endian_store_16(event, pos, codec_capability->sampling_frequency_mask);
    pos += 2;
    
    event[pos++] = codec_capability->supported_frame_durations_mask;
    event[pos++] = codec_capability->supported_audio_channel_counts_mask;
    little_endian_store_16(event, pos, codec_capability->octets_per_frame_min_num);
    pos += 2;
    little_endian_store_16(event, pos, codec_capability->octets_per_frame_max_num);
    pos += 2;
    event[pos++] = codec_capability->frame_blocks_per_sdu_max_num; 
    return pos;
}

static void pacs_client_emit_pac_done(pacs_client_connection_t * connection, le_audio_role_t role){
    btstack_assert(pacs_client_event_callback != NULL);

    uint8_t event[6];
    event[0] = HCI_EVENT_GATTSERVICE_META;
    event[1] = 4;
    event[2] = GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD_DONE;
    little_endian_store_16(event, 3, connection->cid);
    event[5] = (uint8_t)role;
    (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pacs_client_emit_pac(pacs_client_connection_t * connection, le_audio_role_t role, const uint8_t * value, uint8_t value_len){
    btstack_assert(pacs_client_event_callback != NULL);

    if (value_len < 8){
        return;
    }

    uint8_t event[6 + 5 + 10 + 6 * 2 + 5 + 4];
    uint8_t event_size = sizeof(event);

    event[0] = HCI_EVENT_GATTSERVICE_META;
    event[1] = sizeof(event) - 2;
    event[2] = GATTSERVICE_SUBEVENT_PACS_CLIENT_PACK_RECORD;
    little_endian_store_16(event, 3, connection->cid);
    event[5] = (uint8_t)role;

    uint8_t  value_pos = 0;
    uint8_t  pac_records_num = value[value_pos++];
    
    uint16_t event_pos;
    uint8_t  i;
    uint16_t stored_bytes = 0;
    
    for (i = 0; i < pac_records_num; i++){
        event_pos = 6;
        if ((value_len - value_pos) < 7){
            // TODO non existing capability or metadata length field 
            return;
        }

        // Codec ID
        memcpy(&event[event_pos], &value[value_pos], 5);
        event_pos += 5;
        value_pos += 5;
        stored_bytes += 5;

        // capability length
        uint8_t capability_len = value[value_pos++];
    
        if (capability_len > 0){
            if ((value_len - value_pos) >= capability_len){
                // capability
                pacs_codec_capability_t codec_capability;
                le_audio_codec_capabilities_parse_tlv((uint8_t *)&value[value_pos], capability_len, &codec_capability);
                stored_bytes += le_audio_codec_capabilities_copy_to_event_buffer(&codec_capability, &event[event_pos], event_size - event_pos);
                event_pos += stored_bytes;
                value_pos += stored_bytes;
            }
        }
        
        if (value_len <= value_pos){
            // TODO non existing metadata length field 
            return;
        }

        // metadata lenght
        uint8_t metadata_length = value[value_pos++];
        if (metadata_length > 0){
            if ((value_len - value_pos) >= metadata_length){
                // metadata
                le_audio_metadata_t metadata;
                le_audio_util_metadata_parse((uint8_t *) &value[value_pos], metadata_length, &metadata);
                stored_bytes += le_audio_util_metadata_serialize(&metadata, &event[event_pos], event_size - event_pos);
                event_pos += stored_bytes;
                value_pos += stored_bytes;
            }
        }
        
        event[1] = event_pos;
        (*pacs_client_event_callback)(HCI_EVENT_PACKET, 0, event, event_pos);
    }
}

static void pacs_client_handle_gatt_server_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    pacs_client_connection_t * connection = pacs_client_get_connection_for_con_handle(gatt_event_notification_get_handle(packet));
    if (connection == NULL){
        return;
    }

    uint16_t value_handle =  gatt_event_notification_get_value_handle(packet);
    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);

    uint8_t i;
    for (i = 0; i < connection->pacs_characteristics_num; i++){
        if (connection->pacs_characteristics[i].value_handle != value_handle){
            continue;
        }

        switch ((pacs_client_characteristic_index_t)i){
            case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS:
                pacs_client_handle_audio_locations(connection, LE_AUDIO_ROLE_SINK, value, value_length);
                break;
            case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS:
                pacs_client_handle_audio_locations(connection, LE_AUDIO_ROLE_SOURCE, value, value_length);
                break;
            case PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS:
                pacs_client_handle_audio_contexts(connection, GATTSERVICE_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS, value, value_length);
                break;
            case PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS:
                pacs_client_handle_audio_contexts(connection, GATTSERVICE_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS, value, value_length);
                break;
            default:
                break;
        }
        return;            
    }
}

static pacs_client_characteristic_t * pacs_client_get_query_characteristic(pacs_client_connection_t * connection){
    pacs_client_characteristic_t * pacs_characteristic;
    uint16_t uuid16;

    switch (connection->query_characteristic_index){
        case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS:
        case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS:
        case PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS:
        case PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS:
            pacs_characteristic = &connection->pacs_characteristics[(uint8_t)connection->query_characteristic_index];
            if (pacs_characteristic->value_handle == 0){
                return NULL;
            }
            return pacs_characteristic;

        case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK:
            uuid16 = ORG_BLUETOOTH_CHARACTERISTIC_SINK_PAC;
            break;
        case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_PACK:
            uuid16 = ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_PAC;
            break;
        default:
            return NULL;
    }

    uint8_t i;
    for (i = (uint8_t)PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK; i < connection->pacs_characteristics_num; i++){
        pacs_characteristic = &connection->pacs_characteristics[i];
        if (pacs_characteristic->uuid16 == uuid16){
            if (pacs_characteristic->value_handle == 0){
                return NULL;
            }
            return pacs_characteristic;
        }
    }
    return NULL;
}

static void pacs_client_handle_notification_registered(pacs_client_connection_t * connection){
    if (connection->pacs_characteristics_index < (connection->pacs_characteristics_num - 1)){
        connection->pacs_characteristics_index++;
        connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
    } else {
        connection->pacs_characteristics_index = 0;
        connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED;
        pacs_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
    }
}

static void pacs_client_run_for_connection(pacs_client_connection_t * connection){
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;
    pacs_client_characteristic_t * pacs_characteristic;

    uint8_t  value[18];

    switch (connection->state){
        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_WRITE_SERVER:
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_WRITE_RESPONSE;

            reverse_bytes(connection->value, value, connection->value_len);
            status = gatt_client_write_value_of_characteristic(
                &pacs_client_handle_gatt_client_event, connection->con_handle, 
                connection->pacs_characteristics[(uint8_t)connection->query_characteristic_index].value_handle,
                connection->value_len, &value[0]);
            UNUSED(status);
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_READ_SERVER:
            pacs_characteristic = pacs_client_get_query_characteristic(connection);
            btstack_assert(pacs_characteristic != NULL);
            
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_READ_RESPONSE;
            status = gatt_client_read_value_of_characteristic_using_value_handle(
                &pacs_client_handle_gatt_client_event, connection->con_handle, 
                pacs_characteristic->value_handle);
            UNUSED(status);
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(&pacs_client_handle_gatt_client_event, connection->con_handle, ORG_BLUETOOTH_SERVICE_PUBLISHED_AUDIO_CAPABILITIES_SERVICE);
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = connection->start_handle;
            service.end_group_handle   = connection->end_handle;
            service.uuid16 = ORG_BLUETOOTH_SERVICE_PUBLISHED_AUDIO_CAPABILITIES_SERVICE;

            gatt_client_discover_characteristics_for_service(
                &pacs_client_handle_gatt_client_event, 
                connection->con_handle, 
                &service);
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;

            characteristic.value_handle = connection->pacs_characteristics[connection->pacs_characteristics_index].value_handle;
            characteristic.properties   = connection->pacs_characteristics[connection->pacs_characteristics_index].properties;
            characteristic.end_handle   = connection->pacs_characteristics[connection->pacs_characteristics_index].end_handle;
            (void) gatt_client_discover_characteristic_descriptors(&pacs_client_handle_gatt_client_event, connection->con_handle, &characteristic);
            break;
        
        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            // register for notification if supported
            while (true){
                characteristic.properties   = connection->pacs_characteristics[connection->pacs_characteristics_index].properties;
                if ((characteristic.properties & ATT_PROPERTY_NOTIFY) != 0){
                    // notification supported
                    connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
                    characteristic.value_handle = connection->pacs_characteristics[connection->pacs_characteristics_index].value_handle;
                    characteristic.end_handle   = connection->pacs_characteristics[connection->pacs_characteristics_index].end_handle;
                    // (re)register for generic listener instead of using one gatt_client_notification_t per characteristic
                    gatt_client_listen_for_characteristic_value_updates(
                            &connection->notification_listener,
                            &pacs_client_handle_gatt_server_notification,
                            connection->con_handle, NULL);
                    (void) gatt_client_write_client_characteristic_configuration(
                            &pacs_client_handle_gatt_client_event,
                            connection->con_handle,
                            &characteristic,
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                } else {
                    pacs_client_handle_notification_registered(connection);
                    if (connection->state != PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION){
                        break;
                    }
                }
            }
            break;
            
        default:
            break;
    }
}

static bool pacs_client_handle_query_complete(pacs_client_connection_t * connection, uint8_t status){
    switch (connection->state){
        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_WRITE_RESPONSE:
            pacs_client_emit_operation_done(connection, status);
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED;
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_READ_RESPONSE:
            
            if (status != ERROR_CODE_SUCCESS){
                pacs_client_emit_operation_done(connection, status);
                break;
            }

            switch (connection->query_characteristic_index){
                case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK:
                case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_PACK:
                    if (connection->pacs_characteristics_index < (connection->pacs_characteristics_num - 1)){
                        connection->pacs_characteristics_index++;
                        connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_READ_SERVER;
                        return false;
                    }    
                    break;
                default:
                    break;
            }
            pacs_client_emit_operation_done(connection, status);
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED;
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                pacs_client_emit_connection_established(connection, status);
                pacs_client_finalize_connection(connection);
                return false;
            }

            if (connection->service_instances_num == 0){
                pacs_client_emit_connection_established(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                pacs_client_finalize_connection(connection);
                return false;
            }

            connection->pacs_characteristics_num = 4;
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                pacs_client_emit_connection_established(connection, status);
                pacs_client_finalize_connection(connection);
                return false;
            }
            
            if (connection->pacs_characteristics_num == 0){
                pacs_client_emit_connection_established(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                pacs_client_finalize_connection(connection);
                return false;
            }

            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            connection->pacs_characteristics_index = 0;
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (connection->pacs_characteristics_index < (connection->pacs_characteristics_num - 1)){
                connection->pacs_characteristics_index++;
                connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }
            connection->pacs_characteristics_index = 0;
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            break;

        case PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            pacs_client_handle_notification_registered(connection);
            break;
        
        default:
            break;
    }
    return true;
}

static void pacs_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    pacs_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;

    bool call_run = true;
    uint8_t index;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("PACS: Service found, start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one PACS Service instance. ");
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            
            index = 0;
            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_SINK_PAC:
                case ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_PAC:
                    index = connection->pacs_characteristics_num;
                    connection->pacs_characteristics_num++;
                    break;
                
                case ORG_BLUETOOTH_CHARACTERISTIC_SINK_AUDIO_LOCATIONS:
                    index = PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS;
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_AUDIO_LOCATIONS:
                    index = PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS;
                    break;
                
                case ORG_BLUETOOTH_CHARACTERISTIC_AVAILABLE_AUDIO_CONTEXTS:
                    index = PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS;
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_AUDIO_CONTEXTS:
                    index = PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS;
                    break;

                default:
                    btstack_assert(false);
                    return;
            }
            if (connection->pacs_characteristics_num < PACS_ENDPOINT_MAX_NUM){
                connection->pacs_characteristics[index].value_handle = characteristic.value_handle;
                connection->pacs_characteristics[index].properties   = characteristic.properties;
                connection->pacs_characteristics[index].uuid16       = characteristic.uuid16;
                connection->pacs_characteristics[index].end_handle   = characteristic.end_handle;

            } else {
                log_info("Found more PACS Endpoint chrs then it can be stored. ");
            }
            

#ifdef ENABLE_TESTING_SUPPORT
            printf("PACS Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, index %d\n", 
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16, index);
#endif
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                connection->pacs_characteristics[connection->pacs_characteristics_index].ccc_handle = characteristic_descriptor.handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("    PACS Characteristic Configuration Descriptor:  index %d, Handle 0x%04X, UUID 0x%04X\n", 
                    connection->pacs_characteristics_index,
                    characteristic_descriptor.handle,
                    characteristic_descriptor.uuid16);
#endif
            }
            break;
        
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(connection != NULL);                
            connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED;

            switch (connection->query_characteristic_index){
                case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS:
                    pacs_client_handle_audio_locations(connection, LE_AUDIO_ROLE_SINK, 
                        gatt_event_characteristic_value_query_result_get_value(packet), 
                        gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS:
                    pacs_client_handle_audio_locations(connection, LE_AUDIO_ROLE_SOURCE, 
                        gatt_event_characteristic_value_query_result_get_value(packet), 
                        gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                case PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS:
                    pacs_client_handle_audio_contexts(connection, GATTSERVICE_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS,
                                                      gatt_event_characteristic_value_query_result_get_value(packet),
                                                      gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                case PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS:
                    pacs_client_handle_audio_contexts(connection, GATTSERVICE_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS,
                                                      gatt_event_characteristic_value_query_result_get_value(packet),
                                                      gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;

                case PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK:
                    pacs_client_emit_pac(connection, LE_AUDIO_ROLE_SINK, 
                        gatt_event_characteristic_value_query_result_get_value(packet), 
                        gatt_event_characteristic_value_query_result_get_value_length(packet));
                    pacs_client_emit_pac_done(connection, LE_AUDIO_ROLE_SINK);
                    break;
                
                case PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_PACK:
                    pacs_client_emit_pac(connection, LE_AUDIO_ROLE_SOURCE, 
                        gatt_event_characteristic_value_query_result_get_value(packet), 
                        gatt_event_characteristic_value_query_result_get_value_length(packet));
                    pacs_client_emit_pac_done(connection, LE_AUDIO_ROLE_SOURCE);
                    break;
                
                default:
                    break;;
            }

            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = pacs_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = pacs_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        pacs_client_run_for_connection(connection);
    }
}


void published_audio_capabilities_service_client_init(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    pacs_client_event_callback = packet_handler;
}

uint8_t published_audio_capabilities_service_client_connect(pacs_client_connection_t * connection, hci_con_handle_t con_handle, uint16_t * pacs_cid){
    if (pacs_client_get_connection_for_con_handle(con_handle) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = pacs_client_get_next_cid();
    if (pacs_cid != NULL) {
        *pacs_cid = cid;
    }
    memset(connection, 0, sizeof(pacs_client_connection_t));
    connection->cid = cid;
    connection->con_handle = con_handle;
    connection->mtu = ATT_DEFAULT_MTU;
    connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    btstack_linked_list_add(&pacs_connections, (btstack_linked_item_t *) connection);

    pacs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

static uint8_t pacs_client_set_audio_locations_operation(
    uint16_t pacs_cid, uint32_t audio_locations_mask, 
    pacs_client_characteristic_index_t index){
    
    pacs_client_connection_t * connection = pacs_client_get_connection_for_cid(pacs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }

    connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_WRITE_SERVER;
    connection->value_len = 4;
    big_endian_store_32(connection->value, 0, audio_locations_mask);
    connection->query_characteristic_index = index;

    pacs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t published_audio_capabilities_service_client_set_sink_audio_locations(uint16_t pacs_cid, uint32_t audio_locations_mask){
    return pacs_client_set_audio_locations_operation(
        pacs_cid, audio_locations_mask, 
        PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS);
}

uint8_t published_audio_capabilities_service_client_set_source_audio_locations(uint16_t pacs_cid, uint32_t audio_locations_mask){
    return pacs_client_set_audio_locations_operation(
        pacs_cid, audio_locations_mask, 
        PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS);
}


static uint8_t pacs_client_get_operation(
    uint16_t pacs_cid, 
    pacs_client_characteristic_index_t index){
    
    pacs_client_connection_t * connection = pacs_client_get_connection_for_cid(pacs_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }

    pacs_client_characteristic_t * pacs_characteristic = pacs_client_get_query_characteristic(connection);
    if (pacs_characteristic == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    connection->state = PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_READ_SERVER;
    connection->query_characteristic_index = index;
    pacs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t published_audio_capabilities_service_client_get_sink_audio_locations(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS);
}

uint8_t published_audio_capabilities_service_client_get_source_audio_locations(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS);
}

uint8_t published_audio_capabilities_service_client_get_available_audio_contexts(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS);
}

uint8_t published_audio_capabilities_service_client_get_supported_audio_contexts(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS);
}

uint8_t published_audio_capabilities_service_client_get_sink_pacs(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK);
}

uint8_t published_audio_capabilities_service_client_get_source_pacs(uint16_t pacs_cid){
    return pacs_client_get_operation(pacs_cid, PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_PACK);
}

void published_audio_capabilities_service_client_deinit(void){
    pacs_client_event_callback = NULL;
}


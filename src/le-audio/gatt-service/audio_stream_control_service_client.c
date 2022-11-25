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
#include "le-audio/gatt-service/audio_stream_control_service_util.h"
#include "le-audio/gatt-service/audio_stream_control_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define ASCS_CLIENT_MAX_VALUE_BUFFER_SIZE  100

static btstack_packet_handler_t ascs_event_callback;
static btstack_linked_list_t    ascs_connections;
static uint16_t                 ascs_client_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static btstack_packet_callback_registration_t ascs_client_hci_event_callback_registration;

static uint8_t  ascs_client_value_buffer[ASCS_CLIENT_MAX_VALUE_BUFFER_SIZE];
static bool     ascs_client_value_buffer_used = false;

static uint16_t ascs_client_get_value_buffer_size(ascs_client_connection_t * connection){
    return btstack_min(connection->mtu, ASCS_CLIENT_MAX_VALUE_BUFFER_SIZE);
}

static uint16_t ascs_client_get_next_cid(void){
    ascs_client_cid_counter = btstack_next_cid_ignoring_zero(ascs_client_cid_counter);
    return ascs_client_cid_counter;
}

static ascs_client_connection_t * ascs_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &ascs_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ascs_client_connection_t * connection = (ascs_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static ascs_client_connection_t * ascs_get_client_connection_for_cid(uint16_t ascs_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &ascs_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ascs_client_connection_t * connection = (ascs_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != ascs_cid) continue;
        return connection;
    }
    return NULL;
}

static ascs_streamendpoint_t * ascs_get_streamendpoint_for_value_handle(ascs_client_connection_t * connection, uint16_t value_handle){
    uint8_t i;
    for (i = 0; i < connection->streamendpoints_instances_num; i++){
        if (connection->streamendpoints[i].ase_characteristic->ase_characteristic_value_handle != value_handle){
            continue;
        }
        return &connection->streamendpoints[i];
    }
    return NULL;
}

static void ascs_client_finalize_connection(ascs_client_connection_t * connection){
    btstack_linked_list_remove(&ascs_connections, (btstack_linked_item_t*) connection);
}

static void ascs_client_emit_streamendpoint_state(hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state){
    btstack_assert(ascs_event_callback != NULL);
    uint8_t event[7];
    
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    event[pos++] = (uint8_t)state;

    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ascs_client_emit_ase(ascs_client_connection_t * connection, ascs_streamendpoint_t * streamendpoint){
    uint8_t ase_id = streamendpoint->ase_characteristic->ase_id;
    
    switch (streamendpoint->state){
        case ASCS_STATE_CODEC_CONFIGURED:
            ascs_util_emit_codec_configuration(ascs_event_callback, connection->con_handle, ase_id, streamendpoint->state, &streamendpoint->codec_configuration);
            break;
        
        case ASCS_STATE_QOS_CONFIGURED:
            ascs_util_emit_qos_configuration(ascs_event_callback, connection->con_handle, ase_id, streamendpoint->state, &streamendpoint->qos_configuration);
            break;
        
        case ASCS_STATE_ENABLING:
        case ASCS_STATE_STREAMING:
        case ASCS_STATE_DISABLING:
            ascs_util_emit_metadata(ascs_event_callback, connection->con_handle, ase_id, streamendpoint->state, &streamendpoint->metadata);
            break;
        
        default:
            break;           
    }

    ascs_client_emit_streamendpoint_state(connection->con_handle, ase_id, streamendpoint->state);
}

static void ascs_client_emit_connection_established(ascs_client_connection_t * connection, uint8_t status){
    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_CONNECTED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = connection->streamendpoints_instances_num;
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void ascs_client_emit_disconnect(uint16_t cid){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_REMOTE_SERVER_DISCONNECTED;
    little_endian_store_16(event, pos, cid);
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void ascs_client_emit_control_point_operation_response(uint16_t cid, uint8_t opcode, uint8_t ase_id, uint8_t response_code, uint8_t reason){
    uint8_t event[9];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = opcode;

    event[pos++] = ase_id;
    event[pos++] = response_code;
    event[pos++] = reason;

    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t ascs_parse_ase(const uint8_t * value, uint16_t value_size, ascs_streamendpoint_t * streamendpoint){
    UNUSED(value_size);
    uint16_t pos = 0;

    if (value_size < 2){
        return 0;
    }

    streamendpoint->ase_characteristic->ase_id = value[pos++];
    streamendpoint->state = (ascs_state_t)value[pos++];

    if (value_size < pos){
        return pos;
    }
    uint8_t cig_id;
    uint8_t cis_id;

    switch (streamendpoint->state){
        case ASCS_STATE_CODEC_CONFIGURED:
            pos += ascs_util_codec_configuration_parse(&value[pos], value_size - pos, &streamendpoint->codec_configuration);
            break;
        
        case ASCS_STATE_QOS_CONFIGURED:
            pos += ascs_util_qos_configuration_parse(&value[pos], value_size - pos, &streamendpoint->qos_configuration);
            break;
        
        case ASCS_STATE_ENABLING:
        case ASCS_STATE_STREAMING:
        case ASCS_STATE_DISABLING:
            if ((value_size - pos) < 3){
                return 0;
            }
            cig_id = value[pos++];
            cis_id = value[pos++];
            
            if ( (cig_id != streamendpoint->qos_configuration.cig_id) ||
                 (cig_id != streamendpoint->qos_configuration.cis_id) ){
                return 0;
            }

            pos += le_audio_util_metadata_parse((uint8_t *)&value[pos], value_size - pos, &streamendpoint->metadata);
            break;
        
        default:
            break;           
    }
    return pos;
}

static void handle_gatt_server_control_point_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    ascs_client_connection_t * connection = ascs_client_get_connection_for_con_handle(channel);
    if (connection == NULL){
        return;
    }

    uint16_t value_handle =  gatt_event_notification_get_value_handle(packet);
    if (connection->control_point.value_handle != value_handle){
        return;
    }

    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    if (value_length < 5){
        return;
    }

    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);
    
    uint8_t  pos = 0;

    uint8_t opcode  = value[pos++];
    uint8_t ase_num = value[pos++]; // should be 1, as we send only single ASE operation
    btstack_assert(ase_num == 1);

    uint8_t ase_id = value[pos++];
    uint8_t response_code = value[pos++];
    uint8_t reason = value[pos++];
    ascs_client_emit_control_point_operation_response(connection->con_handle, opcode, ase_id, response_code, reason);
}

static void handle_gatt_server_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    ascs_client_connection_t * connection = ascs_client_get_connection_for_con_handle(channel);
    if (connection == NULL){
        return;
    }

    uint16_t value_handle =  gatt_event_notification_get_value_handle(packet);
    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);

    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_value_handle(connection, value_handle);
    if (streamendpoint == NULL){
        return;
    }

    uint16_t bytes_read = ascs_parse_ase(value, value_length, streamendpoint);
    if (bytes_read == value_length){
        ascs_client_emit_ase(connection, streamendpoint);
    }
}

static bool ascs_client_register_notification(ascs_client_connection_t * connection){
    ascs_streamendpoint_t * streamendpoint = &connection->streamendpoints[connection->streamendpoints_index];
    if (streamendpoint == NULL){
        return false;
    }

    ascs_streamendpoint_characteristic_t * ase_characteristic = streamendpoint->ase_characteristic;
    if (ase_characteristic == NULL){
        return false;
    }

    gatt_client_characteristic_t characteristic;
    characteristic.value_handle = ase_characteristic->ase_characteristic_value_handle;
    characteristic.properties   = ase_characteristic->ase_characteristic_properties;
    characteristic.end_handle   = ase_characteristic->ase_characteristic_end_handle;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
                &handle_gatt_client_event, 
                connection->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &streamendpoint->notification_listener, 
            &handle_gatt_server_notification, 
            connection->con_handle, &characteristic);
    } 
    return ERROR_CODE_SUCCESS;
}

uint16_t asce_client_store_ase(ascs_client_connection_t * connection, uint8_t * value, uint16_t value_size){
    UNUSED(value_size);
    uint16_t pos = 0;

    value[pos++] = connection->command_opcode;
    value[pos++] = 1;
    value[pos++] = connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_id;

    switch (connection->command_opcode){
        case ASCS_OPCODE_CONFIG_CODEC:
            pos += asce_util_codec_configuration_request_serialize(connection->codec_configuration, &value[pos], value_size - pos);
            break;
        case ASCS_OPCODE_CONFIG_QOS:
            pos += ascs_util_qos_configuration_serialize(connection->qos_configuration, &value[pos], value_size - pos);
            break;
        case ASCS_OPCODE_ENABLE:
        case ASCS_OPCODE_UPDATE_METADATA:
            pos += asce_util_metadata_serialize(connection->metadata, &value[pos], value_size - pos);
            break;
        default:
            break;           
    }
    return pos;
}

static void ascs_client_run_for_connection(ascs_client_connection_t * connection){
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;

    switch (connection->state){
        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(&handle_gatt_client_event, connection->con_handle, ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE;

            gatt_client_discover_characteristics_for_service(
                &handle_gatt_client_event, 
                connection->con_handle, 
                &service);

            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_QUERY_CHARACTERISTIC_DESCRIPTOR:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read control point client characteristic descriptors [handle 0x%04X]:\n", connection->control_point.value_handle);
#endif
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_CHARACTERISTIC_DESCRIPTOR_RESULT;
            characteristic.value_handle = connection->control_point.value_handle;
            characteristic.properties   = connection->control_point.properties;
            characteristic.end_handle   = connection->control_point.end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, connection->con_handle, &characteristic);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_REGISTER_NOTIFICATION:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_NOTIFICATION_REGISTERED;
    
            characteristic.value_handle = connection->control_point.value_handle;
            characteristic.properties   = connection->control_point.properties;
            characteristic.end_handle   = connection->control_point.end_handle;

            uint8_t status = gatt_client_write_client_characteristic_configuration(
                        &handle_gatt_client_event, 
                        connection->con_handle, 
                        &characteristic, 
                        GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
          
            // notification supported, register for value updates
            if (status == ERROR_CODE_SUCCESS){
                gatt_client_listen_for_characteristic_value_updates(
                    &connection->control_point.notification_listener, 
                    &handle_gatt_server_control_point_notification, 
                    connection->con_handle, &characteristic);
            
#ifdef ENABLE_TESTING_SUPPORT
                printf("Registered control point notification [handle 0x%04X]\n", connection->control_point.value_handle);
#endif          
                return;  
            } 
            
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors [index %d]:\n", connection->streamendpoints_index);
#endif
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_value_handle;
            characteristic.properties   = connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_properties;
            characteristic.end_handle   = connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, connection->con_handle, &characteristic);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic value [index %d, handle 0x%04X]:\n", connection->streamendpoints_index, connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_client_configuration_handle);
#endif
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                &handle_gatt_client_event,
                connection->con_handle, 
                connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_client_configuration_handle);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            status = ascs_client_register_notification(connection);
            if (status == ERROR_CODE_SUCCESS){
#ifdef ENABLE_TESTING_SUPPORT
                printf("Register notification [index %d, handle 0x%04X]: %s\n", 
                    connection->streamendpoints_index, 
                    connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_client_configuration_handle,
                    connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->role == LE_AUDIO_ROLE_SOURCE ? "SOURCE" : "SINK");
#endif          
                return;  
            } 
            
            if (connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_client_configuration_handle != 0){
                connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
            
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED;
            ascs_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_READ:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_READ;

            (void) gatt_client_read_value_of_characteristic_using_value_handle(
                &handle_gatt_client_event, 
                connection->con_handle, 
                connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_value_handle);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE:{
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_WRITTEN;

            uint16_t value_length = asce_client_store_ase(connection, ascs_client_value_buffer, ascs_client_get_value_buffer_size(connection));

            (void)gatt_client_write_value_of_characteristic(
                &handle_gatt_client_event,
                connection->con_handle, 
                connection->control_point.value_handle,
                value_length, 
                ascs_client_value_buffer); 
            break;
        }

        default:
            break;
    }
}

static bool ascs_client_handle_query_complete(ascs_client_connection_t * connection, uint8_t status){
    if (status != ATT_ERROR_SUCCESS){
        switch (connection->state){
            case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                ascs_client_emit_connection_established(connection, status);
                ascs_client_finalize_connection(connection);
                return false;
            default:
                break;
        }
    }

    switch (connection->state){
        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (connection->service_instances_num == 0){
                ascs_client_emit_connection_established(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                ascs_client_finalize_connection(connection);
                return false;
            }
            connection->streamendpoints_index = 0;
            connection->streamendpoints_instances_num = 0;
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_QUERY_CHARACTERISTIC_DESCRIPTOR;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_CHARACTERISTIC_DESCRIPTOR_RESULT:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_REGISTER_NOTIFICATION;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_NOTIFICATION_REGISTERED:
            connection->streamendpoints_index = 0;
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (connection->streamendpoints_index < (connection->streamendpoints_instances_num - 1)){
                connection->streamendpoints_index++;
                connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }
            connection->streamendpoints_index = 0;
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (connection->streamendpoints_index < (connection->streamendpoints_instances_num - 1)){
                connection->streamendpoints_index++;
                connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }

            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED;
            ascs_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED;
            ascs_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_READ:
        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_WRITTEN:
            ascs_client_value_buffer_used = false;
            connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED;
            break;

        case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED:
            if (status != ATT_ERROR_SUCCESS){ 
                break;
            }

            break;

        default:
            break;

    }
    return true;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    ascs_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
    ascs_streamendpoint_t * streamendpoint;

    bool call_run = true;
    uint16_t bytes_read;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("ASCS Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one ASCS Service instance. ");
            }
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);

            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_ASE_CONTROL_POINT:
                    connection->control_point.value_handle = characteristic.value_handle;
                    connection->control_point.properties = characteristic.properties;
                    connection->control_point.end_handle = characteristic.end_handle;
                    connection->control_point.uuid16     = characteristic.uuid16;
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_ASE:
                    if (connection->streamendpoints_instances_num < connection->max_streamendpoints_num){
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_value_handle = characteristic.value_handle;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_properties = characteristic.properties;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_end_handle = characteristic.end_handle;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->role = LE_AUDIO_ROLE_SOURCE;
                        connection->streamendpoints_instances_num++;
                    } else {
                        log_info("Found more ASCS streamendpoint chrs then it can be stored. ");
                    }
                    break;
                case ORG_BLUETOOTH_CHARACTERISTIC_SINK_ASE:
                    if (connection->streamendpoints_instances_num < connection->max_streamendpoints_num){
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_value_handle = characteristic.value_handle;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_properties = characteristic.properties;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->ase_characteristic_end_handle = characteristic.end_handle;
                        connection->streamendpoints[connection->streamendpoints_instances_num].ase_characteristic->role = LE_AUDIO_ROLE_SINK;
                        connection->streamendpoints_instances_num++;
                    } else {
                        log_info("Found more ASCS streamendpoint chrs then it can be stored. ");
                    }
                    break;
                default:
                    btstack_assert(false);
                    return;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("ASCS Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, ", 
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16);

            if (characteristic.uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_ASE_CONTROL_POINT){
                printf("CONTROL POINT\n"); 
            } else {
                printf("%s Streamendpoint\n", characteristic.uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_ASE ? "SOURCE": "SINK"); 
            }
            
#endif
            break;

        
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 != ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                break;
            }
            
            switch (connection->state){
                case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    connection->control_point.client_configuration_handle = characteristic_descriptor.handle;
                    break;

                case AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    connection->streamendpoints[connection->streamendpoints_index].ase_characteristic->ase_characteristic_client_configuration_handle = characteristic_descriptor.handle;
                    break;

                default:
                    btstack_assert(false);
                    break;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("    ASCS Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                characteristic_descriptor.handle,
                characteristic_descriptor.uuid16);
#endif
            break;
        

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(connection != NULL);                

            if (connection->state == AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
#ifdef ENABLE_TESTING_SUPPORT
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
#endif  
                break;
            }

            if (gatt_event_characteristic_value_query_result_get_value_length(packet) == 0 ){
                break;
            }
            
            streamendpoint = ascs_get_streamendpoint_for_value_handle(connection, gatt_event_characteristic_value_query_result_get_value_handle(packet));
            if (streamendpoint == NULL){
                return;
            }

            bytes_read = ascs_parse_ase(gatt_event_notification_get_value(packet), gatt_event_notification_get_value_length(packet), streamendpoint);
            
            if (bytes_read == gatt_event_characteristic_value_query_result_get_value_length(packet)){
                ascs_client_emit_ase(connection, streamendpoint);
            }
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = ascs_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = ascs_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        ascs_client_run_for_connection(connection);
    }
}

static void ascs_stream_endpoint_reset(ascs_streamendpoint_t * streamendpoint){
    streamendpoint->ase_characteristic_value_changed_w2_notify = false;
    streamendpoint->ase_characteristic_client_configuration = 0;
    memset(&streamendpoint->codec_configuration, 0, sizeof(ascs_codec_configuration_t));
    memset(&streamendpoint->qos_configuration, 0, sizeof(ascs_qos_configuration_t));
    memset(&streamendpoint->metadata, 0, sizeof(le_audio_metadata_t));
    
    memset(streamendpoint->ase_characteristic, 0, sizeof(ascs_streamendpoint_characteristic_t));
    streamendpoint->state = ASCS_STATE_IDLE;
}

uint8_t audio_stream_control_service_service_client_connect(ascs_client_connection_t * connection, 
    ascs_streamendpoint_characteristic_t * streamendpoint_characteristics, uint8_t streamendpoint_characteristics_num,
    hci_con_handle_t con_handle, uint16_t * ascs_cid){
    
    if (ascs_client_get_connection_for_con_handle(con_handle) != NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint16_t cid = ascs_client_get_next_cid();
    if (ascs_cid != NULL) {
        *ascs_cid = cid;
    }

    memset(connection, 0, sizeof(ascs_client_connection_t));
    connection->cid = cid;
    connection->con_handle = con_handle;
    connection->mtu = ATT_DEFAULT_MTU;

    connection->max_streamendpoints_num = streamendpoint_characteristics_num;
    uint8_t i;
    for (i = 0; i < connection->max_streamendpoints_num; i++){
        connection->streamendpoints[i].ase_characteristic = &streamendpoint_characteristics[i];
        ascs_stream_endpoint_reset(&connection->streamendpoints[i]);
    }

    connection->service_instances_num = 0;
    connection->streamendpoints_instances_num = 0;
    connection->streamendpoints_index = 0;

    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    btstack_linked_list_add(&ascs_connections, (btstack_linked_item_t *) connection);

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

static uint8_t ascs_client_connection_for_parameters_ready(uint16_t ascs_cid, uint8_t streamendpoint_index, bool write_requested, ascs_client_connection_t ** out_connection){
    // find connection
    ascs_client_connection_t * connection = ascs_get_client_connection_for_cid(ascs_cid);
    // set out variable
    *out_connection = connection;

    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (write_requested && ascs_client_value_buffer_used){
        return ERROR_CODE_CONTROLLER_BUSY;
    } 
    if (streamendpoint_index >= connection->streamendpoints_instances_num){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t audio_stream_control_service_client_read_streamendpoint(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, false, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    
    connection->streamendpoints_index = streamendpoint_index; 
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_READ;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t audio_stream_control_service_client_streamendpoint_configure_codec(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_client_codec_configuration_request_t * codec_configuration){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    if (codec_configuration == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_CONFIG_CODEC;
    connection->codec_configuration = codec_configuration;
    
    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Configure QoS.
 * @param con_handle
 * @param ase_id
 * @param qos_configuration
 */
uint8_t audio_stream_control_service_client_streamendpoint_configure_qos(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_qos_configuration_t * qos_configuration){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    if (qos_configuration == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_CONFIG_QOS;
    connection->qos_configuration = qos_configuration;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Update Metadata.
 * @param con_handle
 * @param ase_id
 * @param metadata_configuration
 */
uint8_t audio_stream_control_service_client_streamendpoint_metadata_update(uint16_t ascs_cid, uint8_t streamendpoint_index, le_audio_metadata_t * metadata){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    if (metadata == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_UPDATE_METADATA;
    connection->metadata = metadata;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Enable streamendpoint.
 * @param con_handle
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_enable(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
   
    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_ENABLE;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Start stream.
 * @param con_handle
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_start_ready(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_RECEIVER_START_READY;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Stop stream.
 * @param con_handle
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_stop_ready(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    
    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_RECEIVER_STOP_READY;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Release stream.
 * @param con_handle
 * @param ase_id
 * @param caching
 */
uint8_t audio_stream_control_service_client_streamendpoint_release(uint16_t ascs_cid, uint8_t streamendpoint_index, bool caching){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    
    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_RELEASE;
    
    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Disable streamendpoint.
 * @param con_handle
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_disable(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    
    ascs_client_value_buffer_used = true;
    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_DISABLE;
    
    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Release streamendpoint.
 * @param con_handle
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_released(uint16_t ascs_cid, uint8_t streamendpoint_index){
    ascs_client_connection_t * connection = NULL;
    uint8_t status = ascs_client_connection_for_parameters_ready(ascs_cid, streamendpoint_index, true, &connection);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    connection->state = AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE;
    ascs_client_value_buffer_used = true;
    connection->streamendpoints_index = streamendpoint_index;
    connection->command_opcode = ASCS_OPCODE_RELEASED;

    ascs_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}


static void ascs_client_reset(ascs_client_connection_t * connection){
    if (connection == NULL){
        return;
    }

    uint8_t i;
    for (i = 0; i < connection->streamendpoints_instances_num; i++){
        gatt_client_stop_listening_for_characteristic_value_updates(&connection->streamendpoints[i].notification_listener);
    }
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    hci_con_handle_t con_handle;
    ascs_client_connection_t * connection;
    
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = ascs_client_get_connection_for_con_handle(con_handle);
            if (connection != NULL){
                btstack_linked_list_remove(&ascs_connections, (btstack_linked_item_t *)connection);
                
                uint16_t cid = connection->cid;
                ascs_client_reset(connection);
                ascs_client_emit_disconnect(cid);
            }
            break;
        default:
            break;
    }
}

void audio_stream_control_service_client_init(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    ascs_event_callback = packet_handler;
    ascs_client_value_buffer_used = false;

    ascs_client_hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&ascs_client_hci_event_callback_registration);
}


void audio_stream_control_service_client_deinit(void){
    ascs_event_callback = NULL;
    ascs_client_value_buffer_used = false;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &ascs_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ascs_client_connection_t * connection = (ascs_client_connection_t *)btstack_linked_list_iterator_next(&it);
        btstack_linked_list_remove(&ascs_connections, (btstack_linked_item_t *)connection);
    }
}


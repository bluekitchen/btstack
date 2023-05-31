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

#define BTSTACK_FILE__ "broadcast_audio_scan_service_client.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "btstack_memory.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/broadcast_audio_scan_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

static btstack_linked_list_t bass_client_connections;

static uint16_t bass_client_cid_counter = 0;
static btstack_packet_handler_t bass_client_event_callback;

static void bass_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t bass_client_get_next_cid(void){
    bass_client_cid_counter = btstack_next_cid_ignoring_zero(bass_client_cid_counter);
    return bass_client_cid_counter;
}

static void bass_client_finalize_connection(bass_client_connection_t * connection){
    btstack_linked_list_remove(&bass_client_connections, (btstack_linked_item_t*) connection);
}

static bass_client_connection_t * bass_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &bass_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        bass_client_connection_t * connection = (bass_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static bass_client_connection_t * bass_client_get_connection_for_cid(uint16_t bass_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &bass_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        bass_client_connection_t * connection = (bass_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->cid != bass_cid) continue;
        return connection;
    }
    return NULL;
}

static bass_client_source_t * bass_client_get_receive_state_for_value_handle(bass_client_connection_t * connection, uint16_t value_handle){
    uint8_t i;
    for (i = 0; i < connection->receive_states_instances_num; i++){
        if (connection->receive_states[i].receive_state_value_handle == value_handle){
            return &connection->receive_states[i];
        }
    }
    return NULL;
}

static bass_client_source_t * bass_client_get_source_for_source_id(bass_client_connection_t * connection, uint8_t source_id){
    uint8_t i;
    for (i = 0; i < connection->receive_states_instances_num; i++){
        if (connection->receive_states[i].source_id == source_id){
            return &connection->receive_states[i];
        }
    }
    return NULL;
}

static void bass_client_reset_source(bass_client_source_t * source){
    if (source == NULL){
        return;
    }
    source->source_id = BASS_INVALID_SOURCE_INDEX;
    source->in_use = false;
    memset(&source->data, 0, sizeof(bass_source_data_t));
}

static void bass_client_emit_connection_established(bass_client_connection_t * connection, uint8_t status){
    btstack_assert(bass_client_event_callback != NULL);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    (*bass_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_scan_operation_complete(bass_client_connection_t * connection, uint8_t status, bass_opcode_t opcode){
    btstack_assert(bass_client_event_callback != NULL);
    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_CLIENT_SCAN_OPERATION_COMPLETE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)opcode;
    (*bass_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_source_operation_complete(bass_client_connection_t * connection, uint8_t status, bass_opcode_t opcode, uint8_t source_id){
    btstack_assert(bass_client_event_callback != NULL);
    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_CLIENT_SOURCE_OPERATION_COMPLETE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)opcode;
    event[pos++] = source_id;
    (*bass_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_receive_state(bass_client_connection_t * connection, uint8_t source_id){
    btstack_assert(bass_client_event_callback != NULL);
    uint8_t pos = 0;
    uint8_t event[7];
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_CLIENT_NOTIFICATION_COMPLETE;
    little_endian_store_16(event, pos, connection->cid);
    pos += 2;
    event[pos++] = source_id;
    (*bass_client_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static bool bass_client_remote_broadcast_receive_state_buffer_valid(uint8_t *buffer, uint16_t buffer_size){
    // minimal with zero subgroups
    if (buffer_size < 15){
        return false;
    }

    uint16_t pos = 0;

    // source_id
    pos++;

    // addr type
    uint8_t adv_type = buffer[pos++];
    if (adv_type > (uint8_t)BD_ADDR_TYPE_LE_RANDOM){
        log_info("Unexpected adv_type 0x%02X", adv_type);
        return false;
    }

    // address
    pos += 6;

    // advertising_sid Range: 0x00-0x0F
    uint8_t advertising_sid = buffer[pos++];
    if (advertising_sid > 0x0F){
        log_info("Advertising sid out of range 0x%02X", advertising_sid);
        return false;
    }

    // broadcast_id
    pos += 3;

    // pa_sync_state
    uint8_t pa_sync_state = buffer[pos++];
    if (pa_sync_state >= (uint8_t)LE_AUDIO_PA_SYNC_STATE_RFU){
        log_info("Unexpected pa_sync_state 0x%02X", pa_sync_state);
        return false;
    }

    // big_encryption
    le_audio_big_encryption_t big_encryption = (le_audio_big_encryption_t) buffer[pos++];
    if (big_encryption == LE_AUDIO_BIG_ENCRYPTION_BAD_CODE){
        // Bad Code
        pos += 16;
    }

    // num subgroups
    uint8_t num_subgroups = buffer[pos++];
    if (num_subgroups > BASS_SUBGROUPS_MAX_NUM){
        log_info("Number of subgroups %u exceedes maximum %u", num_subgroups, BASS_SUBGROUPS_MAX_NUM);
        return false;
    }

    uint8_t i;
    for (i = 0; i < num_subgroups; i++) {
        // check if we can read bis_sync_state + meta_data_length
        // bis_sync_state
        pos += 4;
        // meta_data_length
        if (pos >= buffer_size){
            return false;
        }
        uint8_t metadata_length = buffer[pos++];
        if ((pos + metadata_length) > buffer_size){
            return false;
        }
        // metadata
        pos += metadata_length;
    }
    return true;
}

static void bass_client_handle_gatt_server_notification(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION){
        return;
    }

    bass_client_connection_t * connection = bass_client_get_connection_for_con_handle(channel);
    if (connection == NULL){
        return;
    }

    uint16_t value_handle =  gatt_event_notification_get_value_handle(packet);
    uint16_t value_length = gatt_event_notification_get_value_length(packet);
    uint8_t * value = (uint8_t *)gatt_event_notification_get_value(packet);

    if (bass_client_remote_broadcast_receive_state_buffer_valid(value, value_length)){
        bass_client_source_t * source = bass_client_get_receive_state_for_value_handle(connection, value_handle);
        if (source == NULL){
            return;
        }
        source->source_id = value[0];
        bass_util_source_data_parse(&value[1], value_length - 1, &source->data, true);

        // get big encryption + bad code
        source->big_encryption = (le_audio_big_encryption_t) value[13];
        if (source->big_encryption == LE_AUDIO_BIG_ENCRYPTION_BAD_CODE){
            reverse_128(&value[14], source->bad_code);
        } else {
            memset(source->bad_code, 0, 16);
        }

        bass_client_emit_receive_state(connection, source->source_id);
    }
}

static bool bass_client_register_notification(bass_client_connection_t * connection){
    bass_client_source_t * receive_state = &connection->receive_states[connection->receive_states_index];
    if (receive_state == NULL){
        return false;
    }

    gatt_client_characteristic_t characteristic;
    characteristic.value_handle = receive_state->receive_state_value_handle;
    characteristic.properties   = receive_state->receive_state_properties;
    characteristic.end_handle   = receive_state->receive_state_end_handle;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
            &bass_client_handle_gatt_client_event,
                connection->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &connection->notification_listener,
            &bass_client_handle_gatt_server_notification,
            connection->con_handle, &characteristic);
    } 
    return status;
}

static uint16_t bass_client_prepare_add_source_buffer(bass_client_connection_t * connection){
    const bass_source_data_t * receive_state = connection->control_point_operation_data;

    uint16_t  buffer_offset = connection->buffer_offset;
    uint8_t * buffer        = connection->buffer;
    uint16_t  buffer_size   = btstack_min(sizeof(connection->buffer), connection->mtu);

    btstack_assert(buffer_offset == 0);

    uint8_t  field_data[6];
    uint16_t source_offset = 0;
    uint16_t stored_bytes = 0;
    memset(buffer, 0, buffer_size);

    field_data[0] = (uint8_t)BASS_OPCODE_ADD_SOURCE;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    stored_bytes += bass_util_source_data_header_virtual_memcpy(receive_state, &source_offset, buffer_offset, buffer,
                                                                buffer_size);

    field_data[0] = (uint8_t)receive_state->pa_sync;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    little_endian_store_16(field_data, 0, receive_state->pa_interval);
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 2, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset += 2;

    stored_bytes += bass_util_source_data_subgroups_virtual_memcpy(receive_state, false, &source_offset, buffer_offset,
                                                                   buffer,
                                                                   buffer_size);
    
    return stored_bytes;
}

static uint16_t bass_client_prepare_modify_source_buffer(bass_client_connection_t * connection){
    const bass_source_data_t * receive_state = connection->control_point_operation_data;

    uint16_t  buffer_offset = connection->buffer_offset;
    uint8_t * buffer        = connection->buffer;
    uint16_t  buffer_size   = btstack_min(sizeof(connection->buffer), connection->mtu);

    btstack_assert(buffer_offset == 0);

    uint8_t  field_data[6];
    uint16_t source_offset = 0;
    uint16_t stored_bytes = 0;
    memset(buffer, 0, buffer_size);

    field_data[0] = (uint8_t)BASS_OPCODE_MODIFY_SOURCE;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;
    
    field_data[0] = connection->control_point_operation_source_id;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    field_data[0] = (uint8_t)receive_state->pa_sync;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    little_endian_store_16(field_data, 0, receive_state->pa_interval);
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 2, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset += 2;

    stored_bytes += bass_util_source_data_subgroups_virtual_memcpy(receive_state, false, &source_offset, buffer_offset,
                                                                   buffer,
                                                                   buffer_size);
    return stored_bytes;
}

static uint16_t bass_client_receive_state_len(const bass_source_data_t * source_data){
    uint16_t source_len = 0;
    // opcode(1), address_type(1), address(6), adv_sid(1), broadcast_id(3), pa_sync(1), subgroups_num(1)
    source_len = 1 + 1 + 6 + 1 + 3 + 1 + 1;

    uint8_t i;
    for (i = 0; i < source_data->subgroups_num; i++){
        bass_subgroup_t subgroup = source_data->subgroups[i];
        // bis_sync(4), metadata_length(1), metadata_length
        source_len += 4 + 1 + subgroup.metadata_length;
    }
    return source_len;
}

static void bass_client_run_for_connection(bass_client_connection_t * connection){
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;

    uint8_t  value[18];
    uint16_t stored_bytes;

    switch (connection->state){
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED:
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_START_SCAN:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Write START SCAN [0x%04X]:\n", 
                connection->control_point_value_handle);
#endif

            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_START_SCAN;
            value[0] = BASS_OPCODE_REMOTE_SCAN_STARTED;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic(
                    &bass_client_handle_gatt_client_event, connection->con_handle,
                    connection->control_point_value_handle, 1, &value[0]);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_STOP_SCAN:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Write START SCAN [0x%04X]:\n", 
                connection->control_point_value_handle);
#endif

            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_STOP_SCAN;
            value[0] = BASS_OPCODE_REMOTE_SCAN_STOPPED;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic(
                    &bass_client_handle_gatt_client_event, connection->con_handle,
                    connection->control_point_value_handle, 1, &value[0]);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_ADD_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE;
#ifdef ENABLE_TESTING_SUPPORT
            printf("    ADD SOURCE [0x%04X]:\n", connection->control_point_value_handle);
#endif      
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE;
            stored_bytes = bass_client_prepare_add_source_buffer(connection);
            connection->buffer_offset += stored_bytes;

            gatt_client_write_long_value_of_characteristic(&bass_client_handle_gatt_client_event, connection->con_handle,
                                                           connection->control_point_value_handle, stored_bytes, connection->buffer);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_MODIFY_SOURCE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    MODIFY SOURCE [%d]:\n", connection->control_point_operation_source_id);
#endif    
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_MODIFY_SOURCE;
            stored_bytes = bass_client_prepare_modify_source_buffer(connection);
            connection->buffer_offset += stored_bytes;

            gatt_client_write_long_value_of_characteristic(&bass_client_handle_gatt_client_event, connection->con_handle,
                                                           connection->control_point_value_handle, stored_bytes, connection->buffer);
            
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_REMOVE_SOURCE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    REMOVE SOURCE  [%d]:\n", connection->control_point_operation_source_id);
#endif    
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_REMOVE_SOURCE;
            value[0] = BASS_OPCODE_REMOVE_SOURCE;
            value[1] = connection->control_point_operation_source_id;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic(
                    &bass_client_handle_gatt_client_event, connection->con_handle,
                    connection->control_point_value_handle, 2, &value[0]);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_SET_BROADCAST_CODE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    SET BROADCAST CODE [%d]:\n", connection->control_point_operation_source_id);
#endif    
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_SET_BROADCAST_CODE;
            value[0] = BASS_OPCODE_SET_BROADCAST_CODE;
            value[1] = connection->control_point_operation_source_id;
            reverse_128(connection->broadcast_code, &value[2]);

            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic(
                    &bass_client_handle_gatt_client_event, connection->con_handle,
                    connection->control_point_value_handle, 18, &value[0]);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(&bass_client_handle_gatt_client_event, connection->con_handle, ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = connection->start_handle;
            service.end_group_handle = connection->end_handle;
            service.uuid16 = ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE;

            gatt_client_discover_characteristics_for_service(
                    &bass_client_handle_gatt_client_event,
                connection->con_handle, 
                &service);

            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic descriptors [index %d]:\n", connection->receive_states_index);
#endif
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            characteristic.value_handle = connection->receive_states[connection->receive_states_index].receive_state_value_handle;
            characteristic.properties   = connection->receive_states[connection->receive_states_index].receive_state_properties;
            characteristic.end_handle   = connection->receive_states[connection->receive_states_index].receive_state_end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&bass_client_handle_gatt_client_event, connection->con_handle, &characteristic);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Read client characteristic value [index %d, handle 0x%04X]:\n", connection->receive_states_index, connection->receive_states[connection->receive_states_index].receive_state_ccc_handle);
#endif
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                    &bass_client_handle_gatt_client_event,
                connection->con_handle, 
                connection->receive_states[connection->receive_states_index].receive_state_ccc_handle);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
#ifdef ENABLE_TESTING_SUPPORT
            printf("Register notification [index %d, handle 0x%04X]:\n", connection->receive_states_index, connection->receive_states[connection->receive_states_index].receive_state_ccc_handle);
#endif
      
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            status = bass_client_register_notification(connection);
            if (status == ERROR_CODE_SUCCESS) return;
            
            if (connection->receive_states[connection->receive_states_index].receive_state_ccc_handle != 0){
                connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
            
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;
        default:
            break;
    }
}
// @return true if client valid / run function should be called
static bool bass_client_handle_query_complete(bass_client_connection_t * connection, uint8_t status){
    switch (connection->state){
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                bass_client_emit_connection_established(connection, status);
                bass_client_finalize_connection(connection);
                return false;
            }

            if (connection->service_instances_num == 0){
                bass_client_emit_connection_established(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                bass_client_finalize_connection(connection);
                return false;
            }
            connection->receive_states_index = 0;
            connection->receive_states_instances_num = 0;
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                bass_client_emit_connection_established(connection, status);
                bass_client_finalize_connection(connection);
                return false;
            }

            connection->receive_states_index = 0;
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
            if (connection->receive_states_index < (connection->receive_states_instances_num - 1)){
                connection->receive_states_index++;
                connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                break;
            }
            connection->receive_states_index = 0;
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if (connection->receive_states_index < (connection->receive_states_instances_num - 1)){
                connection->receive_states_index++;
                connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }

            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_connection_established(connection, ERROR_CODE_SUCCESS);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_START_SCAN:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_scan_operation_complete(connection, status, BASS_OPCODE_REMOTE_SCAN_STARTED);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_STOP_SCAN:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_scan_operation_complete(connection, status, BASS_OPCODE_REMOTE_SCAN_STOPPED);
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_ADD_SOURCE, BASS_INVALID_SOURCE_INDEX);
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_MODIFY_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_MODIFY_SOURCE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_REMOVE_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_reset_source(
                    bass_client_get_source_for_source_id(connection, connection->control_point_operation_source_id));
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_REMOVE_SOURCE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_SET_BROADCAST_CODE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_SET_BROADCAST_CODE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED:
            if (status != ATT_ERROR_SUCCESS){ 
                break;
            }

            break;

        default:
            break;

    }
    return true;
}

static void bass_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    bass_client_connection_t * connection = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;

    bool call_run = true;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_MTU:
            connection = bass_client_get_connection_for_con_handle(gatt_event_mtu_get_handle(packet));
            btstack_assert(connection != NULL);
            connection->mtu = gatt_event_mtu_get_MTU(packet);
            break;

        case GATT_EVENT_SERVICE_QUERY_RESULT:
            connection = bass_client_get_connection_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            if (connection->service_instances_num < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                connection->start_handle = service.start_group_handle;
                connection->end_handle   = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("BASS Service: start handle 0x%04X, end handle 0x%04X\n", connection->start_handle, connection->end_handle);
#endif          
                connection->service_instances_num++;
            } else {
                log_info("Found more then one BASS Service instance. ");
            }
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            connection = bass_client_get_connection_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(connection != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);

            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_AUDIO_SCAN_CONTROL_POINT:
                    connection->control_point_value_handle = characteristic.value_handle;
                    break;
                case ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE:
                    if (connection->receive_states_instances_num < connection->max_receive_states_num){
                        connection->receive_states[connection->receive_states_instances_num].receive_state_value_handle = characteristic.value_handle;
                        connection->receive_states[connection->receive_states_instances_num].receive_state_properties = characteristic.properties;
                        connection->receive_states[connection->receive_states_index].receive_state_end_handle = characteristic.end_handle;
                        connection->receive_states_instances_num++;
                    } else {
                        log_info("Found more BASS receive_states chrs then it can be stored. ");
                    }
                    break;
                default:
                    btstack_assert(false);
                    return;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("BASS Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, %s\n", 
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16,
                characteristic.uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE ? "RECEIVE_STATE" : "CONTROL_POINT");
#endif
            break;

        
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            connection = bass_client_get_connection_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(connection != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                connection->receive_states[connection->receive_states_index].receive_state_ccc_handle = characteristic_descriptor.handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("    BASS Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                    characteristic_descriptor.handle,
                    characteristic_descriptor.uuid16);
#endif
            }
            break;
        

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            connection = bass_client_get_connection_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(connection != NULL);                

            if (connection->state == BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
#ifdef ENABLE_TESTING_SUPPORT
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
#endif  
                break;
            }

            if (gatt_event_characteristic_value_query_result_get_value_length(packet) == 0 ){
                break;
            }
               
            // TODO 
            // bass_client_emit_receive_state(connection, 
            //     gatt_event_characteristic_value_query_result_get_value_handle(packet), 
            //     ATT_ERROR_SUCCESS,
            //     gatt_event_notification_get_value(packet),
            //     gatt_event_notification_get_value_length(packet));
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            connection = bass_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(connection != NULL);
            call_run = bass_client_handle_query_complete(connection, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (connection != NULL)){
        bass_client_run_for_connection(connection);
    }
}

void broadcast_audio_scan_service_client_init(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    bass_client_event_callback = packet_handler;
}

uint8_t broadcast_audio_scan_service_client_connect(bass_client_connection_t * connection, 
    bass_client_source_t * receive_states, uint8_t receive_states_num, 
    hci_con_handle_t con_handle, uint16_t * bass_cid){

    btstack_assert(receive_states != NULL);
    btstack_assert(receive_states_num > 0);
    
    if (bass_client_get_connection_for_con_handle(con_handle) != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = bass_client_get_next_cid();
    if (bass_cid != NULL) {
        *bass_cid = cid;
    }
    
    connection->cid = cid;
    connection->con_handle = con_handle;
    connection->mtu = ATT_DEFAULT_MTU;
    
    connection->max_receive_states_num = receive_states_num;
    connection->receive_states = receive_states;
    
    uint8_t i;
    for (i = 0; i < connection->max_receive_states_num; i++){
        connection->receive_states[i].in_use = false;
        connection->receive_states[i].source_id = BASS_INVALID_SOURCE_INDEX;
    }

    connection->service_instances_num = 0;
    connection->receive_states_instances_num = 0;
    connection->receive_states_index = 0;

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    btstack_linked_list_add(&bass_client_connections, (btstack_linked_item_t *) connection);

    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_scanning_started(uint16_t bass_cid){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_START_SCAN;
    bass_client_run_for_connection(connection);
     return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_scanning_stopped(uint16_t bass_cid){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_STOP_SCAN;
    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_add_source(uint16_t bass_cid, const bass_source_data_t * add_source_data){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_ADD_SOURCE;
    connection->control_point_operation_data = add_source_data;
    connection->buffer_offset = 0;
    connection->data_size = bass_client_receive_state_len(add_source_data);

    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_modify_source(uint16_t bass_cid, uint8_t source_id, const bass_source_data_t * modify_source_data){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_MODIFY_SOURCE;
    connection->control_point_operation_data = modify_source_data;
    connection->control_point_operation_source_id = source_id;
    connection->buffer_offset = 0;
    connection->data_size = bass_client_receive_state_len(modify_source_data);

    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_remove_source(uint16_t bass_cid, uint8_t source_id){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_REMOVE_SOURCE;
    connection->control_point_operation_source_id = source_id;

    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t broadcast_audio_scan_service_client_set_broadcast_code(uint16_t bass_cid, uint8_t source_id, const uint8_t * broadcast_code){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_SET_BROADCAST_CODE;
    connection->control_point_operation_source_id = source_id;
    connection->broadcast_code = broadcast_code;

    bass_client_run_for_connection(connection);
    return ERROR_CODE_SUCCESS;
}

const bass_source_data_t * broadcast_audio_scan_service_client_get_source_data(uint16_t bass_cid, uint8_t source_id){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return NULL;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return NULL;
    }
    return (const bass_source_data_t *) &bass_client_get_source_for_source_id(connection, source_id)->data;
}

uint8_t broadcast_audio_scan_service_client_get_encryption_state(uint16_t bass_cid, uint8_t source_id,
                                                                 le_audio_big_encryption_t * out_big_encryption, uint8_t * out_bad_code){
    btstack_assert(out_big_encryption != NULL);
    btstack_assert(out_bad_code != NULL);

    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    bass_client_source_t * source = bass_client_get_source_for_source_id(connection, source_id);
    if (source == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    *out_big_encryption = source->big_encryption;
    memcpy(out_bad_code, source->bad_code, 16);
    return ERROR_CODE_SUCCESS;
}

void broadcast_audio_scan_service_client_deinit(uint16_t bass_cid){
    bass_client_event_callback = NULL;
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return;
    }
    // finalize connections
    bass_client_finalize_connection(connection);
}


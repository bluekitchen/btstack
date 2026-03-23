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
#include "ble/gatt_service_client.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

// BASS Client
static gatt_service_client_t bass_client;
static btstack_linked_list_t bass_client_connections;

static void bass_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

typedef enum {
    BASS_CHARACTERISTIC_INDEX_BROADCAST_AUDIO_SCAN_CONTROL_POINT = 0,
    BASS_CHARACTERISTIC_INDEX_BROADCAST_RECEIVE_STATE_1,
    BASS_CHARACTERISTIC_INDEX_BROADCAST_RECEIVE_STATE_2,
    BASS_CHARACTERISTIC_INDEX_BROADCAST_RECEIVE_STATE_3,
    BASS_CHARACTERISTIC_INDEX_RFU
} bass_client_characteristic_index_t;

static const uint16_t bass_uuid16s[] = {
    ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_AUDIO_SCAN_CONTROL_POINT,
    ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE,
    ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE,
};


static void bass_client_finalize_connection(bass_client_connection_t * connection){
    btstack_linked_list_remove(&bass_client_connections, (btstack_linked_item_t*) connection);
}

static bass_client_connection_t * bass_client_get_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &bass_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        bass_client_connection_t * connection = (bass_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (gatt_service_client_get_con_handle(&connection->basic_connection) == con_handle){
            return connection;
        }
    }
    return NULL;
}

static bass_client_connection_t * bass_client_get_connection_for_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &bass_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        bass_client_connection_t * connection = (bass_client_connection_t *)btstack_linked_list_iterator_next(&it);
        if (gatt_service_client_get_connection_id(&connection->basic_connection) == cid) {
            return connection;
        }
    }
    return NULL;
}

static uint8_t bass_client_get_source_index_for_characteristic_index(bass_client_characteristic_index_t characteristic_index){
    if (characteristic_index == BASS_CHARACTERISTIC_INDEX_BROADCAST_AUDIO_SCAN_CONTROL_POINT){
        return GATT_SERVICE_CLIENT_INVALID_INDEX;
    }
    if (characteristic_index >= BASS_CHARACTERISTIC_INDEX_RFU){
        return GATT_SERVICE_CLIENT_INVALID_INDEX;
    }
    return (uint8_t)characteristic_index - 1;

}

static bass_client_source_t * bass_client_get_source_for_characteristic_index(bass_client_connection_t * connection, bass_client_characteristic_index_t characteristic_index){
    uint8_t source_index = bass_client_get_source_index_for_characteristic_index(characteristic_index);
    if (source_index == GATT_SERVICE_CLIENT_INVALID_INDEX){
        return NULL;
    }
    return &connection->receive_states[source_index];
}

static bass_client_source_t * bass_client_get_source_for_value_handle(bass_client_connection_t * connection, uint16_t value_handle){
    uint16_t characteristic_index = gatt_service_client_characteristic_index_for_value_handle(&connection->basic_connection, value_handle);
    return bass_client_get_source_for_characteristic_index(connection, (bass_client_characteristic_index_t)characteristic_index);
}


static bass_client_source_t * bass_client_get_source_for_source_id(bass_client_connection_t * connection, uint8_t source_id){
    uint8_t i;
    for (i = 0; i < MAX_NR_BASS_RECEIVE_STATES; i++){
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
    btstack_assert(connection != NULL);
    btstack_assert(connection->packet_handler != NULL);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_BASS_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, connection->basic_connection.con_handle);
    pos += 2;
    little_endian_store_16(event, pos, connection->basic_connection.cid);
    pos += 2;
    event[pos++] = status;
    (*connection->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_scan_operation_complete(bass_client_connection_t * connection, uint8_t status, bass_opcode_t opcode){
    btstack_assert(connection != NULL);
    btstack_assert(connection->packet_handler != NULL);

    uint8_t event[7];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_BASS_CLIENT_SCAN_OPERATION_COMPLETE;
    little_endian_store_16(event, pos, connection->basic_connection.cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)opcode;
    (*connection->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_source_operation_complete(bass_client_connection_t * connection, uint8_t status, bass_opcode_t opcode, uint8_t source_id){
    btstack_assert(connection != NULL);
    btstack_assert(connection->packet_handler != NULL);

    uint8_t event[8];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_BASS_CLIENT_SOURCE_OPERATION_COMPLETE;
    little_endian_store_16(event, pos, connection->basic_connection.cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = (uint8_t)opcode;
    event[pos++] = source_id;
    (*connection->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_client_emit_receive_state(bass_client_connection_t * connection, uint8_t source_id){
    btstack_assert(connection != NULL);
    btstack_assert(connection->packet_handler != NULL);

    uint8_t pos = 0;
    uint8_t event[7];
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_BASS_CLIENT_NOTIFICATION_COMPLETE;
    little_endian_store_16(event, pos, connection->basic_connection.cid);
    pos += 2;
    event[pos++] = source_id;
    (*connection->packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static bool bass_client_remote_broadcast_receive_state_buffer_valid(const uint8_t *buffer, uint16_t buffer_size){
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
    if (num_subgroups > MAX_NR_BASS_SUBGROUPS){
        log_info("Number of subgroups %u exceeds maximum %u", num_subgroups, MAX_NR_BASS_SUBGROUPS);
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

static void bass_client_handle_gatt_server_notification(bass_client_connection_t * connection, uint16_t value_handle, uint16_t value_length, const uint8_t * value){
    if (bass_client_remote_broadcast_receive_state_buffer_valid(value, value_length)){
        bass_client_source_t * source = bass_client_get_source_for_value_handle(connection, value_handle);
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

static uint16_t bass_client_prepare_add_source_buffer(bass_client_connection_t * connection){
    const bass_source_data_t * receive_state = connection->control_point_operation_data;

    uint16_t  buffer_offset = connection->long_write_buffer_offset;
    uint8_t * buffer        = connection->long_write_buffer;
    uint16_t  buffer_size   = btstack_min(sizeof(connection->long_write_buffer), gatt_service_client_get_mtu(&connection->basic_connection));

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

    uint16_t  buffer_offset = connection->long_write_buffer_offset;
    uint8_t * buffer        = connection->long_write_buffer;
    uint16_t  buffer_size   = btstack_min(sizeof(connection->long_write_buffer), gatt_service_client_get_mtu(&connection->basic_connection));

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
    // opcode(1), address_type(1), address(6), adv_sid(1), broadcast_id(3), pa_sync(1), subgroups_num(1)
    uint16_t source_len = 1 + 1 + 6 + 1 + 3 + 1 + 1;

    uint8_t i;
    for (i = 0; i < source_data->subgroups_num; i++){
        bass_subgroup_t subgroup = source_data->subgroups[i];
        // bis_sync(4), metadata_length(1), metadata_length
        source_len += 4 + 1 + subgroup.metadata_length;
    }
    return source_len;
}

static void bass_client_run_for_connection(void * context){
    uint16_t connection_id = (uint16_t)(uintptr_t)context;
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(connection_id);
    btstack_assert(connection != NULL);

    uint8_t status;
    uint8_t  value[18];
    uint16_t stored_bytes;
    uint16_t control_point_value_handle = connection->characteristics_storage[BASS_CHARACTERISTIC_INDEX_BROADCAST_AUDIO_SCAN_CONTROL_POINT].value_handle;

    switch (connection->state){
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY:
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_START_SCAN:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Write START SCAN [0x%04X]:\n", control_point_value_handle);
#endif

            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_START_SCAN;
            value[0] = BASS_OPCODE_REMOTE_SCAN_STARTED;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic_with_context(
                    &bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    control_point_value_handle, 1, &value[0],
                    bass_client.service_id, connection_id);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_STOP_SCAN:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Write START SCAN [0x%04X]:\n", control_point_value_handle);
#endif

            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_STOP_SCAN;
            value[0] = BASS_OPCODE_REMOTE_SCAN_STOPPED;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic_with_context(
                    &bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    control_point_value_handle, 1, &value[0],
                    bass_client.service_id, connection_id);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_ADD_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE;
#ifdef ENABLE_TESTING_SUPPORT
            printf("    ADD SOURCE [0x%04X]:\n", control_point_value_handle);
#endif      
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE;
            stored_bytes = bass_client_prepare_add_source_buffer(connection);
            connection->long_write_buffer_offset += stored_bytes;

            status = gatt_client_write_long_value_of_characteristic_with_context(&bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                                                           control_point_value_handle, stored_bytes, connection->long_write_buffer,
                                                           bass_client.service_id, connection_id);
            UNUSED(status);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_MODIFY_SOURCE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    MODIFY SOURCE [%d]:\n", connection->control_point_operation_source_id);
#endif    
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_MODIFY_SOURCE;
            stored_bytes = bass_client_prepare_modify_source_buffer(connection);
            connection->long_write_buffer_offset += stored_bytes;

            status = gatt_client_write_long_value_of_characteristic_with_context(&bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                                                           control_point_value_handle, stored_bytes, connection->long_write_buffer,
                                                           bass_client.service_id, connection_id);
            UNUSED(status);
            break;
        
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_REMOVE_SOURCE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    REMOVE SOURCE  [%d]:\n", connection->control_point_operation_source_id);
#endif    
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_REMOVE_SOURCE;
            value[0] = BASS_OPCODE_REMOVE_SOURCE;
            value[1] = connection->control_point_operation_source_id;
            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic_with_context(
                    &bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    control_point_value_handle, 2, &value[0],
                    bass_client.service_id, connection_id);
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
            status = gatt_client_write_value_of_characteristic_with_context(
                    &bass_client_handle_gatt_client_event, connection->basic_connection.con_handle,
                    control_point_value_handle, 18, &value[0],
                    bass_client.service_id, connection_id);
            UNUSED(status);
            break;

        default:
            break;
    }
}

static void bass_client_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_QUERY_COMPLETE) {
        return;
    }

    bass_client_connection_t * connection = bass_client_get_connection_for_con_handle(gatt_event_query_complete_get_handle(packet));
    btstack_assert(connection != NULL);
    uint8_t status = gatt_event_query_complete_get_att_status(packet);
    switch (connection->state){
        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_START_SCAN:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_emit_scan_operation_complete(connection, status, BASS_OPCODE_REMOTE_SCAN_STARTED);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_STOP_SCAN:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_emit_scan_operation_complete(connection, status, BASS_OPCODE_REMOTE_SCAN_STOPPED);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_ADD_SOURCE, BASS_INVALID_SOURCE_INDEX);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_MODIFY_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_MODIFY_SOURCE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_REMOVE_SOURCE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_reset_source(
                    bass_client_get_source_for_source_id(connection, connection->control_point_operation_source_id));
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_REMOVE_SOURCE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_SET_BROADCAST_CODE:
            connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
            bass_client_emit_source_operation_complete(connection, status, BASS_OPCODE_SET_BROADCAST_CODE, connection->control_point_operation_source_id);
            break;

        case BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY:
            break;

        default:
            break;

    }
}

static void bass_client_replace_subevent_id_and_emit(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size, uint8_t subevent_id){
    btstack_assert(callback != NULL);
    // execute callback
    packet[2] = subevent_id;
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void bass_client_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    bass_client_connection_t * connection;
    uint16_t connection_id;
    uint8_t status;
    uint16_t value_handle;
    uint16_t value_length;
    const uint8_t  * value;

    switch(hci_event_packet_get_type(packet)){
        case HCI_EVENT_GATTSERVICE_META:
            switch (hci_event_gattservice_meta_get_subevent_code(packet)){
                case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED:
                    connection_id = gattservice_subevent_client_connected_get_cid(packet);
                    connection = bass_client_get_connection_for_cid(connection_id);
                    btstack_assert(connection != NULL);

                    status = gattservice_subevent_client_connected_get_status(packet);
                    if (status == ERROR_CODE_SUCCESS){
                        connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
                        bass_client_emit_connection_established(connection, status);
                    } else {
                        connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_IDLE;
                        bass_client_emit_connection_established(connection, status);
                        bass_client_finalize_connection(connection);
                    }
                    break;
                    break;

                case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                    connection_id = gattservice_subevent_client_disconnected_get_cid(packet);
                    connection = bass_client_get_connection_for_cid(connection_id);
                    btstack_assert(connection != NULL);
                    bass_client_finalize_connection(connection);
                    bass_client_replace_subevent_id_and_emit(connection->packet_handler, packet, size, LEAUDIO_SUBEVENT_BASS_CLIENT_DISCONNECTED);
                    break;

                default:
                    break;
            }
            break;
        case GATT_EVENT_NOTIFICATION:
            connection_id = gatt_event_notification_get_connection_id(packet);
            connection = bass_client_get_connection_for_cid(connection_id);
            btstack_assert(connection != NULL);
            value_handle =  gatt_event_notification_get_value_handle(packet);
            value_length =  gatt_event_notification_get_value_length(packet);
            value        =  gatt_event_notification_get_value(packet);

            bass_client_handle_gatt_server_notification(connection, value_handle, value_length, value);
            break;

        default:
            break;
    }
}


void broadcast_audio_scan_service_client_init(void){
    gatt_service_client_register_client_with_uuid16s(&bass_client, &bass_client_packet_handler_internal, bass_uuid16s, sizeof(bass_uuid16s)/sizeof(uint16_t));
}

uint8_t broadcast_audio_scan_service_client_connect(
        hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler,
        bass_client_connection_t * connection,
        uint16_t * bass_cid){

    btstack_assert(packet_handler != NULL);
    btstack_assert(connection != NULL);

    *bass_cid = 0;
    connection->packet_handler = packet_handler;
    connection->gatt_query_can_send_now.callback = &bass_client_run_for_connection;
    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CONNECTION;

    uint8_t i;
    for (i = 0; i < MAX_NR_BASS_RECEIVE_STATES; i++) {
        connection->receive_states[i].in_use = false;
        connection->receive_states[i].source_id = BASS_INVALID_SOURCE_INDEX;
    }

    uint8_t status = gatt_service_client_connect_primary_service_with_uuid16(con_handle,
                                                                             &bass_client,
                                                                             &connection->basic_connection,
                                                                             ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE,
                                                                             connection->characteristics_storage,
                                                                             BASS_CLIENT_CHARACTERISTICS_MAX_COUNT);
    if (status == ERROR_CODE_SUCCESS){
        btstack_linked_list_add(&bass_client_connections, (btstack_linked_item_t*) connection);
        *bass_cid = connection->basic_connection.cid;
    }
    return status;
}

static uint8_t bass_client_request_to_send_gatt_query(bass_client_connection_t * connection) {
    connection->gatt_query_can_send_now.context = (void *)(uintptr_t) connection->basic_connection.cid;
    connection->gatt_query_can_send_now.callback = &bass_client_run_for_connection;

    uint8_t status = gatt_client_request_to_send_gatt_query(&connection->gatt_query_can_send_now, connection->basic_connection.con_handle);

    if (status != ERROR_CODE_SUCCESS){
        connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY;
    }
    return status;
}

uint8_t broadcast_audio_scan_service_client_scanning_started(uint16_t bass_cid){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_START_SCAN;
    return bass_client_request_to_send_gatt_query(connection);
}

uint8_t broadcast_audio_scan_service_client_scanning_stopped(uint16_t bass_cid){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_STOP_SCAN;
    return bass_client_request_to_send_gatt_query(connection);
}

uint8_t broadcast_audio_scan_service_client_add_source(uint16_t bass_cid, const bass_source_data_t * add_source_data){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_ADD_SOURCE;
    connection->control_point_operation_data = add_source_data;
    connection->long_write_buffer_offset = 0;
    connection->data_size = bass_client_receive_state_len(add_source_data);

    return bass_client_request_to_send_gatt_query(connection);
}

uint8_t broadcast_audio_scan_service_client_modify_source(uint16_t bass_cid, uint8_t source_id, const bass_source_data_t * modify_source_data){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_MODIFY_SOURCE;
    connection->control_point_operation_data = modify_source_data;
    connection->control_point_operation_source_id = source_id;
    connection->long_write_buffer_offset = 0;
    connection->data_size = bass_client_receive_state_len(modify_source_data);

    return bass_client_request_to_send_gatt_query(connection);
}

uint8_t broadcast_audio_scan_service_client_remove_source(uint16_t bass_cid, uint8_t source_id){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_REMOVE_SOURCE;
    connection->control_point_operation_source_id = source_id;

    return bass_client_request_to_send_gatt_query(connection);
}

uint8_t broadcast_audio_scan_service_client_set_broadcast_code(uint16_t bass_cid, uint8_t source_id, const uint8_t * broadcast_code){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_SET_BROADCAST_CODE;
    connection->control_point_operation_source_id = source_id;
    connection->broadcast_code = broadcast_code;

    return bass_client_request_to_send_gatt_query(connection);
}

const bass_source_data_t * broadcast_audio_scan_service_client_get_source_data(uint16_t bass_cid, uint8_t source_id){
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return NULL;
    }
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
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
    if (connection->state != BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_READY){
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
    bass_client_connection_t * connection = bass_client_get_connection_for_cid(bass_cid);
    if (connection == NULL){
        return;
    }
    // finalize connections
    bass_client_finalize_connection(connection);
}


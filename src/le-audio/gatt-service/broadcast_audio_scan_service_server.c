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

#define BTSTACK_FILE__ "broadcast_audio_scan_service_server.c"

#include <stdio.h>

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/broadcast_audio_scan_service_server.h"
#include "le-audio/le_audio_util.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define BASS_MAX_NOTIFY_BUFFER_SIZE                             200
#define BASS_INVALID_SOURCE_INDEX                               0xFF
            
static att_service_handler_t    broadcast_audio_scan_service;
static btstack_packet_handler_t bass_server_event_callback;

// characteristic: AUDIO_SCAN_CONTROL_POINT
static uint16_t bass_audio_scan_control_point_handle;

static uint8_t  bass_logic_time = 0;

static bass_server_source_t * bass_sources;
static uint8_t  bass_sources_num = 0;
static bass_server_connection_t * bass_clients;
static uint8_t  bass_clients_num = 0;
static btstack_context_callback_registration_t  scheduled_tasks_callback;

static uint8_t bass_server_get_next_update_counter(void){
    uint8_t next_update_counter;
    if (bass_logic_time == 0xff) {
        next_update_counter = 0;
    } else {
        next_update_counter = bass_logic_time + 1;
    }
    bass_logic_time = next_update_counter;
    return next_update_counter;
}

// returns positive number if counter a > b
static int8_t bass_server_counter_delta(uint8_t counter_a, uint8_t counter_b){
    return (int8_t)(counter_a - counter_b);
}

static uint8_t bass_server_find_empty_or_last_used_source_index(void){
    bass_server_source_t * last_used_source = NULL;
    uint8_t last_used_source_index = BASS_INVALID_SOURCE_INDEX;

    uint8_t i;
    for (i = 0; i < bass_sources_num; i++){
        if (!bass_sources[i].in_use){
            return i;
        }
        if (last_used_source == NULL){
            last_used_source = &bass_sources[i];
            last_used_source_index = i;
            continue;
        }
        if (bass_server_counter_delta(bass_sources[i].update_counter, last_used_source->update_counter) < 0 ){
            last_used_source = &bass_sources[i];
            last_used_source_index = i;
        }
    }
    return last_used_source_index;
}

static bass_server_source_t *  bass_server_find_empty_or_last_used_source(void){
    uint8_t last_used_source_index = bass_server_find_empty_or_last_used_source_index();
    if (last_used_source_index == BASS_INVALID_SOURCE_INDEX){
        return NULL;
    }
    return &bass_sources[last_used_source_index];
}

static bass_server_source_t * bass_server_find_receive_state_for_value_handle(uint16_t attribute_handle){
    uint16_t i;
    for (i = 0; i < bass_sources_num; i++){
        if (attribute_handle == bass_sources[i].bass_receive_state_handle){
            return &bass_sources[i];
        }
    }
    return NULL;
}

static bass_server_source_t * bass_server_find_receive_state_for_client_configuration_handle(uint16_t attribute_handle){
    if (attribute_handle == 0){
        return NULL;
    }
    uint8_t i;
    for (i = 0; i < bass_sources_num; i++){
        if (bass_sources[i].bass_receive_state_client_configuration_handle == attribute_handle){
            return &bass_sources[i];
        }
    }
    return NULL;
}

static bass_server_source_t * bass_server_find_source_for_source_id(uint8_t source_id){
    if (source_id < bass_sources_num){
        return &bass_sources[source_id];
    }
    return NULL;
}

static bass_server_connection_t * bass_server_find_client_for_con_handle(hci_con_handle_t con_handle){
    uint16_t i;
    for (i = 0; i < bass_clients_num; i++){
        if (bass_clients[i].con_handle == con_handle) {
            return &bass_clients[i];
        }
    }
    return NULL;
}

static void bass_server_register_con_handle(hci_con_handle_t con_handle, uint16_t client_configuration){
    bass_server_connection_t * client = bass_server_find_client_for_con_handle(con_handle);
    if (client == NULL){
        client = bass_server_find_client_for_con_handle(HCI_CON_HANDLE_INVALID);
        if (client == NULL){
            return;
        }

    }
    client->con_handle = (client_configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static void bass_server_source_emit_scan_stoped(hci_con_handle_t con_handle){
    btstack_assert(bass_server_event_callback != NULL);
    
    uint8_t event[5];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_SERVER_SCAN_STOPPED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*bass_server_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_server_source_emit_scan_started(hci_con_handle_t con_handle){
    btstack_assert(bass_server_event_callback != NULL);
    
    uint8_t event[5];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_SERVER_SCAN_STARTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*bass_server_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static void bass_server_source_emit_source_state_changed(uint8_t subevent_id, hci_con_handle_t con_handle, uint8_t source_id, le_audio_pa_sync_t pa_sync){
    btstack_assert(bass_server_event_callback != NULL);
    
    uint8_t event[7];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_id;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = source_id;
    event[pos++] = (uint8_t)pa_sync;
    (*bass_server_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void bass_server_source_emit_source_added(hci_con_handle_t con_handle, bass_server_source_t * source){
    bass_server_source_emit_source_state_changed(GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_ADDED, con_handle, source->source_id, source->data.pa_sync);
}

static void bass_server_source_emit_source_modified(hci_con_handle_t con_handle, bass_server_source_t * source){
    bass_server_source_emit_source_state_changed(GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_MODIFIED, con_handle, source->source_id, source->data.pa_sync);
}

static void bass_server_source_emit_source_deleted(hci_con_handle_t con_handle, bass_server_source_t * source){
    bass_server_source_emit_source_state_changed(GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_DELETED, con_handle, source->source_id, LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA);
}

static void bass_server_source_emit_broadcast_code(hci_con_handle_t con_handle, uint8_t source_id, const uint8_t * broadcast_code){
    btstack_assert(bass_server_event_callback != NULL);
    
    uint8_t event[22];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BASS_SERVER_BROADCAST_CODE;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = source_id;
    reverse_128(broadcast_code, &event[pos]);
    pos += 16;
    (*bass_server_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// offset gives position into fully serialized bass record
static uint16_t bass_server_copy_source_to_buffer(bass_server_source_t * source, uint16_t buffer_offset, uint8_t * buffer, uint16_t buffer_size){
    uint8_t  field_data[16];
    uint16_t source_offset = 0;
    uint16_t stored_bytes = 0;
    memset(buffer, 0, buffer_size);

    if (!source->in_use){
        return 0;
    }
    field_data[0] = source->source_id;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    stored_bytes += bass_util_source_data_header_virtual_memcpy(&source->data, &source_offset, buffer_offset, buffer,
                                                                buffer_size);

    field_data[0] = (uint8_t)source->data.pa_sync_state;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    field_data[0] = (uint8_t)source->big_encryption;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, source_offset, buffer, buffer_size,
                                                        buffer_offset);
    source_offset++;

    if (source->big_encryption == LE_AUDIO_BIG_ENCRYPTION_BAD_CODE){
        reverse_128(source->bad_code, &field_data[0]);
        stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 16, source_offset, buffer, buffer_size,
                                                            buffer_offset);
        source_offset += 16;
    }

    stored_bytes += bass_util_source_data_subgroups_virtual_memcpy(&source->data, true, &source_offset, buffer_offset,
                                                                   buffer,
                                                                   buffer_size);
    return stored_bytes;
}

static uint16_t bass_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    
    bass_server_source_t * source = bass_server_find_receive_state_for_value_handle(attribute_handle);
    if (source){
        return bass_server_copy_source_to_buffer(source, offset, buffer, buffer_size);
    }

    source = bass_server_find_receive_state_for_client_configuration_handle(attribute_handle);
    if (source){
        return att_read_callback_handle_little_endian_16(source->bass_receive_state_client_configuration, offset, buffer, buffer_size);
    }

    return 0;
}

static bool bass_server_source_remote_modify_source_buffer_valid(uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size < 10){ 
        log_info("Modify Source opcode, buffer too small");
        return false;
    }
    
    uint8_t pos = 1; // source_id
    return bass_util_pa_sync_state_and_subgroups_in_valid_range(buffer+pos, buffer_size-pos);
}

static void bass_server_add_source_from_buffer(uint8_t *buffer, uint16_t buffer_size, bass_server_source_t * source){
    UNUSED(buffer_size);
    
    source->update_counter = bass_server_get_next_update_counter();
    source->in_use = true;

    bass_util_source_data_parse(buffer, buffer_size, &source->data, false);
}

static bool bass_server_pa_synchronized(bass_server_source_t * source){
    return source->data.pa_sync_state == LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA;
}


static bool bass_server_bis_synchronized(bass_server_source_t * source){
    uint8_t i;
    for (i = 0; i < source->data.subgroups_num; i++){
        if ((source->data.subgroups[i].bis_sync_state > 0) && (source->data.subgroups[i].bis_sync_state < 0xFFFFFFFF)){
            return true;
        }
    }
    return false;
}


static void bass_server_reset_source(bass_server_source_t * source){
    source->in_use = false;
    source->data.address_type = BD_ADDR_TYPE_LE_PUBLIC; 
    memset(source->data.address, 0, sizeof(source->data.address));
    source->data.adv_sid = 0;
    source->data.broadcast_id = 0;
    source->data.pa_sync = LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA;
    source->data.pa_sync_state = LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA;
    source->big_encryption = LE_AUDIO_BIG_ENCRYPTION_NOT_ENCRYPTED;
    memset(source->bad_code, 0, sizeof(source->bad_code));
    source->data.pa_interval = 0;
    source->data.subgroups_num = 0;
    memset(source->data.subgroups, 0, sizeof(source->data.subgroups));
}

static void bass_server_reset_client_long_write_buffer(bass_server_connection_t * client){
    memset(client->long_write_buffer, 0, sizeof(client->long_write_buffer));
    client->long_write_value_size = 0;
}

static int bass_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    // printf("bass_server_write_callback con_handle 0x%02x, attr_handle 0x%02x \n", con_handle, attribute_handle);
    if (attribute_handle != 0 && attribute_handle != bass_audio_scan_control_point_handle){
        bass_server_source_t * source = bass_server_find_receive_state_for_client_configuration_handle(attribute_handle);
        if (source){
            source->bass_receive_state_client_configuration = little_endian_read_16(buffer, 0);
            bass_server_register_con_handle(con_handle, source->bass_receive_state_client_configuration);
        }
        return 0;
    }     
    
    bass_server_connection_t * connection = bass_server_find_client_for_con_handle(con_handle);
    if (connection == NULL){
        return ATT_ERROR_WRITE_REQUEST_REJECTED;
    }

    uint16_t total_value_len = buffer_size + offset;

    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_NONE:
            if (buffer_size > sizeof(connection->long_write_buffer)){
                bass_server_reset_client_long_write_buffer(connection);
                return ATT_ERROR_WRITE_REQUEST_REJECTED;
            }
            memcpy(&connection->long_write_buffer[0], buffer, buffer_size);
            connection->long_write_value_size = total_value_len;
            break;

        case ATT_TRANSACTION_MODE_ACTIVE:
            if (total_value_len > sizeof(connection->long_write_buffer)){
                bass_server_reset_client_long_write_buffer(connection);
                return ATT_ERROR_WRITE_REQUEST_REJECTED;
            }
            // allow overlapped and/or mixed order chunks
            connection->long_write_attribute_handle = attribute_handle;
            
            memcpy(&connection->long_write_buffer[offset], buffer, buffer_size);
            if (total_value_len > connection->long_write_value_size){
                connection->long_write_value_size = total_value_len;
            }
            return 0;

        case ATT_TRANSACTION_MODE_VALIDATE:
            return 0;

        case ATT_TRANSACTION_MODE_EXECUTE:
            attribute_handle = connection->long_write_attribute_handle;
            break;

        default:
            return 0;
    }

    if (attribute_handle == bass_audio_scan_control_point_handle){
        if (connection->long_write_value_size < 2){
            return ATT_ERROR_WRITE_REQUEST_REJECTED;
        }

        bass_opcode_t opcode = (bass_opcode_t)connection->long_write_buffer[0];
        uint8_t  *remote_data = &connection->long_write_buffer[1];
        uint16_t remote_data_size = connection->long_write_value_size - 1;
        
        bass_server_source_t * source;
        uint8_t broadcast_code[16];
        switch (opcode){
            case BASS_OPCODE_REMOTE_SCAN_STOPPED:
                if (remote_data_size != 1){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }
                bass_server_source_emit_scan_stoped(con_handle);
                break;

            case BASS_OPCODE_REMOTE_SCAN_STARTED:
                if (remote_data_size != 1){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }
                bass_server_source_emit_scan_started(con_handle);
                break;

            case BASS_OPCODE_ADD_SOURCE:
                if (!bass_util_source_buffer_in_valid_range(remote_data, remote_data_size)){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }
                source = bass_server_find_empty_or_last_used_source();
                btstack_assert(source != NULL);
                log_info("add source %d", source->source_id);
                bass_server_add_source_from_buffer(remote_data, remote_data_size, source);
                bass_server_source_emit_source_added(con_handle, source);
                // server needs to trigger notification
                break;

            case BASS_OPCODE_MODIFY_SOURCE:
                if (!bass_server_source_remote_modify_source_buffer_valid(remote_data, remote_data_size)){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }
                
                source = bass_server_find_source_for_source_id(remote_data[0]);
                if (source == NULL){
                    return BASS_ERROR_CODE_INVALID_SOURCE_ID;
                }
                bass_util_pa_info_and_subgroups_parse(remote_data + 1, remote_data_size - 1, &source->data, false);
                bass_server_source_emit_source_modified(con_handle, source);
                // server needs to trigger notification
                break;

            case BASS_OPCODE_SET_BROADCAST_CODE:
                if (remote_data_size != 17){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }

                source = bass_server_find_source_for_source_id(remote_data[0]);
                if (source == NULL){
                    return BASS_ERROR_CODE_INVALID_SOURCE_ID;
                }
                reverse_128(&remote_data[1], broadcast_code);
                bass_server_source_emit_broadcast_code(con_handle, source->source_id, broadcast_code);
                break;

            case BASS_OPCODE_REMOVE_SOURCE:
                if (remote_data_size != 1){
                    return ATT_ERROR_WRITE_REQUEST_REJECTED;
                }
                source = bass_server_find_source_for_source_id(remote_data[0]);
                if (source == NULL){
                    return BASS_ERROR_CODE_INVALID_SOURCE_ID;
                }

                if (bass_server_pa_synchronized(source)){
                    log_info("remove source %d rejected, PA synchronised", source->source_id);
                    return 0;
                } 

                if (bass_server_bis_synchronized(source)){
                    log_info("remove source %d rejected, BIS synchronised", source->source_id);
                    return 0;
                }

                bass_server_reset_source(source);
                broadcast_audio_scan_service_server_set_pa_sync_state(source->source_id, LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA);
                bass_server_source_emit_source_deleted(con_handle, source);
                break;

            default:
                bass_server_reset_client_long_write_buffer(connection);
                return BASS_ERROR_CODE_OPCODE_NOT_SUPPORTED;
        }
        bass_server_reset_client_long_write_buffer(connection);
    }
    return 0;
}

static void bass_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    bass_server_connection_t * connection;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);

            connection = bass_server_find_client_for_con_handle(con_handle);
            if (connection == NULL){
                break;
            }
            
            memset(connection, 0, sizeof(bass_server_connection_t));
            connection->con_handle = HCI_CON_HANDLE_INVALID;
            break;
        default:
            break;
    }
}

void broadcast_audio_scan_service_server_init(const uint8_t sources_num, bass_server_source_t * sources, const uint8_t clients_num, bass_server_connection_t * clients){
    // get service handle range
    btstack_assert(sources_num != 0);
    btstack_assert(clients_num != 0);
    
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    bool service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found);

    UNUSED(service_found);

    bass_audio_scan_control_point_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_AUDIO_SCAN_CONTROL_POINT);
    bass_sources_num = 0;
    bass_logic_time = 0;
    bass_sources = sources;

#ifdef ENABLE_TESTING_SUPPORT
    printf("BASS 0x%02x - 0x%02x \n", start_handle, end_handle);
#endif
    uint16_t start_chr_handle = start_handle;
    while ( (start_chr_handle < end_handle) && (bass_sources_num < sources_num )) {
        uint16_t chr_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_chr_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE);
        uint16_t chr_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_chr_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE);
        
        if (chr_value_handle == 0){
            break;
        }
        bass_server_source_t * source = &bass_sources[bass_sources_num];
        bass_server_reset_source(source);

        source->source_id = bass_sources_num;
        source->update_counter = bass_server_get_next_update_counter();
        source->bass_receive_state_client_configuration = 0;

        source->bass_receive_state_handle = chr_value_handle;
        source->bass_receive_state_client_configuration_handle = chr_client_configuration_handle;

#ifdef ENABLE_TESTING_SUPPORT
    printf("    bass_receive_state_%d                 0x%02x \n", bass_sources_num, source->bass_receive_state_handle);
    printf("    bass_receive_state_%d CCC             0x%02x \n", bass_sources_num, source->bass_receive_state_client_configuration_handle);
#endif

        start_chr_handle = chr_client_configuration_handle + 1;
        bass_sources_num++;
    }

    bass_clients_num = clients_num;
    bass_clients = clients;
    memset(bass_clients, 0, sizeof(bass_server_connection_t) * bass_clients_num);
    uint8_t i;
    for (i = 0; i < bass_clients_num; i++){
        bass_clients[i].con_handle = HCI_CON_HANDLE_INVALID;
    }

    log_info("Found BASS service 0x%02x-0x%02x (num sources %d)", start_handle, end_handle, bass_sources_num);

    // register service with ATT Server
    broadcast_audio_scan_service.start_handle   = start_handle;
    broadcast_audio_scan_service.end_handle     = end_handle;
    broadcast_audio_scan_service.read_callback  = &bass_server_read_callback;
    broadcast_audio_scan_service.write_callback = &bass_server_write_callback;
    broadcast_audio_scan_service.packet_handler = bass_server_packet_handler;
    att_server_register_service_handler(&broadcast_audio_scan_service);
}

void broadcast_audio_scan_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    bass_server_event_callback = packet_handler;
}

static void bass_service_can_send_now(void * context){
    bass_server_connection_t * client = (bass_server_connection_t *) context;
    btstack_assert(client != NULL);

    uint8_t source_index;
    for (source_index = 0; source_index < bass_sources_num; source_index++){
        uint8_t task = (1 << source_index);
        if ((client->sources_to_notify & task) != 0){
            client->sources_to_notify &= ~task;
            uint8_t  buffer[BASS_MAX_NOTIFY_BUFFER_SIZE];
            uint16_t bytes_copied = bass_server_copy_source_to_buffer(&bass_sources[source_index], 0, buffer, sizeof(buffer));
            att_server_notify(client->con_handle, bass_sources[source_index].bass_receive_state_handle, &buffer[0], bytes_copied);
            return;
        }
    }

    uint8_t i;
    for (i = 0; i < bass_clients_num; i++){
        client = &bass_clients[i];

        if (client->sources_to_notify != 0){
            scheduled_tasks_callback.callback = &bass_service_can_send_now;
            scheduled_tasks_callback.context  = (void*) client;
            att_server_register_can_send_now_callback(&scheduled_tasks_callback, client->con_handle);
            return;
        }
    }
}

static void bass_server_set_callback(uint8_t source_index){
    // there is only one type of task: notify on source state change
    // as task we register which source is changed, and the change will be propagated to all clients
    uint8_t i;
    uint8_t task = (1 << source_index);

    uint8_t scheduled_tasks = 0;

    for (i = 0; i < bass_clients_num; i++){
        bass_server_connection_t * connection = &bass_clients[i];

        if (connection->con_handle == HCI_CON_HANDLE_INVALID){
            connection->sources_to_notify &= ~task;
            return;
        }    
        
        scheduled_tasks |= connection->sources_to_notify;
        connection->sources_to_notify |= task;

        if (scheduled_tasks == 0){
            scheduled_tasks_callback.callback = &bass_service_can_send_now;
            scheduled_tasks_callback.context  = (void*) connection;
            att_server_register_can_send_now_callback(&scheduled_tasks_callback, connection->con_handle);
        }
    }
}

void broadcast_audio_scan_service_server_set_pa_sync_state(uint8_t source_index, le_audio_pa_sync_state_t sync_state){
    btstack_assert(source_index < bass_sources_num);

    bass_server_source_t * source = &bass_sources[source_index];
    source->data.pa_sync_state = sync_state;
    
    if (source->bass_receive_state_client_configuration != 0){
        bass_server_set_callback(source_index);
    }
}

void broadcast_audio_scan_service_server_add_source(const bass_source_data_t *source_data, uint8_t * source_index){
    *source_index = bass_server_find_empty_or_last_used_source_index();
    if (*source_index == BASS_INVALID_SOURCE_INDEX){
        return;
    }
    bass_server_source_t * last_used_source = &bass_sources[*source_index];
    last_used_source->update_counter = bass_server_get_next_update_counter();
    last_used_source->in_use = true;
    last_used_source->source_id = *source_index;
    memcpy(&last_used_source->data, source_data, sizeof(bass_source_data_t));
}

void broadcast_audio_scan_service_server_deinit(void){
    bass_server_event_callback = NULL;
}

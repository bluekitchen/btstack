/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hearing_access_service_server.c"

/**
 * Implementation of the GATT Hearing Access Service Server
 * To use with your application, add '#import <hearing_access_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "btstack_defines.h"
#include "btstack_event.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "le-audio/gatt-service/hearing_access_service_util.h"
#include "le-audio/gatt-service/hearing_access_service_server.h"

// notification that should be sent to all connected clients
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED                                      0x00000001
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED                                    0x00000002
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE                                     0x00000004
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE                                  0x00000008
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE                                0x00000010
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_UPDATE_NAME                                0x00000020

// notifications that should be sent to a specific client
#define HAS_CONNECTION_TASK_READ_PRESETS                                               0x00000001
#define HAS_CONNECTION_TASK_SEND_CHANGED_PRESETS_ON_RECONNECT                          0x00000002


// Index in an preset record array is identical to the preset record id:
// with an exception that the preset record id of 0 is reserved
#define HAS_PRESET_RECORD_RESERVED_INDEX                                                0x00

// invalid index is used to signal that the max storage capacity has been exceeded.
#define HAS_PRESET_RECORD_INVALID_INDEX                                                 0xFF
#define HAS_PRESET_RECORD_INVALID_POSITION                                              0xFF

static att_service_handler_t hearing_access_service;

static has_server_connection_t * has_clients;
static uint8_t  has_clients_max_num = 0;

static btstack_packet_handler_t has_server_event_callback;

static uint16_t   has_control_point_value_handle;
static uint16_t   has_control_point_client_configuration_handle;
static uint16_t   has_control_point_client_configuration;

static uint16_t   has_hearing_aid_features_value_handle;
// The server shall expose the same value of the Hearing Aid Features to all clients.
static uint8_t    has_hearing_aid_features;

static uint16_t   has_hearing_aid_features_client_configuration_handle;
static uint16_t   has_hearing_aid_features_client_configuration;

static uint16_t   has_active_preset_index_value_handle;
static uint8_t    has_active_preset_index;
static has_preset_record_t * has_preset_records;

static uint8_t  has_preset_records_max_num = 0;
static uint8_t  has_preset_records_num = 0;
static uint8_t  has_queued_preset_records_num = 0;

static uint16_t   has_active_preset_index_client_configuration_handle;
static uint16_t   has_active_preset_index_client_configuration;

static bool has_server_schedule_preset_record_task(void);
static void has_server_can_send_now(void * context);


static uint8_t has_server_preset_position_for_index(uint8_t index){
    if (index == HAS_PRESET_RECORD_RESERVED_INDEX){
        return HAS_PRESET_RECORD_INVALID_POSITION;
    }
    if (index > has_preset_records_max_num){
        return HAS_PRESET_RECORD_INVALID_POSITION;
    }
    return index - 1;
}

static has_preset_record_t * has_server_preset_record_for_index(uint8_t index){
    uint8_t position = has_server_preset_position_for_index(index);
    if (position == HAS_PRESET_RECORD_INVALID_POSITION){
        return NULL;
    }
    return &has_preset_records[position];
}

static uint8_t has_server_preset_record_index(uint8_t position){
    if (position >= has_preset_records_max_num){
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }
    return has_preset_records[position].index;
}

static uint8_t has_server_active_preset_position(void){
    return has_server_preset_position_for_index(has_active_preset_index);
}

static bool has_server_preset_record_available(uint8_t position){
    if (position >= has_preset_records_max_num){
        return false;
    }
    return (has_preset_records[position].index != HAS_PRESET_RECORD_INVALID_INDEX) && ((has_preset_records[position].properties & HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE) > 0u);
}

static void dump_preset_records(char * msg){
#ifdef ENABLE_TESTING_SUPPORT
    printf("%s: regular %d\n", msg, has_preset_records_num);
    uint8_t pos;
    for (pos = 0; pos < has_preset_records_max_num; pos++){
        has_preset_record_t * preset = &has_preset_records[pos];
        if (preset->index != HAS_PRESET_RECORD_INVALID_INDEX) {
            printf("N[%02d] - index %02d, Writable %d, Available %d, Active %d, scheduled_task = %d, Name %s\n", pos,
                   preset->index,
                   (preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE) > 0 ? 1u : 0u,
                   (preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE) > 0 ? 1u : 0u,
                   preset->active ? 1u : 0u, preset->scheduled_task, preset->name);
        }
    }
#else
    UNUSED(msg);
    UNUSED(pos);
    UNUSED(index);
#endif
}

static void has_clear_preset_record(has_preset_record_t * preset) {// clear the last element (optional, for cleanliness)
    preset->index = HAS_PRESET_RECORD_INVALID_INDEX;
    preset->properties = 0;
    preset->name[0] = '\0';
    preset->active = false;
    preset->scheduled_task = 0;
}

static has_server_connection_t * has_server_find_connection_for_con_handle(hci_con_handle_t con_handle){
    uint8_t i;
    for (i = 0; i < has_clients_max_num; i++){
        if (has_clients[i].con_handle == con_handle) {
            return &has_clients[i];
        }
    }
    return NULL;
}

static void has_server_reset_client(has_server_connection_t * connection){
    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->scheduled_preset_record_change_notification = false;
    connection->scheduled_control_point_notification_tasks = 0u;
}

static has_preset_record_t * has_server_preset_iterator_get_next(has_server_connection_t * connection, bool * is_last_preset){
    // check if it is initial value
    if (connection->current_position == HAS_PRESET_RECORD_INVALID_POSITION) {
        // find first index equal to or greater than start_index
        connection->current_position = has_server_preset_position_for_index(connection->start_index);
    }

    bool preset_found = false;
    while (!preset_found && (connection->current_position < (has_preset_records_max_num - 1))){
        connection->current_position++;
        has_preset_record_t *preset = &has_preset_records[connection->current_position];

        if (preset->index != HAS_PRESET_RECORD_INVALID_INDEX) {
            preset_found = true;
            connection->num_presets_already_read++;
            *is_last_preset = (connection->num_presets_to_read == connection->num_presets_already_read) || (has_preset_records_num == connection->num_presets_already_read);
            printf("Preset found R[%02d] [%d: %d], last %d\n", connection->current_position, connection->num_presets_to_read, connection->num_presets_already_read, *is_last_preset);
            return &has_preset_records[connection->current_position];
        }
    }
    return NULL;
}

static has_preset_record_t * has_read_presets_operation_get_changed_preset_record(has_server_connection_t * connection, bool * is_last_preset, uint8_t * prev_index){
    uint8_t num_presets_already_read = 0;
    *prev_index = HAS_PRESET_RECORD_RESERVED_INDEX;
    uint8_t i;
    for (i = 0; i < has_preset_records_max_num; i++){
        has_preset_record_t *preset = &has_preset_records[i];
        if (preset->index != HAS_PRESET_RECORD_INVALID_INDEX) {
            num_presets_already_read++;

            if (preset->scheduled_task > 0u) {
                *is_last_preset = (has_preset_records_num == num_presets_already_read);
                return preset;
            }
            *prev_index = i;
        }
    }
    return NULL;
}


static void has_server_schedule_control_point_task(has_server_connection_t * connection, uint8_t scheduled_task) {
    connection->scheduled_control_point_notification_tasks = scheduled_task;
    has_server_schedule_preset_record_task();
    dump_preset_records("ADD CP Operation");
}

static has_server_connection_t * has_server_add_connection(hci_con_handle_t con_handle){
    uint8_t i;

    for (i = 0; i < has_clients_max_num; i++){
        if (has_clients[i].con_handle == HCI_CON_HANDLE_INVALID){
            has_server_reset_client(&has_clients[i]);
            has_clients[i].con_handle = con_handle;
            log_info("added client 0x%02x, index %d", con_handle, i);
            return &has_clients[i];
        }
    }
    return NULL;
}

static uint16_t has_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    has_server_connection_t * connection = has_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        connection = has_server_add_connection(con_handle);

        if (connection == NULL){
            // has_server_emit_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new connection.", has_clients_max_num);
            return 0;
        } else {
            // has_server_emit_connected(con_handle, ERROR_CODE_SUCCESS);
        }
    }

    if (attribute_handle == has_hearing_aid_features_value_handle){
        return att_read_callback_handle_byte(has_hearing_aid_features, offset, buffer, buffer_size);
    }

    if (attribute_handle == has_active_preset_index_value_handle){
        return att_read_callback_handle_byte(has_active_preset_index, offset, buffer, buffer_size);
    }

    if (attribute_handle == has_control_point_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(has_control_point_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == has_hearing_aid_features_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(has_hearing_aid_features_client_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == has_active_preset_index_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(has_active_preset_index_client_configuration, offset, buffer, buffer_size);
    }

    return 0;
}

static bool has_valid_opcode_parameters_length(has_opcode_t opcode, uint16_t parameter_length){
    switch (opcode) {
        case HAS_OPCODE_READ_PRESETS_REQUEST:
            return (parameter_length == 2u);

        case HAS_OPCODE_WRITE_PRESET_NAME:
            return (parameter_length < 42u) && (parameter_length > 1u);

        case HAS_OPCODE_SET_ACTIVE_PRESET:
        case HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY:
            return (parameter_length == 1u);
            
        case HAS_OPCODE_SET_NEXT_PRESET:
        case HAS_OPCODE_SET_PREVIOUS_PRESET:
        case HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY:
        case HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY:
            return (parameter_length == 0u);
        
        default:
            btstack_unreachable();
    }
}

static has_preset_record_t * find_regular_preset_record_with_index(uint8_t index){
    uint8_t i;
    for (i = 0; i < has_preset_records_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        if (preset->index == index){
            return preset;
        }
    }
    return NULL;
}

static uint8_t is_read_operation_valid(has_server_connection_t * connection, uint8_t index, uint8_t num_presets){
    if ((index == HAS_PRESET_RECORD_INVALID_INDEX) || (num_presets == 0)){
        return ATT_ERROR_OUT_OF_RANGE;
    }

    if ((has_preset_records_num == 0) ||
        (index > has_preset_records[has_preset_records_num - 1].index) ){
        return ATT_ERROR_OUT_OF_RANGE;
    }

    uint8_t i;
    for (i = 0; i < has_clients_max_num; i++) {
        if (has_clients[i].scheduled_control_point_notification_tasks != 0u) {
            return ATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS;
        }
    }
    return ATT_ERROR_SUCCESS;
}

static void has_server_preset_iterator_init(has_server_connection_t * connection, uint8_t start_index, uint8_t num_presets){
    connection->start_index = start_index;
    connection->num_presets_to_read = num_presets;
    connection->current_position = HAS_PRESET_RECORD_INVALID_POSITION;
    connection->num_presets_already_read = 0;
}

static uint8_t has_server_get_next_empty_preset_record_index(void){
    if ( has_preset_records_num >= has_preset_records_max_num ){
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }

    uint8_t i;
    // reusing old
    for (i = 0; i < has_preset_records_max_num; i++){
        if (has_preset_records[i].index == HAS_PRESET_RECORD_INVALID_INDEX){
            return i + 1;
        }
    }
    return HAS_PRESET_RECORD_INVALID_INDEX;
}

static uint8_t has_server_get_previous_available_index_relative_to_active_index(void){
    if (has_preset_records_num < 1u ){
        // there is no records
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }
    if (has_active_preset_index == HAS_PRESET_RECORD_INVALID_INDEX){
        // no active index set
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }

    uint8_t active_preset_position = has_server_active_preset_position();

    uint8_t pos;
    // search backward from current active index to the HAS_PRESET_RECORD_START_INDEX
    for (pos = active_preset_position - 1; pos >= 0; pos--){
        if (has_server_preset_record_available(pos)){
            return has_server_preset_record_index(pos);
        }
    }

    // search backward from the end to the current active index
    for (pos = has_preset_records_max_num - 1; pos >= active_preset_position + 1; pos--){
        if (has_server_preset_record_available(pos)){
            return has_server_preset_record_index(pos);
        }
    }
    return active_preset_position;
}

static uint8_t has_server_get_next_index_relative_to_active_index(void){
    if (has_preset_records_num < 1u ){
        // there is no records
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }
    if (has_active_preset_index == HAS_PRESET_RECORD_INVALID_INDEX){
        // no active index set
        return HAS_PRESET_RECORD_INVALID_INDEX;
    }

    uint8_t active_preset_position = has_server_active_preset_position();

    uint8_t i;
    // search forward from current active index to the last one
    for (i = active_preset_position + 1; i <= has_preset_records_max_num - 1; i++){
        if (has_server_preset_record_available(i)){
            return has_server_preset_record_index(i);
        }
    }

    // search forward from HAS_PRESET_RECORD_START_INDEX to the current active
    for (i = 0; i <= active_preset_position - 1; i++){
        if (has_server_preset_record_available(i)){
            return has_server_preset_record_index(i);
        }
    }

    return has_active_preset_index;
}

static int has_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);

    has_server_connection_t * connection = has_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        connection = has_server_add_connection(con_handle);

        if (connection == NULL){
            // has_server_emit_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new connection.", has_clients_max_num);
            return 0;
        } else {
            // has_server_emit_connected(con_handle, ERROR_CODE_SUCCESS);
        }
    }

    if (attribute_handle == has_control_point_value_handle){
        if (buffer_size < 1){
            return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_PARAMETERS_LENGTH;
        }
        uint8_t pos = 0;
        has_opcode_t opcode = (has_opcode_t)buffer[pos++];

        switch (opcode) {
            case HAS_OPCODE_READ_PRESETS_REQUEST:
            case HAS_OPCODE_WRITE_PRESET_NAME:
            case HAS_OPCODE_SET_ACTIVE_PRESET:
            case HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY:
                if (has_control_point_client_configuration == 0){
                    return ATT_ERROR_CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_IMPROPERLY_CONFIGURED;
                }
                if (!has_valid_opcode_parameters_length(opcode, buffer_size - pos)){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_PARAMETERS_LENGTH;
                }
                break;

            case HAS_OPCODE_READ_PRESET_RESPONSE:
            case HAS_OPCODE_PRESET_CHANGED:
                return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_OPCODE;

            case HAS_OPCODE_SET_NEXT_PRESET:
            case HAS_OPCODE_SET_PREVIOUS_PRESET:
            case HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY:
            case HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY:
                if (!has_valid_opcode_parameters_length(opcode, buffer_size - pos)){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_PARAMETERS_LENGTH;
                }
                break;
            default:
                return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_OPCODE;
        }

        has_preset_record_t * preset;
        uint8_t att_error_code;
        uint8_t index;
        uint8_t num_presets;
        uint8_t status;

        if (connection->scheduled_control_point_notification_tasks != 0u){
            return ATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS;
        }

        switch (opcode) {
            case HAS_OPCODE_READ_PRESETS_REQUEST:
                index = buffer[pos++];
                num_presets = buffer[pos];

                att_error_code = is_read_operation_valid(connection, index, num_presets);
                if (att_error_code != ATT_ERROR_SUCCESS) {
                    return att_error_code;
                }

                has_server_preset_iterator_init(connection, index, num_presets);
                has_server_schedule_control_point_task(connection, HAS_CONNECTION_TASK_READ_PRESETS);
                break;

            case HAS_OPCODE_WRITE_PRESET_NAME: {
                index = buffer[pos++];
                preset = find_regular_preset_record_with_index(index);
                if (preset == NULL) {
                    return ATT_ERROR_OUT_OF_RANGE;
                }

                char name[HAS_PRESET_RECORD_NAME_MAX_LENGTH];
                uint16_t name_size = btstack_min(HAS_PRESET_RECORD_NAME_MAX_LENGTH, buffer_size - pos + 1);
                memcpy(name, &buffer[pos], name_size - 1);
                name[name_size] = '\0';

                status = hearing_access_service_server_preset_record_set_name(index, name);
                switch (status){
                    case ERROR_CODE_COMMAND_DISALLOWED:
                        return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_WRITE_NAME_NOT_ALLOWED;
                    case ERROR_CODE_SUCCESS:
                        break;
                    default:
                        return ATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS;
                }
                break;
            }
            case HAS_OPCODE_SET_ACTIVE_PRESET:
                index = buffer[pos++];
                preset = find_regular_preset_record_with_index(index);
                if (preset == NULL){
                    return ATT_ERROR_OUT_OF_RANGE;
                }
                status = hearing_access_service_server_preset_record_set_active(index);
                switch (status){
                    case ERROR_CODE_SUCCESS:
                        break;
                    default:
                        return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                break;

            case HAS_OPCODE_SET_NEXT_PRESET:
                index = has_server_get_next_index_relative_to_active_index();
                if (index == HAS_PRESET_RECORD_INVALID_INDEX){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                hearing_access_service_server_preset_record_set_active(index);
                break;

            case HAS_OPCODE_SET_PREVIOUS_PRESET:
                index = has_server_get_previous_available_index_relative_to_active_index();
                if (index == HAS_PRESET_RECORD_INVALID_INDEX){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                hearing_access_service_server_preset_record_set_active(index);
                break;

            case HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY:
                if ( (has_hearing_aid_features & HEARING_AID_FEATURE_MASK_PRESET_SYNCHRONIZATION_SUPPORT) == 0u){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_SYNCHRONIZATION_NOT_SUPPORTED;
                }
                index = buffer[pos++];
                preset = find_regular_preset_record_with_index(index);
                if (preset == NULL){
                    return ATT_ERROR_OUT_OF_RANGE;
                }

                status = hearing_access_service_server_preset_record_set_active(index);
                switch (status){
                    case ERROR_CODE_SUCCESS:
                        break;
                    default:
                        return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                break;

            case HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY:
                if ( (has_hearing_aid_features & HEARING_AID_FEATURE_MASK_PRESET_SYNCHRONIZATION_SUPPORT) == 0u){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_SYNCHRONIZATION_NOT_SUPPORTED;
                }
                index = has_server_get_next_index_relative_to_active_index();
                if (index == HAS_PRESET_RECORD_INVALID_INDEX){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                hearing_access_service_server_preset_record_set_active(index);
                break;

            case HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY:
                if ( (has_hearing_aid_features & HEARING_AID_FEATURE_MASK_PRESET_SYNCHRONIZATION_SUPPORT) == 0u){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_SYNCHRONIZATION_NOT_SUPPORTED;
                }
                index = has_server_get_previous_available_index_relative_to_active_index();
                if (index == HAS_PRESET_RECORD_INVALID_INDEX){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE;
                }
                hearing_access_service_server_preset_record_set_active(index);
                break;

            default:
                btstack_assert(false);
                break;
        }
        return 0;
    }

    if (attribute_handle == has_control_point_client_configuration_handle){
        has_control_point_client_configuration = little_endian_read_16(buffer, 0);
//        if (connection->scheduled_control_point_notification_tasks != 0u){
//            return 0;
//        }
//
//        if (has_control_point_client_configuration > 0){
//            has_server_preset_iterator_init(connection, HAS_PRESET_RECORD_START_INDEX, has_preset_records_num);
//            has_server_schedule_control_point_task(connection, HAS_CP_NOTIFICATION_TASK_READ_ALL_PRESETS);
//        }
        return 0;
    }
    
    if (attribute_handle == has_hearing_aid_features_client_configuration_handle){
        has_hearing_aid_features_client_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }

    if (attribute_handle == has_active_preset_index_client_configuration_handle){
        has_active_preset_index_client_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }
    return 0;
}

static void has_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    static has_server_connection_t * connection;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            connection = has_server_find_connection_for_con_handle(con_handle);
            if (connection == NULL){
                break;
            }
            
            memset(connection, 0, sizeof(has_server_connection_t));
            connection->con_handle = HCI_CON_HANDLE_INVALID;
            break;
        default:
            break;
    }
}

void hearing_access_service_server_init(uint8_t hearing_aid_features, uint8_t preset_records_num, has_preset_record_t * preset_records, uint8_t clients_num, has_server_connection_t * clients){
	btstack_assert(hearing_aid_features <= 0x3F);
    btstack_assert(clients_num != 0);

    // The server shall not set the value of the Active Preset Index characteristic to the Index field of a preset record
    // with the isAvailable bit of the Properties field set to 0b0 (i.e., an unavailable preset record).
    // btstack_assert(active_preset_index <= 0x3F);
    
    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_HEARING_ACCESS, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    has_hearing_aid_features = hearing_aid_features;

    // get characteristic value handle and client configuration handle
    
    has_hearing_aid_features_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES);
    has_hearing_aid_features_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES);
	
	has_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT);
    has_control_point_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT);

	has_active_preset_index_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX);
    has_active_preset_index_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX);

    log_info("Found HAS service 0x%02x-0x%02x", start_handle, end_handle);

    has_clients_max_num = clients_num;
    has_clients = clients;
    memset(has_clients, 0, sizeof(has_server_connection_t) * has_clients_max_num);
    uint8_t i;
    for (i = 0; i < has_clients_max_num; i++){
        has_clients[i].con_handle = HCI_CON_HANDLE_INVALID;
    }
    // 0x00 and 0xFF are reserved indexes
    has_preset_records_max_num = btstack_min(preset_records_num - 1, 0xFD);
    has_preset_records_num = 0;
    has_preset_records = preset_records;
    has_active_preset_index  = HAS_PRESET_RECORD_INVALID_INDEX;

    for (i = 0; i < preset_records_num; i++){
        has_preset_records[i].index = HAS_PRESET_RECORD_INVALID_INDEX;
    }

#ifdef ENABLE_TESTING_SUPPORT
    printf("HAS 0x%02x - 0x%02x \n", start_handle, end_handle);

    printf("    hearing_aid_features     0x%02x \n", has_hearing_aid_features_value_handle);
    printf("    hearing_aid_features CCC 0x%02x \n", has_hearing_aid_features_client_configuration_handle);

    printf("    control_point            0x%02x \n", has_control_point_value_handle);
    printf("    control_point CCC        0x%02x \n", has_control_point_client_configuration_handle);

    printf("    active_preset_index      0x%02x \n", has_active_preset_index_value_handle);
    printf("    active_preset_index CCC  0x%02x \n", has_active_preset_index_client_configuration_handle);
#endif

    // register service with ATT Server
    hearing_access_service.start_handle   = start_handle;
    hearing_access_service.end_handle     = end_handle;
    hearing_access_service.read_callback  = &has_server_read_callback;
    hearing_access_service.write_callback = &has_server_write_callback;
    hearing_access_service.packet_handler = has_packet_handler;
    att_server_register_service_handler(&hearing_access_service);
}

void hearing_access_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    has_server_event_callback = packet_handler;
}

static bool has_setup_next_connection_for_preset_changed_notification(void){
    uint8_t i;
    has_server_connection_t * connection = NULL;
    for (i = 0; i < has_clients_max_num; i++){
        connection = &has_clients[i];
        if (connection->con_handle == HCI_CON_HANDLE_INVALID){
            continue;
        }
        if (connection->scheduled_preset_record_change_notification) {
            connection->scheduled_tasks_callback.callback = &has_server_can_send_now;
            connection->scheduled_tasks_callback.context  = (void*) connection;
            att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
            return true;
        }
    }
    return false;
}


static void has_server_can_send_now(void * context) {
    has_server_connection_t *connection = (has_server_connection_t *) context;
    btstack_assert(connection != NULL);

    bool is_last_preset_record;
    uint8_t prev_index;
    has_preset_record_t *preset;

    if ((connection->scheduled_control_point_notification_tasks & HAS_CONNECTION_TASK_SEND_CHANGED_PRESETS_ON_RECONNECT) > 0u) {
        preset = has_server_preset_iterator_get_next(connection, &is_last_preset_record);
        uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
        uint8_t pos = 0;
        value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;

        switch (preset->index) {
            case 8:
                is_last_preset_record = true;
                value[0] = preset->index;
                att_server_notify(connection->con_handle, has_active_preset_index_value_handle, &value[0], pos);
                break;
            case 3:
            case 6:
            case 7:
                break;
            default:
                value[pos++] = (uint8_t) HAS_CHANGEID_GENERIC_UPDATE;
                value[pos++] = is_last_preset_record ? 1u : 0u;
                value[pos++] = prev_index;

                value[pos++] = preset->index;
                value[pos++] = preset->properties;

                uint16_t mtu = att_server_get_mtu(connection->con_handle);
                btstack_assert(mtu > (pos + 3));
                uint16_t available_payload_size = mtu - 3 - pos;
                uint16_t name_len = btstack_min(available_payload_size, strlen(preset->name) + 1);
                btstack_strcpy((char *) &value[pos], name_len, preset->name);
                pos += name_len;
                (void) att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);
                break;
        }

        if (is_last_preset_record) {
            printf("can send now: Read all, HAS_SERVER_CONNECTION_STATE_READY\n");
            connection->scheduled_control_point_notification_tasks = 0u;
        } else {
            has_server_schedule_preset_record_task();
        }
        return;

    } else if ((connection->scheduled_control_point_notification_tasks & HAS_CONNECTION_TASK_READ_PRESETS) > 0u) {
        preset = has_server_preset_iterator_get_next(connection, &is_last_preset_record);
        if (!preset){
            return;
        }
        uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
        uint8_t pos = 0;
        value[pos++] = (uint8_t) HAS_OPCODE_READ_PRESET_RESPONSE;
        value[pos++] = is_last_preset_record ? 1u : 0u;
        value[pos++] = preset->index;
        value[pos++] = preset->properties;

        uint16_t mtu = att_server_get_mtu(connection->con_handle);
        btstack_assert(mtu > (pos + 3));
        uint16_t available_payload_size = mtu - 3 - pos;
        uint16_t name_len = btstack_min(available_payload_size, strlen(preset->name) + 1);
        btstack_strcpy((char *) &value[pos], name_len, preset->name);
        pos += name_len;

        (void) att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

        if (is_last_preset_record) {
            printf("can send now: Read, HAS_SERVER_CONNECTION_STATE_READY\n");
            connection->scheduled_control_point_notification_tasks = 0u;
        } else {
            has_server_schedule_preset_record_task();
        }
        return;

    } else {
        preset = has_read_presets_operation_get_changed_preset_record(connection, &is_last_preset_record, &prev_index);
        if (!preset){
            return;
        }
        if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t) HAS_CHANGEID_GENERIC_UPDATE;
            value[pos++] = is_last_preset_record ? 1u : 0u;
            value[pos++] = prev_index;
            value[pos++] = preset->index;
            value[pos++] = preset->properties;

            uint16_t mtu = att_server_get_mtu(connection->con_handle);
            btstack_assert(mtu > (pos + 3));
            uint16_t available_payload_size = mtu - 3 - pos;
            uint16_t name_len = btstack_min(available_payload_size, strlen(preset->name));
            btstack_strcpy((char *) &value[pos], name_len, preset->name);
            pos += name_len;

            att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[4];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t) HAS_CHANGEID_PRESET_RECORD_DELETED;
            value[pos++] = 1u;
            value[pos++] = preset->index;

            att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[4];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t) HAS_CHANGEID_PRESET_RECORD_AVAILABLE;
            value[pos++] = 1u;
            value[pos++] = preset->index;

            att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], sizeof(value));

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[4];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t) HAS_CHANGEID_PRESET_RECORD_UNAVAILABLE;
            value[pos++] = 1u;
            value[pos++] = preset->index;

            att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], sizeof(value));

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[1];
            value[0] = preset->index;

            att_server_notify(connection->con_handle, has_active_preset_index_value_handle, &value[0], sizeof(value));

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_UPDATE_NAME) {
            connection->scheduled_preset_record_change_notification = false;

            uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t) HAS_CHANGEID_GENERIC_UPDATE;
            value[pos++] = 1u;
            value[pos++] = prev_index;
            value[pos++] = preset->index;
            value[pos++] = preset->properties;

            uint16_t mtu = att_server_get_mtu(connection->con_handle);
            btstack_assert(mtu > (pos + 3));
            uint16_t available_payload_size = mtu - 3 - pos;
            uint16_t name_len = btstack_min(available_payload_size, strlen(preset->name));
            btstack_strcpy((char *) &value[pos], name_len, preset->name);
            pos += name_len;

            att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);
        }
    }

    if (has_setup_next_connection_for_preset_changed_notification()){
        printf("can send now: has_setup_next_connection_for_preset_changed_notification\n");
        return;
    }

    if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED){
        has_preset_records_num--;
        has_clear_preset_record(preset);
        dump_preset_records("After delete preset");
    }
    // we are done
    preset->scheduled_task = 0;

    if (has_queued_preset_records_num > 0){
        has_server_schedule_preset_record_task();
    }
}


static bool has_server_schedule_preset_record_task(void){
    uint8_t i;
    int16_t connection_index = -1;
    has_server_connection_t * connection;

    for (i = 0; i < has_clients_max_num; i++){
        connection = &has_clients[i];

        if (connection->con_handle == HCI_CON_HANDLE_INVALID){
            continue;
        }

        // find first connection to trigger notifications
        if ( (connection->con_handle != HCI_CON_HANDLE_INVALID) && (connection_index < 0)){
            connection_index = i;
        }
        
        // register task for all clients
        connection->scheduled_preset_record_change_notification = true;
    }

    if (connection_index > -1){
        connection = &has_clients[(uint8_t)connection_index];
        connection->scheduled_tasks_callback.callback = &has_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_request_to_send_indication(&connection->scheduled_tasks_callback, connection->con_handle);
        return true;
    }

    return false;
}

static uint8_t has_server_preset_record_status(has_preset_record_t * preset){
    if (preset->index >= has_preset_records_max_num){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    if (preset->index == HAS_PRESET_RECORD_INVALID_INDEX){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t i;
    for (i = 0; i < has_clients_max_num; i++){
        if (has_clients[i].con_handle == HCI_CON_HANDLE_INVALID){
            continue;
        }
        if (has_clients[i].scheduled_control_point_notification_tasks != 0u){
            return ERROR_CODE_CONTROLLER_BUSY;
        }
    }

    if (preset->scheduled_task > 0u){
        return ERROR_CODE_CONTROLLER_BUSY;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_add_preset(uint8_t properties, char * name){
    uint8_t index = has_server_get_next_empty_preset_record_index();

    if (index == HAS_PRESET_RECORD_INVALID_INDEX){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    btstack_assert(preset != NULL);

    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED;
    preset->index = index;
    preset->properties = properties;
    btstack_strcpy(preset->name, HAS_PRESET_RECORD_NAME_MAX_LENGTH, name);

    preset->active = false;
    preset->con_handle = HCI_CON_HANDLE_INVALID;

    // increase count
    has_preset_records_num++;

    // schedule notifications
    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }
    dump_preset_records("Server after ADD Preset");
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_delete_preset(uint8_t index){
    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    if (preset == NULL){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t status = has_server_preset_record_status(preset);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    has_clear_preset_record(preset);
    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED;
    has_preset_records_num--;

    // schedule notifications
    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }

    dump_preset_records("Server after DELETE Preset");
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_preset_record_set_active(uint8_t index){
    dump_preset_records("Server before SET Active");
    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    if (preset == NULL){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t status = has_server_preset_record_status(preset);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (preset->active){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    if ((preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE) == 0u ){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // reset old active
    if (has_active_preset_index != HAS_PRESET_RECORD_INVALID_INDEX) {
        has_preset_records[has_server_preset_position_for_index(has_active_preset_index)].active = false;
    }

    // update
    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE;
    preset->active = true;
    preset->properties |= HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE;
    has_active_preset_index = index;

    // schedule notifications
    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }

    dump_preset_records("Server after SET Active");
    return ERROR_CODE_SUCCESS;
}


uint8_t hearing_access_service_server_preset_record_set_available(uint8_t index){
    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    if (preset == NULL){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t status = has_server_preset_record_status(preset);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (preset->active){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE;
    preset->properties |= HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE;

    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }
    dump_preset_records("Server after SET available");
    return ERROR_CODE_SUCCESS;
}


uint8_t hearing_access_service_server_preset_record_set_unavailable(uint8_t index){
    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    if (preset == NULL){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t status = has_server_preset_record_status(preset);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (preset->active){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE;
    preset->properties &= ~HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE;

    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }

    dump_preset_records("Server after SET unavailable");
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_preset_record_set_name(uint8_t index, const char * name){
    btstack_assert(name != NULL);

    has_preset_record_t * preset = has_server_preset_record_for_index(index);
    if (preset == NULL){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    uint8_t status = has_server_preset_record_status(preset);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if ((preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE) == 0u ){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if ((preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE) == 0u ){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    preset->scheduled_task = HAS_NOTIFICATION_TASK_PRESET_RECORD_UPDATE_NAME;
    btstack_strcpy(preset->name, HAS_PRESET_RECORD_NAME_MAX_LENGTH, name);

    if (!has_server_schedule_preset_record_task()){
        preset->scheduled_task = 0;
    }
    dump_preset_records("After update name");
    return ERROR_CODE_SUCCESS;
}


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

#define HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED                                      0x00000001
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED                                    0x00000002
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE                                     0x00000004

#define HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE                                  0x00000008
#define HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE                                0x00000010
#define HAS_NOTIFICATION_TASK_ACTIVE_PRESET_INDEX                                      0x00000020
#define HAS_NOTIFICATION_TASK_CONTROL_POINT_OPERATION                                  0x00000040

#define HAS_CP_NOTIFICATION_TASK_READ_PRESETS                                          0x00000001
#define HAS_CP_NOTIFICATION_TASK_WRITE_PRESET                                          0x00000002

#define HAS_EMPTY_PRESET_RECORD_INDEX                                                  0x00
#define HAS_INVALID_PRESET_RECORD_POSITION                                             0xFF

static att_service_handler_t hearing_access_service;

static has_server_connection_t * has_clients;
static uint8_t  has_clients_num = 0;

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

static bool has_server_schedule_task(void);
static void has_server_can_send_now(void * context);

static void has_dump_queue(const char * msg){
#ifdef ENABLE_TESTING_SUPPORT
    printf("\n%s: queued %d\n", msg, has_queued_preset_records_num);
    uint8_t i;
    for (i = has_preset_records_num; i < has_queued_preset_records_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        printf("Q[%02d] - %02d, %s, P:0x%02x, A:%d, scheduled_task = 0x%04X\n", i, preset->index, preset->name, preset->properties, preset->active?1u:0u, preset->scheduled_task);
    }
#else
    UNUSED(msg);
#endif
}

static void dump_preset_records(char * msg){
#ifdef ENABLE_TESTING_SUPPORT
    printf("%s: regular %d, queued %d\n", msg, has_preset_records_num, has_queued_preset_records_num);
    uint8_t i;
    for (i = 0; i < has_preset_records_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        printf("N[%d] - %d, %s, P:0x%02x, A:%d, scheduled_task = %d\n", i, preset->index, preset->name, preset->properties, preset->active?1u:0u, preset->scheduled_task);
    }
    for (i = has_preset_records_num; i < has_preset_records_max_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        printf("Q[%d] - %d, %s, P:0x%02x, A:%d, scheduled_task = %d\n", i, preset->index, preset->name, preset->properties, preset->active?1u:0u, preset->scheduled_task);
    }
#else
    UNUSED(msg);
    UNUSED(pos);
    UNUSED(index);
#endif
}

static void has_clear_preset_record(uint8_t record_pos) {// clear the last element (optional, for cleanliness)
    has_preset_records[record_pos].index = HAS_EMPTY_PRESET_RECORD_INDEX;
    has_preset_records[record_pos].properties = 0;
    has_preset_records[record_pos].name[0] = '\0';
    has_preset_records[record_pos].active = false;
    has_preset_records[record_pos].scheduled_task = 0;
    has_preset_records[record_pos].calculated_position = HAS_INVALID_PRESET_RECORD_POSITION;
}

static has_server_connection_t * has_server_find_connection_for_con_handle(hci_con_handle_t con_handle){
    uint8_t i;
    for (i = 0; i < has_clients_num; i++){
        if (has_clients[i].con_handle == con_handle) {
            return &has_clients[i];
        }
    }
    return NULL;
}


static void has_shift_left_preset_records(uint8_t start_pos, uint8_t end_pos){
    uint8_t i;
    // Shift queued elements to the left.
    // Record with index start_pos will be lost/removed.
    for (i = start_pos; i < end_pos; i++) {
        has_preset_records[i] = has_preset_records[i + 1];
    }
}

static void has_remove_task_from_queue(void){
    uint8_t end_pos = has_preset_records_num + has_queued_preset_records_num - 1;
    has_shift_left_preset_records(has_preset_records_num, end_pos);
    has_clear_preset_record(end_pos);
    has_queued_preset_records_num--;

    has_dump_queue("After remove");
}

static void has_delete_preset_record(uint8_t delete_pos){
    has_remove_task_from_queue();

    // shift elements to the left to remove preset record with index delete_pos
    uint8_t end_pos = has_preset_records_num + has_queued_preset_records_num - 1;
    has_shift_left_preset_records(delete_pos, end_pos);
    has_preset_records_num--;
    has_clear_preset_record(end_pos);
}

static void has_server_reset_client(has_server_connection_t * connection){
    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->scheduled_preset_record_change_notification = false;
    connection->scheduled_control_point_notification_tasks = false;
}

static uint8_t find_position_of_regular_preset_record_with_index(uint8_t index){
    uint8_t i;
    for (i = 0; i < has_preset_records_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        if (preset->index == index){
            return i;
        }
    }
    return HAS_INVALID_PRESET_RECORD_POSITION;
}

static has_preset_record_t * has_read_presets_operation_get_next_preset_record(has_server_connection_t * connection, bool * is_last_preset){
    btstack_assert(has_preset_records_num > 0);
    // check if it is initial value
    if (connection->preset_position == HAS_INVALID_PRESET_RECORD_POSITION) {
        // find first index equal to or greater than start_index
        uint8_t i;
        for (i = 0; i < has_preset_records_num; i++) {
            has_preset_record_t *preset = &has_preset_records[i];
            if (preset->index >= connection->start_index) {
                connection->preset_position = i;
                printf("Initial preset found R[%02d].index = %02d [%d]\n", connection->preset_position, preset->index, connection->num_presets_to_read);
                break;
            }
        }
        btstack_assert(connection->preset_position != HAS_INVALID_PRESET_RECORD_POSITION);
    } else {
        connection->preset_position++;
        printf("Next preset found R[%02d].index = %02d [%d]\n", connection->preset_position, has_preset_records[connection->preset_position].index, connection->num_presets_to_read);
    }
    connection->num_presets_to_read--;

    *is_last_preset = (connection->num_presets_to_read == 0) || (connection->preset_position == (has_preset_records_num - 1));
    return &has_preset_records[connection->preset_position];
}


static uint8_t has_add_cp_operation_to_queue(has_server_connection_t * connection, uint8_t scheduled_task) {
    // queue the change at the tail for later processing
    uint8_t queue_pos = has_preset_records_num + has_queued_preset_records_num;

    connection->scheduled_control_point_notification_tasks |= scheduled_task;

    has_preset_records[queue_pos].con_handle = connection->con_handle;
    has_preset_records[queue_pos].scheduled_task = HAS_NOTIFICATION_TASK_CONTROL_POINT_OPERATION;
    has_preset_records[queue_pos].index = 0;
    has_preset_records[queue_pos].properties = 0;
    has_preset_records[queue_pos].active = false;
    has_preset_records[queue_pos].calculated_position = HAS_INVALID_PRESET_RECORD_POSITION;
    // increase count
    has_queued_preset_records_num++;
    has_dump_queue("ADD CP Operation");
    return queue_pos;
}

static uint8_t has_add_preset_record_change_to_queue(uint8_t index, uint8_t properties, const char *name, uint8_t scheduled_task) {
    // queue the change at the tail for later processing
    uint8_t queue_pos = has_preset_records_num + has_queued_preset_records_num;

    has_preset_records[queue_pos].index = index;
    has_preset_records[queue_pos].properties = properties;
    btstack_strcpy(has_preset_records[queue_pos].name, HAS_PRESET_RECORD_NAME_MAX_LENGTH, name);
    has_preset_records[queue_pos].scheduled_task = scheduled_task;
    has_preset_records[queue_pos].active = false;
    has_preset_records[queue_pos].calculated_position = HAS_INVALID_PRESET_RECORD_POSITION;
    has_preset_records[queue_pos].con_handle = HCI_CON_HANDLE_INVALID;
    // increase count
    has_queued_preset_records_num++;
    return queue_pos;
}

static bool preset_calc_final_state(uint8_t index, has_preset_record_t * out_preset) {
    bool exists = false;
    memset(out_preset, 0, sizeof(has_preset_record_t));
    uint8_t i;
    // get existing item
    for (i = 0; i < has_preset_records_num; i++) {
        if (has_preset_records[i].index == index) {
            *out_preset = has_preset_records[i];
            exists = true;
        }
    }
    for ( ; i < has_preset_records_num + has_queued_preset_records_num; i++) {
        has_preset_record_t *preset = &has_preset_records[i];
        if (preset->index == index) {
            switch (preset->scheduled_task) {
                case HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED:
                    *out_preset = has_preset_records[i];
                    out_preset->active = false;
                    exists = true;
                    break;
                case HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED:
                    exists = false;
                    out_preset->active = false;
                    break;
                case HAS_NOTIFICATION_TASK_ACTIVE_PRESET_INDEX:
                    out_preset->active = true;
                    break;
                case HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE:
                    out_preset->properties |= HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE;
                    break;
                case HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE:
                    out_preset->properties &= ~HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE;
                    break;
                default:
                    btstack_unreachable();
            }
        } else {
            if (preset->scheduled_task == HAS_NOTIFICATION_TASK_ACTIVE_PRESET_INDEX) {
                out_preset->active = false;
            }
        }
    }
    return exists;
}

static has_server_connection_t * has_server_add_connection(hci_con_handle_t con_handle){
    uint8_t i;

    for (i = 0; i < has_clients_num; i++){
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
            log_info("There are already %d clients connected. No memory for new connection.", has_clients_num);
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
            return (parameter_length == 2);

        case HAS_OPCODE_WRITE_PRESET_NAME:
            return (parameter_length < 42);

        case HAS_OPCODE_SET_ACTIVE_PRESET:
        case HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY:
            return (parameter_length == 1);
            
        case HAS_OPCODE_SET_NEXT_PRESET:
        case HAS_OPCODE_SET_PREVIOUS_PRESET:
        case HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY:
        case HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY:
            return (parameter_length == 0);
        
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

static uint8_t is_read_operation_valid(has_server_connection_t * connection){
    if ((connection->scheduled_control_point_notification_tasks & HAS_CP_NOTIFICATION_TASK_READ_PRESETS) != 0u){
        return ATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS;
    }

    uint8_t i;
    for (i = 0; i < has_clients_num; i++) {
        if ((has_clients[i].scheduled_control_point_notification_tasks & HAS_CP_NOTIFICATION_TASK_WRITE_PRESET) != 0u) {
            return ATT_ERROR_PROCEDURE_ALREADY_IN_PROGRESS;
        }
    }

    if ((has_preset_records_num == 0) ||
        (connection->start_index > has_preset_records[has_preset_records_num - 1].index) ){
        return ATT_ERROR_OUT_OF_RANGE;
    }
    return ATT_ERROR_SUCCESS;
}

static int has_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);

    has_server_connection_t * connection = has_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        connection = has_server_add_connection(con_handle);

        if (connection == NULL){
            // has_server_emit_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new connection.", has_clients_num);
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
            default:
                return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_OPCODE;
        }

        has_preset_record_t * preset;
        uint8_t att_error_code;
        uint8_t index;
        uint8_t num_presets;

        switch (opcode) {
            case HAS_OPCODE_READ_PRESETS_REQUEST:
//                if (connection->state == HAS_SERVER_CONNECTION_STATE_PENDING_ATT_RESPONSE){
//                    att_error_code = is_read_operation_valid(connection);
//                    if (att_error_code == ATT_ERROR_SUCCESS){
//                        connection->state = HAS_SERVER_CONNECTION_STATE_PENDING_ATT_INDICATION;
//                        has_server_schedule_task();
//                    } else {
//                        has_remove_task_from_queue();
//                        connection->state = HAS_SERVER_CONNECTION_STATE_READY;
//                    }
//                    return ATT_READ_ERROR_CODE_OFFSET + att_error_code;
//                }
                index = buffer[pos++];
                num_presets = buffer[pos];

                if ((index == 0) || (num_presets == 0)){
                    return ATT_ERROR_OUT_OF_RANGE;
                }

                if ( (has_preset_records_num + has_queued_preset_records_num) >= (has_preset_records_max_num - 1) ){
                    return ATT_ERROR_INSUFFICIENT_RESOURCES;
                }

                connection->start_index = index;
                connection->num_presets_to_read  = num_presets;
                connection->preset_position = HAS_INVALID_PRESET_RECORD_POSITION;
                // try
                connection->state = HAS_SERVER_CONNECTION_STATE_PENDING_ATT_INDICATION;

                att_error_code = is_read_operation_valid(connection);
                if (att_error_code == ATT_ERROR_SUCCESS) {
                    printf("Schedule read %d presets from index %d\n", index, num_presets);
                    connection->state = HAS_SERVER_CONNECTION_STATE_PENDING_ATT_INDICATION;
                    has_add_cp_operation_to_queue(connection, HAS_CP_NOTIFICATION_TASK_READ_PRESETS);
                    has_server_schedule_task();
                } else {
                    connection->state = HAS_SERVER_CONNECTION_STATE_READY;
                }
                return att_error_code;
//
//                has_add_cp_operation_to_queue(connection, HAS_NOTIFICATION_TASK_CONTROL_POINT_OPERATION);
//                has_server_schedule_task();
//                return ATT_INTERNAL_WRITE_RESPONSE_PENDING;

            case HAS_OPCODE_WRITE_PRESET_NAME:
                index = buffer[pos++];
                preset = find_regular_preset_record_with_index(index);
                if (preset == NULL){
                    return ATT_ERROR_OUT_OF_RANGE;
                }
                if ((preset->properties & HEARING_AID_PRESET_PROPERTIES_MASK_NAME_WRITABLE) == 0u){
                    return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_WRITE_NAME_NOT_ALLOWED;
                }
                break;

            case HAS_OPCODE_SET_ACTIVE_PRESET:
                break;
            case HAS_OPCODE_SET_NEXT_PRESET:
                break;
            case HAS_OPCODE_SET_PREVIOUS_PRESET:
                break;
            case HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY:
                break;
            case HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY:
                break;
            case HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY:
                break;
            default:
                return HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_OPCODE;
        }
    }

    if (attribute_handle == has_control_point_client_configuration_handle){
        has_control_point_client_configuration = little_endian_read_16(buffer, 0);
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
    has_active_preset_index  = HAS_EMPTY_PRESET_RECORD_INDEX;
    // get characteristic value handle and client configuration handle
    
    has_hearing_aid_features_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES);
    has_hearing_aid_features_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_FEATURES);
	
	has_control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT);
    has_control_point_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEARING_AID_PRESET_CONTROL_POINT);

	has_active_preset_index_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX);
    has_active_preset_index_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ACTIVE_PRESET_INDEX);

    log_info("Found HAS service 0x%02x-0x%02x", start_handle, end_handle);

    has_clients_num = clients_num;
    has_clients = clients;
    memset(has_clients, 0, sizeof(has_server_connection_t) * has_clients_num);
    uint8_t i;
    for (i = 0; i < has_clients_num; i++){
        has_clients[i].con_handle = HCI_CON_HANDLE_INVALID;
    }

    has_preset_records_max_num = preset_records_num;
    has_preset_records_num = 0;
    has_preset_records = preset_records;
    has_active_preset_index = HAS_EMPTY_PRESET_RECORD_INDEX;

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
    for (i = 0; i < has_clients_num; i++){
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

uint8_t find_position_of_regular_preset_record_to_insert(has_preset_record_t * new_preset){
    if (new_preset->calculated_position != HAS_INVALID_PRESET_RECORD_POSITION){
        return new_preset->calculated_position;
    }

    uint8_t i;
    // add to the head
    uint8_t insert_pos = 0;

    if (has_preset_records_num == 0){
        new_preset->calculated_position = insert_pos;
        return insert_pos;
    }

    for (i = 0; i < has_preset_records_num + 1; i++){
        has_preset_record_t * preset = &has_preset_records[i];
        insert_pos = i;

        if (preset->index == HAS_INVALID_PRESET_RECORD_POSITION){
            break;
        }

        if (preset->index > new_preset->index){
            break;
        }
    }

    new_preset->calculated_position = insert_pos;
    return insert_pos;
}

static void has_insert_preset_record(uint8_t insert_pos, uint8_t index, uint8_t properties, char * name){
    // move up, the queued change will be overwritten
    has_preset_records_num++;
    has_queued_preset_records_num--;

    uint8_t i;
    for (i = has_preset_records_num - 1; i > insert_pos; i--){
        has_preset_records[i] = has_preset_records[i-1];
    }
    
    // insert
    has_preset_records[insert_pos].index = index;
    has_preset_records[insert_pos].properties = properties;
    btstack_strcpy(has_preset_records[insert_pos].name, HAS_PRESET_RECORD_NAME_MAX_LENGTH, name);
    has_preset_records[insert_pos].active = false;
    has_preset_records[insert_pos].scheduled_task = 0;
    has_preset_records[insert_pos].calculated_position = HAS_INVALID_PRESET_RECORD_POSITION;
}

uint8_t find_position_of_regular_preset_record_to_delete(has_preset_record_t * delete_preset){
    if (delete_preset->calculated_position != HAS_INVALID_PRESET_RECORD_POSITION){
        return delete_preset->calculated_position;
    }

    uint8_t i;
    // add to the head
    uint8_t delete_pos = HAS_INVALID_PRESET_RECORD_POSITION;

    for (i = 0; i < has_preset_records_num; i++){
        has_preset_record_t * preset = &has_preset_records[i];

        if (preset->index == delete_preset->index){
            delete_pos = i;
            break;
        }
    }
    return delete_pos;
}

static void has_set_active_preset_record(uint8_t update_pos) {
    btstack_assert(update_pos < has_preset_records_num);
    has_remove_task_from_queue();

    // reset old active
    uint8_t i;
    for (i = 0; i < has_preset_records_num; i++) {
        has_preset_records[i].active = false;
    }

    has_preset_record_t * preset_record = &has_preset_records[update_pos];
    // update
    has_active_preset_index = preset_record->index;
    has_preset_records[update_pos].active = true;

    dump_preset_records("After execute active");
}

static void has_set_preset_record_availability(uint8_t update_pos, bool available) {
    btstack_assert(update_pos < has_preset_records_num);
    has_remove_task_from_queue();

    has_preset_record_t * preset_record = &has_preset_records[update_pos];
    // update
    has_active_preset_index = preset_record->index;
    if (available){
        has_preset_records[update_pos].properties |= HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE;
    } else {
        has_preset_records[update_pos].properties &= ~HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE;
    }

    dump_preset_records("After execute availability");
}

static void has_server_can_send_now(void * context){
    has_server_connection_t * connection = (has_server_connection_t *) context;
    btstack_assert(connection != NULL);

    // first preset record in "wait to process" queue:
    uint8_t queued_preset_pos = has_preset_records_num;
    has_preset_record_t * queued_preset = &has_preset_records[queued_preset_pos];

    if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_CONTROL_POINT_OPERATION){
        if (connection->state == HAS_SERVER_CONNECTION_STATE_PENDING_ATT_RESPONSE){
            att_server_response_ready(connection->con_handle);
            return;
        }

        if ((connection->scheduled_control_point_notification_tasks & HAS_CP_NOTIFICATION_TASK_READ_PRESETS) > 0u){
            btstack_assert(connection->state == HAS_SERVER_CONNECTION_STATE_PENDING_ATT_INDICATION);

            bool is_last_preset_record;
            has_preset_record_t *preset = has_read_presets_operation_get_next_preset_record(connection,
                                                                                            &is_last_preset_record);

            uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
            uint8_t pos = 0;
            value[pos++] = (uint8_t) HAS_OPCODE_READ_PRESET_RESPONSE;
            value[pos++] = is_last_preset_record ? 1u : 0u;
            value[pos++] = preset->index;
            value[pos++] = preset->properties;

            uint16_t mtu = att_server_get_mtu(connection->con_handle);
            btstack_assert(mtu > (pos + 3));
            uint16_t available_payload_size = mtu - 3 - pos;
            uint16_t name_len = btstack_min(available_payload_size, strlen(preset->name));
            btstack_strcpy((char *) &value[pos], name_len, preset->name);
            pos += name_len;

            (void) att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

            if (is_last_preset_record) {
                connection->state = HAS_SERVER_CONNECTION_STATE_READY;
                connection->scheduled_control_point_notification_tasks &= ~HAS_CP_NOTIFICATION_TASK_READ_PRESETS;
                has_remove_task_from_queue();

            }
        }

    } else if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED) {
        connection->scheduled_preset_record_change_notification = false;

        uint8_t insert_pos = find_position_of_regular_preset_record_to_insert(queued_preset);
        btstack_assert(insert_pos != HAS_INVALID_PRESET_RECORD_POSITION);

        uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
        uint8_t pos = 0;
        value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
        value[pos++] = (uint8_t)HAS_CHANGEID_GENERIC_UPDATE;
        value[pos++] = (insert_pos == queued_preset_pos) ? 1 : 0; // is_last
        value[pos++] = (insert_pos == 0) ? HAS_EMPTY_PRESET_RECORD_INDEX : has_preset_records[insert_pos-1].index;
        value[pos++] = queued_preset->index;
        value[pos++] = queued_preset->properties;

        uint16_t mtu = att_server_get_mtu(connection->con_handle);
        btstack_assert(mtu > (pos + 3));
        uint16_t available_payload_size = mtu - 3 - pos;
        uint16_t name_len = btstack_min(available_payload_size, strlen(queued_preset->name));
        btstack_strcpy((char *)&value[pos], name_len, queued_preset->name);
        pos += name_len;

        att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

        if (has_setup_next_connection_for_preset_changed_notification()){
            return;
        }
        // we are done
        has_insert_preset_record(insert_pos, value[4], value[5], (char *) &value[6]);
        
    } else if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED){
        connection->scheduled_preset_record_change_notification = false;

        uint8_t delete_pos = find_position_of_regular_preset_record_to_delete(queued_preset);
        btstack_assert(delete_pos != HAS_INVALID_PRESET_RECORD_POSITION);

        uint8_t value[4];
        uint8_t pos = 0;
        value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
        value[pos++] = (uint8_t)HAS_CHANGEID_PRESET_RECORD_DELETED;
        value[pos++] = (delete_pos == queued_preset_pos) ? 1 : 0; // is_last
        value[pos++] = queued_preset->index;

        att_server_indicate(connection->con_handle, has_control_point_value_handle, &value[0], pos);

        if (has_setup_next_connection_for_preset_changed_notification()){
            return;
        }
        // we are done
        has_delete_preset_record(delete_pos);

    } else if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE){
        connection->scheduled_preset_record_change_notification = false;

        uint8_t update_pos = find_position_of_regular_preset_record_with_index(queued_preset->index);
        btstack_assert(update_pos != HAS_INVALID_PRESET_RECORD_POSITION);
        uint8_t value[4];
        uint8_t pos = 0;
        value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
        value[pos++] = (uint8_t)HAS_CHANGEID_PRESET_RECORD_AVAILABLE;
        value[pos++] = (update_pos == queued_preset_pos) ? 1 : 0; // is_last
        value[pos++] = queued_preset->index;

        att_server_indicate(connection->con_handle, has_active_preset_index_value_handle, &value[0], 0);

        if (has_setup_next_connection_for_preset_changed_notification()){
            return;
        }
        // we are done
        has_set_preset_record_availability(update_pos, true);

    } else if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE){
        connection->scheduled_preset_record_change_notification = false;

        uint8_t update_pos = find_position_of_regular_preset_record_with_index(queued_preset->index);
        btstack_assert(update_pos != HAS_INVALID_PRESET_RECORD_POSITION);
        uint8_t value[4];
        uint8_t pos = 0;
        value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
        value[pos++] = (uint8_t)HAS_CHANGEID_PRESET_RECORD_UNAVAILABLE;
        value[pos++] = (update_pos == queued_preset_pos) ? 1 : 0; // is_last
        value[pos++] = queued_preset->index;

        att_server_indicate(connection->con_handle, has_active_preset_index_value_handle, &value[0], 0);

        if (has_setup_next_connection_for_preset_changed_notification()){
            return;
        }
        // we are done
        has_set_preset_record_availability(update_pos, false);

    } else if (queued_preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE) {
        connection->scheduled_preset_record_change_notification = false;

        uint8_t update_pos = find_position_of_regular_preset_record_with_index(queued_preset->index);
        btstack_assert(update_pos != HAS_INVALID_PRESET_RECORD_POSITION);
        uint8_t value[1];
        value[0] = queued_preset->index;

        att_server_notify(connection->con_handle, has_active_preset_index_value_handle, &value[0], sizeof(value));

        if (has_setup_next_connection_for_preset_changed_notification()) {
            return;
        }
        // we are done
        has_set_active_preset_record(update_pos);
    }

    if (has_queued_preset_records_num > 0){
        has_server_schedule_task();
    }
}


static bool has_server_schedule_task(void){
    uint8_t i;
    int16_t connection_index = -1;
    has_server_connection_t * connection;

    for (i = 0; i < has_clients_num; i++){
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
        printf("Schedule Task 0%04X\n", has_preset_records[has_preset_records_num + has_queued_preset_records_num - 1].scheduled_task);
        connection = &has_clients[(uint8_t)connection_index];
        connection->scheduled_tasks_callback.callback = &has_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_request_to_send_indication(&connection->scheduled_tasks_callback, connection->con_handle);
        return true;
    }

    return false;
}

static bool is_add_operation_valid(uint8_t index){
    has_preset_record_t preset;
    bool exists = preset_calc_final_state(index, &preset);
    return exists == false;
}

static bool is_delete_operation_valid(uint8_t index){
    has_preset_record_t preset;
    bool exists = preset_calc_final_state(index, &preset);
    return exists;
}

static bool is_set_active_operation_valid(uint8_t index){
    has_preset_record_t preset;
    bool exists = preset_calc_final_state(index, &preset);
    if (!exists){
        return false;
    }
    if ( (preset.properties & HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE) == 0u ){
        return false;
    }
    return (preset.active == false);
}

static bool is_set_available_operation_valid(uint8_t index){
    has_preset_record_t preset;
    bool exists = preset_calc_final_state(index, &preset);
    return exists && ((preset.properties & HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE) > 0u );
}

static bool is_set_unavailable_operation_valid(uint8_t index){
    has_preset_record_t preset;
    bool exists = preset_calc_final_state(index, &preset);
    return exists && ((preset.properties & HEARING_AID_PRESET_PROPERTIES_MASK_NAME_IS_AVAILABLE) == 0u );
}

uint8_t hearing_access_service_server_add_preset(uint8_t index, uint8_t properties, char * name){
    if (!is_add_operation_valid(index)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if ((has_preset_records_num + has_queued_preset_records_num + 1) > has_preset_records_max_num){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    uint8_t queue_pos = has_add_preset_record_change_to_queue(index, properties, name,
                                                              HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED);

    // schedule notifications
    if (!has_server_schedule_task()){
        uint8_t insert_pos = find_position_of_regular_preset_record_to_insert(&has_preset_records[queue_pos]);
        has_insert_preset_record(insert_pos, index, properties, name);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_delete_preset(uint8_t index){
    if (!is_delete_operation_valid(index)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if ((has_preset_records_num + has_queued_preset_records_num + 1) > has_preset_records_max_num){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    // queue the change at the tail for later processing
    uint8_t queue_pos = has_add_preset_record_change_to_queue(index, 0, "", HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED);

    // schedule notifications
    if (!has_server_schedule_task()){
        uint8_t delete_pos = find_position_of_regular_preset_record_to_delete(&has_preset_records[queue_pos]);
        has_delete_preset_record(delete_pos);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_preset_record_set_active(uint8_t index){
    if (!is_set_active_operation_valid(index)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if ((has_preset_records_num + has_queued_preset_records_num + 1) > has_preset_records_max_num){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    uint8_t queue_pos = has_add_preset_record_change_to_queue(index, 0, "", HAS_NOTIFICATION_TASK_PRESET_RECORD_ACTIVE);

    if (!has_server_schedule_task()){
        uint8_t update_pos = find_position_of_regular_preset_record_with_index(has_preset_records[queue_pos].index);
        has_set_active_preset_record(update_pos);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hearing_access_service_server_preset_record_set_available(uint8_t index){
    if (!is_set_available_operation_valid(index)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if ((has_preset_records_num + has_queued_preset_records_num + 1) > has_preset_records_max_num){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    uint8_t queue_pos = has_add_preset_record_change_to_queue(index, 0, "",
                                                              HAS_NOTIFICATION_TASK_PRESET_RECORD_AVAILABLE);

    if (!has_server_schedule_task()){
        uint8_t update_pos = find_position_of_regular_preset_record_with_index(has_preset_records[queue_pos].index);
        has_set_active_preset_record(update_pos);
    }
    return ERROR_CODE_SUCCESS;
}


uint8_t hearing_access_service_server_preset_record_set_unavailable(uint8_t index){
    if (!is_set_unavailable_operation_valid(index)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if ((has_preset_records_num + has_queued_preset_records_num + 1) > has_preset_records_max_num){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    uint8_t queue_pos = has_add_preset_record_change_to_queue(index, 0, "",
                                                              HAS_NOTIFICATION_TASK_PRESET_RECORD_UNAVAILABLE);

    if (!has_server_schedule_task()){
        uint8_t update_pos = find_position_of_regular_preset_record_with_index(has_preset_records[queue_pos].index);
        has_set_active_preset_record(update_pos);
    }
    return ERROR_CODE_SUCCESS;
}

#ifdef ENABLE_TESTING_SUPPORT
void hearing_access_service_server_execute(void){
    while (has_queued_preset_records_num > 0){
        // first preset record in "wait to process" queue:
        uint8_t preset_pos = has_preset_records_num;
        has_preset_record_t * preset = &has_preset_records[preset_pos];

        if (preset->scheduled_task == 0){
            return;
        }

        if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_ADDED) {

            uint8_t insert_pos = find_position_of_regular_preset_record_to_insert(preset);
            btstack_assert(insert_pos != HAS_INVALID_PRESET_RECORD_POSITION);

            uint8_t value[6 + HAS_PRESET_RECORD_NAME_MAX_LENGTH];
            uint8_t pos = 0;
            value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
            value[pos++] = (uint8_t)HAS_CHANGEID_GENERIC_UPDATE;
            value[pos++] = (insert_pos == preset_pos) ? 1 : 0; // is_last
            value[pos++] = (insert_pos == 0) ? HAS_EMPTY_PRESET_RECORD_INDEX : has_preset_records[insert_pos-1].index;
            value[pos++] = preset->index;
            value[pos++] = preset->properties;

            // we are done
            has_insert_preset_record(insert_pos, value[4], value[5], (char *) &value[6]);

        } else if (preset->scheduled_task == HAS_NOTIFICATION_TASK_PRESET_RECORD_DELETED){

            uint8_t delete_pos = find_position_of_regular_preset_record_to_delete(preset);
            if (delete_pos != HAS_INVALID_PRESET_RECORD_POSITION){
                uint8_t value[4];
                uint8_t pos = 0;
                value[pos++] = (uint8_t)HAS_OPCODE_PRESET_CHANGED;
                value[pos++] = (uint8_t)HAS_CHANGEID_PRESET_RECORD_DELETED;
                value[pos++] = (delete_pos == preset_pos) ? 1 : 0; // is_last
                value[pos++] = preset->index;

                // we are done
                has_delete_preset_record(delete_pos);
            }
        }
    }

   dump_preset_records("After execute");
}
#endif

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

#define BTSTACK_FILE__ "object_transfer_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <media_control_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#define printf_testing(...) printf(__VA_ARGS__)
#else
#define printf_testing(...)
#endif

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"
#include "l2cap.h"

#include "le-audio/gatt-service/object_transfer_service_server.h"
#include "le-audio/le_audio_util.h"
#include "bluetooth_psm.h"

#define OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE                   0x01
#define OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE                   0x02
#define OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE                   0x04

typedef enum {
    OTS_FEATURE_INDEX = 0, 
    OTS_OBJECT_NAME_INDEX, 
    OTS_OBJECT_TYPE_INDEX, 
    OTS_OBJECT_SIZE_INDEX, 
    OTS_OBJECT_FIRST_CREATED_INDEX, 
    OTS_OBJECT_LAST_MODIFIED_INDEX, 
    OTS_OBJECT_ID_INDEX, 
    OTS_OBJECT_PROPERTIES_INDEX, 
    OTS_OBJECT_ACTION_CONTROL_POINT_INDEX, 
    OTS_OBJECT_LIST_CONTROL_POINT_INDEX, 
    OTS_OBJECT_LIST_FILTER1_INDEX, 
    OTS_OBJECT_LIST_FILTER2_INDEX, 
    OTS_OBJECT_LIST_FILTER3_INDEX, 
    OTS_OBJECT_CHANGED_INDEX, 
    OTS_CHARACTERISTICS_NUM,
    OTS_CHARACTERISTICS_RFU
} ots_characteristic_index_t;

typedef struct {
    uint16_t  value_handle;
    uint16_t  client_configuration_handle;
} ots_characteristic_t;

static att_service_handler_t    object_transfer_service_server;
static btstack_packet_handler_t ots_server_event_callback;
static const ots_operations_t * ots_server_operations;

static btstack_linked_list_t ots_connections;
static uint8_t  ots_connections_num = 0;

static ots_characteristic_t  ots_characteristics[OTS_CHARACTERISTICS_NUM];

static uint32_t ots_oacp_features;
static uint32_t ots_olcp_features;

static void ots_server_schedule_task(ots_server_connection_t * connection, uint8_t task);

static ots_server_connection_t * ots_server_find_connection_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_server_connection_t * connection = (ots_server_connection_t*) btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static ots_server_connection_t * ots_server_find_connection_for_credit_based_cid(hci_con_handle_t credit_based_cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_server_connection_t * connection = (ots_server_connection_t*) btstack_linked_list_iterator_next(&it);

        if (connection->con_handle == HCI_CON_HANDLE_INVALID) continue;
        if (connection->credit_based_cid != credit_based_cid) continue;
        return connection;
    }
    return NULL;
}

static void ots_server_register_object_changed(ots_server_connection_t * connection, uint8_t change_flags){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_server_connection_t * con = (ots_server_connection_t*) btstack_linked_list_iterator_next(&it);
        if (con->con_handle != connection->con_handle){
            con->change_flags = change_flags;
            ots_server_schedule_task(con, OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE);
        }
    }
}

static void ots_server_reset_connection(ots_server_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->operation_timer);

    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    connection->oacp_configuration = 0;
    connection->olcp_configuration = 0;
    connection->object_changed_configuration = 0;
    connection->scheduled_tasks = 0;

    connection->current_object = NULL;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    connection->current_object_object_read_transfer_in_progress = false;

    connection->oacp_data_chunk_offset = 0;
    connection->oacp_data_chunk_length = 0;

    connection->oacp_abort_read = false;
    connection->oacp_truncate = false;
    connection->olcp_opcode = OLCP_OPCODE_READY;
    connection->oacp_opcode = OACP_OPCODE_READY;
}

static bool ots_server_is_current_object_locked(ots_server_connection_t * connection){
    if (connection->current_object_locked){
        return true;
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_server_connection_t * con = (ots_server_connection_t*) btstack_linked_list_iterator_next(&it);
        if (con->con_handle != connection->con_handle){
            if (con->current_object_locked) {
                return true;
            }
        }
    }
    return false;
}

static void ots_server_remove_connection_for_con_handle(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return;
    }
    ots_server_reset_connection(connection);
    btstack_linked_list_remove((btstack_linked_list_t *) &ots_connections, (btstack_linked_item_t *) connection);
}

static ots_server_connection_t * ots_server_find_or_add_connection_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    ots_server_connection_t * free_connection = NULL;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_server_connection_t * connection = (ots_server_connection_t*) btstack_linked_list_iterator_next(&it);
        
        if (connection->con_handle != HCI_CON_HANDLE_INVALID){
            if (connection->con_handle == con_handle) {
                return connection;
            }
        } else {
            free_connection = connection;
        }
    }
    if (free_connection){
        free_connection->con_handle = con_handle;
    }
    btstack_linked_list_add((btstack_linked_list_t *) &ots_connections, (btstack_linked_item_t *) free_connection);
    return free_connection;
}

static uint16_t ots_server_get_client_configuration_handle_for_characteristic_index(ots_characteristic_index_t index){
    return ots_characteristics[index].client_configuration_handle;
}

static uint16_t ots_server_get_value_handle_for_characteristic_index(ots_characteristic_index_t index){
    return ots_characteristics[index].value_handle;
}

static void ots_server_reset_current_object_name(ots_server_connection_t * connection){
    UNUSED(connection);
    // TODO
}

static void ots_server_emit_current_object_name_changed(ots_server_connection_t * connection){
    UNUSED(connection);
    // TODO
}

static void ots_server_emit_current_object_first_created_time_changed(ots_server_connection_t * connection){
    UNUSED(connection);
    // TODO
}

static void ots_server_emit_current_object_last_modified_time_changed(ots_server_connection_t * connection){
    UNUSED(connection);
    // TODO
}

static void ots_server_emit_current_object_properties_changed(ots_server_connection_t * connection){
    UNUSED(connection);
    // TODO
}

// static bool ots_current_object_valid_for_filters(ots_server_connection_t * connection){
//     return true;
// }

static void ots_server_emit_current_object_filter_changed(ots_server_connection_t * connection, uint8_t filter_index){
    UNUSED(connection);
    UNUSED(filter_index);
    // TODO
}

static void ots_server_reset_long_write_filter(ots_server_connection_t * connection){
    connection->long_write_filter_type = OTS_FILTER_TYPE_NO_FILTER;
    connection->long_write_data_length = 0;
    connection->long_write_filter_index = 0;
    memset(connection->long_write_data, 0, sizeof(connection->long_write_data));
    connection->long_write_attribute_handle = 0;
}

static bool ots_current_object_valid(ots_server_connection_t * connection){
    return (connection->current_object != NULL);
}

bool object_transfer_service_server_current_object_valid(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return false;
    }
    return ots_current_object_valid(connection);
}

static bool ots_server_current_object_procedure_permitted(ots_server_connection_t * connection, uint32_t object_property_mask){
    if (!ots_current_object_valid(connection)){
        return false;
    }
    if ( (object_property_mask < OBJECT_PROPERTY_MASK_DELETE) || (object_property_mask > OBJECT_PROPERTY_MASK_MARK)){
        return false;
    }

    return (connection->current_object->properties & object_property_mask ) != 0;
}

bool object_transfer_service_server_current_object_procedure_permitted(hci_con_handle_t con_handle, uint32_t object_property_mask){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return false;
    }
    return ots_server_current_object_procedure_permitted(connection, object_property_mask);
}

bool object_transfer_service_server_current_object_locked(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return false;
    }
    if (!ots_current_object_valid(connection)){
        return false;
    }
    return ots_server_is_current_object_locked(connection);
}

bool object_transfer_service_server_current_object_transfer_in_progress(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return false;
    }
    if (!ots_current_object_valid(connection)){
        return false;
    }
    return connection->current_object_object_transfer_in_progress;
}

uint8_t object_transfer_service_server_current_object_set_lock(hci_con_handle_t con_handle, bool locked){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_current_object_valid(connection)){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    connection->current_object_locked = locked;
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_current_object_set_transfer_in_progress(hci_con_handle_t con_handle, bool transfer_in_progress){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!ots_current_object_valid(connection)){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    connection->current_object_object_transfer_in_progress = transfer_in_progress;
    return ERROR_CODE_SUCCESS;
}


static void btstack_utc_store_time(btstack_utc_t * time_value, uint8_t * time_buffer_out, uint16_t time_buffer_out_size){
    if (time_buffer_out_size < 7){
        return;
    }
    little_endian_store_16(time_buffer_out, 0, time_value->year);
    time_buffer_out[2] = time_value->month;
    time_buffer_out[3] = time_value->day;
    time_buffer_out[4] = time_value->hours;
    time_buffer_out[5] = time_value->minutes;
    time_buffer_out[6] = time_value->seconds;
}

static void btstack_utc_read_time(uint8_t * time_buffer, uint16_t time_buffer_size, btstack_utc_t * time_out){
    if (time_buffer_size < 7){
        return;
    }

    time_out->year    = little_endian_read_16(time_buffer, 0);
    time_out->month   = time_buffer[2];
    time_out->day     = time_buffer[3];
    time_out->hours   = time_buffer[4];
    time_out->minutes = time_buffer[5];
    time_out->seconds = time_buffer[6];
}

static uint16_t ots_server_store_filter_list(const ots_filter_t * filter, uint8_t i, uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){
    UNUSED(i);

    uint8_t  field_data[5];
    uint16_t filters_offset = 0;
    uint16_t stored_bytes = 0;
    
    memset(buffer, 0, buffer_size);

    field_data[0] = (uint8_t)filter->type;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, filters_offset, buffer, buffer_size, buffer_offset);
    filters_offset++;

    if (filter->value_length == 0){
        return stored_bytes;
    }

    uint16_t data_size = filter->value_length;
    stored_bytes += le_audio_util_virtual_memcpy_helper(filter->value, data_size, filters_offset, buffer, buffer_size, buffer_offset);
    return stored_bytes;
}

static uint16_t ots_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    ots_server_connection_t * connection = NULL;

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_FEATURE_INDEX)){
        ots_server_find_or_add_connection_for_con_handle(con_handle);
        
        uint8_t ots_features[8];
        little_endian_store_32(ots_features, 0, ots_oacp_features);
        little_endian_store_32(ots_features, 4, ots_olcp_features);
        return att_read_callback_handle_blob(ots_features, sizeof(ots_features), offset, buffer, buffer_size); 
    }

    // handle indication
    if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection == NULL){
            return 0;
        }
        return att_read_callback_handle_little_endian_16(connection->oacp_configuration, offset, buffer, buffer_size); 
    }
    
    if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection == NULL){
            return 0;
        }
        return att_read_callback_handle_little_endian_16(connection->olcp_configuration, offset, buffer, buffer_size); 
    }

    if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_CHANGED_INDEX)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection == NULL){
            return 0;
        }
        return att_read_callback_handle_little_endian_16(connection->object_changed_configuration, offset, buffer, buffer_size); 
    }

    connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }

    if ((offset == 0) && !ots_current_object_valid(connection)){
        return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
    }

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_NAME_INDEX)){
        if (buffer == NULL){
            // get len and check if we have up to date value
            if ((offset != 0) && !ots_current_object_valid(connection)){
                return (uint16_t)ATT_READ_ERROR_CODE_OFFSET + (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
            }
        }
        return att_read_callback_handle_blob((const uint8_t *)connection->current_object->name, strlen(connection->current_object->name), offset, buffer, buffer_size);
    }

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_TYPE_INDEX)){
        if (connection->current_object->type != 0){
            return att_read_callback_handle_little_endian_16(connection->current_object->type, offset, buffer, buffer_size);
        } else {
            att_read_callback_handle_blob((const uint8_t *)connection->current_object->type_uuid128, sizeof(connection->current_object->type_uuid128), offset, buffer, buffer_size);
        }
        return 0;
    } 

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_SIZE_INDEX)){
        uint8_t object_size[8];
        little_endian_store_32(object_size, 0, connection->current_object->current_size);
        little_endian_store_32(object_size, 4, connection->current_object->allocated_size);
        return att_read_callback_handle_blob(object_size, sizeof(object_size), offset, buffer, buffer_size); 
    }

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_FIRST_CREATED_INDEX)){
        uint8_t time_buffer[7];
        btstack_utc_store_time(&connection->current_object->first_created, &time_buffer[0], sizeof(time_buffer));
        return att_read_callback_handle_blob(time_buffer, sizeof(time_buffer), offset, buffer, buffer_size);
    } 

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LAST_MODIFIED_INDEX)){
        uint8_t time_buffer[7];
        btstack_utc_store_time(&connection->current_object->last_modified, &time_buffer[0], sizeof(time_buffer));
        return att_read_callback_handle_blob(time_buffer, sizeof(time_buffer), offset, buffer, buffer_size);
    } 

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_ID_INDEX)){
        uint8_t object_id[6];
        reverse_48((const uint8_t *)connection->current_object->luid, object_id);

        return att_read_callback_handle_blob((const uint8_t *)object_id, OTS_OBJECT_ID_LEN, offset, buffer, buffer_size);
    } 
    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_PROPERTIES_INDEX)){
        uint8_t properties[4];
        little_endian_store_32(properties, 0, connection->current_object->properties);
        return att_read_callback_handle_blob(properties, sizeof(properties), offset, buffer, buffer_size);
    } 

    uint8_t i;
    for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
        if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i)){
            return ots_server_store_filter_list(ots_server_operations->get_filter(connection->con_handle, i), i, buffer, buffer_size, offset);
        }
    }
    return 0;
}


static void ots_server_can_send_now(void * context){
    ots_server_connection_t * connection = (ots_server_connection_t *) context;
    if (connection->con_handle == HCI_CON_HANDLE_INVALID){
        ots_server_reset_connection(connection);
        return;
    }
    
    if ((connection->scheduled_tasks & OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE) != 0){
        connection->scheduled_tasks &= ~OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE;

        uint8_t value[3 + 4];
        uint16_t value_length = 3;
        value[0] = OACP_OPCODE_RESPONSE_CODE; 
        value[1] = (uint8_t)connection->oacp_opcode;
        value[2] = (uint8_t)connection->oacp_result_code;
        
        uint16_t attribute_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX);
        
        switch (connection->oacp_opcode){
            case OACP_OPCODE_CALCULATE_CHECKSUM:
                little_endian_store_32(value, 3, connection->oacp_result_crc);
                value_length = 7;
                break;
            default:
                break;
        }
        
        att_server_indicate(connection->con_handle, attribute_handle, value, value_length);

        if (( connection->oacp_opcode == OACP_OPCODE_READ) && (connection->oacp_result_code == OACP_RESULT_CODE_SUCCESS)) {
            l2cap_request_can_send_now_event(connection->credit_based_cid);
        }
        connection->oacp_opcode = OACP_OPCODE_READY;
    } else if ((connection->scheduled_tasks & OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE) != 0){
        connection->scheduled_tasks &= ~OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE;

        uint8_t value[7];
        uint8_t pos = 0;
        value[pos++] = OLCP_OPCODE_RESPONSE_CODE;
        value[pos++] = (uint8_t)connection->olcp_opcode;
        value[pos++] = (uint8_t)connection->olcp_result_code;

        if (connection->olcp_opcode == OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS){
            little_endian_store_32(value, pos, connection->olcp_result_num_objects);
            pos += 4;
        }
        uint16_t attribute_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX);
        att_server_indicate(connection->con_handle, attribute_handle, value, pos);

        // allow next transaction
        connection->olcp_opcode = OLCP_OPCODE_READY;

    } else if ((connection->scheduled_tasks & OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE) != 0) {
        connection->scheduled_tasks &= ~OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE;
        if (connection->current_object != NULL){
            uint8_t value[7];
            value[0] = connection->change_flags;
            reverse_48((uint8_t *)connection->current_object->luid, &value[1]);

            uint16_t attribute_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_CHANGED_INDEX);
            att_server_indicate(connection->con_handle, attribute_handle, value, sizeof(value));
        }
    }

    if (connection->scheduled_tasks != 0){
        connection->scheduled_tasks_callback.callback = &ots_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

static void ots_server_schedule_task(ots_server_connection_t * connection, uint8_t task){
    if (connection->con_handle == HCI_CON_HANDLE_INVALID){
        ots_server_reset_connection(connection);
        return;
    }

    uint16_t configuration;
    switch (task){
        case OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE:
            configuration = connection->oacp_configuration;
            break;
        case OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE:
            configuration = connection->olcp_configuration;
            break;
        case OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE:
            configuration = connection->object_changed_configuration;
            break;
        default:
            btstack_unreachable();
            return;
    }

    if (configuration == 0){
        return;
    }

    uint16_t scheduled_tasks = connection->scheduled_tasks;
    connection->scheduled_tasks |= task;

    if (scheduled_tasks == 0){
        connection->scheduled_tasks_callback.callback = &ots_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

static bool ots_server_gatt_uuid_size_valid(uint16_t uuid_size){
    return (uuid_size == 2) || (uuid_size == 16);
}

static bool ots_server_supports_object_type(ots_object_type_t object_type){
    switch (object_type){
        case OTS_OBJECT_TYPE_GROUP:
        case OTS_OBJECT_TYPE_MEDIA_PLAYER_ICON:
        case OTS_OBJECT_TYPE_TRACK_SEGMENTS:
        case OTS_OBJECT_TYPE_TRACK:
        case OTS_OBJECT_TYPE_DIRECTORY_LISTING:
            return true;
        default:
            return false;
    }
}



int ots_server_handle_action_control_point_operation(ots_server_connection_t * connection, uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size == 0){
        connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
        return 0;
    }

    // allow only a single transaction per time
    if (connection->oacp_opcode != OACP_OPCODE_READY) {
        connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
        return 0;
    }

    uint8_t pos = 0;
    connection->oacp_opcode = buffer[pos++];
    if ((connection->oacp_opcode == OACP_OPCODE_READY) || (connection->oacp_opcode >= OACP_OPCODE_RFU)){
        connection->oacp_result_code = OACP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
        return 0;
    } 

    if ((ots_oacp_features & (1 << connection->oacp_opcode)) == 0 ){
        connection->oacp_result_code = OACP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
        return 0;
    }

    if (connection->oacp_configuration < 1 || connection->oacp_configuration > 2){
        return ATT_ERROR_RESPONSE_ATT_ERROR_CCCD_IMPROPERLY_CONFIGURED;
    }

    uint16_t data_size = buffer_size - pos;
    uint8_t mode;
    uint32_t offset;
    uint32_t length;
    uint32_t object_size;
    ots_object_type_t type_uuid16;
    uint8_t  gatt_uuid_size;

    switch (connection->oacp_opcode){
        case OACP_OPCODE_CREATE:
            if (data_size < 6){
                connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            if (!ots_server_gatt_uuid_size_valid(data_size - 4)) {
                connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            object_size = little_endian_read_32(buffer, pos);
            pos += 4;
            gatt_uuid_size = buffer_size - pos;

            // 1. Unsupported Type - The Server does not accept an object of the type specified in the Type parameter.
            if (gatt_uuid_size != 2) {
                connection->oacp_result_code =  OACP_RESULT_CODE_UNSUPPORTED_TYPE;
                break;
            }

            type_uuid16 = (ots_object_type_t)little_endian_read_16(buffer, pos);
            if (!ots_server_supports_object_type(type_uuid16)){
                connection->oacp_result_code =  OACP_RESULT_CODE_UNSUPPORTED_TYPE;
                break;
            }

            // 2. Insufficient Resources - The Server cannot accept an object of the size specified in the Size parameter.
            if (!ots_server_operations->can_allocate_object_of_size(connection->con_handle, object_size)){
                connection->oacp_result_code =  OACP_RESULT_CODE_INSUFFICIENT_RESOURCES;
                break;
            }

            // 3. Invalid Parameter - The Parameter received does not meet the requirements of the service
            if (object_size == 0){
                connection->oacp_result_code =  OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            connection->oacp_result_code = ots_server_operations->create_object(connection->con_handle, object_size, type_uuid16);
            if (connection->oacp_result_code == OACP_RESULT_CODE_SUCCESS) {
                ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) |
                                                               (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_CREATED));
            }
            break;

        case OACP_OPCODE_DELETE:
            if (connection->current_object == NULL){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }
            // 1. Invalid Object
            if (!object_transfer_service_server_current_object_valid(connection->con_handle)){
                connection->oacp_result_code =  OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }

            // 2. Procedure Not Permitted - The object’s properties do not permit deletion of the object.
            if (!object_transfer_service_server_current_object_procedure_permitted(connection->con_handle, OBJECT_PROPERTY_MASK_DELETE)){
                connection->oacp_result_code =  OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 3. Object Locked by server
            if (object_transfer_service_server_current_object_locked(connection->con_handle)){
                connection->oacp_result_code =  OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            // 4. Object Locked - An object transfer is in progress that is using the Current Object
            if (object_transfer_service_server_current_object_transfer_in_progress(connection->con_handle)){
                connection->oacp_result_code =  OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            connection->oacp_result_code = ots_server_operations->delete_object(connection->con_handle);

            if (connection->oacp_result_code == OACP_RESULT_CODE_SUCCESS){
                memset(connection->current_object, 0, sizeof(ots_object_t));
                connection->current_object = NULL;
                connection->current_object_locked = false;
                connection->current_object_object_transfer_in_progress = false;
                ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_DELETED));
            }
            break;
        
        case OACP_OPCODE_CALCULATE_CHECKSUM:
            if (data_size < 8){
                connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            // 1. Invalid Object
            if (!object_transfer_service_server_current_object_valid(connection->con_handle)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }
            // 2. Invalid Parameter - The sum of the values of the Offset and Length parameters
            //                        exceeds the value of the Current Size field of the Object Size characteristic.
            offset = little_endian_read_32(buffer, pos);
            length = little_endian_read_32(buffer, pos+4);

            if ((offset + length) > object_transfer_service_server_current_object_size(connection->con_handle)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            // 3. Object Locked by server
            if (object_transfer_service_server_current_object_locked(connection->con_handle)){
                connection->oacp_result_code = OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            // 4. Object Locked - An object transfer is in progress that is using the Current Object
            if (object_transfer_service_server_current_object_transfer_in_progress(connection->con_handle)){
                connection->oacp_result_code = OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            connection->oacp_result_code = ots_server_operations->calculate_checksum(connection->con_handle, offset, length, &connection->oacp_result_crc);
            break;
        
        case OACP_OPCODE_EXECUTE:
            // 1. Invalid Object
            if (!object_transfer_service_server_current_object_valid(connection->con_handle)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }

            // 2. Procedure Not Permitted - The object’s properties do not permit execution of the object.
            if (!object_transfer_service_server_current_object_procedure_permitted(connection->con_handle, OBJECT_PROPERTY_MASK_EXECUTE)){
                connection->oacp_result_code = OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 3. Object Locked by server
            if (object_transfer_service_server_current_object_locked(connection->con_handle)){
                connection->oacp_result_code =  OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            connection->oacp_result_code = ots_server_operations->execute(connection->con_handle);
            if (connection->oacp_result_code == OACP_RESULT_CODE_SUCCESS){
                connection->oacp_data_chunk_offset = 0;
                connection->oacp_data_chunk_length = 0;
                connection->current_object_locked = false;
                connection->current_object_object_transfer_in_progress = false;
                ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_CONTENTS_CHANGED));
            }
            break;
        
        case OACP_OPCODE_READ:   
            if (data_size < 8){
                connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            // 1. Invalid Object
            if (!ots_current_object_valid(connection)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }

            // 2. Procedure Not Permitted - The object’s properties do not permit reading the object
            if (!ots_server_current_object_procedure_permitted(connection, OBJECT_PROPERTY_MASK_READ)){
                connection->oacp_result_code = OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 3. Channel Unavailable - An Object Transfer Channel was not available for use
            if (connection->credit_based_cid == 0){
                connection->oacp_result_code = OACP_RESULT_CODE_CHANNEL_UNAVAILABLE;
                break;
            }

            offset = little_endian_read_32(buffer, pos);
            length = little_endian_read_32(buffer, pos + 4);

            // 4. Invalid Parameter - The value of the Offset parameter exceeds the value of the Current Size field of the Object Size characteristic
            if (offset > connection->current_object->current_size){
                connection->oacp_result_code =  OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            // 5. Invalid Parameter - The sum of the values of the Offset and Length parameters exceeds the value of the Current Size field of the Object Size characteristic.
            if ((offset + length) >connection->current_object->current_size){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            // 6. Insufficient Resources - The value of the Length parameter exceeds the number of octets that the Server has the capacity to read from the object.
            if (length > connection->remote_mtu){
                connection->oacp_result_code = OACP_RESULT_CODE_INSUFFICIENT_RESOURCES;
                break;
            }

            // 7. Object Locked - An object transfer is in progress that is using the Current Object
            if (connection->current_object_object_transfer_in_progress){
                connection->oacp_result_code = OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            connection->current_object_locked = true;

            connection->oacp_data_chunk_length = length;
            connection->oacp_data_chunk_offset = offset;
            connection->oacp_data_bytes_read = 0;
            connection->oacp_result_code = OACP_RESULT_CODE_SUCCESS;
            break;
        
        case OACP_OPCODE_WRITE:
            if (data_size < 9){
                connection->oacp_result_code =  OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            offset = little_endian_read_32(buffer, pos);
            pos += 4;
            length = little_endian_read_32(buffer, pos);
            pos += 4;

            mode = buffer[pos++];


            // 1. Invalid Object
            if (!ots_current_object_valid(connection)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_OBJECT;
                break;
            }

            // 2. Procedure Not Permitted - The object’s properties do not permit writing the object
            if (!ots_server_current_object_procedure_permitted(connection, OBJECT_PROPERTY_MASK_WRITE)){
                connection->oacp_result_code = OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 3. Procedure Not Permitted - Patching was attempted but patching is not supported by the Server.
            if (((length + offset) < connection->current_object->current_size) &&
                !ots_server_current_object_procedure_permitted(connection, OBJECT_PROPERTY_MASK_PATCH)){
                connection->oacp_result_code = OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 4. Procedure Not Permitted - Truncation was attempted but the object’s properties do not permit truncation of the object contents.
            if ((mode == 2) &&
                !ots_server_current_object_procedure_permitted(connection, OBJECT_PROPERTY_MASK_TRUNCATE)){
                connection->oacp_result_code = OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED;
                break;
            }

            // 5. Channel Unavailable - An Object Transfer Channel was not available for use
            if (connection->credit_based_cid == 0){
                connection->oacp_result_code = OACP_RESULT_CODE_CHANNEL_UNAVAILABLE;
                break;
            }

            // 6. Invalid Parameter - The Mode parameter contains an RFU value.
            if ((mode != 0) && (mode != 2)){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }


            // 7. Invalid Parameter - The value of the Offset parameter exceeds the value of the Current Size field of the Object Size characteristic.
            if (offset > connection->current_object->current_size){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            // 8. Invalid Parameter - The sum of the values of the Offset and Length parameters exceeds the value of the Allocated Size field
            //                        of the Object Size characteristic AND the Server does NOT support appending additional data to an object.
            if ((offset + length) > connection->current_object->allocated_size){
                if (!ots_server_current_object_procedure_permitted(connection, OBJECT_PROPERTY_MASK_APPEND)){
                    connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                    break;
                }

                // 9. Insufficient Resources - The value of the Length parameter exceeds the number of octets that the Server has the capacity to write to the object.
                connection->oacp_result_code = ots_server_operations->increase_allocated_size(connection->con_handle, offset + length);
                if (connection->oacp_result_code != OACP_RESULT_CODE_SUCCESS){
                    break;
                }
                connection->current_object->current_size = offset + length;
                if (connection->current_object->current_size < connection->current_object->allocated_size){
                    connection->current_object->allocated_size = connection->current_object->current_size;
                }
            }

            // 10. Object Locked - The Current Object is locked by the Server.
            if (ots_server_is_current_object_locked(connection)){
                connection->oacp_result_code = OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            // 11. Object Locked - An object transfer is in progress that is using the Current Object
            if (connection->current_object_object_transfer_in_progress){
                connection->oacp_result_code =  OACP_RESULT_CODE_OBJECT_LOCKED;
                break;
            }

            connection->current_object_locked = true;

            connection->oacp_data_chunk_length = length;
            connection->oacp_data_chunk_offset = offset;
            connection->oacp_truncate = mode == 2;
            connection->oacp_write_offset = 0;
            connection->oacp_result_code = OACP_RESULT_CODE_SUCCESS;
            break;
        
        case OACP_OPCODE_ABORT:
            // 1. No OACP Read operation is in progress – there is nothing to abort.
            if (!connection->current_object_object_read_transfer_in_progress){
                connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
                break;
            }

            // 2. The Abort Op Code was not sent by the same Client who started the OACP Read operation.
            // 3. The requested procedure failed for a reason other than those enumerated above in this table.

            if (connection->credit_based_cid == 0) {
                connection->oacp_result_code = OACP_RESULT_CODE_CHANNEL_UNAVAILABLE;
                break;
            }
            connection->oacp_abort_read = true;
            connection->oacp_result_code = OACP_RESULT_CODE_SUCCESS;
            break;
        
        default:
            btstack_unreachable();
            return 0;
    }

    ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
    return 0;
}

// static bool ots_server_object_passes_filters(ots_server_connection_t * connection, ots_object_t * object){
//     return true;
// }

static int ots_server_handle_list_control_point_write(ots_server_connection_t * connection, uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size == 0){
        connection->olcp_result_code = OLCP_RESULT_CODE_INVALID_PARAMETER;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE);
        return 0;
    }

    connection->olcp_opcode = buffer[0];
    if ((connection->olcp_opcode == OLCP_OPCODE_READY) || (connection->olcp_opcode >= OLCP_OPCODE_RFU)){
        connection->olcp_result_code = OLCP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        ots_server_schedule_task(connection, OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE);
        return 0;
    }

    if (connection->olcp_configuration < 1 || connection->olcp_configuration > 2){
        return ATT_ERROR_RESPONSE_ATT_ERROR_CCCD_IMPROPERLY_CONFIGURED;
    }
    olcp_list_sort_order_t order;
    uint8_t object_id[6];

    switch (connection->olcp_opcode){
        case OLCP_OPCODE_FIRST:
            connection->olcp_result_code = ots_server_operations->first(connection->con_handle);
            break;

        case OLCP_OPCODE_LAST:
            connection->olcp_result_code = ots_server_operations->last(connection->con_handle);
            break;
        case OLCP_OPCODE_PREVIOUS:
            connection->olcp_result_code = ots_server_operations->previous(connection->con_handle);
            break;   
        case OLCP_OPCODE_NEXT:
            connection->olcp_result_code = ots_server_operations->next(connection->con_handle);
            break;
        case OLCP_OPCODE_GOTO:
            if (buffer_size < 7){
                connection->olcp_result_code = OLCP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }
            reverse_48(&buffer[1], object_id);
            connection->olcp_result_code = ots_server_operations->go_to(connection->con_handle, (ots_object_id_t *)object_id);
            break;

        case OLCP_OPCODE_ORDER:
            if (buffer_size < 2){
                connection->olcp_result_code = OLCP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }

            order = (olcp_list_sort_order_t)buffer[1];
            if ((order == OLCP_LIST_SORT_ORDER_NONE) || (order >= OLCP_LIST_SORT_ORDER_RFU)){
                connection->olcp_result_code = OLCP_RESULT_CODE_INVALID_PARAMETER;
                break;
            }
            connection->olcp_result_code = ots_server_operations->sort(connection->con_handle, order);
            break;
        case OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS:
            connection->olcp_result_code = ots_server_operations->number_of_objects(connection->con_handle, &connection->olcp_result_num_objects);
            break;
        case OLCP_OPCODE_CLEAR_MARKING:
            connection->olcp_result_code = ots_server_operations->clear_marking(connection->con_handle);
            break;
        default:
            btstack_unreachable();
            return 0;
    }

    ots_server_schedule_task(connection, OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE);
    return 0;
}

static uint8_t ots_server_filter_buffer_valid_write(uint8_t * buffer, uint16_t buffer_size, uint16_t offset, ots_server_connection_t * connection){
    if (buffer_size < 1){
        return ATT_ERROR_WRITE_REQUEST_REJECTED;
    }
    uint8_t data_size = (uint8_t)buffer_size + (uint8_t)offset - 1;
    uint8_t max_data_size = sizeof(connection->long_write_data);

    if (data_size > max_data_size){
        return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
    }

    if (offset != 0){
        return ATT_ERROR_SUCCESS;
    }

    memset(connection->long_write_data, 0, sizeof(connection->long_write_data));
    connection->long_write_data_length = data_size;
    connection->long_write_filter_type = (ots_filter_type_t) buffer[0];

    if (connection->long_write_filter_type >= OTS_FILTER_TYPE_RFU){
        return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
    }

    uint8_t status = ATT_ERROR_SUCCESS;
    switch (connection->long_write_filter_type){
        case OTS_FILTER_TYPE_NO_FILTER:
        case OTS_FILTER_TYPE_MARKED_OBJECTS:
            if (connection->long_write_data_length != 0){
                status =  ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            break;

        case OTS_FILTER_TYPE_CREATED_BETWEEN:
        case OTS_FILTER_TYPE_MODIFIED_BETWEEN:
            if (connection->long_write_data_length != 14){
                status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            break;

        case OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN:
        case OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN:
            if (connection->long_write_data_length != 8){
                status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            } else {
                uint32_t min_size = little_endian_read_32(buffer, 1);
                uint32_t max_size = little_endian_read_32(buffer, 5);
                if (min_size > max_size){
                    status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
                }
            }
            break;

        case OTS_FILTER_TYPE_OBJECT_TYPE:
            if (connection->long_write_data_length != 2){
                status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }

            // TODO allow 16B values as well
            break;

        default:
            break;
    }
    return status;
}

static void ots_server_emit_filter_event(ots_server_connection_t * connection, uint8_t filter_index){
    btstack_assert(ots_server_event_callback != NULL);

    uint8_t event[8 + OTS_MAX_STRING_LENGHT];

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_SERVER_FILTER;
    little_endian_store_16(event, pos, connection->con_handle);
    pos += 2;
    event[pos++] = filter_index;
    event[pos++] = (uint8_t)connection->long_write_filter_type;
    event[pos++] = connection->long_write_data_length;
    memcpy(&event[pos], connection->long_write_data, connection->long_write_data_length);
    pos += connection->long_write_data_length;

    (*ots_server_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void ots_server_emit_disconnect_event(hci_con_handle_t con_handle){
    btstack_assert(ots_server_event_callback != NULL);

    uint8_t event[5];

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_LEAUDIO_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = LEAUDIO_SUBEVENT_OTS_SERVER_DISCONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;

    (*ots_server_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static int ots_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    ots_server_connection_t * connection = NULL;

    if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX)){
        if (buffer_size != 2){
            return 0;
        }
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection != NULL){
            connection->oacp_configuration = little_endian_read_16(buffer, 0);
        }
        return 0;
    
    } else if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX)){
        if (buffer_size != 2){
            return 0;
        }
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection != NULL){
            connection->olcp_configuration = little_endian_read_16(buffer, 0);
        }
        return 0;

    } else if (attribute_handle == ots_server_get_client_configuration_handle_for_characteristic_index(OTS_OBJECT_CHANGED_INDEX)){
        if (buffer_size != 2){
            return 0;
        }
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection != NULL){
            connection->object_changed_configuration = little_endian_read_16(buffer, 0);
        }
        return 0;
    }

    connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }
    uint8_t i;
    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_CANCEL:
            connection->current_object_object_transfer_in_progress = false;
            ots_server_reset_long_write_filter(connection);
            break;

        case ATT_TRANSACTION_MODE_EXECUTE:
            for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
                uint16_t filter_value_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i);

                if (connection->long_write_attribute_handle == filter_value_handle){
                    ots_server_emit_current_object_filter_changed(connection, i);
                    ots_server_reset_long_write_filter(connection);
                    break;
                }
            }
            if (!ots_current_object_valid(connection)){
                connection->current_object_object_transfer_in_progress = false;
                return 0;
            }
            if (connection->long_write_attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_NAME_INDEX)){
                connection->current_object_object_transfer_in_progress = false;
                memcpy(connection->current_object->name, connection->long_write_data, connection->long_write_data_length);
                ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED));
                ots_server_emit_current_object_name_changed(connection);
            }
            return 0;

        case ATT_TRANSACTION_MODE_ACTIVE:
            if (offset == 0){
                if (connection->long_write_attribute_handle != 0){
                    connection->current_object_object_transfer_in_progress = false;
                    return ATT_ERROR_INSUFFICIENT_RESOURCES;
                }
                connection->long_write_attribute_handle = attribute_handle;
            } else {
                if (connection->long_write_attribute_handle != attribute_handle){
                    connection->current_object_object_transfer_in_progress = false;
                    return ATT_ERROR_INSUFFICIENT_RESOURCES;
                }
            }
            connection->current_object_object_transfer_in_progress = true;
            break;

        default:
            break;
    }

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX)){
        return ots_server_handle_action_control_point_operation(connection, buffer, buffer_size);

    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX)){
        return ots_server_handle_list_control_point_write(connection, buffer, buffer_size);

    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_NAME_INDEX)){
        if (connection->current_object == NULL){
            return ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }

        uint16_t total_value_len = buffer_size + offset;
        // handle long write
        switch (transaction_mode){
            case ATT_TRANSACTION_MODE_NONE:
                if (buffer_size > OTS_MAX_STRING_LENGHT){
                    return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
                }

                if (ots_server_operations->find_object_with_name(con_handle, (const char*)buffer)){
                    return ATT_ERROR_RESPONSE_OTS_OBJECT_NAME_ALREADY_EXISTS;
                }

                btstack_strcpy(&connection->current_object->name[0], OTS_MAX_STRING_LENGHT, (const char *)buffer);
                break;

            case ATT_TRANSACTION_MODE_ACTIVE:
                if (total_value_len >= (OTS_MAX_STRING_LENGHT - 1)){
                    ots_server_reset_current_object_name(connection);
                    return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
                }
                if (offset == 0){
                    memset(connection->long_write_data, 0, OTS_MAX_STRING_LENGHT);
                }
                memcpy(&connection->long_write_data[offset], (const char *)buffer, buffer_size);
                break;

            default:
                break;
        }
        
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_PROPERTIES_INDEX)){
        if (buffer_size >= 4){
            uint32_t properties = little_endian_read_32(buffer, 0);
            if ((properties >> 8) > 0){
                return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            connection->current_object->properties = properties;
            ots_server_emit_current_object_properties_changed(connection);
        }
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_FIRST_CREATED_INDEX)){
        if (buffer_size >= 7){
            btstack_utc_read_time(buffer, buffer_size, &connection->current_object->first_created);
            ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED));
            ots_server_emit_current_object_first_created_time_changed(connection);
        }
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LAST_MODIFIED_INDEX)){
        if (buffer_size >= 7){
            btstack_utc_read_time(buffer, buffer_size, &connection->current_object->last_modified);
            ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED));
            ots_server_emit_current_object_last_modified_time_changed(connection);
        }
    } else {
        uint8_t status;
        for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
            if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i)){

                switch (transaction_mode){
                    case ATT_TRANSACTION_MODE_NONE:
                        status = ots_server_filter_buffer_valid_write(buffer, buffer_size, offset, connection);
                        if (status != ATT_ERROR_SUCCESS){
                            ots_server_reset_long_write_filter(connection);
                            return status;
                        }

                        memcpy(connection->long_write_data, &buffer[1], connection->long_write_data_length );
                        ots_server_emit_filter_event(connection, i);
                        ots_server_reset_long_write_filter(connection);
                        break;

                    case ATT_TRANSACTION_MODE_ACTIVE:
                        if (offset == 0){
                            status = ots_server_filter_buffer_valid_write(buffer, buffer_size, offset, connection);
                            if (status != ATT_ERROR_SUCCESS){
                                ots_server_reset_long_write_filter(connection);
                                return status;
                            }
                            memcpy(connection->long_write_data, &buffer[1], connection->long_write_data_length );
                        } else {
                            connection->long_write_data_length += buffer_size;
                            memcpy(&connection->long_write_data[offset - 1], buffer, buffer_size);
                        }
                        break;

                    default:
                        break;
                }
                return 0;
            }
        }
    }
    return 0;
}

static void ots_server_operations_timer_timeout_handler(btstack_timer_source_t * timer){
    hci_con_handle_t con_handle = (hci_con_handle_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);

    if (connection == NULL){
        return;
    }

    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    connection->current_object_object_read_transfer_in_progress = false;
    l2cap_le_disconnect(connection->credit_based_cid);
}

static void ots_server_operations_start_timer(ots_server_connection_t * connection){
    btstack_run_loop_set_timer_handler(&connection->operation_timer, ots_server_operations_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->operation_timer, (void *)(uintptr_t)connection->con_handle);

    btstack_run_loop_set_timer(&connection->operation_timer, 30000);
    btstack_run_loop_add_timer(&connection->operation_timer);
}

static void ots_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_address;
    uint16_t psm;
    UNUSED(psm); // used only in log_info

    uint16_t cid;
    uint16_t mtu;
    hci_con_handle_t handle;
    ots_server_connection_t * connection;
    const uint8_t * data;
    oacp_result_code_t result_code;
    uint32_t bytes_to_read;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = ots_server_find_connection_for_credit_based_cid(cid);
                    if (connection != NULL && (connection->credit_based_cid == cid)){
                        connection->credit_based_cid = 0;
                        connection->remote_mtu = 0;
                        break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    handle = hci_event_disconnection_complete_get_connection_handle(packet);
                    ots_server_remove_connection_for_con_handle(handle);
                    ots_server_emit_disconnect_event(handle);
                    break;

                // LE CBM
                case L2CAP_EVENT_CBM_INCOMING_CONNECTION:
                    handle = l2cap_event_cbm_incoming_connection_get_handle(packet);
                    connection = ots_server_find_connection_for_con_handle(handle);
                    if (connection == NULL){
                        break;
                    }
                    psm = l2cap_event_cbm_incoming_connection_get_psm(packet);
                    cid = l2cap_event_cbm_incoming_connection_get_local_cid(packet);

                    log_info("L2CAP: Accepting incoming LE connection request for 0x%02x, PSM %02x", cid, psm); 
                    l2cap_cbm_accept_connection(cid, connection->receive_buffer, sizeof(connection->receive_buffer), connection->initial_credits);
                    break;


                case L2CAP_EVENT_CBM_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    handle = l2cap_event_cbm_channel_opened_get_handle(packet);
                    connection = ots_server_find_connection_for_con_handle(handle);
                    if (connection == NULL){
                        break;
                    }

                    l2cap_event_cbm_channel_opened_get_address(packet, event_address);
                    psm = l2cap_event_cbm_channel_opened_get_psm(packet);
                    cid = l2cap_event_cbm_channel_opened_get_local_cid(packet);
                    handle = l2cap_event_cbm_channel_opened_get_handle(packet);
                    mtu = l2cap_event_cbm_channel_opened_get_remote_mtu(packet);

                    if (l2cap_event_cbm_channel_opened_get_status(packet) == ERROR_CODE_SUCCESS) {
                        log_info("L2CAP: LE Data Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x, remote mtu 0x%02x",
                               bd_addr_to_str(event_address), handle, psm, cid, little_endian_read_16(packet, 15), mtu);
                        connection->credit_based_cid = cid;
                        connection->remote_mtu = mtu;
                    } else {
                        log_info("L2CAP: LE Data Channel connection to device %s failed. status code %u", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    cid = l2cap_event_can_send_now_get_local_cid(packet);
                    connection = ots_server_find_connection_for_credit_based_cid(cid);
                    if (connection == NULL){
                        break;
                    }
                    if (!connection->current_object_locked){
                        break;
                    }

                    connection->current_object_object_transfer_in_progress = true;
                    connection->current_object_object_read_transfer_in_progress = true;

                    bytes_to_read = btstack_min(connection->remote_mtu, connection->oacp_data_chunk_length - connection->oacp_data_bytes_read);

                    if (bytes_to_read > 0){
                        result_code = ots_server_operations->read(connection->con_handle,  connection->oacp_data_chunk_offset + connection->oacp_data_bytes_read, bytes_to_read, &data);
                        if (result_code == OACP_RESULT_CODE_SUCCESS){
                            connection->oacp_data_bytes_read += bytes_to_read;
                            l2cap_send(connection->credit_based_cid, data, bytes_to_read);
                            l2cap_request_can_send_now_event(connection->credit_based_cid);
                            if (connection->oacp_data_chunk_length > connection->oacp_data_bytes_read){
                                break;
                            }

                        }
                        ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_CONTENTS_CHANGED));
                    }

                    connection->oacp_data_chunk_offset = 0;
                    connection->oacp_data_chunk_length = 0;
                    connection->oacp_data_bytes_read = 0;
                    connection->current_object_object_transfer_in_progress = false;
                    connection->current_object_object_read_transfer_in_progress = false;
                    connection->current_object_locked = false;
                    btstack_run_loop_remove_timer(&connection->operation_timer);
                    ots_server_register_object_changed(connection, (1 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED));
                    break;

               case L2CAP_EVENT_PACKET_SENT:
                    cid = l2cap_event_packet_sent_get_local_cid(packet);
                    break;
                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            connection = ots_server_find_connection_for_credit_based_cid(channel);
            if (connection == NULL){
                break;
            }
            if (!connection->current_object_locked){
                break;
            }
            if (connection->oacp_write_offset == 0){
                ots_server_operations_start_timer(connection);
                connection->current_object_object_transfer_in_progress = true;
            }

            if ((connection->oacp_write_offset + size) <= connection->oacp_data_chunk_length){
                ots_server_operations->write(connection->con_handle, connection->oacp_write_offset +  connection->oacp_data_chunk_offset, packet, size);
                connection->oacp_write_offset += size;
            } else {
                // TODO
                // In the event that the Server receives data via the Object Transfer Channel in excess of the expected number of octets,
                // the Server shall close the Object Transfer Channel to prevent the Client from sending further data through the
                // Object Transfer Channel. The Server may discard the data it has received.
                break;
            }

            if (connection->oacp_write_offset == connection->oacp_data_chunk_length) {
                uint32_t data_size = connection->oacp_data_chunk_offset + connection->oacp_data_chunk_length;
                if (connection->oacp_truncate){
                    connection->current_object->current_size = data_size;
                }
                if (data_size > connection->current_object->current_size) {
                    if (data_size > connection->current_object->allocated_size) {
                        connection->current_object->allocated_size = data_size;
                    }
                    connection->current_object->current_size = data_size;
                }

                btstack_run_loop_remove_timer(&connection->operation_timer);
            }
            break;

        default:
            break;;
    }
}

uint8_t object_transfer_service_server_init(uint32_t oacp_features, uint32_t olcp_features, 
    uint8_t storage_connections_num, ots_server_connection_t * storage_connections, 
    const ots_operations_t * operations){
    
    btstack_assert(storage_connections_num != 0);
    btstack_assert(storage_connections != NULL);
    btstack_assert(operations != NULL);
    
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    bool service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER, &start_handle, &end_handle);
    
    if (!service_found){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    log_info("Found OTS service 0x%02x-0x%02x", start_handle, end_handle);

    // le data channel setup
    uint8_t status = l2cap_cbm_register_service(&ots_server_packet_handler, BLUETOOTH_PSM_OTS, LEVEL_2);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

#ifdef ENABLE_TESTING_SUPPORT
    printf("Found OTS Service 0x%02x - 0x%02x \n", start_handle, end_handle);
#endif

    const uint16_t characteristic_uuids[] = {
        ORG_BLUETOOTH_CHARACTERISTIC_OTS_FEATURE                , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_NAME                , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_TYPE                , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_SIZE                , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_FIRST_CREATED       , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LAST_MODIFIED       , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ID                  ,
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_PROPERTIES          , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT, 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT  , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER         , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER         , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER         , 
        ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED             , 
    };

#ifdef ENABLE_TESTING_SUPPORT
    const char * characteristic_uuid_names[]= {
        "ots feature                ", 
        "object name                ", 
        "object type                ", 
        "object size                ", 
        "object first created       ", 
        "object last modified       ", 
        "object id                  ", 
        "object properties          ", 
        "object action control point", 
        "object list control point  ", 
        "object list filter 1       ", 
        "object list filter 2       ", 
        "object list filter 3       ", 
        "object changed             ", 
    };
#endif

    ots_oacp_features = oacp_features;
    ots_olcp_features = olcp_features;
    ots_server_operations = operations;

    ots_connections_num = storage_connections_num;
    memset(storage_connections, 0, sizeof(ots_server_connection_t) * ots_connections_num);
    uint16_t i;
    for (i = 0; i < ots_connections_num; i++){
        ots_server_connection_t * connection = &storage_connections[i];
        ots_server_reset_connection(connection);
        connection->initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;
        btstack_linked_list_add(&ots_connections, (btstack_linked_item_t *) connection);
    }

    uint16_t ots_services_end_handle = end_handle;
    uint16_t ots_services_start_handle = start_handle;

    for (i = 0; i < OTS_CHARACTERISTICS_NUM; i++){
        ots_characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(ots_services_start_handle, ots_services_end_handle, characteristic_uuids[i]);
        ots_characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(ots_services_start_handle, ots_services_end_handle, characteristic_uuids[i]);

        if (ots_characteristics[i].client_configuration_handle != 0){
            ots_services_start_handle = ots_characteristics[i].client_configuration_handle + 1;
        } else {
            ots_services_start_handle = ots_characteristics[i].value_handle + 1;
        }

#ifdef ENABLE_TESTING_SUPPORT
        printf("    %s      0x%02x\n", characteristic_uuid_names[i], ots_characteristics[i].value_handle);
        if (ots_characteristics[i].client_configuration_handle != 0){
            printf("    %s CCC  0x%02x\n", characteristic_uuid_names[i], ots_characteristics[i].client_configuration_handle);
        }
#endif
    }

    // register service with ATT Server
    object_transfer_service_server.start_handle   = start_handle;
    object_transfer_service_server.end_handle     = end_handle;
    object_transfer_service_server.read_callback  = &ots_server_read_callback;
    object_transfer_service_server.write_callback = &ots_server_write_callback;
    object_transfer_service_server.packet_handler = ots_server_packet_handler;
    att_server_register_service_handler(&object_transfer_service_server);

    return ERROR_CODE_SUCCESS;
}

void object_transfer_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    ots_server_event_callback = packet_handler;
}

uint8_t object_transfer_service_server_update_current_object_filter(hci_con_handle_t con_handle, uint8_t filter_index, ots_filter_t * filter){
    UNUSED(con_handle);
    UNUSED(filter_index);
    UNUSED(filter);

    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_delete_current_object(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    memset(connection->current_object, 0, sizeof(ots_object_t));

    connection->current_object = NULL;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_set_current_object(hci_con_handle_t con_handle, ots_object_t * object){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->current_object = object;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;

    if (object){
        connection->current_object->current_size = object->current_size;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_update_current_object_name(hci_con_handle_t con_handle, char * name){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->current_object == NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    if (connection->current_object_locked || connection->current_object_object_transfer_in_progress){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    uint16_t name_length = strlen(name);
    if (name_length > OTS_MAX_STRING_LENGHT){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    btstack_strcpy(connection->current_object->name, OTS_MAX_STRING_LENGHT, name);
    connection->change_flags = (0 << OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE) | (1 << OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED);
    ots_server_schedule_task(connection, OTS_TASK_SEND_OBJECT_CHANGED_RESPONSE);
    return ERROR_CODE_SUCCESS;
}

uint32_t object_transfer_service_server_current_object_size(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return 0;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }

    if (connection->current_object == NULL){
        return 0;
    }
    return connection->current_object->current_size;
}

uint8_t object_transfer_service_server_current_object_increase_allocated_size(hci_con_handle_t con_handle, uint32_t size){
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }

    if (connection->current_object == NULL){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    return connection->current_object->current_size = size;
}

uint32_t object_transfer_service_server_current_object_allocated_size(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return 0;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }

    if (connection->current_object == NULL){
        return 0;
    }
    return connection->current_object->allocated_size;
}

char * object_transfer_service_server_current_object_name(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return NULL;
    }

    if (connection->current_object == NULL){
        return NULL;
    }
    return connection->current_object->name;
}

uint16_t object_transfer_service_server_get_cbm_channel_remote_mtu(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return 0;
    }

    if (connection->credit_based_cid != 0){
        return connection->remote_mtu;
    }
    return 0;
}

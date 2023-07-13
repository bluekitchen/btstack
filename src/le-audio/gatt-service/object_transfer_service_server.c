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

#define BTSTACK_FILE__ "object_transfer_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <media_control_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/object_transfer_service_server.h"
#include "le-audio/le_audio_util.h"

#define OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE                   0x01
#define OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE                   0x02

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

static uint32_t ots_object_id_counter;

static att_service_handler_t    object_transfer_service_server;
static btstack_packet_handler_t ots_server_event_callback;
static const ots_operations_t * ots_server_operations;

static btstack_linked_list_t ots_connections;
static uint8_t  ots_connections_num = 0;

static btstack_linked_list_t ots_objects;
static uint16_t  ots_objects_num = 0;

static ots_characteristic_t  ots_characteristics[OTS_CHARACTERISTICS_NUM];

static uint32_t ots_oacp_features;
static uint32_t ots_olcp_features;

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

static void ots_server_reset_connection(ots_server_connection_t * connection){
    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    connection->oacp_configuration = 0;
    connection->olcp_configuration = 0;
    connection->object_changed_configuration = 0;
    connection->scheduled_tasks = 0;

    uint8_t i;
    for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
        memset(&connection->filters[i], 0, sizeof(ots_filter_t));
        connection->filters[i].type = OTS_FILTER_TYPE_NO_FILTER;
    }
}

static void ots_server_reset_connection_for_con_handle(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return;
    }
    ots_server_reset_connection(connection);
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

    return free_connection;
}

static uint16_t ots_server_get_client_configuration_handle_for_characteristic_index(ots_characteristic_index_t index){
    return ots_characteristics[index].client_configuration_handle;
}

static uint16_t ots_server_get_value_handle_for_characteristic_index(ots_characteristic_index_t index){
    return ots_characteristics[index].value_handle;
}

static void ots_server_reset_current_object_name(ots_server_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_name_changed(ots_server_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_first_created_time_changed(ots_server_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_last_modified_time_changed(ots_server_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_properties_changed(ots_server_connection_t * connection){
    // TODO
}

static bool ots_current_object_valid_for_filters(ots_server_connection_t * connection){
    // TODO
    return true;
}

static bool ots_current_object_exists(ots_server_connection_t * connection){
    return (connection->current_object != NULL);
}

static void ots_server_emit_current_object_filter_changed(ots_server_connection_t * connection, uint8_t filter_index){
    // TODO
}

static void ots_server_reset_long_write_filter(ots_server_connection_t * connection){
    connection->long_write_filter_type = OTS_FILTER_TYPE_NO_FILTER;
    connection->long_write_data_size = 0;
    memset(connection->long_write_data, 0, sizeof(connection->long_write_data));
    connection->long_write_attribute_handle = 0;
}

static bool ots_current_object_valid(ots_server_connection_t * connection){
    return ots_current_object_exists(connection) && ots_current_object_valid_for_filters(connection);
}

static void btstack_utc_store_time(btstack_utc_t * time, uint8_t * time_buffer_out, uint16_t time_buffer_out_size){
    if (time_buffer_out_size < 7){
        return;
    }
    little_endian_store_16(time_buffer_out, 0, time->year);
    time_buffer_out[2] = time->month;
    time_buffer_out[3] = time->day;
    time_buffer_out[4] = time->hours;
    time_buffer_out[5] = time->minutes;
    time_buffer_out[6] = time->seconds;
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
    uint8_t  field_data[5];
    uint16_t filters_offset = 0;
    uint16_t stored_bytes = 0;
    
    memset(buffer, 0, buffer_size);

    field_data[0] = (uint8_t)filter->type;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, filters_offset, buffer, buffer_size, buffer_offset);
    filters_offset++;

    if (filter->data_size == 0){
        return stored_bytes;
    }

    uint16_t data_size = filter->data_size;
    stored_bytes += le_audio_util_virtual_memcpy_helper(filter->data, data_size, filters_offset, buffer, buffer_size, buffer_offset);
    return stored_bytes;
}

static uint16_t ots_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    ots_server_connection_t * connection = NULL;

    // printf("ots_server_read_callback attribute handle 0x%02x\n", attribute_handle);

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
        if (connection->current_object->type_uuid16 != 0){
            return att_read_callback_handle_little_endian_16(connection->current_object->type_uuid16, offset, buffer, buffer_size); 
        } else {
            att_read_callback_handle_blob((const uint8_t *)connection->current_object->type_uuid128, sizeof(connection->current_object->type_uuid128), offset, buffer, buffer_size);
        }
        return 0;
    } 

    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_SIZE_INDEX)){
        uint8_t object_size[8];
        little_endian_store_32(object_size, 0, connection->current_size);
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
        return att_read_callback_handle_blob((const uint8_t *)connection->current_object->luid, OTS_OBJECT_ID_LEN, offset, buffer, buffer_size);
    
    } 
    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_PROPERTIES_INDEX)){
        uint8_t properties[4];
        little_endian_store_32(properties, 0, connection->current_object->properties);
        return att_read_callback_handle_blob(properties, sizeof(properties), offset, buffer, buffer_size);
    } 

    uint8_t i;
    for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
        if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i)){
            return ots_server_store_filter_list(&connection->filters[i], i, buffer, buffer_size, offset);
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

        uint8_t value[3];
        value[0] = OACP_OPCODE_RESPONSE_CODE; 
        value[1] = (uint8_t)connection->oacp_opcode;
        value[2] = (uint8_t)connection->oacp_result_code;
        
        // allow next transaction
        connection->oacp_opcode = OACP_OPCODE_READY;
        
        uint16_t attribute_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX);
        printf("OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE 0x%02x\n", attribute_handle);

        att_server_indicate(connection->con_handle, attribute_handle, value, sizeof(value));

    } else if ((connection->scheduled_tasks & OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE) != 0){
        connection->scheduled_tasks &= ~OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE;

        uint8_t value[3];
        value[0] = OLCP_OPCODE_RESPONSE_CODE;
        value[1] = (uint8_t)connection->olcp_opcode;
        value[2] = (uint8_t)connection->olcp_result_code;

        // allow next transaction
        connection->olcp_opcode = OLCP_OPCODE_READY;

        uint16_t attribute_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX);
        printf("OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE 0x%02x\n", attribute_handle);

        att_server_indicate(connection->con_handle, attribute_handle, value, sizeof(value));
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
        default:
            btstack_unreachable();
            return;
    }

    if (configuration == 0){
        return;
    }

    uint16_t scheduled_tasks = connection->scheduled_tasks;
    connection->scheduled_tasks |= task;

    printf("scheduled tasks 0x%02x\n", connection->scheduled_tasks);

    if (scheduled_tasks == 0){
        connection->scheduled_tasks_callback.callback = &ots_server_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

static bool ots_server_valid_gatt_uuid_size(uint16_t uuid_size){
    return (uuid_size == 2) || (uuid_size == 16);
}

static int ots_server_handle_action_control_point_write(ots_server_connection_t * connection, uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size == 0){
        connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
        return 0;
    }

    // allow only a single transaction per time
    if (connection->oacp_opcode != OACP_OPCODE_READY) {
        connection->oacp_result_code = OACP_RESULT_CODE_OPERATION_FAILED;
        return 0;
    }

    uint8_t pos = 0;
    connection->oacp_opcode = buffer[pos++];
    if ((connection->oacp_opcode == OACP_OPCODE_READY) || (connection->oacp_opcode >= OACP_OPCODE_RFU)){
        connection->oacp_result_code = OACP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        return 0;
    } 

    if ((ots_oacp_features & (1 << connection->oacp_opcode)) == 0 ){
        connection->oacp_result_code = OACP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        return 0;
    }

    uint32_t object_size;
    uint8_t gatt_uuid_size;

    switch (connection->oacp_opcode){
        case OACP_OPCODE_CREATE:
            if ((buffer_size - pos) < 6){
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                return 0;
            }

            gatt_uuid_size = buffer_size - pos - 4;
            if (!ots_server_valid_gatt_uuid_size(gatt_uuid_size)) {
                connection->oacp_result_code = OACP_RESULT_CODE_INVALID_PARAMETER;
                return 0;
            }
            object_size = little_endian_read_32(buffer, pos);
            pos += 4;

            connection->oacp_result_code = ots_server_operations->create(connection->con_handle, object_size, gatt_uuid_size, &buffer[pos]);
            break;

        case OACP_OPCODE_DELETE:
            // TODO
            break;
        case OACP_OPCODE_CALCULATE_CHECKSUM:
            // TODO
            
            break;   
        case OACP_OPCODE_EXECUTE:
            // TODO
           
            break;
        case OACP_OPCODE_READ:   
            // TODO
           
            break;     
        case OACP_OPCODE_WRITE:
            // TODO
            
            break;
        case OACP_OPCODE_ABORT:
            // TODO
            
            break;
        default:
            btstack_unreachable();
            return 0;
    }

    ots_server_schedule_task(connection, OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE);
    return 0;
}

static bool ots_server_object_passes_filters(ots_server_connection_t * connection, ots_object_t * object){
    return true;
}

static int ots_server_handle_list_control_point_write(ots_server_connection_t * connection, uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size == 0){
        connection->olcp_result_code = OLCP_RESULT_CODE_INVALID_PARAMETER;
        return 0;
    }

    connection->olcp_opcode = buffer[0];
    if ((connection->olcp_opcode == OLCP_OPCODE_READY) || (connection->olcp_opcode >= OLCP_OPCODE_RFU)){
        connection->olcp_result_code = OLCP_RESULT_CODE_OP_CODE_NOT_SUPPORTED;
        return 0;
    }
    btstack_linked_list_iterator_t it;
    switch (connection->olcp_opcode){
        case OLCP_OPCODE_FIRST:
            connection->olcp_result_code = OLCP_RESULT_CODE_NO_OBJECT;
            connection->current_object = NULL;

            btstack_linked_list_iterator_init(&it, &ots_objects);
            while (btstack_linked_list_iterator_has_next(&it)){
                ots_object_t * object = (ots_object_t*) btstack_linked_list_iterator_next(&it);
                if (object->allocated_size != 0){
                    if (!ots_server_object_passes_filters(connection, object)){
                        continue;
                    }
                    connection->current_object = object;
                    connection->olcp_result_code = OLCP_RESULT_CODE_SUCCESS;
                    break;
                }
            }
            break;

        case OLCP_OPCODE_LAST:
            // TODO
            break;
        case OLCP_OPCODE_PREVIOUS:
            // TODO
            break;   
        case OLCP_OPCODE_NEXT:
            connection->olcp_result_code = OLCP_RESULT_CODE_NO_OBJECT;
            connection->current_object = NULL;

            while (btstack_linked_list_iterator_has_next(&it)){
                ots_object_t * object = (ots_object_t*) btstack_linked_list_iterator_next(&it);
                if (object->allocated_size != 0){
                    if (!ots_server_object_passes_filters(connection, object)){
                        continue;
                    }
                    connection->current_object = object;
                    connection->olcp_result_code = OLCP_RESULT_CODE_SUCCESS;
                    break;
                }
            }
            break;
        case OLCP_OPCODE_GOTO:   
            // TODO
            break;     
        case OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS:
            // TODO
            break;
        case OLCP_OPCODE_CLEAR_MARKING:
            // TODO
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

    connection->long_write_data_size = data_size;
    if (offset != 0){
        return ATT_ERROR_SUCCESS;
    }

    connection->long_write_filter_type = (ots_filter_type_t) buffer[0];

    if (connection->long_write_filter_type >= OTS_FILTER_TYPE_RFU){
        return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
    }

    uint8_t status = ATT_ERROR_SUCCESS;
    switch (connection->long_write_filter_type){
        case OTS_FILTER_TYPE_NO_FILTER:
        case OTS_FILTER_TYPE_MARKED_OBJECTS:
            if (connection->long_write_data_size != 0){
                status =  ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            break;

        case OTS_FILTER_TYPE_CREATED_BETWEEN:
        case OTS_FILTER_TYPE_MODIFIED_BETWEEN:
            if (connection->long_write_data_size != 14){
                status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            break;

        case OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN:
        case OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN:
            if (connection->long_write_data_size != 8){
                status = ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
            }
            break;

        case OTS_FILTER_TYPE_OBJECT_TYPE:
            break;

        default:
            break;
    }
    return status;
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

    if (!ots_current_object_valid(connection)){
        return 0;
    }

    uint8_t i;
    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_CANCEL:
            ots_server_reset_long_write_filter(connection);
            break;

        case ATT_TRANSACTION_MODE_EXECUTE:
            for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
                uint16_t filter_value_handle = ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i);

                if (connection->long_write_attribute_handle == filter_value_handle){
                    memset(connection->filters[i].data, 0, sizeof(connection->filters[i].data));
                    connection->filters[i].type = connection->long_write_filter_type;
                    connection->filters[i].data_size = connection->long_write_data_size;
                    memcpy(connection->filters[i].data, connection->long_write_data, connection->long_write_data_size);
                    ots_server_emit_current_object_filter_changed(connection, i);
                    break;
                }
            }

            if (connection->long_write_attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_NAME_INDEX)) {
                memcpy(connection->current_object->name, connection->long_write_data, connection->long_write_data_size);
                ots_server_emit_current_object_name_changed(connection);
            }
            ots_server_reset_long_write_filter(connection);
            return 0;

        case ATT_TRANSACTION_MODE_ACTIVE:
            if (offset == 0){
                if (connection->long_write_attribute_handle != 0){
                    return ATT_ERROR_INSUFFICIENT_RESOURCES;
                }
                connection->long_write_attribute_handle = attribute_handle;
            } else {
                if (connection->long_write_attribute_handle != attribute_handle){
                    return ATT_ERROR_INSUFFICIENT_RESOURCES;
                }
            }
            break;

        default:
            break;
    }

    uint8_t att_status;
    if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_ACTION_CONTROL_POINT_INDEX)){
        att_status = ots_server_handle_action_control_point_write(connection, buffer, buffer_size);
        if (att_status == 0){
            ots_server_schedule_task(connection, OTS_TASK_SEND_OACP_PROCEDURE_RESPONSE);
        }
        return att_status;

    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_CONTROL_POINT_INDEX)){
        att_status = ots_server_handle_list_control_point_write(connection, buffer, buffer_size);
        if (att_status == 0){
            ots_server_schedule_task(connection, OTS_TASK_SEND_OLCP_PROCEDURE_RESPONSE);
        }
        return att_status;

    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_NAME_INDEX)){
        uint16_t total_value_len = buffer_size + offset;
        // handle long write
        switch (transaction_mode){
            case ATT_TRANSACTION_MODE_NONE:
                if (buffer_size > OTS_MAX_NAME_LENGHT){
                    return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
                }
                btstack_strcpy(&connection->current_object->name[0], OTS_MAX_NAME_LENGHT, (const char *)buffer);
                break;

            case ATT_TRANSACTION_MODE_ACTIVE:
                if (total_value_len >= (OTS_MAX_NAME_LENGHT - 1)){
                    ots_server_reset_current_object_name(connection);
                    return ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED;
                }
                if (offset == 0){
                    memset(connection->long_write_data, 0, OTS_MAX_NAME_LENGHT);
                }
                memcpy(&connection->long_write_data[offset], (const char *)buffer, buffer_size);
                break;

            default:
                break;
        }
        
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_PROPERTIES_INDEX)){
        if (buffer_size >= 4){
            connection->current_object->properties = little_endian_read_32(buffer, 0);
            ots_server_emit_current_object_properties_changed(connection);
        }
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_FIRST_CREATED_INDEX)){
        if (buffer_size >= 7){
            btstack_utc_read_time(buffer, buffer_size, &connection->current_object->first_created);
            ots_server_emit_current_object_first_created_time_changed(connection);
        }
    } else if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LAST_MODIFIED_INDEX)){
        if (buffer_size >= 7){
            btstack_utc_read_time(buffer, buffer_size, &connection->current_object->last_modified);
            ots_server_emit_current_object_last_modified_time_changed(connection);
        }
    } else {
        uint8_t status;
        for (i = 0; i < OTS_MAX_NUM_FILTERS; i++){
            if (attribute_handle == ots_server_get_value_handle_for_characteristic_index(OTS_OBJECT_LIST_FILTER1_INDEX + i)){
                status = ots_server_filter_buffer_valid_write(buffer, buffer_size, offset, connection);
                if (status != ATT_ERROR_SUCCESS){
                    ots_server_reset_long_write_filter(connection);
                    return status;
                }

                switch (transaction_mode){
                    case ATT_TRANSACTION_MODE_NONE:
                        connection->filters[i].data_size   = connection->long_write_data_size;
                        connection->filters[i].type = connection->long_write_filter_type;
                        memcpy(connection->filters[i].data, &buffer[1], connection->filters[i].data_size);
                        break;

                    case ATT_TRANSACTION_MODE_ACTIVE:
                        if (offset == 0){
                            memset(connection->long_write_data, 0, sizeof(connection->long_write_data));
                            memcpy(connection->long_write_data, &buffer[1], connection->long_write_data_size);
                        } else {
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

static void ots_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            ots_server_reset_connection_for_con_handle(hci_event_disconnection_complete_get_connection_handle(packet));
            break;
        default:
            break;
    }
}

uint8_t object_transfer_service_server_init(uint32_t oacp_features, uint32_t olcp_features, 
    uint8_t storage_clients_num, ots_server_connection_t * storage_clients, 
    uint16_t storage_objects_num, ots_object_t * storage_objects,
    const ots_operations_t * operations){
    
    btstack_assert(storage_clients_num != 0);
    btstack_assert(storage_clients != NULL);
    btstack_assert(storage_objects_num != 0);
    btstack_assert(storage_objects != NULL);
    btstack_assert(operations != NULL);
    
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    bool service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER, &start_handle, &end_handle);
    
    if (!service_found){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    log_info("Found OTS service 0x%02x-0x%02x", start_handle, end_handle);

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

    ots_connections_num = storage_clients_num;
    memset(storage_clients, 0, sizeof(ots_server_connection_t) * ots_connections_num);
    uint16_t i;
    for (i = 0; i < ots_connections_num; i++){
        ots_server_reset_connection(&storage_clients[i]);
        btstack_linked_list_add(&ots_connections, (btstack_linked_item_t *) &storage_clients[i]);
    }

    ots_objects_num = storage_objects_num;
    memset(storage_objects, 0, sizeof(ots_object_t) * ots_objects_num);
    for (i = 0; i < ots_objects_num; i++){
        btstack_linked_list_add(&ots_objects, (btstack_linked_item_t *) &storage_objects[i]);
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

static void object_transfer_service_server_get_next_object_id(ots_object_id_t * object_id_out){
    ots_object_id_counter++;
    if (ots_object_id_counter < 0x0100) {
        ots_object_id_counter = 0x0100;
    }
    memset((uint8_t *)object_id_out, 0, OTS_OBJECT_ID_LEN);
    little_endian_store_32((uint8_t *)object_id_out, 2, ots_object_id_counter);
}

static ots_object_t * ots_server_find_free_object(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_objects);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_object_t * object = (ots_object_t*) btstack_linked_list_iterator_next(&it);
        if (object->allocated_size == 0){
            return object;
        } 
    }
    return NULL;
}

static ots_object_t * ots_server_find_object_for_luid(ots_object_id_t * luid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_objects);
    while (btstack_linked_list_iterator_has_next(&it)){
        ots_object_t * object = (ots_object_t*) btstack_linked_list_iterator_next(&it);
        if (memcmp(object->luid, luid, sizeof(ots_object_id_t)) == 0){
            return object;
        } 
    }
    return NULL;
}

uint8_t object_transfer_service_server_create_object_with_type_uuid16(ots_object_id_t * object_id, char * name, uint32_t properties, uint16_t type_uuid16,
    uint32_t allocated_size, btstack_utc_t * first_created, btstack_utc_t * last_modified){

    ots_object_t * object = ots_server_find_object_for_luid(object_id);
    if (object != NULL){
        return AVRCP_BROWSING_ERROR_CODE_INVALID_PARAMETER;
    }

    object = ots_server_find_free_object();
    if (object == NULL){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    
    if (strlen(name) > OTS_MAX_NAME_LENGHT){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    memcpy(&object->luid, object_id, sizeof(ots_object_id_t));
   
    object->properties = properties;
    btstack_strcpy(object->name, OTS_MAX_NAME_LENGHT, name);
    object->type_uuid16 = type_uuid16;
    object->allocated_size = allocated_size;
    memcpy(&object->first_created, first_created, sizeof(btstack_utc_t));
    memcpy(&object->last_modified, first_created, sizeof(btstack_utc_t));
    btstack_linked_list_add(&ots_objects, (btstack_linked_item_t *) object);
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_set_current_object(hci_con_handle_t con_handle, ots_object_id_t * luid){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    ots_object_t * object = ots_server_find_object_for_luid(luid);
    if (object == NULL){
        return AVRCP_BROWSING_ERROR_CODE_INVALID_PARAMETER;
    }

    connection->current_object = object;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;    
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_update_current_object_name(hci_con_handle_t con_handle, ots_object_id_t * luid, char * name){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    ots_object_t * object = ots_server_find_object_for_luid(luid);
    if (object == NULL){
        return AVRCP_BROWSING_ERROR_CODE_INVALID_PARAMETER;
    }
    uint16_t name_length = strlen(name);
    if (name_length > OTS_MAX_NAME_LENGHT){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    btstack_strcpy(object->name, OTS_MAX_NAME_LENGHT, name);
    connection->current_object = object;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_update_current_object_filter(hci_con_handle_t con_handle, uint8_t filter_index, ots_filter_t * filter){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    ots_server_connection_t * connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (filter_index >= OTS_MAX_NUM_FILTERS){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if (filter->type >= OTS_FILTER_TYPE_RFU){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    if (filter->data_size > OTS_MAX_NAME_LENGHT){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    connection->filters[filter_index].type = filter->type;
    connection->filters[filter_index].data_size = filter->data_size;

    if (filter->data_size > 0){
        memset(connection->filters[filter_index].data, 0, sizeof(connection->filters[filter_index].data));
        memcpy(connection->filters[filter_index].data, filter->data, filter->data_size);
    }
     
    return ERROR_CODE_SUCCESS;
}

uint8_t object_transfer_service_server_reset_current_object(hci_con_handle_t con_handle){
    ots_server_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    connection->current_object = NULL;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    
    return ERROR_CODE_SUCCESS;
}

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
    OTS_OBJECT_LIST_FILTER_INDEX, 
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

static btstack_linked_list_t ots_connections;
static uint8_t  ots_connections_num = 0;

static ots_characteristic_t  ots_characteristics[OTS_CHARACTERISTICS_NUM];

static uint32_t ots_oacp_features;
static uint32_t ots_olcp_features;

static object_transfer_service_connection_t * ots_server_find_connection_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        object_transfer_service_connection_t * connection = (object_transfer_service_connection_t*) btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static object_transfer_service_connection_t * ots_server_find_or_add_connection_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    object_transfer_service_connection_t * free_connection = NULL;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &ots_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        object_transfer_service_connection_t * connection = (object_transfer_service_connection_t*) btstack_linked_list_iterator_next(&it);
        
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

static uint16_t ots_server_get_client_configuration_handle(ots_characteristic_index_t index){
    return ots_characteristics[index].client_configuration_handle;
}

static uint16_t ots_server_get_client_value_handle(ots_characteristic_index_t index){
    return ots_characteristics[index].value_handle;
}

static void ots_server_emit_current_object_name_changed(object_transfer_service_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_first_created_time_changed(object_transfer_service_connection_t * connection){
    // TODO
}

static void ots_server_emit_current_object_last_modified_time_changed(object_transfer_service_connection_t * connection){
    // TODO
}


static bool ots_current_object_valid_for_filters(object_transfer_service_connection_t * connection){
    // TODO
    return true;
}

static bool ots_current_object_exists(object_transfer_service_connection_t * connection){
    // TODO
    return true;
}

static bool ots_current_object_valid(object_transfer_service_connection_t * connection){
    return ots_current_object_exists(connection) && ots_current_object_valid_for_filters(connection);
}

static void btstack_utc_store_time(btstack_utc_t * time, uint8_t * time_buffer_out){
    little_endian_store_16(time_buffer_out, 0, time->year);
    time_buffer_out[2] = time->month;
    time_buffer_out[3] = time->day;
    time_buffer_out[4] = time->hours;
    time_buffer_out[5] = time->minutes;
    time_buffer_out[6] = time->seconds;
}

static void btstack_utc_read_time(uint8_t * time_buffer, btstack_utc_t * time_out){
    time_out->year    = little_endian_read_16(time_buffer, 0);
    time_out->month   = time_buffer[2];
    time_out->day     = time_buffer[3];
    time_out->hours   = time_buffer[4];
    time_out->minutes = time_buffer[5];
    time_out->seconds = time_buffer[6];
}

static uint16_t ots_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    object_transfer_service_connection_t * connection = NULL;

    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OTS_FEATURE)){
        ots_server_find_or_add_connection_for_con_handle(con_handle);
        
        uint8_t ots_features[8];
        little_endian_store_32(ots_features, 0, ots_oacp_features);
        little_endian_store_32(ots_features, 4, ots_olcp_features);
        return att_read_callback_handle_blob(ots_features, sizeof(ots_features), offset, buffer, buffer_size); 
    }

    // handle indication
    if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection == NULL){
            return 0;
        }
        return att_read_callback_handle_little_endian_16(connection->oacp_configuration, offset, buffer, buffer_size); 
    }
    
    if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection == NULL){
            return 0;
        }
        return att_read_callback_handle_little_endian_16(connection->olcp_configuration, offset, buffer, buffer_size); 
    }

    if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED)){
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

    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_NAME)){
        if (buffer == NULL){
            // get len and check if we have up to date value
            if ((offset != 0) && !ots_current_object_valid(connection)){
                return (uint16_t)ATT_READ_ERROR_CODE_OFFSET + (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
            }
        } else {
            // actual read (after everything was validated)
            if ((offset == 0) && !ots_current_object_valid(connection)){
                return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
            }
        }
        return att_read_callback_handle_blob((const uint8_t *)connection->current_object.name, strlen(connection->current_object.name), offset, buffer, buffer_size);
    }

    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_TYPE)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        if (connection->current_object.type_uuid16 != 0){
            return att_read_callback_handle_little_endian_16(connection->current_object.type_uuid16, offset, buffer, buffer_size); 
        } else {
            att_read_callback_handle_blob((const uint8_t *)connection->current_object.type_uuid128, sizeof(connection->current_object.type_uuid128), offset, buffer, buffer_size);
        }
        return 0;
    } 
    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_SIZE)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        uint8_t object_size[8];
        little_endian_store_32(object_size, 0, connection->current_size);
        little_endian_store_32(object_size, 4, connection->current_object.allocated_size);
        return att_read_callback_handle_blob(object_size, sizeof(object_size), offset, buffer, buffer_size); 
    }

    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_FIRST_CREATED)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }

        uint8_t time_buffer[7];
        btstack_utc_store_time(&connection->current_object.first_created, &time_buffer[0]);
        return att_read_callback_handle_blob(time_buffer, sizeof(time_buffer), offset, buffer, buffer_size);
    } 
    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LAST_MODIFIED)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        uint8_t time_buffer[7];
        btstack_utc_store_time(&connection->current_object.last_modified, &time_buffer[0]);
       
        return att_read_callback_handle_blob(time_buffer, sizeof(time_buffer), offset, buffer, buffer_size);
    } 
    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ID)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        // TODO
        return 0;
    } 
    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_PROPERTIES)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        // TODO
        return 0;
    } 
    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER)){
        if (!ots_current_object_valid(connection)){
            return (uint16_t)ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED;
        }
        // TODO
        return 0;
    } 

    return 0;
}

static int ots_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    object_transfer_service_connection_t * connection = NULL;

    if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection != NULL){
            connection->oacp_configuration = little_endian_read_16(buffer, 0);
        }
        return 0;
    
    } else if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT)){
        connection = ots_server_find_or_add_connection_for_con_handle(con_handle);
        if (connection != NULL){
            connection->olcp_configuration = little_endian_read_16(buffer, 0);
        }
        return 0;

    } else if (attribute_handle == ots_server_get_client_configuration_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED)){
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

    if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT)){
        // TODO
    
    } else if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT)){
        // TODO
    
    } else if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER)){
        // TODO
    
    } else if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_NAME)){
        btstack_strcpy(connection->current_object.name, sizeof(connection->current_object.name), (const char *)buffer);
        ots_server_emit_current_object_name_changed(connection);
    
    } else if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_FIRST_CREATED)){
        btstack_utc_read_time(buffer, &connection->current_object.first_created);
        ots_server_emit_current_object_first_created_time_changed(connection);
    
    } else if (attribute_handle == ots_server_get_client_value_handle(ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LAST_MODIFIED)){
        btstack_utc_read_time(buffer, &connection->current_object.last_modified);
        ots_server_emit_current_object_last_modified_time_changed(connection);
    } 
    
    return 0;
}

static void ots_server_reset_connection_for_con_handle(hci_con_handle_t con_handle){
    object_transfer_service_connection_t * connection = ots_server_find_connection_for_con_handle(con_handle);
    if (connection == NULL){
        return;
    }
    connection->con_handle = HCI_CON_HANDLE_INVALID;
    connection->current_object_locked = false;
    connection->current_object_object_transfer_in_progress = false;
    connection->oacp_configuration = 0;
    connection->olcp_configuration = 0;
    connection->object_changed_configuration = 0;
    connection->scheduled_tasks = 0;
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

uint8_t object_transfer_service_server_init(uint32_t oacp_features, uint32_t olcp_features, uint8_t const clients_num, object_transfer_service_connection_t * clients){
    btstack_assert(clients_num != 0);
    
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
        "object list filter         ", 
        "object changed             ", 
    };
#endif

    ots_oacp_features = oacp_features;
    ots_olcp_features = olcp_features;

    ots_connections_num = clients_num;
    memset(clients, 0, sizeof(object_transfer_service_connection_t) * ots_connections_num);
    
    uint16_t i;
    for (i = 0; i < ots_connections_num; i++){
        clients[i].con_handle = HCI_CON_HANDLE_INVALID;
        btstack_linked_list_add(&ots_connections, (btstack_linked_item_t *) &clients[i]);
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

void object_transfer_service_server_get_next_object_id(ots_object_id_t * object_id_out){
    ots_object_id_counter++;
    if (ots_object_id_counter < 0x0100) {
        ots_object_id_counter = 0x0100;
    }
    memset((uint8_t *)object_id_out, 0, OTS_OBJECT_ID_LEN);
    little_endian_store_32((uint8_t *)object_id_out, 2, ots_object_id_counter);
}

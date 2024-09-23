/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "battery_service_v1_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add `#import <battery_service.gatt>` to your .gatt file
 */

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#define BS_CONNECTIONS_MAX_NUM 10
#include "ble/gatt-service/battery_service_v1_server.h"

#define BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED                          0x0001

static btstack_linked_list_t battery_services;


static battery_service_v1_server_connection_t * battery_service_server_connection_for_con_handle(battery_service_v1_t * service, hci_con_handle_t con_handle){
    if (service == NULL){
        return NULL;
    }

    uint8_t i;
    for (i = 0; i < service->connections_max_num; i++){
        if (service->connections[i].con_handle == con_handle){
            return &service->connections[i];
        }
    }
    return NULL;
}

static battery_service_v1_server_connection_t * battery_service_server_add_connection_for_con_handle(battery_service_v1_t * service, hci_con_handle_t con_handle){
    if (service == NULL){
        return NULL;
    }

    uint8_t i;
    for (i = 0; i < service->connections_max_num; i++){
        if (service->connections[i].con_handle == HCI_CON_HANDLE_INVALID){
            service->connections[i].con_handle = con_handle;
            service->connections[i].service = service;
            return &service->connections[i];
        }
    }
    return NULL;
}


static battery_service_v1_t * battery_service_service_for_attribute_handle(uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * item = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        if (attribute_handle < item->start_handle) continue;
        if (attribute_handle > item->end_handle)   continue;
        return item;
    }
    return NULL;
}


static battery_service_v1_t * battery_service_service_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * service = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        uint8_t i;
        for (i = 0; i < service->connections_max_num; i++){
            if (service->connections[i].con_handle == con_handle){
                return service;
            }
        }
    }
    return NULL;
}


static uint16_t battery_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);

    battery_service_v1_t * service = battery_service_service_for_attribute_handle(attribute_handle);
    if (service == NULL){
        return 0;
    }

    battery_service_v1_server_connection_t * connection = battery_service_server_connection_for_con_handle(service, con_handle);
    if (connection == NULL){
        connection = battery_service_server_add_connection_for_con_handle(service, con_handle);
        if (connection == NULL){
            return 0;
        }
    }

    if (attribute_handle == service->battery_value_handle){
        return att_read_callback_handle_byte(service->battery_value, offset, buffer, buffer_size);
    }
    if (attribute_handle == service->battery_value_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(connection->battery_value_client_configuration, offset, buffer, buffer_size);
    }
    return 0;
}

static int battery_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(offset);
    UNUSED(buffer_size);

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE){
        return 0;
    }

    battery_service_v1_t * service = battery_service_service_for_attribute_handle(attribute_handle);
    if (service == NULL){
        return 0;
    }

    battery_service_v1_server_connection_t * connection = battery_service_server_connection_for_con_handle(service, con_handle);
    if (connection == NULL){
        connection = battery_service_server_add_connection_for_con_handle(service, con_handle);
        if (connection == NULL){
            return 0;
        }
    }

    if (attribute_handle == service->battery_value_client_configuration_handle){
        connection->battery_value_client_configuration = little_endian_read_16(buffer, 0);
    }
    return 0;
}

static void battery_service_can_send_now(void * context){
    battery_service_v1_server_connection_t * connection = (battery_service_v1_server_connection_t *) context;
    if (connection == NULL){
        return;
    }
    battery_service_v1_t * service = connection->service;
    if (service == NULL){
        return;
    }

    if ( (connection->scheduled_tasks & BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED;
        att_server_notify(connection->con_handle, service->battery_value_handle, &service->battery_value, 1);
    }

    if (connection->scheduled_tasks > 0u){
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

void battery_service_v1_server_init(void){

}

void battery_service_v1_server_register(battery_service_v1_t *service, battery_service_v1_server_connection_t *connections, uint8_t connection_max_num){
    btstack_assert(service != NULL);
    btstack_assert(connections != NULL);
    btstack_assert(connection_max_num > 0u);

    // get service handle range
    uint16_t end_handle   = 0xffff;
    uint16_t start_handle = 0;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * service = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        if (service->end_handle > start_handle){
            start_handle = service->end_handle + 1;
        }
    }

    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    service->start_handle = start_handle;
    service->end_handle   = end_handle;

    // get characteristic value handle and client configuration handle
    service->battery_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
    service->battery_value_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);

    memset(connections, 0, sizeof(battery_service_v1_server_connection_t) * connection_max_num);
    uint8_t i;
    for (i = 0; i < connection_max_num; i++){
        connections[i].con_handle = HCI_CON_HANDLE_INVALID;
    }
    service->connections_max_num = connection_max_num;
    service->connections = connections;
    

    service->service_handler.read_callback  = &battery_service_read_callback;
    service->service_handler.write_callback = &battery_service_write_callback;
    att_server_register_service_handler(&service->service_handler);

    log_info("Found Battery Service 0x%02x-0x%02x", start_handle, end_handle);

    btstack_linked_list_add(&battery_services, (btstack_linked_item_t *) service);
}


static void battery_service_set_callback(battery_service_v1_server_connection_t * connection, uint8_t task){
    if (connection->con_handle == HCI_CON_HANDLE_INVALID){
        connection->scheduled_tasks = 0;
        return;
    }

    uint8_t scheduled_tasks = connection->scheduled_tasks;
    connection->scheduled_tasks |= task;
    if (scheduled_tasks == 0){
        connection->scheduled_tasks_callback.callback = &battery_service_can_send_now;
        connection->scheduled_tasks_callback.context  = (void*) connection;
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}


void battery_service_v1_server_set_battery_value(battery_service_v1_t * service, uint8_t value){
    btstack_assert(service != NULL);
    if (service->battery_value == value){
        return;
    }

    service->battery_value = value;

    uint8_t i;
    for (i = 0; i < service->connections_max_num; i++){
        battery_service_v1_server_connection_t * connection = &service->connections[i];
        if (connection->battery_value_client_configuration != 0){
            battery_service_set_callback(connection, BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED);
        }
    }
}

void battery_service_v1_server_deregister(battery_service_v1_t *service){
    btstack_linked_list_iterator_remove((btstack_linked_item_t * )service);
    // TODO cansel can send now 
}

void battery_service_v1_server_deinit(void){
    // deregister listeners
    // empty list
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * service = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        battery_service_v1_server_deregister(service);       
    }
}

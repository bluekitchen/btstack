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

#include "ble/gatt-service/battery_service_v1_server.h"

#define BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED                          0x0001

// list of uuids
static const uint16_t bas_uuid16s[BAS_CHARACTERISTIC_INDEX_NUM] = {
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATUS,
        ORG_BLUETOOTH_CHARACTERISTIC_ESTIMATED_SERVICE_DATE,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_CRITCAL_STATUS,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_ENERGY_STATUS,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_TIME_STATUS,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_STATUS,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_INFORMATION,
        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_INFORMATION,
        ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING,
        ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING,
        ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING,
};

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

    uint8_t index;
    uint8_t event[18];
    uint8_t pos = 0;

    for (index = 0; index < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; index++){
        if (attribute_handle != service->characteristics[index].value_handle){
            continue;
        }


        switch ((bas_characteristic_index_t) index){
            case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL:             
                return att_read_callback_handle_byte(service->battery_value, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS: 
                event[pos++] = service->battery_level.flags;
                little_endian_store_16(event, pos, service->battery_level.power_state_flags);
                pos += 2;
                if ((service->battery_level.flags & BATTERY_LEVEL_STATUS_BITMASK_IDENTIFIER_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->battery_level.identifier);
                    pos += 2;
                }
                if ((service->battery_level.flags & BATTERY_LEVEL_STATUS_BITMASK_BATTERY_LEVEL_PRESENT) > 0u){
                    event[pos++] = service->battery_level.battery_level;
                }
                if ((service->battery_level.flags & BATTERY_LEVEL_STATUS_BITMASK_ADDITIONAL_STATUS_PRESENT) > 0u){
                    event[pos++] = service->battery_level.additional_status_flags;
                }
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE:   
                little_endian_store_24(event, pos, service->estimated_service_date_days);
                pos += 3;
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS: 
                return att_read_callback_handle_byte(service->battery_critcal_status_flags, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS:  
                event[pos++] = service->energy_status.flags;   
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_EXTERNAL_SOURCE_POWER_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.external_source_power_medfloat16);
                    pos += 2;
                }
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_PRESENT_VOLTAGE_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.present_voltage_medfloat16);
                    pos += 2;
                }
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.available_energy_medfloat16);
                    pos += 2;
                }
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_BATTERY_CAPACITY_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.available_battery_capacity_medfloat16);
                    pos += 2;
                }
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_CHARGE_RATE_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.charge_rate_medfloat16);
                    pos += 2;
                }
                if ((service->energy_status.flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_AT_LAST_CHARGE_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->energy_status.available_energy_at_last_charge_medfloat16);
                    pos += 2;
                }   
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS:  
                event[pos++] = service->time_status.flags;  
                little_endian_store_24(event, pos, service->time_status.time_until_discharged_minutes);
                pos += 3;
                if ((service->time_status.flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_DISCHARGED_ON_STANDBY_PRESENT) > 0u){
                    little_endian_store_24(event, pos, service->time_status.time_until_discharged_on_standby_minutes);
                    pos += 3;
                } 
                if ((service->time_status.flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_RECHARGED_PRESENT) > 0u){
                    little_endian_store_24(event, pos, service->time_status.time_until_recharged_minutes);
                    pos += 3;
                }  
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS:  
                event[pos++] = service->health_status.flags; 
                if ((service->health_status.flags & BATTERY_HEALTH_STATUS_BITMASK_HEALTH_SUMMARY_PRESENT) > 0u){
                    event[pos++] = service->health_status.summary;
                }
                if ((service->health_status.flags & BATTERY_HEALTH_STATUS_BITMASK_CYCLE_COUNT_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->health_status.cycle_count);
                    pos += 2;
                }
                if ((service->health_status.flags & BATTERY_HEALTH_STATUS_BITMASK_CURRENT_TEMPERATURE_PRESENT) > 0u){
                    event[pos++] = service->health_status.current_temperature_degree_celsius;
                }
                if ((service->health_status.flags & BATTERY_HEALTH_STATUS_BITMASK_DEEP_DISCHARGE_COUNT_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->health_status.deep_discharge_count);
                    pos += 2;
                }
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_INFORMATION:
                event[pos++] = service->health_information.flags;
                if ((service->health_information.flags & BATTERY_HEALTH_INFORMATION_BITMASK_CYCLE_COUNT_DESIGNED_LIFETIME_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->health_information.cycle_count_designed_lifetime);
                    pos += 2;
                }
                if ((service->health_information.flags & BATTERY_HEALTH_INFORMATION_BITMASK_DESIGNED_OPERATING_TEMPERATURE_PRESENT) > 0u){
                    event[pos++] = service->health_information.min_designed_operating_temperature_degree_celsius;
                }
                if ((service->health_information.flags & BATTERY_HEALTH_INFORMATION_BITMASK_DESIGNED_OPERATING_TEMPERATURE_PRESENT) > 0u){
                    event[pos++] = service->health_information.max_designed_operating_temperature_degree_celsius;
                }
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION:
                little_endian_store_16(event, pos, service->information.flags);
                pos += 2;
                event[pos++] = service->information.features;
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_MANUFACTURE_DATE_PRESENT) > 0u){
                    little_endian_store_24(event, pos, service->information.manufacture_date_days);
                    pos += 3;
                } 
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_EXPIRATION_DATE_PRESENT) > 0u){
                    little_endian_store_24(event, pos, service->information.expiration_date_days);
                    pos += 3;
                }  
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_DESIGNED_CAPACITY_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->information.designed_capacity_kWh_medfloat16);
                    pos += 2;
                }
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_LOW_ENERGY_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->information.low_energy_kWh_medfloat16);
                    pos += 2;
                }
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_CRITICAL_ENERGY_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->information.critical_energy_kWh_medfloat16);
                    pos += 2;
                }
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_CHEMISTRY_PRESENT) > 0u){
                    event[pos++] = service->information.chemistry;
                }
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_NOMINAL_VOLTAGE_PRESENT) > 0u){
                    little_endian_store_16(event, pos, service->information.nominal_voltage_medfloat16);
                    pos += 2;
                }
                if ((service->information.flags & BATTERY_INFORMATION_BITMASK_AGGREGATION_GROUP_PRESENT) > 0u){
                    event[pos++] = service->information.aggregation_group;
                }
                return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_MANUFACTURER_NAME_STRING: 
                return att_read_callback_handle_blob(service->manufacturer_name, service->manufacturer_name_len, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_MODEL_NUMBER_STRING:       
                return att_read_callback_handle_blob(service->model_number, service->model_number_len, offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_SERIAL_NUMBER_STRING:      
                return att_read_callback_handle_blob(service->serial_number, service->serial_number_len, offset, buffer, buffer_size);
    
            default:
                return 0;
        }
    }

    for (index = 0; index < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; index++){
        if (attribute_handle != service->characteristics[index].client_configuration_handle){
            continue;
        }
        return att_read_callback_handle_little_endian_16(connection->configurations[index], offset, buffer, buffer_size);
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

    uint8_t index;
    for (index = 0; index < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; index++){
        if (attribute_handle != service->characteristics[index].client_configuration_handle){
            continue;
        }
        connection->configurations[index] = little_endian_read_16(buffer, 0);
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
        att_server_notify(connection->con_handle, service->characteristics[BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL].value_handle, &service->battery_value, 1);
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

    uint8_t i;
    for (i = 0; i < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; i++){
        // get characteristic value handle and client configuration handle
        service->characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, bas_uuid16s[i]);
        service->characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, bas_uuid16s[i]);
    }
    
    memset(connections, 0, sizeof(battery_service_v1_server_connection_t) * connection_max_num);
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
        if (connection->configurations[BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL] != 0){
            battery_service_set_callback(connection, BATTERY_SERVICE_TASK_BATTERY_VALUE_CHANGED);
        }
    }
}

void battery_service_v1_server_deregister(battery_service_v1_t *service){
    btstack_linked_list_remove(&battery_services, (btstack_linked_item_t * )service);
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

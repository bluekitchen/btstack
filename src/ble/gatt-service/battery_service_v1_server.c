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
#include <stdio.h>
#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/battery_service_v1_server.h"
#include "hci_event_builder.h"
#include "bluetooth_data_types.h"

#define BAS_TASK_BATTERY_LEVEL_CHANGED                                  0x0001
#define BAS_TASK_BATTERY_LEVEL_STATUS_CHANGED                           0x0002
#define BAS_TASK_ESTIMATED_SERVICE_DATE_CHANGED                         0x0004
#define BAS_TASK_BATTERY_CRITCAL_STATUS_CHANGED                         0x0008
#define BAS_TASK_BATTERY_ENERGY_STATUS_CHANGED                          0x0010
#define BAS_TASK_BATTERY_TIME_STATUS_CHANGED                            0x0020
#define BAS_TASK_BATTERY_HEALTH_STATUS_CHANGED                          0x0040
#define BAS_TASK_BATTERY_HEALTH_INFORMATION_CHANGED                     0x0080
#define BAS_TASK_BATTERY_INFORMATION_CHANGED                            0x0100
#define BAS_TASK_MANUFACTURER_NAME_STRING_CHANGED                       0x0200
#define BAS_TASK_MODEL_NUMBER_STRING_CHANGED                            0x0400
#define BAS_TASK_SERIAL_NUMBER_STRING_CHANGED                           0x0800

// list of uuids
static const uint16_t bas_uuid16s[BAS_CHARACTERISTIC_INDEX_NUM] = {
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_ESTIMATED_SERVICE_DATE,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_CRITICAL_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_ENERGY_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_TIME_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_STATUS,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_INFORMATION,
    ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_INFORMATION,
    ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING,
    ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING,
    ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING,
};

static const char * bas_uuid16_name[BAS_CHARACTERISTIC_INDEX_NUM] = {
    "BATTERY_LEVEL",
    "BATTERY_LEVEL_STATUS",
    "ESTIMATED_SERVICE_DATE",
    "BATTERY_CRITCAL_STATUS",
    "BATTERY_ENERGY_STATUS",
    "BATTERY_TIME_STATUS",
    "BATTERY_HEALTH_STATUS",
    "BATTERY_HEALTH_INFORMATION",
    "BATTERY_INFORMATION",
    "MANUFACTURER_NAME_STRING",
    "MODEL_NUMBER_STRING",
    "SERIAL_NUMBER_STRING",
};

static uint16_t bas_service_id_counter = 0;
static btstack_linked_list_t battery_services;
static btstack_packet_handler_t battery_service_app_callback;

#define MEDFLOAT16_POSITIVE_INFINITY            0x07FE
#define MEDFLOAT16_NOT_A_NUMBER                 0x07FF
#define MEDFLOAT16_NOT_AT_THIS_RESOLUTION       0x0800
#define MEDFLOAT16_RFU                          0x0801 
#define MEDFLOAT16_NEGATIVE_INFINITY            0x0802

static bool bas_server_medfloat16_is_real_number(uint16_t value_medfloat16){
    switch (value_medfloat16){
        case MEDFLOAT16_POSITIVE_INFINITY:
        case MEDFLOAT16_NOT_A_NUMBER:
        case MEDFLOAT16_NOT_AT_THIS_RESOLUTION:
        case MEDFLOAT16_RFU: 
        case MEDFLOAT16_NEGATIVE_INFINITY:
            return false;
        default:
            return true;
    }
}

static uint16_t bas_server_get_task_for_characteristic_index(bas_characteristic_index_t index){
    switch (index){
        case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL:
            return BAS_TASK_BATTERY_LEVEL_CHANGED;             
        case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS:
            return BAS_TASK_BATTERY_LEVEL_STATUS_CHANGED;      
        case BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE:
            return BAS_TASK_ESTIMATED_SERVICE_DATE_CHANGED;    
        case BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS:
            return BAS_TASK_BATTERY_CRITCAL_STATUS_CHANGED;    
        case BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS:
            return BAS_TASK_BATTERY_ENERGY_STATUS_CHANGED;     
        case BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS:
            return BAS_TASK_BATTERY_TIME_STATUS_CHANGED;       
        case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS:
            return BAS_TASK_BATTERY_HEALTH_STATUS_CHANGED;     
        case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_INFORMATION:
            return BAS_TASK_BATTERY_HEALTH_INFORMATION_CHANGED;
        case BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION:
            return BAS_TASK_BATTERY_INFORMATION_CHANGED;       
        case BAS_CHARACTERISTIC_INDEX_MANUFACTURER_NAME_STRING:
            return BAS_TASK_MANUFACTURER_NAME_STRING_CHANGED;  
        case BAS_CHARACTERISTIC_INDEX_MODEL_NUMBER_STRING:
            return BAS_TASK_MODEL_NUMBER_STRING_CHANGED;       
        case BAS_CHARACTERISTIC_INDEX_SERIAL_NUMBER_STRING:
            return BAS_TASK_SERIAL_NUMBER_STRING_CHANGED;
        default:
            btstack_assert(false);
            return 0;
    }   
}

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
        if (attribute_handle < item->service_handler.start_handle) continue;
        if (attribute_handle > item->service_handler.end_handle)   continue;
        return item;
    }
    return NULL;
}


static uint8_t bas_serialize_characteristic(battery_service_v1_t * service, bas_characteristic_index_t index, uint8_t * buffer, uint8_t buffer_size){
    UNUSED(buffer_size);
    uint8_t pos = 0;
    switch ((bas_characteristic_index_t) index){
        case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL:
            buffer[pos++] = service->battery_level;
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS:
            if (service->level_status == NULL){
                return 0;
            }
            buffer[pos++] = service->level_status->flags;
            little_endian_store_16(buffer, pos, service->level_status->power_state_flags);
            pos += 2;
            if ((service->level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_IDENTIFIER_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->level_status->identifier);
                pos += 2;
            }
            if ((service->level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_BATTERY_LEVEL_PRESENT) > 0u){
                buffer[pos++] = service->level_status->battery_level;
            }
            if ((service->level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_ADDITIONAL_STATUS_PRESENT) > 0u){
                buffer[pos++] = service->level_status->additional_status_flags;
            }
            break;

        case BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE:
            little_endian_store_24(buffer, pos, service->estimated_service_date_days);
            pos += 3;
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS:
            buffer[pos++] = service->critical_status_flags;
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS:
            if (service->energy_status == NULL){
                return 0;
            }
            buffer[pos++] = service->energy_status->flags;
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_EXTERNAL_SOURCE_POWER_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->external_source_power_medfloat16);
                pos += 2;
            }
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_PRESENT_VOLTAGE_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->present_voltage_medfloat16);
                pos += 2;
            }
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->available_energy_medfloat16);
                pos += 2;
            }
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_BATTERY_CAPACITY_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->available_battery_capacity_medfloat16);
                pos += 2;
            }
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_CHARGE_RATE_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->charge_rate_medfloat16);
                pos += 2;
            }
            if ((service->energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_AT_LAST_CHARGE_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->energy_status->available_energy_at_last_charge_medfloat16);
                pos += 2;
            }
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS:
            if (service->time_status == NULL){
                return 0;
            }
            buffer[pos++] = service->time_status->flags;
            little_endian_store_24(buffer, pos, service->time_status->time_until_discharged_minutes);
            pos += 3;
            if ((service->time_status->flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_DISCHARGED_ON_STANDBY_PRESENT) > 0u){
                little_endian_store_24(buffer, pos, service->time_status->time_until_discharged_on_standby_minutes);
                pos += 3;
            }
            if ((service->time_status->flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_RECHARGED_PRESENT) > 0u){
                little_endian_store_24(buffer, pos, service->time_status->time_until_recharged_minutes);
                pos += 3;
            }
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS:
            if (service->health_status == NULL){
                return 0;
            }
            buffer[pos++] = service->health_status->flags;
            if ((service->health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_HEALTH_SUMMARY_PRESENT) > 0u){
                buffer[pos++] = service->health_status->summary;
            }
            if ((service->health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_CYCLE_COUNT_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->health_status->cycle_count);
                pos += 2;
            }
            if ((service->health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_CURRENT_TEMPERATURE_PRESENT) > 0u){
                buffer[pos++] = service->health_status->current_temperature_degree_celsius;
            }
            if ((service->health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_DEEP_DISCHARGE_COUNT_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->health_status->deep_discharge_count);
                pos += 2;
            }
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_INFORMATION:
            if (service->health_information == NULL){
                return 0;
            }
            buffer[pos++] = service->health_information->flags;
            if ((service->health_information->flags & BATTERY_HEALTH_INFORMATION_BITMASK_CYCLE_COUNT_DESIGNED_LIFETIME_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->health_information->cycle_count_designed_lifetime);
                pos += 2;
            }
            if ((service->health_information->flags & BATTERY_HEALTH_INFORMATION_BITMASK_DESIGNED_OPERATING_TEMPERATURE_PRESENT) > 0u){
                buffer[pos++] = service->health_information->min_designed_operating_temperature_degree_celsius;
            }
            if ((service->health_information->flags & BATTERY_HEALTH_INFORMATION_BITMASK_DESIGNED_OPERATING_TEMPERATURE_PRESENT) > 0u){
                buffer[pos++] = service->health_information->max_designed_operating_temperature_degree_celsius;
            }
            break;

        case BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION:
            if (service->information == NULL){
                return 0;
            }
            little_endian_store_16(buffer, pos, service->information->flags);
            pos += 2;
            buffer[pos++] = service->information->features;
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_MANUFACTURE_DATE_PRESENT) > 0u){
                little_endian_store_24(buffer, pos, service->information->manufacture_date_days);
                pos += 3;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_EXPIRATION_DATE_PRESENT) > 0u){
                little_endian_store_24(buffer, pos, service->information->expiration_date_days);
                pos += 3;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_DESIGNED_CAPACITY_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->information->designed_capacity_kWh_medfloat16);
                pos += 2;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_LOW_ENERGY_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->information->low_energy_kWh_medfloat16);
                pos += 2;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_CRITICAL_ENERGY_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->information->critical_energy_kWh_medfloat16);
                pos += 2;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_CHEMISTRY_PRESENT) > 0u){
                buffer[pos++] = service->information->chemistry;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_NOMINAL_VOLTAGE_PRESENT) > 0u){
                little_endian_store_16(buffer, pos, service->information->nominal_voltage_medfloat16);
                pos += 2;
            }
            if ((service->information->flags & BATTERY_INFORMATION_BITMASK_AGGREGATION_GROUP_PRESENT) > 0u){
                buffer[pos++] = service->information->aggregation_group;
            }
            break;
        default:
            break;
    }
    return pos;
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
    uint8_t event[19];
    uint8_t pos = 0;

    for (index = 0; index < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; index++){
        if (attribute_handle != service->characteristics[index].value_handle){
            continue;
        }

        switch ((bas_characteristic_index_t) index){
            case BAS_CHARACTERISTIC_INDEX_MANUFACTURER_NAME_STRING:
                if (service->manufacturer_name == NULL){
                    return 0;
                }
                return att_read_callback_handle_blob((uint8_t *)service->manufacturer_name, strlen(service->manufacturer_name), offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_MODEL_NUMBER_STRING:
                if (service->model_number == NULL){
                    return 0;
                }
                return att_read_callback_handle_blob((uint8_t *)service->model_number, strlen(service->model_number), offset, buffer, buffer_size);
    
            case BAS_CHARACTERISTIC_INDEX_SERIAL_NUMBER_STRING:
                if (service->serial_number == NULL){
                    return 0;
                }
                return att_read_callback_handle_blob((uint8_t *)service->serial_number, strlen(service->serial_number), offset, buffer, buffer_size);
    
            default:
                pos = bas_serialize_characteristic(service, index, event, sizeof(event));
                if (pos == 1u){
                    return att_read_callback_handle_byte(event[0], offset, buffer, buffer_size);
                }
                if (pos > 1u){
                    return att_read_callback_handle_blob(event, pos, offset, buffer, buffer_size);
                }
                return 0;
        }
    }

    if (attribute_handle == service->battery_level_status_broadcast_configuration_handle){
        return att_read_callback_handle_little_endian_16(service->battery_level_status_broadcast_configuration, offset, buffer, buffer_size);
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

    if (attribute_handle == service->battery_level_status_broadcast_configuration_handle){
        uint8_t new_value = little_endian_read_16(buffer, 0);
        bool broadcast_old = (service->battery_level_status_broadcast_configuration & 1) != 0;
        bool broadcast_new = (new_value & 1) != 0;
        service->battery_level_status_broadcast_configuration = new_value;
        if (broadcast_old != broadcast_new){
            // emit broadcast start/stop based on value of broadcast_new
            uint8_t event[5];
            hci_event_builder_context_t context;
            uint8_t subevent_type = broadcast_new ? GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL_BROADCAST_START : GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL_BROADCAST_STOP;
            hci_event_builder_init(&context, event, sizeof(buffer), HCI_EVENT_GATTSERVICE_META, subevent_type);
            hci_event_builder_add_16(&context, service->service_id);
            if (battery_service_app_callback != NULL){
                (*battery_service_app_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            }
        }
        return 0;
    }

    uint8_t index;
    for (index = 0; index < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; index++){
        if (attribute_handle != service->characteristics[index].client_configuration_handle){
            continue;
        }
        connection->configurations[index] = little_endian_read_16(buffer, 0);
        return 0;
    }
    return 0;
}


static bool bas_characteristic_notify_configured(battery_service_v1_server_connection_t * connection, bas_characteristic_index_t index){
    return (connection->configurations[index] & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) != 0u;
}
static bool bas_characteristic_indicate_configured(battery_service_v1_server_connection_t * connection, bas_characteristic_index_t index){
    return (connection->configurations[index] & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION) != 0u;
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

    // if battery is removed, no indications or notification should be sent
//    if ( (service->level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_BATTERY_LEVEL_PRESENT) == 0u){
//        return;
//    }

    bas_characteristic_index_t index;
    uint8_t event[19];
    uint8_t pos = 0;
    bool task_valid = true;


    if ((connection->scheduled_tasks & BAS_TASK_BATTERY_LEVEL_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_LEVEL_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL;
    
    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_LEVEL_STATUS_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_LEVEL_STATUS_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS;
    
    } else if ((connection->scheduled_tasks & BAS_TASK_ESTIMATED_SERVICE_DATE_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_ESTIMATED_SERVICE_DATE_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE;
    
    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_CRITCAL_STATUS_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_CRITCAL_STATUS_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS;
    
    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_ENERGY_STATUS_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_ENERGY_STATUS_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS;
    
    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_TIME_STATUS_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_TIME_STATUS_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS;

    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_HEALTH_STATUS_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_HEALTH_STATUS_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS;

    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_HEALTH_INFORMATION_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_HEALTH_INFORMATION_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS;

    } else if ((connection->scheduled_tasks & BAS_TASK_BATTERY_INFORMATION_CHANGED) > 0u){
        connection->scheduled_tasks &= ~BAS_TASK_BATTERY_INFORMATION_CHANGED;
        index = BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION;

    } else {
        // TODO  BAS_TASK_MANUFACTURER_NAME_STRING_CHANGED 
        // TODO  BAS_TASK_MODEL_NUMBER_STRING_CHANGED
        // TODO  BAS_TASK_SERIAL_NUMBER_STRING_CHANGED

        task_valid = false;
    }

    if (task_valid){
        pos = bas_serialize_characteristic(service, index, event, sizeof(event));
            
        if (bas_characteristic_notify_configured(connection, index)){
            att_server_notify(connection->con_handle, service->characteristics[index].value_handle, event, pos);
        } else if (bas_characteristic_indicate_configured(connection, index)){
            att_server_notify(connection->con_handle, service->characteristics[index].value_handle, event, pos);
        }
    }

    if (connection->scheduled_tasks > 0u){
        att_server_register_can_send_now_callback(&connection->scheduled_tasks_callback, connection->con_handle);
    }
}

void battery_service_v1_server_init(void){

}

void battery_service_v1_server_register(battery_service_v1_t *service, battery_service_v1_server_connection_t *connections, uint8_t connection_max_num, uint16_t * out_service_id){
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
        if (service->service_handler.end_handle > start_handle){
            start_handle = service->service_handler.end_handle + 1;
        }
    }

    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    // get next service id
    bas_service_id_counter = btstack_next_cid_ignoring_zero(bas_service_id_counter);
    service->service_id = bas_service_id_counter;
    if (out_service_id != NULL) {
        *out_service_id = bas_service_id_counter;
    }

    service->service_handler.start_handle = start_handle;
    service->service_handler.end_handle = end_handle;
    printf("start handle 0x%04X, end0x%04X\n", service->service_handler.start_handle , service->service_handler.end_handle);
    uint8_t i;
    for (i = 0; i < (uint8_t) BAS_CHARACTERISTIC_INDEX_NUM; i++){
        // get characteristic value handle and client configuration handle
        service->characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, bas_uuid16s[i]);
        service->characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, bas_uuid16s[i]);
        printf("%30s: 0x%04X, CCC 0x%04X\n", bas_uuid16_name[i], service->characteristics[i].value_handle , service->characteristics[i].client_configuration_handle );
    }
    
    service->battery_level_status_broadcast_configuration_handle = gatt_server_get_server_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATUS);

    memset(connections, 0, sizeof(battery_service_v1_server_connection_t) * connection_max_num);
    for (i = 0; i < connection_max_num; i++){
        connections[i].con_handle = HCI_CON_HANDLE_INVALID;
    }
    service->connections_max_num = connection_max_num;
    service->connections = connections;

    service->service_handler.start_handle = start_handle;
    service->service_handler.end_handle = end_handle;
    service->service_handler.read_callback  = &battery_service_read_callback;
    service->service_handler.write_callback = &battery_service_write_callback;
    att_server_register_service_handler(&service->service_handler);

    log_info("Found Battery Service 0x%02x-0x%02x", start_handle, end_handle);

    btstack_linked_list_add_tail(&battery_services, (btstack_linked_item_t *) service);
}


static void bas_server_set_callback_for_connection(battery_service_v1_server_connection_t * connection, bas_characteristic_index_t index, uint8_t task){
    UNUSED(index);
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

static void bas_server_set_callback(battery_service_v1_t * service, bas_characteristic_index_t index){
    // if battery is removed, no indications or notification should be sent

    if (service->level_status == NULL){
        return;
    }
    if ((index != BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS) && (service->level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_BATTERY_LEVEL_PRESENT) == 0u){
        return;
    }
    uint8_t task = bas_server_get_task_for_characteristic_index(index);
    uint8_t i;
    for (i = 0; i < service->connections_max_num; i++){
        if (service->connections[i].configurations[index] > 0u){
            bas_server_set_callback_for_connection(&service->connections[i], index, task);
        }
    }
}

uint8_t battery_service_v1_server_set_battery_level(battery_service_v1_t * service, uint8_t battery_level){
    btstack_assert(service != NULL);
    if (battery_level > 100){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if (service->battery_level != battery_level){
        service->battery_level = battery_level;
        bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_battery_level_status(battery_service_v1_t * service, const battery_level_status_t * battery_level_status){
    btstack_assert(service != NULL);
    btstack_assert(battery_level_status != NULL);
    
    if ((battery_level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if ((battery_level_status->flags & BATTERY_LEVEL_STATUS_BITMASK_ADDITIONAL_STATUS_PRESENT) > 0u){
       if ((battery_level_status->additional_status_flags & BATTERY_LEVEL_ADDITIONAL_STATUS_BITMASK_RFU) != 0u){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    
    service->level_status = battery_level_status;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_estimated_service_date_days(battery_service_v1_t * service, uint32_t estimated_service_date_days){
    btstack_assert(service != NULL);
    
    if (estimated_service_date_days > 0xFFFFFF){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    service->estimated_service_date_days = estimated_service_date_days;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_critcal_status_flags(battery_service_v1_t * service, uint8_t critcal_status_flags){
    btstack_assert(service != NULL);
    
    if ((critcal_status_flags & BATTERY_CRITCAL_STATUS_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    service->critical_status_flags = critcal_status_flags;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_energy_status(battery_service_v1_t * service, const battery_energy_status_t * energy_status){
    btstack_assert(service != NULL);
    btstack_assert(energy_status != NULL);
    
    if ((energy_status->flags & BATTERY_ENERGY_STATUS_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    service->energy_status = energy_status;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_time_status(battery_service_v1_t * service, const battery_time_status_t * time_status){
    btstack_assert(service != NULL);
    btstack_assert(time_status != NULL);
    
    if ((time_status->flags & BATTERY_TIME_STATUS_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if (time_status->time_until_discharged_minutes > 0xFFFFFF){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if ((time_status->flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_DISCHARGED_ON_STANDBY_PRESENT) > 0u){
        if (time_status->time_until_discharged_on_standby_minutes > 0xFFFFFF){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((time_status->flags & BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_RECHARGED_PRESENT) > 0u){
        if (time_status->time_until_recharged_minutes > 0xFFFFFF){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }

    service->time_status = time_status;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_health_status(battery_service_v1_t * service, const battery_health_status_t * health_status){
    btstack_assert(service != NULL);
    btstack_assert(health_status != NULL);
    
    if ((health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if ((health_status->flags & BATTERY_HEALTH_STATUS_BITMASK_HEALTH_SUMMARY_PRESENT) > 0u){
        if (health_status->summary > 100){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }

    service->health_status = health_status;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_health_information(battery_service_v1_t * service, const battery_health_information_t * health_information){
    btstack_assert(service != NULL);
    btstack_assert(health_information != NULL);
    
    if ((health_information->flags & BATTERY_HEALTH_INFORMATION_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    service->health_information = health_information;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_INFORMATION);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_information(battery_service_v1_t * service, const battery_information_t * information){
    btstack_assert(service != NULL);
    btstack_assert(information != NULL);
    
    if ((information->flags & BATTERY_INFORMATION_BITMASK_RFU) != 0u){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_MANUFACTURE_DATE_PRESENT) > 0u){
        if (information->manufacture_date_days > 0xFFFFFF){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_EXPIRATION_DATE_PRESENT) > 0u){
        if (information->expiration_date_days > 0xFFFFFF){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_CHEMISTRY_PRESENT) > 0u){
        if (information->chemistry >= BATTERY_CHEMISTRY_RFU_START && information->chemistry <= BATTERY_CHEMISTRY_RFU_END){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_AGGREGATION_GROUP_PRESENT) > 0u){
        if (information->aggregation_group == 0xFF){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_DESIGNED_CAPACITY_PRESENT) > 0u){
        if (!bas_server_medfloat16_is_real_number(information->designed_capacity_kWh_medfloat16)){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_LOW_ENERGY_PRESENT) > 0u){
        if (!bas_server_medfloat16_is_real_number(information->low_energy_kWh_medfloat16)){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_CRITICAL_ENERGY_PRESENT) > 0u){
        if (!bas_server_medfloat16_is_real_number(information->critical_energy_kWh_medfloat16)){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    if ((information->flags & BATTERY_INFORMATION_BITMASK_NOMINAL_VOLTAGE_PRESENT) > 0u){
        if (!bas_server_medfloat16_is_real_number(information->nominal_voltage_medfloat16)){
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        }
    }
    service->information = information;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_manufacturer_name(battery_service_v1_t * service, const char * manufacturer_name){
    btstack_assert(service != NULL);
    btstack_assert(manufacturer_name != NULL);
    
    service->manufacturer_name = manufacturer_name;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_MANUFACTURER_NAME_STRING);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_model_number(battery_service_v1_t * service, const char * model_number){
    btstack_assert(service != NULL);
    btstack_assert(model_number != NULL);
    
    service->model_number = model_number;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_MODEL_NUMBER_STRING);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_v1_server_set_serial_number(battery_service_v1_t * service, const char * serial_number){
    btstack_assert(service != NULL);
    btstack_assert(serial_number != NULL);
    
    service->serial_number = serial_number;
    bas_server_set_callback(service, BAS_CHARACTERISTIC_INDEX_SERIAL_NUMBER_STRING);
    return ERROR_CODE_SUCCESS;
}

void battery_service_v1_server_set_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    battery_service_app_callback = callback;
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

uint16_t battery_service_v1_server_get_broadcast_advertisement(uint16_t adv_interval, uint8_t * adv_buffer, uint16_t adv_size){

    if (adv_size < 7) return 0u;

    uint16_t pos = 0;
    // adv flags
    adv_buffer[pos++] = 2;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_FLAGS;
    adv_buffer[pos++] = 0x04;

    // adv interval
    adv_buffer[pos++] = 3;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL;
    little_endian_store_16(adv_buffer, pos, adv_interval);
    pos += 2;

    uint16_t pos_without_data = pos;

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * service = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        if ((service->battery_level_status_broadcast_configuration & 1) != 0) {
            uint8_t value_buffer[10];
            uint16_t value_len = bas_serialize_characteristic(service, BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS, value_buffer, sizeof(value_buffer));
            if ((pos + 4 + value_len) <= adv_size) {
                adv_buffer[pos++] = 3 + value_len;
                adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID;
                little_endian_store_16(adv_buffer, pos, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
                pos += 2;
                memcpy(&adv_buffer[pos], value_buffer, value_len);
                pos += value_len;
            } else {
                break;
            }
        }
    }

    // indicate nothing to broadcast
    if (pos == pos_without_data) {
        pos = 0;
    }

    return pos;
}

uint16_t battery_service_v1_server_get_broadcast_advertisement_single(battery_service_v1_t * service_single, uint16_t adv_interval, uint8_t * adv_buffer, uint16_t adv_size){

    if (adv_size < 7) return 0u;

    uint16_t pos = 0;
    // adv flags
    adv_buffer[pos++] = 2;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_FLAGS;
    adv_buffer[pos++] = 0x04;

    // adv interval
    adv_buffer[pos++] = 3;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL;
    little_endian_store_16(adv_buffer, pos, adv_interval);
    pos += 2;

    uint16_t pos_without_data = pos;

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &battery_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_v1_t * service = (battery_service_v1_t*) btstack_linked_list_iterator_next(&it);
        if (service == service_single) {
            if ((service->battery_level_status_broadcast_configuration & 1) != 0) {
                uint8_t value_buffer[10];
                uint16_t value_len = bas_serialize_characteristic(service,
                                                                  BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS,
                                                                  value_buffer, sizeof(value_buffer));
                if ((pos + 4 + value_len) <= adv_size) {
                    adv_buffer[pos++] = 3 + value_len;
                    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID;
                    little_endian_store_16(adv_buffer, pos, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
                    pos += 2;
                    memcpy(&adv_buffer[pos], value_buffer, value_len);
                    pos += value_len;
                } else {
                    break;
                }
            }
        }
    }

    // indicate nothing to broadcast
    if (pos == pos_without_data) {
        pos = 0;
    }

    return pos;
}


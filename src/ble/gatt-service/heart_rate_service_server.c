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

#define BTSTACK_FILE__ "heart_rate_service_server.c"


#include "bluetooth.h"
#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/heart_rate_service_server.h"

#define HEART_RATE_RESET_ENERGY_EXPENDED 0x01
#define HEART_RATE_CONTROL_POINT_NOT_SUPPORTED 0x80

typedef enum {
    HEART_RATE_SERVICE_VALUE_FORMAT = 0,
    HEART_RATE_SERVICE_SENSOR_CONTACT_STATUS,
    HEART_RATE_SERVICE_ENERGY_EXPENDED_STATUS = 3,
    HEART_RATE_SERVICE_RR_INTERVAL
} heart_rate_service_flag_bit_t;

typedef struct {
    hci_con_handle_t con_handle;

    // characteristic: Heart Rate Mesurement 
    uint16_t measurement_value_handle;
    uint16_t measurement_bpm;
    uint8_t  energy_expended_supported;
    uint16_t energy_expended_kJ; // kilo Joules
    int rr_interval_count;
    int rr_offset;
    uint16_t * rr_intervals;
    heart_rate_service_sensor_contact_status_t sensor_contact;

    // characteristic descriptor: Client Characteristic Configuration
    uint16_t measurement_client_configuration_descriptor_handle;
    uint16_t measurement_client_configuration_descriptor_notify;
    btstack_context_callback_registration_t measurement_callback;

    // characteristic: Body Sensor Location 
    uint16_t sensor_location_value_handle;
    heart_rate_service_body_sensor_location_t sensor_location;
    
    // characteristic: Heart Rate Control Point
    uint16_t control_point_value_handle;
} heart_rate_t;

static att_service_handler_t heart_rate_service;
static heart_rate_t heart_rate;

static uint16_t heart_rate_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    if (attribute_handle == heart_rate.measurement_client_configuration_descriptor_handle){
        if (buffer && (buffer_size >= 2u)){
            little_endian_store_16(buffer, 0, heart_rate.measurement_client_configuration_descriptor_notify);
        } 
        return 2;
    }
    
    if (attribute_handle == heart_rate.sensor_location_value_handle){
        if (buffer && (buffer_size >= 1u)){
            buffer[0] = heart_rate.sensor_location;
        }
        return 1;
    }
    return 0;
}

static int heart_rate_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);
    
    if (attribute_handle == heart_rate.measurement_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        heart_rate.measurement_client_configuration_descriptor_notify = little_endian_read_16(buffer, 0);
        heart_rate.con_handle = con_handle;
        return 0;
    }
    
    if (attribute_handle == heart_rate.control_point_value_handle){
        uint16_t cmd = little_endian_read_16(buffer, 0);
        switch (cmd){
            case HEART_RATE_RESET_ENERGY_EXPENDED:
                heart_rate.energy_expended_kJ = 0;
                heart_rate.con_handle = con_handle;
                break;
            default:
                return HEART_RATE_CONTROL_POINT_NOT_SUPPORTED;
        }
        return 0;
    }
    return 0;
}


void heart_rate_service_server_init(heart_rate_service_body_sensor_location_t location, int energy_expended_supported){
    heart_rate_t * instance = &heart_rate;

    instance->sensor_location = location;
    instance->energy_expended_supported = energy_expended_supported;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_HEART_RATE, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

    // get Heart Rate Mesurement characteristic value handle and client configuration handle
    instance->measurement_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT);
    instance->measurement_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT);
    // get Body Sensor Location characteristic value handle and client configuration handle
    instance->sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
    // get Hear Rate Control Point characteristic value handle and client configuration handle
    instance->control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_CONTROL_POINT);
    
    log_info("Measurement     value handle 0x%02x", instance->measurement_value_handle);
    log_info("Client Config   value handle 0x%02x", instance->measurement_client_configuration_descriptor_handle);
    log_info("Sensor location value handle 0x%02x", instance->sensor_location_value_handle);
    log_info("Control Point   value handle 0x%02x", instance->control_point_value_handle);
    // register service with ATT Server
    heart_rate_service.start_handle   = start_handle;
    heart_rate_service.end_handle     = end_handle;
    heart_rate_service.read_callback  = &heart_rate_service_read_callback;
    heart_rate_service.write_callback = &heart_rate_service_write_callback;
    
    att_server_register_service_handler(&heart_rate_service);
}


static void heart_rate_service_can_send_now(void * context){
    heart_rate_t * instance = (heart_rate_t *) context;
    uint8_t flags = (1 << HEART_RATE_SERVICE_VALUE_FORMAT);
    flags |= (instance->sensor_contact << HEART_RATE_SERVICE_SENSOR_CONTACT_STATUS);
    if (instance->energy_expended_supported){
        flags |= (1u << HEART_RATE_SERVICE_ENERGY_EXPENDED_STATUS);
    }
    if (instance->rr_interval_count){
        flags |= (1u << HEART_RATE_SERVICE_RR_INTERVAL);
    }

    uint8_t value[100];
    int pos = 0;

    value[pos++] = flags;
    little_endian_store_16(value, pos, instance->measurement_bpm);
    pos += 2;
    if (instance->energy_expended_supported){
        little_endian_store_16(value, pos, instance->energy_expended_kJ);
        pos += 2;
    }

    uint16_t bytes_left = btstack_min(sizeof(value), att_server_get_mtu(instance->con_handle) - 3u - pos);

    while ((bytes_left > 2u) && instance->rr_interval_count){
        little_endian_store_16(value, pos, instance->rr_intervals[0]);
        pos +=2;
        bytes_left -= 2u;
        instance->rr_intervals++;
        instance->rr_interval_count--;
    }

    att_server_notify(instance->con_handle, instance->measurement_value_handle, &value[0], pos);

    if (instance->rr_interval_count){
        instance->measurement_callback.callback = &heart_rate_service_can_send_now;
        instance->measurement_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->measurement_callback, instance->con_handle);
    } 
}

void heart_rate_service_add_energy_expended(uint16_t energy_expended_kJ){
    heart_rate_t * instance = &heart_rate;
    // limit energy expended to 0xffff
    if (instance->energy_expended_kJ <= (0xffffu - energy_expended_kJ)){
        instance->energy_expended_kJ += energy_expended_kJ;
    } else {
        instance->energy_expended_kJ = 0xffff;
    }
}

void heart_rate_service_server_update_heart_rate_values(uint16_t heart_rate_bpm, 
    heart_rate_service_sensor_contact_status_t sensor_contact, int rr_interval_count, uint16_t * rr_intervals){
    heart_rate_t * instance = &heart_rate;

    instance->measurement_bpm = heart_rate_bpm;
    instance->sensor_contact = sensor_contact;
    instance->rr_interval_count = rr_interval_count;
    instance->rr_intervals = rr_intervals;
    instance->rr_offset = 0;

    if (instance->measurement_client_configuration_descriptor_notify){
        instance->measurement_callback.callback = &heart_rate_service_can_send_now;
        instance->measurement_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->measurement_callback, instance->con_handle);
    }
}
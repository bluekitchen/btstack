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

#define BTSTACK_FILE__ "cycling_power_service_server.c"


#include "bluetooth.h"
#include "btstack_defines.h"
#include "bluetooth_data_types.h"
#include "btstack_event.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "l2cap.h"
#include "hci.h"

#include "ble/gatt-service/cycling_power_service_server.h"

#define CYCLING_POWER_MAX_BROACAST_MSG_SIZE         31
#define CONTROL_POINT_PROCEDURE_TIMEOUT_MS          30
#define CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED     0xFFFF

typedef enum {
    CP_MASK_BIT_PEDAL_POWER_BALANCE = 0,
    CP_MASK_BIT_ACCUMULATED_TORQUE,
    CP_MASK_BIT_WHEEL_REVOLUTION_DATA,
    CP_MASK_BIT_CRANK_REVOLUTION_DATA,
    CP_MASK_BIT_EXTREME_MAGNITUDES,
    CP_MASK_BIT_EXTREME_ANGLES,
    CP_MASK_BIT_TOP_DEAD_SPOT_ANGLE,
    CP_MASK_BIT_BOTTOM_DEAD_SPOT_ANGLE,
    CP_MASK_BIT_ACCUMULATED_ENERGY,
    CP_MASK_BIT_RESERVED
} cycling_power_mask_bit_t;

typedef enum {
    CP_OPCODE_IDLE = 0,
    CP_OPCODE_SET_CUMULATIVE_VALUE,
    CP_OPCODE_UPDATE_SENSOR_LOCATION,
    CP_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS,
    CP_OPCODE_SET_CRANK_LENGTH,
    CP_OPCODE_REQUEST_CRANK_LENGTH,
    CP_OPCODE_SET_CHAIN_LENGTH,
    CP_OPCODE_REQUEST_CHAIN_LENGTH,
    CP_OPCODE_SET_CHAIN_WEIGHT,
    CP_OPCODE_REQUEST_CHAIN_WEIGHT,
    CP_OPCODE_SET_SPAN_LENGTH,
    CP_OPCODE_REQUEST_SPAN_LENGTH,
    CP_OPCODE_START_OFFSET_COMPENSATION,
    CP_OPCODE_MASK_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT,
    CP_OPCODE_REQUEST_SAMPLING_RATE,
    CP_OPCODE_REQUEST_FACTORY_CALIBRATION_DATE,
    CP_OPCODE_START_ENHANCED_OFFSET_COMPENSATION,
    CP_OPCODE_RESPONSE_CODE = 32
} cycling_power_opcode_t;

typedef enum {
    CP_RESPONSE_VALUE_SUCCESS = 1,
    CP_RESPONSE_VALUE_OP_CODE_NOT_SUPPORTED,
    CP_RESPONSE_VALUE_INVALID_PARAMETER,
    CP_RESPONSE_VALUE_OPERATION_FAILED,
    CP_RESPONSE_VALUE_NOT_AVAILABLE,
    CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE
} cycling_power_response_value_t;

typedef enum {
    CP_CONNECTION_INTERVAL_STATUS_NONE = 0,
    CP_CONNECTION_INTERVAL_STATUS_RECEIVED,
    CP_CONNECTION_INTERVAL_STATUS_ACCEPTED,
    CP_CONNECTION_INTERVAL_STATUS_W4_L2CAP_RESPONSE,
    CP_CONNECTION_INTERVAL_STATUS_W4_UPDATE,
    CP_CONNECTION_INTERVAL_STATUS_REJECTED
} cycling_power_con_interval_status_t;

typedef struct {
    hci_con_handle_t con_handle;
    // GATT connection management
    uint16_t con_interval;
    uint16_t con_interval_min;
    uint16_t con_interval_max;
    cycling_power_con_interval_status_t  con_interval_status;

    // Cycling Power Measurement 
    uint16_t measurement_value_handle;
    int16_t  instantaneous_power_W;

    cycling_power_pedal_power_balance_reference_t  pedal_power_balance_reference; 
    uint8_t  pedal_power_balance_percentage;    // percentage, resolution 1/2, 
                                                // If the sensor provides the power balance referenced to the left pedal, 
                                                // the power balance is calculated as [LeftPower/(LeftPower + RightPower)]*100 in units of percent
    
    cycling_power_torque_source_t torque_source;
    uint16_t accumulated_torque_Nm;             // newton-meters, resolution 1/32,
                                                // The Accumulated Torque value may decrease
    // wheel revolution data:
    uint32_t cumulative_wheel_revolutions;      // CANNOT roll over
    uint16_t last_wheel_event_time_s;           // seconds, resolution 1/2048
    // crank revolution data:
    uint16_t cumulative_crank_revolutions;
    uint16_t last_crank_event_time_s;           // seconds, resolution 1/1024
    // extreme force magnitudes
    int16_t  maximum_force_magnitude_N;
    int16_t  minimum_force_magnitude_N;
    int16_t  maximum_torque_magnitude_Nm;       // newton-meters, resolution 1/32
    int16_t  minimum_torque_magnitude_Nm;       // newton-meters, resolution 1/32
    // extreme angles
    uint16_t maximum_angle_degree;                 // 12bit, degrees
    uint16_t minimum_angle_degree;                 // 12bit, degrees, concatenated with previous into 3 octets
                                                // i.e. if the Maximum Angle is 0xABC and the Minimum Angle is 0x123, the transmitted value is 0x123ABC.
    uint16_t top_dead_spot_angle_degree;
    uint16_t bottom_dead_spot_angle_degree;            // The Bottom Dead Spot Angle field represents the crank angle when the value of the Instantaneous Power value becomes negative.
    uint16_t accumulated_energy_kJ;             // kilojoules; CANNOT roll over

    // uint8_t  offset_compensation;

    // CP Measurement Notification (Client Characteristic Configuration)
    uint16_t measurement_client_configuration_descriptor_handle;
    uint16_t measurement_client_configuration_descriptor_notify;
    btstack_context_callback_registration_t measurement_notify_callback;

    // CP Measurement Broadcast (Server Characteristic Configuration)
    uint16_t measurement_server_configuration_descriptor_handle;
    uint16_t measurement_server_configuration_descriptor_broadcast;
    btstack_context_callback_registration_t measurement_broadcast_callback;

    // Cycling Power Feature
    uint16_t feature_value_handle;
    uint32_t feature_flags;                             // see cycling_power_feature_flag_t
    uint16_t masked_measurement_flags;
    uint16_t default_measurement_flags;

    // Sensor Location
    uint16_t sensor_location_value_handle;
    cycling_power_sensor_location_t sensor_location;    // see cycling_power_sensor_location_t
    cycling_power_sensor_location_t * supported_sensor_locations;
    uint16_t num_supported_sensor_locations;
    uint16_t crank_length_mm;                           // resolution 1/2 mm
    uint16_t chain_length_mm;                           // resolution 1 mm
    uint16_t chain_weight_g;                            // resolution 1 gram
    uint16_t span_length_mm;                            // resolution 1 mm
    
    gatt_date_time_t factory_calibration_date;
    
    uint8_t  sampling_rate_Hz;                          // resolution 1 Herz
    
    uint16_t  current_force_magnitude_N;
    uint16_t  current_torque_magnitude_Nm;               // newton-meters, resolution 1/32
    uint16_t manufacturer_company_id;
    uint8_t num_manufacturer_specific_data;
    uint8_t * manufacturer_specific_data;

    // Cycling Power Vector
    uint16_t vector_value_handle;
    uint16_t vector_cumulative_crank_revolutions;
    uint16_t vector_last_crank_event_time_s;                           // seconds, resolution 1/1024
    uint16_t vector_first_crank_measurement_angle_degree;
    int16_t  * vector_instantaneous_force_magnitude_N_array;           // newton
    uint16_t force_magnitude_count;
    int16_t  * vector_instantaneous_torque_magnitude_Nm_array;         // newton-meter, resolution 1/32
    uint16_t torque_magnitude_count;
    cycling_power_instantaneous_measurement_direction_t vector_instantaneous_measurement_direction;

    // CP Vector Notification (Client Characteristic Configuration)
    uint16_t vector_client_configuration_descriptor_handle;
    uint16_t vector_client_configuration_descriptor_notify;
    btstack_context_callback_registration_t vector_notify_callback;

    // CP Control Point
    uint16_t control_point_value_handle;
    // CP Control Point Indication (Client Characteristic Configuration)
    uint16_t control_point_client_configuration_descriptor_handle;
    uint16_t control_point_client_configuration_descriptor_indicate;
    btstack_context_callback_registration_t control_point_indicate_callback;

    cycling_power_opcode_t request_opcode;
    cycling_power_response_value_t response_value;

    btstack_packet_handler_t calibration_callback;
    uint8_t w4_indication_complete;
} cycling_power_t;

static att_service_handler_t cycling_power_service;
static cycling_power_t cycling_power;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;

static uint16_t cycling_power_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(offset);
    cycling_power_t * instance = &cycling_power;

    if (attribute_handle == instance->measurement_client_configuration_descriptor_handle){
        if (buffer && (buffer_size >= 2u)){
            little_endian_store_16(buffer, 0, instance->measurement_client_configuration_descriptor_notify);
        } 
        return 2;
    }

    if (attribute_handle == instance->measurement_server_configuration_descriptor_handle){
        if (buffer && (buffer_size >= 2u)){
            little_endian_store_16(buffer, 0, instance->measurement_server_configuration_descriptor_broadcast);
        } 
        return 2;
    }

    if (attribute_handle == instance->vector_client_configuration_descriptor_handle){
        if (buffer && (buffer_size >= 2u)){
            little_endian_store_16(buffer, 0, instance->vector_client_configuration_descriptor_notify);
        } 
        return 2;
    }

    if (attribute_handle == instance->control_point_client_configuration_descriptor_handle){
        if (buffer && (buffer_size >= 2u)){
            little_endian_store_16(buffer, 0, instance->control_point_client_configuration_descriptor_indicate);
        } 
        return 2;
    }

    if (attribute_handle == instance->feature_value_handle){
        if (buffer && (buffer_size >= 4u)){
            little_endian_store_32(buffer, 0, instance->feature_flags);
        } 
        return 4;
    }   
    
    if (attribute_handle == instance->sensor_location_value_handle){
        if (buffer && (buffer_size >= 1u)){
            buffer[0] = instance->sensor_location;
        } 
        return 1;
    }   
    return 0;
}

static int has_feature(cycling_power_feature_flag_t feature){
    cycling_power_t * instance = &cycling_power;
    return (instance->feature_flags & (1u << feature)) != 0u;
}

static int cycling_power_vector_instantaneous_measurement_direction(void){
    cycling_power_t * instance = &cycling_power;
    return instance->vector_instantaneous_measurement_direction;
}

static uint16_t cycling_power_service_default_measurement_flags(void){
    cycling_power_t * instance = &cycling_power;
    uint16_t measurement_flags = 0;
    uint8_t flag[] = {
        (uint8_t) has_feature(CP_FEATURE_FLAG_PEDAL_POWER_BALANCE_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_PEDAL_POWER_BALANCE_SUPPORTED) && instance->pedal_power_balance_reference,
        (uint8_t) has_feature(CP_FEATURE_FLAG_ACCUMULATED_TORQUE_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_ACCUMULATED_TORQUE_SUPPORTED) && instance->torque_source,
        (uint8_t) has_feature(CP_FEATURE_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED) && (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_FORCE),
        (uint8_t) has_feature(CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED) && (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE),
        (uint8_t) has_feature(CP_FEATURE_FLAG_EXTREME_ANGLES_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_TOP_AND_BOTTOM_DEAD_SPOT_ANGLE_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_TOP_AND_BOTTOM_DEAD_SPOT_ANGLE_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_ACCUMULATED_ENERGY_SUPPORTED),
        (uint8_t) has_feature(CP_FEATURE_FLAG_OFFSET_COMPENSATION_INDICATOR_SUPPORTED)
    };

    int i;
    for (i = CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_PRESENT; i <= CP_MEASUREMENT_FLAG_OFFSET_COMPENSATION_INDICATOR; i++){
        measurement_flags |= flag[i] << i;
    }

    return measurement_flags;
}

static uint16_t cycling_power_service_get_measurement_flags(cycling_power_t * instance){
    if (!instance) return 0;
    if (instance->masked_measurement_flags != CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED){
        return instance->masked_measurement_flags;
    } 
    if (instance->default_measurement_flags == CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED){
        instance->default_measurement_flags = cycling_power_service_default_measurement_flags();
    }
    return instance->default_measurement_flags;
}


uint16_t cycling_power_service_measurement_flags(void){
    cycling_power_t * instance = &cycling_power;
    return cycling_power_service_get_measurement_flags(instance);
}

uint8_t cycling_power_service_vector_flags(void){
    uint8_t vector_flags = 0;
    uint8_t flag[] = {
        (uint8_t )has_feature(CP_FEATURE_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED),
        (uint8_t )has_feature(CP_FEATURE_FLAG_EXTREME_ANGLES_SUPPORTED),
        (uint8_t )has_feature(CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED) && (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_FORCE),
        (uint8_t )has_feature(CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED) && (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE),
        (uint8_t )has_feature(CP_FEATURE_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION_SUPPORTED) && cycling_power_vector_instantaneous_measurement_direction()
    };

    int i;
    for (i = CP_VECTOR_FLAG_CRANK_REVOLUTION_DATA_PRESENT; i <= CP_VECTOR_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION; i++){
        vector_flags |= flag[i] << i;
    }
    return vector_flags;
}

static void cycling_power_service_vector_can_send_now(void * context){
    cycling_power_t * instance = (cycling_power_t *) context;
    if (!instance){
        log_error("cycling_power_service_measurement_can_send_now: instance is null");
        return;
    }
    uint8_t value[50];
    uint8_t vector_flags = cycling_power_service_vector_flags();
    int pos = 0;
    
    value[pos++] = vector_flags;
    int i;
    for (i = CP_VECTOR_FLAG_CRANK_REVOLUTION_DATA_PRESENT; i <= CP_VECTOR_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION; i++){
        if ((vector_flags & (1u << i)) == 0u) continue;
        switch ((cycling_power_vector_flag_t) i){
            case CP_VECTOR_FLAG_CRANK_REVOLUTION_DATA_PRESENT:
                little_endian_store_16(value, pos, instance->cumulative_crank_revolutions);
                pos += 2;
                little_endian_store_16(value, pos, instance->last_crank_event_time_s);
                pos += 2;
                break;
            case CP_VECTOR_FLAG_INSTANTANEOUS_FORCE_MAGNITUDE_ARRAY_PRESENT:{
                uint16_t att_mtu = att_server_get_mtu(instance->con_handle);
                uint16_t bytes_left = 0;
                if (att_mtu > (pos + 3u)){
                    bytes_left = btstack_min(sizeof(value), att_mtu - 3u - pos);
                }
                while ((bytes_left > 2u) && instance->force_magnitude_count){
                    little_endian_store_16(value, pos, instance->vector_instantaneous_force_magnitude_N_array[0]);
                    pos += 2;
                    bytes_left -= 2u;
                    instance->vector_instantaneous_force_magnitude_N_array++;
                    instance->force_magnitude_count--;
                }
                break;
            }
            case CP_VECTOR_FLAG_INSTANTANEOUS_TORQUE_MAGNITUDE_ARRAY_PRESENT:{
                uint16_t att_mtu = att_server_get_mtu(instance->con_handle);
                uint16_t bytes_left = 0;
                if (att_mtu > (pos + 3u)){
                    bytes_left = btstack_min(sizeof(value), att_mtu - 3u - pos);
                }

                while ((bytes_left > 2u) && instance->torque_magnitude_count){
                    little_endian_store_16(value, pos, instance->vector_instantaneous_torque_magnitude_Nm_array[0]);
                    pos += 2;
                    bytes_left -= 2u;
                    instance->vector_instantaneous_torque_magnitude_Nm_array++;
                    instance->torque_magnitude_count--;
                }
                break;
            }
            case CP_VECTOR_FLAG_FIRST_CRANK_MEASUREMENT_ANGLE_PRESENT:
                little_endian_store_16(value, pos, instance->vector_first_crank_measurement_angle_degree);
                pos += 2;
                break;
            case CP_VECTOR_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION:
                break;
            default:
                break;
        }
    }

    att_server_notify(instance->con_handle, instance->vector_value_handle, &value[0], pos); 
}

static int cycling_power_measurement_flag_value_size(cycling_power_measurement_flag_t flag){
    switch (flag){
        case CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_PRESENT:
            return 1;
        case CP_MEASUREMENT_FLAG_WHEEL_REVOLUTION_DATA_PRESENT:
            return 6;
        case CP_MEASUREMENT_FLAG_CRANK_REVOLUTION_DATA_PRESENT:
        case CP_MEASUREMENT_FLAG_EXTREME_FORCE_MAGNITUDES_PRESENT:
        case CP_MEASUREMENT_FLAG_EXTREME_TORQUE_MAGNITUDES_PRESENT:
            return 4;
        case CP_MEASUREMENT_FLAG_EXTREME_ANGLES_PRESENT:
            return 3;
        case CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_PRESENT:
        case CP_MEASUREMENT_FLAG_TOP_DEAD_SPOT_ANGLE_PRESENT:
        case CP_MEASUREMENT_FLAG_BOTTOM_DEAD_SPOT_ANGLE_PRESENT:
        case CP_MEASUREMENT_FLAG_ACCUMULATED_ENERGY_PRESENT:
            return 2;
        default:
            return 0;
    }
}

static int cycling_power_store_measurement_flag_value(cycling_power_t * instance, cycling_power_measurement_flag_t flag, uint8_t * value){
    if (!instance) return 0;

    int pos = 0;
    switch (flag){
        case CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_PRESENT:
            value[pos++] = instance->pedal_power_balance_percentage;
            break;
        case CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_PRESENT:
            little_endian_store_16(value, pos, instance->accumulated_torque_Nm);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_WHEEL_REVOLUTION_DATA_PRESENT:
            little_endian_store_32(value, pos, instance->cumulative_wheel_revolutions);
            pos += 4;
            little_endian_store_16(value, pos, instance->last_wheel_event_time_s);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_CRANK_REVOLUTION_DATA_PRESENT:
            little_endian_store_16(value, pos, instance->cumulative_crank_revolutions);
            pos += 2;
            little_endian_store_16(value, pos, instance->last_crank_event_time_s);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_EXTREME_FORCE_MAGNITUDES_PRESENT:
            little_endian_store_16(value, pos, (uint16_t)instance->maximum_force_magnitude_N);
            pos += 2;
            little_endian_store_16(value, pos, (uint16_t)instance->minimum_force_magnitude_N);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_EXTREME_TORQUE_MAGNITUDES_PRESENT:
            little_endian_store_16(value, pos, (uint16_t)instance->maximum_torque_magnitude_Nm);
            pos += 2;
            little_endian_store_16(value, pos, (uint16_t)instance->minimum_torque_magnitude_Nm);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_EXTREME_ANGLES_PRESENT:
            little_endian_store_24(value, pos, (instance->maximum_angle_degree << 12) | instance->minimum_angle_degree);
            pos += 3;
            break;
        case CP_MEASUREMENT_FLAG_TOP_DEAD_SPOT_ANGLE_PRESENT:
            little_endian_store_16(value, pos, (uint16_t)instance->top_dead_spot_angle_degree);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_BOTTOM_DEAD_SPOT_ANGLE_PRESENT:
            little_endian_store_16(value, pos, (uint16_t)instance->bottom_dead_spot_angle_degree);
            pos += 2;
            break;
        case CP_MEASUREMENT_FLAG_ACCUMULATED_ENERGY_PRESENT:
            little_endian_store_16(value, pos, (uint16_t)instance->accumulated_energy_kJ);
            pos += 2;
            break;
        default:
            break;
    }
    return pos;
}


static int cycling_power_store_measurement(cycling_power_t * instance, uint8_t * value, uint16_t max_value_size){
    if (max_value_size < 4u) return 0u;
    if (!instance) return 0;

    uint16_t measurement_flags = cycling_power_service_get_measurement_flags(instance);
    int pos = 0;
    little_endian_store_16(value, 0, measurement_flags);
    pos += 2;
    little_endian_store_16(value, 2, instance->instantaneous_power_W);
    pos += 2;
    int flag_index;
    uint16_t bytes_left = max_value_size - pos;
    for (flag_index = 0; flag_index < CP_MEASUREMENT_FLAG_RESERVED; flag_index++){
        if ((measurement_flags & (1u << flag_index)) == 0u) continue;
        cycling_power_measurement_flag_t flag = (cycling_power_measurement_flag_t) flag_index;
        uint16_t value_size = cycling_power_measurement_flag_value_size(flag);
        if (value_size > bytes_left ) return pos;
        cycling_power_store_measurement_flag_value(instance, flag, &value[pos]);
        pos += value_size;
        bytes_left -= value_size;
    }
    return pos;
}

int cycling_power_get_measurement_adv(uint16_t adv_interval, uint8_t * adv_buffer, uint16_t adv_size){
    if (adv_size < 12u) return 0u;
    cycling_power_t * instance =  &cycling_power;
    int pos = 0;
    // adv flags
    adv_buffer[pos++] = 2;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_FLAGS;
    adv_buffer[pos++] = 0x4;

    // adv interval
    adv_buffer[pos++] = 3;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL;
    little_endian_store_16(adv_buffer, pos, adv_interval);
    pos += 2;
    //
    int value_len = cycling_power_store_measurement(instance, &adv_buffer[pos + 4], CYCLING_POWER_MAX_BROACAST_MSG_SIZE - (pos + 4));
    adv_buffer[pos++] = 3 + value_len;
    adv_buffer[pos++] = BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID;
    little_endian_store_16(adv_buffer, pos, ORG_BLUETOOTH_SERVICE_CYCLING_POWER);
    pos += 2;
    // value data already in place cycling_power_get_measurement
    pos += value_len;
    // set ADV_NONCONN_IND
    return pos;
}

static void cycling_power_service_broadcast_can_send_now(void * context){
    cycling_power_t * instance = (cycling_power_t *) context;
    if (!instance){
        log_error("cycling_power_service_broadcast_can_send_now: instance is null");
        return;
    }
    uint8_t value[CYCLING_POWER_MAX_BROACAST_MSG_SIZE];
    int pos = cycling_power_store_measurement(instance, &value[0], sizeof(value));
    att_server_notify(instance->con_handle, instance->measurement_value_handle, &value[0], pos); 
}

static void cycling_power_service_measurement_can_send_now(void * context){
    cycling_power_t * instance = (cycling_power_t *) context;
    if (!instance){
        log_error("cycling_power_service_measurement_can_send_now: instance is null");
        return;
    }
    uint8_t value[40];
    int pos = cycling_power_store_measurement(instance, &value[0], sizeof(value));
    att_server_notify(instance->con_handle, instance->measurement_value_handle, &value[0], pos); 
}

static void cycling_power_service_response_can_send_now(void * context){
    cycling_power_t * instance = (cycling_power_t *) context;
    if (!instance){
        log_error("cycling_power_service_response_can_send_now: instance is null");
        return;
    }
    
    if (instance->response_value == CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE){
        log_error("cycling_power_service_response_can_send_now: CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE");
        return;
    } 

    // use preprocessor instead of btstack_max to get compile-time constant
#if (CP_SENSOR_LOCATION_RESERVED > (CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE + 5))
    #define MAX_RESPONSE_PAYLOAD CP_SENSOR_LOCATION_RESERVED
#else
    #define MAX_RESPONSE_PAYLOAD (CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE + 5)
#endif

    uint8_t value[3 + MAX_RESPONSE_PAYLOAD];
    int pos = 0;
    value[pos++] = CP_OPCODE_RESPONSE_CODE;
    value[pos++] = instance->request_opcode;
    value[pos++] = instance->response_value;
    if (instance->response_value == CP_RESPONSE_VALUE_SUCCESS){
        switch (instance->request_opcode){
            case CP_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS:{
                int i;
                for (i=0; i<instance->num_supported_sensor_locations; i++){
                    value[pos++] = instance->supported_sensor_locations[i]; 
                }
                break;
            }
            case CP_OPCODE_REQUEST_CRANK_LENGTH:
                little_endian_store_16(value, pos, instance->crank_length_mm);
                pos += 2;
                break;
            case CP_OPCODE_REQUEST_CHAIN_LENGTH:
                little_endian_store_16(value, pos, instance->chain_length_mm);
                pos += 2;
                break;
            case CP_OPCODE_REQUEST_CHAIN_WEIGHT:
                little_endian_store_16(value, pos, instance->chain_weight_g);
                pos += 2;
                break;
            case CP_OPCODE_REQUEST_SPAN_LENGTH:
                little_endian_store_16(value, pos, instance->span_length_mm);
                pos += 2;
                break;
            case CP_OPCODE_REQUEST_FACTORY_CALIBRATION_DATE:
                little_endian_store_16(value, pos, instance->factory_calibration_date.year);
                pos += 2;
                value[pos++] = instance->factory_calibration_date.month;
                value[pos++] = instance->factory_calibration_date.day;
                value[pos++] = instance->factory_calibration_date.hours;
                value[pos++] = instance->factory_calibration_date.minutes;
                value[pos++] = instance->factory_calibration_date.seconds;
                break;
            case CP_OPCODE_REQUEST_SAMPLING_RATE:
                value[pos++] = instance->sampling_rate_Hz;
                break;
            case CP_OPCODE_START_OFFSET_COMPENSATION:
            case CP_OPCODE_START_ENHANCED_OFFSET_COMPENSATION:{
                uint16_t calibrated_value = 0xffff;
                if (has_feature(CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED)){
                    if (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_FORCE) {
                        calibrated_value = instance->current_force_magnitude_N;
                    } else if (has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) == CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE){
                        calibrated_value = instance->current_torque_magnitude_Nm;
                    }
                }
                
                if (calibrated_value == CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION){
                     value[pos++] = (uint8_t) calibrated_value;
                     // do not include manufacturer ID and data
                     break;
                } else if (calibrated_value == CP_CALIBRATION_STATUS_MANUFACTURER_SPECIFIC_ERROR_FOLLOWS){
                    value[pos++] = (uint8_t) calibrated_value;
                } else {
                    little_endian_store_16(value, pos, calibrated_value);
                    pos += 2;
                }
                
                if (instance->request_opcode == CP_OPCODE_START_OFFSET_COMPENSATION) break;
                little_endian_store_16(value, pos, instance->manufacturer_company_id);
                pos += 2;
                int data_len = (instance->num_manufacturer_specific_data < CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE) ? instance->num_manufacturer_specific_data : (CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE - 1);
                value[pos++] = data_len;
                (void)memcpy(&value[pos],
                             instance->manufacturer_specific_data, data_len);
                pos += data_len;
                value[pos++] = 0;
                break;
            }
            case CP_OPCODE_MASK_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT:
                break;
            default:
                break;
        }
    } 
    uint8_t status = att_server_indicate(instance->con_handle, instance->control_point_value_handle, &value[0], pos); 
    if (status == ERROR_CODE_SUCCESS){
        instance->w4_indication_complete = 1;
        instance->request_opcode = CP_OPCODE_IDLE;
    } else {
        log_error("can_send_now failed 0x%2x", status);
    }
}

static void cycling_power_service_server_emit_start_calibration(const cycling_power_t *instance, bool enhanced) {

    cycling_power_sensor_measurement_context_t measurement_type =
            has_feature(CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT) ? CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE : CP_SENSOR_MEASUREMENT_CONTEXT_FORCE;

    uint8_t event[7];
    int index = 0;
    event[index++] = HCI_EVENT_GATTSERVICE_META;
    event[index++] = sizeof(event) - 2u;
    event[index++] = GATTSERVICE_SUBEVENT_CYCLING_POWER_START_CALIBRATION;
    little_endian_store_16(event, index, instance->con_handle);
    index += 2;
    event[index++] = (uint8_t) measurement_type;
    event[index++] = enhanced ? 1 : 0;
    (*instance->calibration_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static int cycling_power_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(con_handle);
    UNUSED(offset);
    UNUSED(buffer_size);
    int i;
    cycling_power_sensor_location_t location;
    cycling_power_t * instance = &cycling_power;

    if (transaction_mode != ATT_TRANSACTION_MODE_NONE){
        return 0;
    }

    if (attribute_handle == instance->measurement_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->measurement_client_configuration_descriptor_notify = little_endian_read_16(buffer, 0);
        instance->con_handle = con_handle;
        log_info("cycling_power_service_write_callback: measurement enabled %d", instance->measurement_client_configuration_descriptor_notify);
        return 0;
    }

    if (attribute_handle == instance->measurement_server_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->measurement_server_configuration_descriptor_broadcast = little_endian_read_16(buffer, 0);
        instance->con_handle = con_handle;
        uint8_t event[5];
        int index = 0;
        event[index++] = HCI_EVENT_GATTSERVICE_META;
        event[index++] = sizeof(event) - 2u;
        
        if (instance->measurement_server_configuration_descriptor_broadcast){
            event[index++] = GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_START;
            log_info("cycling_power_service_write_callback: start broadcast");
        } else {
            event[index++] = GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_STOP;
            log_info("cycling_power_service_write_callback: stop broadcast");
        }
        little_endian_store_16(event, index, con_handle);
        index += 2;
        (*instance->calibration_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
        return 0;
    }

    if (attribute_handle == instance->vector_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->con_handle = con_handle;

#ifdef ENABLE_ATT_DELAYED_RESPONSE          
        switch (instance->con_interval_status){
            case CP_CONNECTION_INTERVAL_STATUS_REJECTED:
                return CYCLING_POWER_ERROR_CODE_INAPPROPRIATE_CONNECTION_PARAMETERS;
        
            case CP_CONNECTION_INTERVAL_STATUS_ACCEPTED:
            case CP_CONNECTION_INTERVAL_STATUS_RECEIVED:
                if ((instance->con_interval > instance->con_interval_max) || (instance->con_interval < instance->con_interval_min)){
                    instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_W4_L2CAP_RESPONSE;
                    gap_request_connection_parameter_update(instance->con_handle, instance->con_interval_min, instance->con_interval_max, 4, 100);    // 15 ms, 4, 1s
                    return ATT_ERROR_WRITE_RESPONSE_PENDING;
                }
                instance->vector_client_configuration_descriptor_notify = little_endian_read_16(buffer, 0); 
                return 0;
            default:
                return ATT_ERROR_WRITE_RESPONSE_PENDING;
                
        }
#endif
    }

    if (attribute_handle == instance->control_point_client_configuration_descriptor_handle){
        if (buffer_size < 2u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->control_point_client_configuration_descriptor_indicate = little_endian_read_16(buffer, 0);
        instance->con_handle = con_handle;
        log_info("cycling_power_service_write_callback: indication enabled %d", instance->control_point_client_configuration_descriptor_indicate);
        return 0;
    }

    if (attribute_handle == instance->feature_value_handle){
        if (buffer_size < 4u){
            return ATT_ERROR_INVALID_OFFSET;
        }
        instance->feature_flags = little_endian_read_32(buffer, 0);
        return 0;
    }

    if (attribute_handle == instance->control_point_value_handle){
        if (instance->control_point_client_configuration_descriptor_indicate == 0u) return CYCLING_POWER_ERROR_CODE_CCC_DESCRIPTOR_IMPROPERLY_CONFIGURED;
        if (instance->w4_indication_complete != 0u){
            return CYCLING_POWER_ERROR_CODE_PROCEDURE_ALREADY_IN_PROGRESS;
        } 
        int pos = 0;
        instance->request_opcode = (cycling_power_opcode_t) buffer[pos++];
        instance->response_value = CP_RESPONSE_VALUE_OP_CODE_NOT_SUPPORTED;
        
        switch (instance->request_opcode){
            case CP_OPCODE_SET_CUMULATIVE_VALUE:
                if (!has_feature(CP_FEATURE_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED)) break;
                instance->cumulative_wheel_revolutions = little_endian_read_32(buffer, pos);
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            
            case CP_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS:
                if (!has_feature(CP_FEATURE_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            
            case CP_OPCODE_UPDATE_SENSOR_LOCATION:
                if (!has_feature(CP_FEATURE_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED)) break;
                location = (cycling_power_sensor_location_t) buffer[pos];
                instance->response_value = CP_RESPONSE_VALUE_INVALID_PARAMETER;
                for (i=0; i<instance->num_supported_sensor_locations; i++){
                    if (instance->supported_sensor_locations[i] == location){
                        instance->sensor_location = location;
                        instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                        break;
                    }
                }
                break;
            
            case CP_OPCODE_REQUEST_CRANK_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_CRANK_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            case CP_OPCODE_SET_CRANK_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_CRANK_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->crank_length_mm = little_endian_read_16(buffer, pos);
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;

            case CP_OPCODE_REQUEST_CHAIN_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_CHAIN_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            case CP_OPCODE_SET_CHAIN_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_CHAIN_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->chain_length_mm = little_endian_read_16(buffer, pos);
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;

            case CP_OPCODE_REQUEST_CHAIN_WEIGHT:
                if (!has_feature(CP_FEATURE_FLAG_CHAIN_WEIGHT_ADJUSTMENT_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            case CP_OPCODE_SET_CHAIN_WEIGHT:
                if (!has_feature(CP_FEATURE_FLAG_CHAIN_WEIGHT_ADJUSTMENT_SUPPORTED)) break;
                instance->chain_weight_g = little_endian_read_16(buffer, pos);
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;

            case CP_OPCODE_REQUEST_SPAN_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_SPAN_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            case CP_OPCODE_SET_SPAN_LENGTH:
                if (!has_feature(CP_FEATURE_FLAG_SPAN_LENGTH_ADJUSTMENT_SUPPORTED)) break;
                instance->span_length_mm = little_endian_read_16(buffer, pos);
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;

            case CP_OPCODE_REQUEST_FACTORY_CALIBRATION_DATE:
                if (!has_feature(CP_FEATURE_FLAG_FACTORY_CALIBRATION_DATE_SUPPORTED)) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
                
            case CP_OPCODE_REQUEST_SAMPLING_RATE:
                if (!instance->vector_value_handle) break;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;

            case CP_OPCODE_START_OFFSET_COMPENSATION:
                if (has_feature(CP_FEATURE_FLAG_OFFSET_COMPENSATION_SUPPORTED)){
                    instance->response_value = CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE;
                    cycling_power_service_server_emit_start_calibration(instance, false);
                } else {
                    instance->response_value = CP_RESPONSE_VALUE_INVALID_PARAMETER;
                }
                break;

            case CP_OPCODE_START_ENHANCED_OFFSET_COMPENSATION:
                if (has_feature(CP_FEATURE_FLAG_ENHANCED_OFFSET_COMPENSATION_SUPPORTED)){
                    instance->response_value = CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE;
                    cycling_power_service_server_emit_start_calibration(instance, true);
                } else {
                    instance->response_value = CP_RESPONSE_VALUE_INVALID_PARAMETER;
                }
                break;

            case CP_OPCODE_MASK_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT:{
                if (!has_feature(CP_FEATURE_FLAG_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT_MASKING_SUPPORTED)) break;
                uint16_t mask_bitmap = little_endian_read_16(buffer, pos);
                uint16_t masked_measurement_flags = instance->default_measurement_flags;
                uint16_t index = 0;
                
                for (i = 0; i < CP_MASK_BIT_RESERVED; i++){
                    uint8_t clear_bit = (mask_bitmap & (1u << i)) ? 1u : 0u;
                    
                    masked_measurement_flags &= ~(clear_bit << index);
                    index++;
                    // following measurement flags have additional flag         
                    switch ((cycling_power_mask_bit_t)i){
                        case CP_MASK_BIT_PEDAL_POWER_BALANCE:
                        case CP_MASK_BIT_ACCUMULATED_TORQUE:
                        case CP_MASK_BIT_EXTREME_MAGNITUDES:
                            masked_measurement_flags &= ~(clear_bit << index);
                            index++;
                            break;
                        default:
                            break;
                    }
                }
                instance->masked_measurement_flags = masked_measurement_flags;
                instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
                break;
            }
            default:
                break;
        }
        
        if (instance->control_point_client_configuration_descriptor_indicate){
            instance->control_point_indicate_callback.callback = &cycling_power_service_response_can_send_now;
            instance->control_point_indicate_callback.context  = (void*) instance;
            att_server_register_can_send_now_callback(&instance->control_point_indicate_callback, instance->con_handle);
        }
        return 0;
    }
    return 0;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    cycling_power_t * instance = &cycling_power;
    uint8_t event_type = hci_event_packet_get_type(packet);
    uint16_t con_handle;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch (event_type){
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    instance->con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    // print connection parameters (without using float operations)
                    instance->con_interval = gap_subevent_le_connection_complete_get_conn_interval(packet);
                    instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_RECEIVED;
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
#ifdef ENABLE_ATT_DELAYED_RESPONSE
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                    if (instance->con_interval_status != CP_CONNECTION_INTERVAL_STATUS_W4_UPDATE) return;
                    
                    if ((instance->con_interval > instance->con_interval_max) || (instance->con_interval < instance->con_interval_min)){
                        instance->con_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                        instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_ACCEPTED;
                    } else {
                        instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_REJECTED;
                    }
                    att_server_response_ready(l2cap_event_connection_parameter_update_response_get_handle(packet));
                    break;
#endif
                default:
                    break;
            }
            break;

#ifdef ENABLE_ATT_DELAYED_RESPONSE
        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            if (instance->con_interval_status != CP_CONNECTION_INTERVAL_STATUS_W4_L2CAP_RESPONSE) return;
            
            if (l2cap_event_connection_parameter_update_response_get_result(packet) == ERROR_CODE_SUCCESS){
                instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_W4_UPDATE;
            } else {
                instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_REJECTED;
                att_server_response_ready(l2cap_event_connection_parameter_update_response_get_handle(packet));
            }
            break;
#endif

        case HCI_EVENT_DISCONNECTION_COMPLETE:{
            if (!instance) return;
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            if (con_handle == HCI_CON_HANDLE_INVALID) return;

            instance->masked_measurement_flags = CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED;
            instance->w4_indication_complete = 0;
        
            uint8_t event[5];
            int index = 0;
            event[index++] = HCI_EVENT_GATTSERVICE_META;
            event[index++] = sizeof(event) - 2u;
            
            event[index++] = GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_STOP;
            little_endian_store_16(event, index, con_handle);
            index += 2;
            (*instance->calibration_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));

            break;
        }
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            instance->w4_indication_complete = 0;
            break;
        default:
            break;
     }
}

void cycling_power_service_server_init(uint32_t feature_flags,
                                       cycling_power_pedal_power_balance_reference_t pedal_power_balance_reference, cycling_power_torque_source_t torque_source,
                                       cycling_power_sensor_location_t * supported_sensor_locations, uint16_t num_supported_sensor_locations,
                                       cycling_power_sensor_location_t   current_sensor_location){

    cycling_power_t * instance = &cycling_power;
    // TODO: remove hardcoded initialization
    instance->con_interval_min = 6;
    instance->con_interval_max = 6;
    instance->con_interval_status = CP_CONNECTION_INTERVAL_STATUS_NONE;
    instance->w4_indication_complete = 0;

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_event_callback_registration.callback = &packet_handler;
    l2cap_add_event_handler(&l2cap_event_callback_registration);

    instance->sensor_location = current_sensor_location;
    instance->num_supported_sensor_locations = 0;
    if (supported_sensor_locations != NULL){
        instance->num_supported_sensor_locations = num_supported_sensor_locations;
        instance->supported_sensor_locations = supported_sensor_locations;
    }
    
    instance->feature_flags = feature_flags;
    instance->default_measurement_flags = CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED;
    instance->masked_measurement_flags  = CYCLING_POWER_MEASUREMENT_FLAGS_CLEARED;
    instance->pedal_power_balance_reference = pedal_power_balance_reference;
    instance->torque_source = torque_source;

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_CYCLING_POWER, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
	UNUSED(service_found);

    // get CP Mesurement characteristic value handle and client configuration handle
    instance->measurement_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_MEASUREMENT);
    instance->measurement_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_MEASUREMENT);
    instance->measurement_server_configuration_descriptor_handle = gatt_server_get_server_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_MEASUREMENT);
    
    // get CP Feature characteristic value handle and client configuration handle
    instance->feature_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_FEATURE);
    // get CP Sensor Location characteristic value handle and client configuration handle
    instance->sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION);
    
    // get CP Vector characteristic value handle and client configuration handle
    instance->vector_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_VECTOR);
    instance->vector_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_VECTOR);

    // get Body Sensor Location characteristic value handle and client configuration handle
    instance->sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION);
    
    // get SP Control Point characteristic value handle and client configuration handle
    instance->control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_CONTROL_POINT);
    instance->control_point_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_CONTROL_POINT);

    log_info("Measurement     value handle 0x%02x", instance->measurement_value_handle);
    log_info("M. Client Cfg   value handle 0x%02x", instance->measurement_client_configuration_descriptor_handle);
    log_info("M. Server Cfg   value handle 0x%02x", instance->measurement_server_configuration_descriptor_handle);

    log_info("Feature         value handle 0x%02x", instance->feature_value_handle);
    log_info("Sensor location value handle 0x%02x", instance->sensor_location_value_handle);

    log_info("Vector          value handle 0x%02x", instance->vector_value_handle);
    log_info("Vector Cfg.     value handle 0x%02x", instance->vector_client_configuration_descriptor_handle);

    log_info("Control Point   value handle 0x%02x", instance->control_point_value_handle);
    log_info("Control P. Cfg. value handle 0x%02x", instance->control_point_client_configuration_descriptor_handle);
    
    cycling_power_service.start_handle   = start_handle;
    cycling_power_service.end_handle     = end_handle;
    cycling_power_service.read_callback  = &cycling_power_service_read_callback;
    cycling_power_service.write_callback = &cycling_power_service_write_callback;
    cycling_power_service.packet_handler = &packet_handler; 
    att_server_register_service_handler(&cycling_power_service);
}


void cycling_power_service_server_add_torque(int16_t torque_Nm){
    cycling_power_t * instance = &cycling_power;
    instance->accumulated_torque_Nm += torque_Nm;
}

void cycling_power_service_server_add_wheel_revolution(int32_t wheel_revolution, uint16_t wheel_event_time_s){
    cycling_power_t * instance = &cycling_power;
    instance->last_wheel_event_time_s = wheel_event_time_s;
    if (wheel_revolution < 0){
        uint32_t wheel_revolution_to_subtract = (uint32_t) (-wheel_revolution);
        if (instance->cumulative_wheel_revolutions > wheel_revolution_to_subtract){
            instance->cumulative_wheel_revolutions -= wheel_revolution_to_subtract;
        } else {
            instance->cumulative_wheel_revolutions = 0;
        } 
    } else {
        if (instance->cumulative_wheel_revolutions < (0xffffffff - wheel_revolution)){
            instance->cumulative_wheel_revolutions += wheel_revolution;
        } else {
            instance->cumulative_wheel_revolutions = 0xffffffff;
        } 
    }
} 

void cycling_power_service_server_add_crank_revolution(uint16_t crank_revolution, uint16_t crank_event_time_s){
    cycling_power_t * instance = &cycling_power;
    instance->last_crank_event_time_s = crank_event_time_s;
    instance->cumulative_crank_revolutions += crank_revolution;
} 

void cycling_power_service_add_energy(uint16_t energy_kJ){
    cycling_power_t * instance = &cycling_power;
    if (instance->accumulated_energy_kJ <= (0xffffu - energy_kJ)){
        instance->accumulated_energy_kJ += energy_kJ;
    } else {
        instance->accumulated_energy_kJ = 0xffff;
    }
} 

void cycling_power_service_server_set_instantaneous_power(int16_t instantaneous_power_W){
    cycling_power_t * instance = &cycling_power;
    instance->instantaneous_power_W = instantaneous_power_W;
}

void cycling_power_service_server_set_pedal_power_balance(uint8_t pedal_power_balance_percentage){
    cycling_power_t * instance = &cycling_power;
    instance->pedal_power_balance_percentage = pedal_power_balance_percentage;
}

void cycling_power_service_server_set_force_magnitude_values(int force_magnitude_count, int16_t * force_magnitude_N_array){
    cycling_power_t * instance = &cycling_power;
    instance->force_magnitude_count = force_magnitude_count;
    instance->vector_instantaneous_force_magnitude_N_array = force_magnitude_N_array;
}

void cycling_power_service_server_set_torque_magnitude_values(int torque_magnitude_count, int16_t * torque_magnitude_Nm_array){
    cycling_power_t * instance = &cycling_power;
    instance->torque_magnitude_count = torque_magnitude_count;
    instance->vector_instantaneous_torque_magnitude_Nm_array = torque_magnitude_Nm_array;
}

void cycling_power_service_server_set_first_crank_measurement_angle(uint16_t first_crank_measurement_angle_degree){
    cycling_power_t * instance = &cycling_power;
    instance->vector_first_crank_measurement_angle_degree = first_crank_measurement_angle_degree;
}

void cycling_power_service_server_set_instantaneous_measurement_direction(cycling_power_instantaneous_measurement_direction_t direction){
    cycling_power_t * instance = &cycling_power;
    instance->vector_instantaneous_measurement_direction = direction;
}

void cycling_power_service_server_set_force_magnitude(int16_t min_force_magnitude_N, int16_t max_force_magnitude_N){
    cycling_power_t * instance = &cycling_power;
    instance->minimum_force_magnitude_N = min_force_magnitude_N;
    instance->maximum_force_magnitude_N = max_force_magnitude_N;
} 

void cycling_power_service_server_set_torque_magnitude(int16_t min_torque_magnitude_Nm, int16_t max_torque_magnitude_Nm){
    cycling_power_t * instance = &cycling_power;
    instance->minimum_torque_magnitude_Nm = min_torque_magnitude_Nm;
    instance->maximum_torque_magnitude_Nm = max_torque_magnitude_Nm;
} 

void cycling_power_service_server_set_angle(uint16_t min_angle_degree, uint16_t max_angle_degree){
    cycling_power_t * instance = &cycling_power;
    instance->minimum_angle_degree = min_angle_degree;
    instance->maximum_angle_degree = max_angle_degree;
} 

void cycling_power_service_server_set_top_dead_spot_angle(uint16_t top_dead_spot_angle_degree){
    cycling_power_t * instance = &cycling_power;
    instance->top_dead_spot_angle_degree = top_dead_spot_angle_degree;
} 

void cycling_power_service_server_set_bottom_dead_spot_angle(uint16_t bottom_dead_spot_angle_degree){
    cycling_power_t * instance = &cycling_power;
    instance->bottom_dead_spot_angle_degree = bottom_dead_spot_angle_degree;
} 

static int gatt_date_is_valid(gatt_date_time_t date){
    if ((date.year != 0u) && ((date.year < 1582u) || (date.year > 9999u))) return 0u;
    if ((date.month != 0u) && (date.month > 12u)) return 0u;
    if ((date.day != 0u) && (date.day > 31u)) return 0u;

    if (date.hours > 23u) return 0u;
    if (date.minutes > 59u) return 0u;
    if (date.seconds > 59u) return 0u;
    return 1;
}

int cycling_power_service_server_set_factory_calibration_date(gatt_date_time_t date){
    if (!gatt_date_is_valid(date)) return 0;

    cycling_power_t * instance = &cycling_power;
    instance->factory_calibration_date = date;
    return 1;
}

void cycling_power_service_server_set_sampling_rate(uint8_t sampling_rate_Hz){
    cycling_power_t * instance = &cycling_power;
    instance->sampling_rate_Hz = sampling_rate_Hz;
}


void cycling_power_service_server_update_values(void){
    cycling_power_t * instance = &cycling_power;

    if (instance->measurement_server_configuration_descriptor_broadcast){
        instance->measurement_broadcast_callback.callback = &cycling_power_service_broadcast_can_send_now;
        instance->measurement_broadcast_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->measurement_broadcast_callback, instance->con_handle);
    }
    
    if (instance->measurement_client_configuration_descriptor_notify){
        instance->measurement_notify_callback.callback = &cycling_power_service_measurement_can_send_now;
        instance->measurement_notify_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->measurement_notify_callback, instance->con_handle);
    }

    if (instance->vector_client_configuration_descriptor_notify){
        instance->vector_notify_callback.callback = &cycling_power_service_vector_can_send_now;
        instance->vector_notify_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->vector_notify_callback, instance->con_handle);
    }
}

void cycling_power_service_server_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("cycling_power_service_server_packet_handler called with NULL callback");
        return;
    }
    cycling_power_t * instance = &cycling_power;
    instance->calibration_callback = callback;
}

void cycling_power_server_calibration_done(cycling_power_sensor_measurement_context_t measurement_type, uint16_t calibrated_value){
    cycling_power_t * instance = &cycling_power;
    if (instance->response_value != CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE){
        return;
    } 
    instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
    
    switch (measurement_type){
        case CP_SENSOR_MEASUREMENT_CONTEXT_FORCE:
            instance->current_force_magnitude_N = calibrated_value;
            break;
        case CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE:
            instance->current_torque_magnitude_Nm = calibrated_value;
            break;
        default:
            instance->response_value = CP_RESPONSE_VALUE_INVALID_PARAMETER;
            break;
    }

    if (instance->response_value == CP_RESPONSE_VALUE_SUCCESS){
        switch (calibrated_value){
            case CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION:
            case CP_CALIBRATION_STATUS_MANUFACTURER_SPECIFIC_ERROR_FOLLOWS:
                instance->response_value = CP_RESPONSE_VALUE_OPERATION_FAILED;
                instance->response_value = CP_RESPONSE_VALUE_OPERATION_FAILED;
                break;
            default:
                break;
        }
    }
    
    if (instance->control_point_client_configuration_descriptor_indicate){
        instance->control_point_indicate_callback.callback = &cycling_power_service_response_can_send_now;
        instance->control_point_indicate_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->control_point_indicate_callback, instance->con_handle);
    }
}

void cycling_power_server_enhanced_calibration_done(cycling_power_sensor_measurement_context_t measurement_type,  
                uint16_t calibrated_value, uint16_t manufacturer_company_id, 
                uint8_t num_manufacturer_specific_data, uint8_t * manufacturer_specific_data){
    cycling_power_t * instance = &cycling_power;
    if (instance->response_value != CP_RESPONSE_VALUE_W4_VALUE_AVAILABLE) return;
    instance->response_value = CP_RESPONSE_VALUE_SUCCESS;
    
    switch (measurement_type){
        case CP_SENSOR_MEASUREMENT_CONTEXT_FORCE:
            instance->current_force_magnitude_N = calibrated_value;
            break;
        case CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE:
            instance->current_torque_magnitude_Nm = calibrated_value;
            break;
        default:
            instance->response_value = CP_RESPONSE_VALUE_INVALID_PARAMETER;
            break;
    }
            
    if (instance->response_value == CP_RESPONSE_VALUE_SUCCESS){
        switch (calibrated_value){
            case CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION:
            case CP_CALIBRATION_STATUS_MANUFACTURER_SPECIFIC_ERROR_FOLLOWS:
                instance->response_value = CP_RESPONSE_VALUE_OPERATION_FAILED;
                instance->response_value = CP_RESPONSE_VALUE_OPERATION_FAILED;
                break;
            default:
                break;
        }
        instance->manufacturer_company_id = manufacturer_company_id;
        instance->num_manufacturer_specific_data = num_manufacturer_specific_data;
        instance->manufacturer_specific_data = manufacturer_specific_data;
    }
        
    if (instance->control_point_client_configuration_descriptor_indicate){
        instance->control_point_indicate_callback.callback = &cycling_power_service_response_can_send_now;
        instance->control_point_indicate_callback.context  = (void*) instance;
        att_server_register_can_send_now_callback(&instance->control_point_indicate_callback, instance->con_handle);
    }   
}

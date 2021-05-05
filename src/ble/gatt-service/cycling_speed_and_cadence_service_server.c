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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "cycling_speed_and_cadence_service_server.c"


#include "bluetooth.h"
#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/cycling_speed_and_cadence_service_server.h"

typedef enum {
	CSC_RESPONSE_VALUE_SUCCESS = 1,
	CSC_RESPONSE_VALUE_OP_CODE_NOT_SUPPORTED,
	CSC_RESPONSE_VALUE_INVALID_PARAMETER,
	CSC_RESPONSE_VALUE_OPERATION_FAILED
} csc_response_value_t;

typedef struct {
	hci_con_handle_t con_handle;

	uint8_t wheel_revolution_data_supported;
	uint8_t crank_revolution_data_supported;
	uint8_t multiple_sensor_locations_supported;

	// characteristic: CSC Mesurement 
	uint16_t measurement_value_handle;
	uint32_t cumulative_wheel_revolutions;
	uint16_t last_wheel_event_time; // Unit has a resolution of 1/1024s
	uint16_t cumulative_crank_revolutions;
	uint16_t last_crank_event_time; // Unit has a resolution of 1/1024s
	
	// characteristic descriptor: Client Characteristic Configuration
	uint16_t measurement_client_configuration_descriptor_handle;
	uint16_t measurement_client_configuration_descriptor_notify;
	btstack_context_callback_registration_t measurement_callback;

	// sensor locations bitmap
	uint16_t feature_handle;
	
	// sensor locations
	uint16_t sensor_location_value_handle;
	cycling_speed_and_cadence_sensor_location_t sensor_location;
	uint32_t supported_sensor_locations;

	// characteristic: Heart Rate Control Point
	uint16_t control_point_value_handle;
	uint16_t control_point_client_configuration_descriptor_handle;
	uint16_t control_point_client_configuration_descriptor_indicate;
	btstack_context_callback_registration_t control_point_callback;

	csc_opcode_t request_opcode;
	csc_response_value_t response_value;
} cycling_speed_and_cadence_t;

static att_service_handler_t cycling_speed_and_cadence_service;
static cycling_speed_and_cadence_t cycling_speed_and_cadence;

static uint16_t cycling_speed_and_cadence_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);
	UNUSED(attribute_handle);
	UNUSED(offset);
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;

	if (attribute_handle == instance->measurement_client_configuration_descriptor_handle){
		if (buffer && (buffer_size >= 2u)){
			little_endian_store_16(buffer, 0, instance->measurement_client_configuration_descriptor_notify);
		} 
		return 2;
	}

	if (attribute_handle == instance->control_point_client_configuration_descriptor_handle){
		if (buffer && (buffer_size >= 2u)){
			little_endian_store_16(buffer, 0, instance->control_point_client_configuration_descriptor_indicate);
		} 
		return 2;
	}

	if (attribute_handle == instance->feature_handle){
		if (buffer && (buffer_size >= 2u)){
			uint16_t feature = (instance->wheel_revolution_data_supported << CSC_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED);
			feature |= (instance->crank_revolution_data_supported << CSC_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED);
			feature |= (instance->multiple_sensor_locations_supported << CSC_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED);
			little_endian_store_16(buffer, 0, feature);
		} 
		return 2;
	}	
	
	if (attribute_handle == instance->sensor_location_value_handle){
		if (buffer && (buffer_size >= 1u)){
			buffer[0] = instance->sensor_location;
		} 
		return 1;
	}	
	return 0;
}

static void cycling_speed_and_cadence_service_csc_measurement_can_send_now(void * context){
	cycling_speed_and_cadence_t * instance = (cycling_speed_and_cadence_t *) context;
	if (!instance){
		return;
	}
	uint8_t flags = (instance->wheel_revolution_data_supported << CSC_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED);
	flags |= (instance->crank_revolution_data_supported << CSC_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED);

	uint8_t value[11];
	int pos = 0;

	value[pos++] = flags;
	if (instance->wheel_revolution_data_supported){
		little_endian_store_32(value, pos, instance->cumulative_wheel_revolutions);
		pos += 4;
		little_endian_store_16(value, pos, instance->last_wheel_event_time);
		pos += 2;
	}

	if (instance->crank_revolution_data_supported){
		little_endian_store_16(value, pos, instance->cumulative_crank_revolutions);
		pos += 2;
		little_endian_store_16(value, pos, instance->last_crank_event_time);
		pos += 2;
	}

	att_server_notify(instance->con_handle, instance->measurement_value_handle, &value[0], pos); 
}

static void cycling_speed_and_cadence_service_response_can_send_now(void * context){
	cycling_speed_and_cadence_t * instance = (cycling_speed_and_cadence_t *) context;
	if (!instance){
		return;
	}
		
	uint8_t value[3 + sizeof(cycling_speed_and_cadence_sensor_location_t)];
	int pos = 0;
	value[pos++] = CSC_OPCODE_RESPONSE_CODE;
	value[pos++] = instance->request_opcode;
	value[pos++] = instance->response_value;
	switch (instance->request_opcode){
		case CSC_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS:{
			int loc;
			for (loc = CSC_SERVICE_SENSOR_LOCATION_OTHER; loc < (CSC_SERVICE_SENSOR_LOCATION_RESERVED-1); loc++){
				if (instance->supported_sensor_locations & (1u << loc)){
					value[pos++] = loc;
				}
			}
			break;
		}
		default:
			break;
	}
	csc_opcode_t temp_request_opcode = instance->request_opcode;
	instance->request_opcode = CSC_OPCODE_IDLE;

	(void) att_server_indicate(instance->con_handle, instance->control_point_value_handle, &value[0], pos); 
	switch (temp_request_opcode){
		case CSC_OPCODE_SET_CUMULATIVE_VALUE:
			if (instance->response_value != CSC_RESPONSE_VALUE_SUCCESS) break;
			if (instance->measurement_client_configuration_descriptor_notify){
				instance->measurement_callback.callback = &cycling_speed_and_cadence_service_csc_measurement_can_send_now;
				instance->measurement_callback.context  = (void*) instance;
				att_server_register_can_send_now_callback(&instance->measurement_callback, instance->con_handle);
			}
			break;
 		default:
			break;
	}
}

static int cycling_speed_and_cadence_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(con_handle);
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;

	if (attribute_handle == instance->measurement_client_configuration_descriptor_handle){
		if (buffer_size < 2u){
			return ATT_ERROR_INVALID_OFFSET;
		}
		instance->measurement_client_configuration_descriptor_notify = little_endian_read_16(buffer, 0);
		instance->con_handle = con_handle;
		return 0;
	}

	if (attribute_handle == instance->control_point_client_configuration_descriptor_handle){
		if (buffer_size < 2u){
			return ATT_ERROR_INVALID_OFFSET;
		}
		instance->control_point_client_configuration_descriptor_indicate = little_endian_read_16(buffer, 0);
		instance->con_handle = con_handle;
		return 0;
	}

	if (attribute_handle == instance->control_point_value_handle){
		if (instance->control_point_client_configuration_descriptor_indicate == 0u) return CYCLING_SPEED_AND_CADENCE_ERROR_CODE_CCC_DESCRIPTOR_IMPROPERLY_CONFIGURED;
		if (instance->request_opcode != CSC_OPCODE_IDLE) return CYCLING_SPEED_AND_CADENCE_ERROR_CODE_PROCEDURE_ALREADY_IN_PROGRESS;

		instance->request_opcode = (csc_opcode_t) buffer[0];
		instance->response_value = CSC_RESPONSE_VALUE_SUCCESS;
		
		switch (instance->request_opcode){
			case CSC_OPCODE_SET_CUMULATIVE_VALUE:
				if (instance->wheel_revolution_data_supported){
					instance->cumulative_wheel_revolutions = little_endian_read_32(buffer, 1);
					break;
				}
				instance->response_value = CSC_RESPONSE_VALUE_OPERATION_FAILED;
				break;
	 		case CSC_OPCODE_START_SENSOR_CALIBRATION:
				break;
	 		case CSC_OPCODE_UPDATE_SENSOR_LOCATION:
	 			if (instance->multiple_sensor_locations_supported){
					cycling_speed_and_cadence_sensor_location_t sensor_location = (cycling_speed_and_cadence_sensor_location_t) buffer[1];
					if (sensor_location >= CSC_SERVICE_SENSOR_LOCATION_RESERVED){
						instance->response_value = CSC_RESPONSE_VALUE_INVALID_PARAMETER;
						break;
					}
					instance->sensor_location = sensor_location;
					break;
				}
				instance->response_value = CSC_RESPONSE_VALUE_OPERATION_FAILED;
				break;
	 		case CSC_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS:
				break;
			default:
				instance->response_value = CSC_RESPONSE_VALUE_OP_CODE_NOT_SUPPORTED;
				break;
		}
	
		if (instance->control_point_client_configuration_descriptor_indicate){
			instance->control_point_callback.callback = &cycling_speed_and_cadence_service_response_can_send_now;
			instance->control_point_callback.context  = (void*) instance;
			att_server_register_can_send_now_callback(&instance->control_point_callback, instance->con_handle);
		}
		return 0;
	}

	return 0;
}

void cycling_speed_and_cadence_service_server_init(uint32_t supported_sensor_locations, 
	uint8_t multiple_sensor_locations_supported, uint8_t wheel_revolution_data_supported, uint8_t crank_revolution_data_supported){
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;
	
	instance->wheel_revolution_data_supported = wheel_revolution_data_supported;
	instance->crank_revolution_data_supported = crank_revolution_data_supported;
	instance->multiple_sensor_locations_supported = multiple_sensor_locations_supported;
	instance->supported_sensor_locations = supported_sensor_locations;

	instance->sensor_location = CSC_SERVICE_SENSOR_LOCATION_OTHER;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_CYCLING_SPEED_AND_CADENCE, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

	// // get CSC Mesurement characteristic value handle and client configuration handle
	instance->measurement_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT);
	instance->measurement_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT);
	
	// get CSC Feature characteristic value handle and client configuration handle
	instance->feature_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE);
	
	// get Body Sensor Location characteristic value handle and client configuration handle
	instance->sensor_location_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION);
	
	// get SC Control Point characteristic value handle and client configuration handle
	instance->control_point_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SC_CONTROL_POINT);
	instance->control_point_client_configuration_descriptor_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SC_CONTROL_POINT);
	log_info("Measurement     value handle 0x%02x", instance->measurement_value_handle);
	log_info("Measurement Cfg value handle 0x%02x", instance->measurement_client_configuration_descriptor_handle);
	log_info("Feature         value handle 0x%02x", instance->feature_handle);
	log_info("Sensor location value handle 0x%02x", instance->sensor_location_value_handle);
	log_info("Control Point   value handle 0x%02x", instance->control_point_value_handle);
	log_info("Control P. Cfg. value handle 0x%02x", instance->control_point_client_configuration_descriptor_handle);
	
	cycling_speed_and_cadence_service.start_handle   = start_handle;
	cycling_speed_and_cadence_service.end_handle     = end_handle;
	cycling_speed_and_cadence_service.read_callback  = &cycling_speed_and_cadence_service_read_callback;
	cycling_speed_and_cadence_service.write_callback = &cycling_speed_and_cadence_service_write_callback;
	
	att_server_register_service_handler(&cycling_speed_and_cadence_service);
}

static void cycling_speed_and_cadence_service_calculate_cumulative_wheel_revolutions(int32_t revolutions_change){
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;
	if (revolutions_change < 0){
		if (instance->cumulative_wheel_revolutions > -revolutions_change){
			instance->cumulative_wheel_revolutions += revolutions_change;
		} else {
			instance->cumulative_wheel_revolutions = 0;
		} 
	} else {
		if (instance->cumulative_wheel_revolutions < (0xffffffff - revolutions_change)){
			instance->cumulative_wheel_revolutions += revolutions_change;
		} else {
			instance->cumulative_wheel_revolutions = 0xffffffff;
		} 
	}
}

static void cycling_speed_and_cadence_service_calculate_cumulative_crank_revolutions(uint16_t revolutions_change){
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;
	
	if (instance->cumulative_crank_revolutions <= (0xffffu - revolutions_change)){
		instance->cumulative_crank_revolutions += revolutions_change;
	} else {
		instance->cumulative_crank_revolutions = 0xffff;
	}
}

// The Cumulative Wheel Revolutions value may decrement (e.g. If the bicycle is rolled in reverse), but shall not decrease below 0void cycling_speed_and_cadence_service_add_wheel_revolutions(int32_t wheel_revolutions, uint16_t last_wheel_event_time){
void cycling_speed_and_cadence_service_server_update_values(int32_t wheel_revolutions, uint16_t last_wheel_event_time, uint16_t crank_revolutions, uint16_t last_crank_event_time){
	cycling_speed_and_cadence_t * instance = &cycling_speed_and_cadence;
	
	cycling_speed_and_cadence_service_calculate_cumulative_wheel_revolutions(wheel_revolutions);
	instance->last_wheel_event_time = last_wheel_event_time;

	cycling_speed_and_cadence_service_calculate_cumulative_crank_revolutions(crank_revolutions);
	instance->last_crank_event_time = last_crank_event_time;

	if (instance->measurement_client_configuration_descriptor_notify){
		instance->measurement_callback.callback = &cycling_speed_and_cadence_service_csc_measurement_can_send_now;
		instance->measurement_callback.context  = (void*) instance;
		att_server_register_can_send_now_callback(&instance->measurement_callback, instance->con_handle);
	}
}

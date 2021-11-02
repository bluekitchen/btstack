/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

/**
 * @title Cycling Speed and Cadence Service Server
 * 
 */

#ifndef CYCLING_SPEED_AND_CADENCE_SERVICE_SERVER_H
#define CYCLING_SPEED_AND_CADENCE_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Cycling Speed and Cadence Service allows to query 
 * device's speed- and cadence-related data for use in sports and 
 * fitness applications.
 *
 * To use with your application, add `#import <cycling_speed_and_cadence_service.gatt>` 
 * to your .gatt file. 
 */

/* API_START */

typedef enum {
	CSC_SERVICE_SENSOR_LOCATION_OTHER = 0,
	CSC_SERVICE_SENSOR_LOCATION_TOP_OF_SHOE,
	CSC_SERVICE_SENSOR_LOCATION_IN_SHOE,
	CSC_SERVICE_SENSOR_LOCATION_HIP,
	CSC_SERVICE_SENSOR_LOCATION_FRONT_WHEEL,
	CSC_SERVICE_SENSOR_LOCATION_LEFT_CRANK,
	CSC_SERVICE_SENSOR_LOCATION_RIGHT_CRANK,
	CSC_SERVICE_SENSOR_LOCATION_LEFT_PEDAL,
	CSC_SERVICE_SENSOR_LOCATION_RIGHT_PEDAL,
	CSC_SERVICE_SENSOR_LOCATION_FRONT_HUB,
	CSC_SERVICE_SENSOR_LOCATION_REAR_DROPOUT,
	CSC_SERVICE_SENSOR_LOCATION_CHAINSTAY,
	CSC_SERVICE_SENSOR_LOCATION_REAR_WHEEL,
	CSC_SERVICE_SENSOR_LOCATION_REAR_HUB,
	CSC_SERVICE_SENSOR_LOCATION_CHEST,
	CSC_SERVICE_SENSOR_LOCATION_SPIDER,
	CSC_SERVICE_SENSOR_LOCATION_CHAIN_RING,
	CSC_SERVICE_SENSOR_LOCATION_RESERVED
} cycling_speed_and_cadence_sensor_location_t;

typedef enum {
	CSC_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED = 0,
	CSC_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED,
	CSC_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED
} csc_feature_flag_bit_t;

typedef enum {
	CSC_OPCODE_IDLE = 0,
	CSC_OPCODE_SET_CUMULATIVE_VALUE = 1,
	CSC_OPCODE_START_SENSOR_CALIBRATION,
	CSC_OPCODE_UPDATE_SENSOR_LOCATION,
	CSC_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS,
	CSC_OPCODE_RESPONSE_CODE = 16
} csc_opcode_t;

/**
 * @brief Init Server with ATT DB
 */
void cycling_speed_and_cadence_service_server_init(uint32_t supported_sensor_locations, 
	uint8_t multiple_sensor_locations_supported, uint8_t wheel_revolution_data_supported, uint8_t crank_revolution_data_supported);

/**
 * @brief Update heart rate (unit: beats per minute)
 * @note triggers notifications if subscribed
 */
void cycling_speed_and_cadence_service_server_update_values(int32_t wheel_revolutions, uint16_t last_wheel_event_time, uint16_t crank_revolutions, uint16_t last_crank_event_time);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


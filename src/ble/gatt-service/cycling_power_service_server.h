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
 * @title Cycling Power Service Server
 * 
 */

#ifndef CYCLING_POWER_SERVICE_SERVER_H
#define CYCLING_POWER_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Cycling Power Service allows to query device's power- and 
 * force-related data and optionally speed- and cadence-related data for 
 * use in sports and fitness applications.
 *
 * To use with your application, add `#import <cycling_power_service.gatt>` 
 * to your .gatt file.
 */

/* API_START */
#define CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE   16

typedef enum {
    CP_PEDAL_POWER_BALANCE_REFERENCE_UNKNOWN = 0,
    CP_PEDAL_POWER_BALANCE_REFERENCE_LEFT,
    CP_PEDAL_POWER_BALANCE_REFERENCE_NOT_SUPPORTED
} cycling_power_pedal_power_balance_reference_t;

typedef enum {
    CP_TORQUE_SOURCE_WHEEL = 0,
    CP_TORQUE_SOURCE_CRANK,
    CP_TORQUE_SOURCE_NOT_SUPPORTED
} cycling_power_torque_source_t;

typedef enum {
    CP_SENSOR_MEASUREMENT_CONTEXT_FORCE = 0,
    CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE
} cycling_power_sensor_measurement_context_t;

typedef enum {
    CP_DISTRIBUTED_SYSTEM_UNSPECIFIED = 0,
    CP_DISTRIBUTED_SYSTEM_NOT_SUPPORTED,
    CP_DISTRIBUTED_SYSTEM_SUPPORTED
} cycling_power_distributed_system_t;

typedef enum {
    CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_PRESENT = 0,
    CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_REFERENCE, // 0 - unknown, 1 - left
    CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_PRESENT,
    CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_SOURCE, // 0 - wheel based, 1 - crank based
    CP_MEASUREMENT_FLAG_WHEEL_REVOLUTION_DATA_PRESENT,
    CP_MEASUREMENT_FLAG_CRANK_REVOLUTION_DATA_PRESENT,
    CP_MEASUREMENT_FLAG_EXTREME_FORCE_MAGNITUDES_PRESENT,
    CP_MEASUREMENT_FLAG_EXTREME_TORQUE_MAGNITUDES_PRESENT,
    CP_MEASUREMENT_FLAG_EXTREME_ANGLES_PRESENT,
    CP_MEASUREMENT_FLAG_TOP_DEAD_SPOT_ANGLE_PRESENT,
    CP_MEASUREMENT_FLAG_BOTTOM_DEAD_SPOT_ANGLE_PRESENT,
    CP_MEASUREMENT_FLAG_ACCUMULATED_ENERGY_PRESENT,
    CP_MEASUREMENT_FLAG_OFFSET_COMPENSATION_INDICATOR,
    CP_MEASUREMENT_FLAG_RESERVED
} cycling_power_measurement_flag_t;

typedef enum {
    CP_INSTANTANEOUS_MEASUREMENT_DIRECTION_UNKNOWN = 0,
    CP_INSTANTANEOUS_MEASUREMENT_DIRECTION_TANGENTIAL_COMPONENT,
    CP_INSTANTANEOUS_MEASUREMENT_DIRECTION_RADIAL_COMPONENT,
    CP_INSTANTANEOUS_MEASUREMENT_DIRECTION_LATERAL_COMPONENT
} cycling_power_instantaneous_measurement_direction_t;

typedef enum {
    CP_VECTOR_FLAG_CRANK_REVOLUTION_DATA_PRESENT = 0,
    CP_VECTOR_FLAG_FIRST_CRANK_MEASUREMENT_ANGLE_PRESENT, 
    CP_VECTOR_FLAG_INSTANTANEOUS_FORCE_MAGNITUDE_ARRAY_PRESENT,
    CP_VECTOR_FLAG_INSTANTANEOUS_TORQUE_MAGNITUDE_ARRAY_PRESENT,
    CP_VECTOR_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION = 4, // 2 bit
    CP_VECTOR_FLAG_RESERVED = 6
} cycling_power_vector_flag_t;

typedef enum {
    CP_SENSOR_LOCATION_OTHER,
    CP_SENSOR_LOCATION_TOP_OF_SHOE,
    CP_SENSOR_LOCATION_IN_SHOE,
    CP_SENSOR_LOCATION_HIP,
    CP_SENSOR_LOCATION_FRONT_WHEEL,
    CP_SENSOR_LOCATION_LEFT_CRANK,
    CP_SENSOR_LOCATION_RIGHT_CRANK,
    CP_SENSOR_LOCATION_LEFT_PEDAL,
    CP_SENSOR_LOCATION_RIGHT_PEDAL,
    CP_SENSOR_LOCATION_FRONT_HUB,
    CP_SENSOR_LOCATION_REAR_DROPOUT,
    CP_SENSOR_LOCATION_CHAINSTAY,
    CP_SENSOR_LOCATION_REAR_WHEEL,
    CP_SENSOR_LOCATION_REAR_HUB,
    CP_SENSOR_LOCATION_CHEST,
    CP_SENSOR_LOCATION_SPIDER,
    CP_SENSOR_LOCATION_CHAIN_RING,
    CP_SENSOR_LOCATION_RESERVED
} cycling_power_sensor_location_t;

typedef enum {
    CP_FEATURE_FLAG_PEDAL_POWER_BALANCE_SUPPORTED = 0,
    CP_FEATURE_FLAG_ACCUMULATED_TORQUE_SUPPORTED,
    CP_FEATURE_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED,
    CP_FEATURE_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED,
    CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED,
    CP_FEATURE_FLAG_EXTREME_ANGLES_SUPPORTED,
    CP_FEATURE_FLAG_TOP_AND_BOTTOM_DEAD_SPOT_ANGLE_SUPPORTED,
    CP_FEATURE_FLAG_ACCUMULATED_ENERGY_SUPPORTED,
    CP_FEATURE_FLAG_OFFSET_COMPENSATION_INDICATOR_SUPPORTED,
    CP_FEATURE_FLAG_OFFSET_COMPENSATION_SUPPORTED,
    CP_FEATURE_FLAG_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT_MASKING_SUPPORTED,
    CP_FEATURE_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED,
    CP_FEATURE_FLAG_CRANK_LENGTH_ADJUSTMENT_SUPPORTED,
    CP_FEATURE_FLAG_CHAIN_LENGTH_ADJUSTMENT_SUPPORTED,
    CP_FEATURE_FLAG_CHAIN_WEIGHT_ADJUSTMENT_SUPPORTED,
    CP_FEATURE_FLAG_SPAN_LENGTH_ADJUSTMENT_SUPPORTED,
    CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT, // 0-force based, 1-torque based
    CP_FEATURE_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION_SUPPORTED,
    CP_FEATURE_FLAG_FACTORY_CALIBRATION_DATE_SUPPORTED,
    CP_FEATURE_FLAG_ENHANCED_OFFSET_COMPENSATION_SUPPORTED,
    CP_FEATURE_FLAG_DISTRIBUTED_SYSTEM_SUPPORT = 20,  // 0-unspecified, 1-not for use in distr. system, 2-used in distr. system, 3-reserved
    CP_FEATURE_FLAG_RESERVED = 22
} cycling_power_feature_flag_t;

typedef enum {
    CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION = 0x01,  
    CP_CALIBRATION_STATUS_MANUFACTURER_SPECIFIC_ERROR_FOLLOWS = 0xFF
} cycling_power_calibration_status_t;


/**
 * @brief Init Server with ATT DB
 *
 * @param feature_flags
 * @param pedal_power_balance_reference
 * @param torque_source
 * @param supported_sensor_locations
 * @param num_supported_sensor_locations
 * @param current_sensor_location
 */
void cycling_power_service_server_init(uint32_t feature_flags,
                                       cycling_power_pedal_power_balance_reference_t pedal_power_balance_reference, cycling_power_torque_source_t torque_source,
                                       cycling_power_sensor_location_t * supported_sensor_locations, uint16_t num_supported_sensor_locations,
                                       cycling_power_sensor_location_t current_sensor_location);

/**
 * @brief Setup measurement as advertisement data
 * @param adv_interval
 * @param adv_buffer
 * @param adv_size
 * @return
 */
int cycling_power_get_measurement_adv(uint16_t adv_interval, uint8_t * adv_buffer, uint16_t adv_size);

/**
 * @brief Register callback for the calibration and broadcast updates.
 *
 * @param callback
 */
void cycling_power_service_server_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Report calibration done.
 * @param measurement_type
 * @param calibrated_value
 */
void  cycling_power_server_calibration_done(cycling_power_sensor_measurement_context_t measurement_type, uint16_t calibrated_value);

/**
 * @brief Report enhanced calibration done.
 *
 * @param measurement_type
 * @param calibrated_value
 * @param manufacturer_company_id
 * @param num_manufacturer_specific_data
 * @param manufacturer_specific_data
 */
void cycling_power_server_enhanced_calibration_done(cycling_power_sensor_measurement_context_t measurement_type,
                                                    uint16_t calibrated_value, uint16_t manufacturer_company_id,
                                                    uint8_t num_manufacturer_specific_data, uint8_t * manufacturer_specific_data);

/**
 * @brief Set factory calibration date.
 * @param date
 * @return 1 if successful
 */
int  cycling_power_service_server_set_factory_calibration_date(gatt_date_time_t date);

/**
 * @brief Set sampling rate.
 * @param sampling_rate_Hz
 */
void cycling_power_service_server_set_sampling_rate(uint8_t sampling_rate_Hz);

/**
 * @brief Accumulate torque value.
 * @param torque_Nm
 */
void cycling_power_service_server_add_torque(int16_t torque_Nm);

/**
 * @brief Accumulate wheel revolution value and set the time of the last measurement.
 * @param wheel_revolution
 * @param wheel_event_time_s  unit: seconds
 */
void cycling_power_service_server_add_wheel_revolution(int32_t wheel_revolution, uint16_t wheel_event_time_s);

/**
 * @brief Accumulate crank revolution value and set the time of the last measurement.
 * @param crank_revolution
 * @param crank_event_time_s  unit: seconds
 */
void cycling_power_service_server_add_crank_revolution(uint16_t crank_revolution, uint16_t crank_event_time_s);

/**
 * @brief Accumulate energy.
 * @param energy_kJ  unit: kilo Joule
 */
void cycling_power_service_add_energy(uint16_t energy_kJ);

/**
 * @brief Set the value of the power. The Instantaneous Power field represents either
 * the total power the user is producing or a part of the total power depending on the
 * type of sensor (e.g., single sensor or distributed power sensor system).
 *
 * @param instantaneous_power_W  unit: watt
 */
void cycling_power_service_server_set_instantaneous_power(int16_t instantaneous_power_W);

/**
 * @brief Set the pedal power balance value. The Pedal Power Balance field represents the ratio between
 * the total amount of power measured by the sensor and a reference (either unknown or left).
 *
 * @param pedal_power_balance_percentage
 */
void cycling_power_service_server_set_pedal_power_balance(uint8_t pedal_power_balance_percentage);

/**
 * @brief Set the minimum and maximum force value measured in a single crank revolution.
 *
 * This, so called, Extreme Force Magnitude field pair will be included in the Cycling Power Measurement
 * characteristic only if the device supports the Extreme Magnitudes feature and
 * the Sensor Measurement Context of the Cycling Power Feature characteristic is set to 0 (Force-based).
 *
 * @param min_force_magnitude_N  unit: newton
 * @param max_force_magnitude_N  unit: newton
 */
void cycling_power_service_server_set_force_magnitude(int16_t min_force_magnitude_N, int16_t max_force_magnitude_N);

/**
 * @brief Set the minimum and maximum torque value measured in a single crank revolution.
 *
 * This, so called, Extreme Torque Magnitude field pair will be included in the Cycling Power Measurement
 * characteristic only if the device supports the Extreme Magnitudes feature and
 * the Sensor Measurement Context of the Cycling Power Feature characteristic is set to 1 (Torque-based).
 *
 * @param min_torque_magnitude_Nm  unit: newton meter
 * @param max_torque_magnitude_Nm  unit: newton meter
 */
void cycling_power_service_server_set_torque_magnitude(int16_t min_torque_magnitude_Nm, int16_t max_torque_magnitude_Nm);

/**
 * @brief Set the minimum and maximum angle of the crank measured in a single crank revolution (unit: degree).
 *
 * This, so called, Extreme Angles Magnitude field pair will be included in the Cycling Power Measurement
 * characteristic only if the device supports the Extreme Angles feature.
 *
 * @param min_angle_degree
 * @param max_angle_degree
 */
void cycling_power_service_server_set_angle(uint16_t min_angle_degree, uint16_t max_angle_degree);

/**
 * @brief Set the value of the crank angle measured when the value of the Instantaneous Power value becomes positive.
 *
 * This field will be included in the Cycling Power Measurement characteristic
 * only if the device supports the Top and Bottom Dead Spot Angles feature.
 *
 * @param top_dead_spot_angle_degree
 */
void cycling_power_service_server_set_top_dead_spot_angle(uint16_t top_dead_spot_angle_degree);

/**
 * @brief Set the value of the crank angle measured when the value of the Instantaneous Power value becomes negative.
 *
 * This field will be included in the Cycling Power Measurement characteristic
 * only if the device supports the Top and Bottom Dead Spot Angles feature.
 *
 * @param bottom_dead_spot_angle_degree
 */
void cycling_power_service_server_set_bottom_dead_spot_angle(uint16_t bottom_dead_spot_angle_degree);

/**
 * @brief Set the raw sensor force magnitude measurements.
 *
 * @param force_magnitude_count
 * @param force_magnitude_N_array  unit: newton
 */
void cycling_power_service_server_set_force_magnitude_values(int force_magnitude_count, int16_t * force_magnitude_N_array);

/**
 * @brief Set the raw sensor torque magnitude measurements.
 *
 * @param force_magnitude_count
 * @param force_magnitude_N_array  unit: newton meter
 */
void cycling_power_service_server_set_torque_magnitude_values(int torque_magnitude_count, int16_t * torque_magnitude_Nm_array);

/**
 * @brief Set the instantaneous measurement direction. The field describes
 * the direction of the Instantaneous Magnitude values the Server measures
 * (e.g., Unknown, Tangential Component, Radial Component, or Lateral Component).
 *
 * @param direction
 */
void cycling_power_service_server_set_instantaneous_measurement_direction(cycling_power_instantaneous_measurement_direction_t direction);

/**
 * Set first crank measurement angle. The value will be present only in the first packet of
 * the Cycling Power Vector notification.
 *
 * @param first_crank_measurement_angle_degree
 */
void cycling_power_service_server_set_first_crank_measurement_angle(uint16_t first_crank_measurement_angle_degree);

/**
 * Get measurement flags bitmask.
 * @return measurement_flags_bitmask, see cycling_power_measurement_flag_t
 */
uint16_t cycling_power_service_measurement_flags(void);

/**
 * Get vector flags bitmask.
 * @return vector_flags_bitmask, see cycling_power_vector_flag_t
 */
uint8_t  cycling_power_service_vector_flags(void);

/**
 * @brief Trigger notification if subscribed
 */
void cycling_power_service_server_update_values(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


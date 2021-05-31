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

/**
 * @title Heart Rate Service Server
 * 
 */

#ifndef HEART_RATE_SERVICE_SERVER_H
#define HEART_RATE_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The heart rate service server provides heart rate measurements via notifications.
 * 
 * Each notification reports the heart rate measurement in beats per minute, and if enabled, 
 * the total energy expended in kilo Joules, as well as RR-intervals in 1/1024 seconds resolution.
 *
 * The Energy Expended field represents the accumulated energy expended
 * in kilo Joules since the last time it was reset. If the maximum value of 65535
 * kilo Joules is reached, it will remain at this value, until a reset command
 * from the client is received.
 *
 * The RR-Interval represents the time between two consecutive R waves in 
 * an Electrocardiogram (ECG) waveform. If needed, the RR-Intervals are sent in
 * multiple notifications.
 *
 * To use with your application, add `#import <heart_rate_service.gatt>` to your .gatt file.
 * After adding it to your .gatt file, you call *heart_rate_server_init(body_sensor_location, energy_expended_supported)*
 * with the intended sensor location, and a flag indicating if energy expanded is supported.
 *
 * If heart rate measurement changes, you can call 
 * *heart_rate_service_server_update_heart_rate_values(heart_rate_bpm, service_sensor_contact_status, rr_interval_count, rr_intervals)*. 
 * This function will trigger sending Notifications if the client enables them.
 * 
 * If energy expanded is supported, you can call *heart_rate_service_add_energy_expended(energy_expended_kJ)* 
 * with the newly expanded energy. The accumulated energy expended value
 * will be emitted with the next heart rate measurement.
 */

/* API_START */

typedef enum {
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_OTHER = 0,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_CHEST,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_WRIST,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_FINGER,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_HAND,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_EAR_LOBE,
	HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_FOOT
} heart_rate_service_body_sensor_location_t;

typedef enum {
	HEART_RATE_SERVICE_SENSOR_CONTACT_UNKNOWN = 0,
	HEART_RATE_SERVICE_SENSOR_CONTACT_UNSUPPORTED,
	HEART_RATE_SERVICE_SENSOR_CONTACT_NO_CONTACT,
	HEART_RATE_SERVICE_SENSOR_CONTACT_HAVE_CONTACT
} heart_rate_service_sensor_contact_status_t;


/**
 * @brief Init Battery Service Server with ATT DB
 * @param body_sensor_location
 * @param energy_expended_supported
 */
void heart_rate_service_server_init(heart_rate_service_body_sensor_location_t body_sensor_location, int energy_expended_supported);


/**
 * @brief Add Energy Expended to the internal accumulator.
 * @param energy_expended_kJ   energy expended in kilo Joules since the last update
 */
void heart_rate_service_add_energy_expended(uint16_t energy_expended_kJ);

/**
 * @brief Update heart rate (unit: beats per minute)
 * @note triggers notifications if subscribed
 * @param heart_rate_bpm 		beats per minute
 * @param contact    
 * @param rr_interval_count 
 * @param rr_intervals      resolution in 1/1024 seconds
 */
void heart_rate_service_server_update_heart_rate_values(uint16_t heart_rate_bpm, 
	heart_rate_service_sensor_contact_status_t contact, int rr_interval_count, uint16_t * rr_intervals);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


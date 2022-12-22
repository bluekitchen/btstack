/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Audio Input Control Service Server
 * 
 */

#ifndef AUDIO_INPUT_CONTROL_SERVICE_SERVER_H
#define AUDIO_INPUT_CONTROL_SERVICE_SERVER_H

#include <stdint.h>

#include "ble/att_db.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Audio Input Control Service allows to query your device's audio input settings such as a Bluetooth 
 * audio stream, microphone, etc. 
 * 
 * To use with your application, add `#import <audio_input_control_service.gatt>` to your .gatt file. 
 *
 */
#define AICS_MAX_NUM_SERVICES 5
#define AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH           30

#define AICS_ERROR_CODE_INVALID_CHANGE_COUNTER          0x80
#define AICS_ERROR_CODE_OPCODE_NOT_SUPPORTED            0x81
#define AICS_ERROR_CODE_MUTE_DISABLED                   0x82
#define AICS_ERROR_CODE_VALUE_OUT_OF_RANGE              0x83
#define AICS_ERROR_CODE_GAIN_MODE_CHANGE_NOT_ALLOWED    0x84

typedef enum {
    AICS_OPCODE_SET_GAIN_SETTING        = 0x01,
    AICS_OPCODE_UMUTE                   = 0x02,
    AICS_OPCODE_MUTE                    = 0x03,
    AICS_OPCODE_SET_MANUAL_GAIN_MODE    = 0x04,
    AICS_OPCODE_SET_AUTOMATIC_GAIN_MODE = 0x05
} aics_opcode_t;

typedef enum {
    AICS_MUTE_MODE_NOT_MUTED = 0,
    AICS_MUTE_MODE_MUTED,
    AICS_MUTE_MODE_DISABLED
} aics_mute_mode_t;

typedef enum {
    AICS_GAIN_MODE_MANUAL_ONLY = 0,
    AICS_GAIN_MODE_AUTOMATIC_ONLY,
    AICS_GAIN_MODE_MANUAL,
    AICS_GAIN_MODE_AUTOMATIC
} aics_gain_mode_t;

typedef enum {
    AICS_AUDIO_INPUT_STATUS_INACTIVE = 0,
    AICS_AUDIO_INPUT_STATUS_ACTIVE
} aics_audio_input_status_t;

typedef enum {
    AICS_AUDIO_INPUT_TYPE_UNSPECIFIED = 0,
    AICS_AUDIO_INPUT_TYPE_BLUETOOTH_AUDIO_STREAM,
    AICS_AUDIO_INPUT_TYPE_MICROPHONE,
    AICS_AUDIO_INPUT_TYPE_ANALOG_INTERFACE,
    AICS_AUDIO_INPUT_TYPE_DIGITAL_INTERFACE,
    AICS_AUDIO_INPUT_TYPE_RADIO,
    AICS_AUDIO_INPUT_TYPE_STREAMING_AUDIO_SOURCE
} aics_audio_input_type_t;

typedef struct {
    int8_t  gain_setting_db;
    aics_mute_mode_t mute_mode;
    aics_gain_mode_t gain_mode;
} aics_audio_input_state_t;

typedef struct {
    uint8_t  gain_settings_units; // 1 unit == 0.1 dB
    int8_t   gain_settings_minimum;
    int8_t   gain_settings_maximum;
} aics_gain_settings_properties_t;

typedef struct {
    aics_audio_input_state_t audio_input_state;
    aics_gain_settings_properties_t gain_settings_properties;

    aics_audio_input_type_t audio_input_type;
    char audio_input_description[AICS_MAX_AUDIO_INPUT_DESCRIPTION_LENGTH];

    btstack_packet_handler_t packet_handler;
} aics_info_t;


/* API_START */
typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t con_handle;

    // service
    uint16_t start_handle;
    uint16_t end_handle;
    uint8_t index;

    aics_info_t * info;

    att_service_handler_t    service_handler;
    btstack_context_callback_registration_t  scheduled_tasks_callback;
    uint8_t scheduled_tasks;

    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE
    uint16_t audio_input_state_value_handle;
    uint8_t  audio_input_state_change_counter;

    uint16_t audio_input_state_client_configuration_handle;
    uint16_t audio_input_state_client_configuration;
    btstack_context_callback_registration_t audio_input_state_callback;

    // ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE
    uint16_t gain_settings_properties_value_handle;
    
    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE
    uint16_t audio_input_type_value_handle;
    
    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS
    uint16_t audio_input_status_value_handle;
    aics_audio_input_status_t audio_input_status;

    uint16_t audio_input_status_client_configuration_handle;
    uint16_t audio_input_status_client_configuration;
    btstack_context_callback_registration_t audio_input_status_callback;

    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT
    uint16_t audio_input_control_value_handle;

    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION
    uint16_t audio_input_description_value_handle;
    uint8_t  audio_input_description_len;

    uint16_t audio_input_description_client_configuration_handle;
    uint16_t audio_input_description_client_configuration;
    btstack_context_callback_registration_t audio_input_description_callback;
    
} audio_input_control_service_server_t;


/**
 * @brief Init Audio Input Control Service Server with ATT DB
 * @param aics service storage
 */
void audio_input_control_service_server_init(audio_input_control_service_server_t * aics);

/**
 * @brief Set audio input state of the AICS service. If successful, all registered clients will be notified of change.
 * @param aics service
 * @param audio_input_state see aics_audio_input_state_t
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS if gain setting is out of a valid range [gain_settings_minimum, gain_settings_maximum] .
 */
uint8_t audio_input_control_service_server_set_audio_input_state(audio_input_control_service_server_t * aics, aics_audio_input_state_t * audio_input_state);

/**
 * @brief Set audio input status of the AICS service. If successful, all registered clients will be notified of change.
 * @param aics service
 * @param audio_input_status see aics_audio_input_status_t
 */
void audio_input_control_service_server_set_audio_input_status(audio_input_control_service_server_t * aics, aics_audio_input_status_t audio_input_status);

/**
 * @brief Set audio input description of the AICS service. If successful, all registered clients will be notified of change.
 * @param aics service
 * @param audio_input_desc
 */
void audio_input_control_service_server_set_audio_input_description(audio_input_control_service_server_t * aics, const char * audio_input_desc);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


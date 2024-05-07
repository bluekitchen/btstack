/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
#include "le-audio/gatt-service/audio_input_control_service_util.h"

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
    
} aics_server_connection_t;


/**
 * @brief Init Audio Input Control Service Server with ATT DB. Following events will be received via server->info->packet_handler:
 * - LEAUDIO_SUBEVENT_AICS_SERVER_MUTE_MODE
 * - LEAUDIO_SUBEVENT_AICS_SERVER_GAIN_MODE
 * - LEAUDIO_SUBEVENT_AICS_SERVER_GAIN_CHANGED
 * - LEAUDIO_SUBEVENT_AICS_SERVER_AUDIO_INPUT_DESC_CHANGED
 * @param connection service storage
 */
void audio_input_control_service_server_init(aics_server_connection_t * connection);

/**
 * @brief Set audio input state of the AICS service. If successful, all registered clients will be notified of change.
 * @param connection service
 * @param audio_input_state see aics_audio_input_state_t
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS if gain setting is out of a valid range [gain_settings_minimum, gain_settings_maximum] .
 */
uint8_t audio_input_control_service_server_set_audio_input_state(aics_server_connection_t * connection, const aics_audio_input_state_t *audio_input_state);

/**
 * @brief Set audio input status of the AICS service. If successful, all registered clients will be notified of change.
 * @param connection service
 * @param audio_input_status see aics_audio_input_status_t
 */
void audio_input_control_service_server_set_audio_input_status(aics_server_connection_t * connection, const aics_audio_input_status_t audio_input_status);

/**
 * @brief Set audio input description of the AICS service. If successful, all registered clients will be notified of change.
 * @param aics service
 * @param audio_input_desc
 */
void audio_input_control_service_server_set_audio_input_description(aics_server_connection_t * aics, const char * audio_input_desc);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


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
 * @title Volume Offset Control Service Server
 * 
 */

#ifndef VOLUME_OFFSET_CONTROL_SERVICE_SERVER_H
#define VOLUME_OFFSET_CONTROL_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Volume Offset Control Service allows to query your device's volume offset settings such as a Bluetooth 
 * audio stream, microphone, etc. 
 * 
 * To use with your application, add `#import <volume_offset_control_service.gatt>` to your .gatt file. 
 *
 */
#define VOCS_MAX_NUM_SERVICES 5
#define VOCS_MAX_AUDIO_OUTPUT_DESCRIPTION_LENGTH          30

#define VOCS_ERROR_CODE_INVALID_CHANGE_COUNTER          0x80
#define VOCS_ERROR_CODE_OPCODE_NOT_SUPPORTED            0x81
#define VOCS_ERROR_CODE_VALUE_OUT_OF_RANGE              0x82

typedef enum {
    VOCS_OPCODE_SET_VOLUME_OFFSET        = 0x01
} vocs_opcode_t;

typedef struct {
    int16_t  volume_offset;
    uint32_t audio_location;
    char     audio_output_description[VOCS_MAX_AUDIO_OUTPUT_DESCRIPTION_LENGTH];

    btstack_packet_handler_t packet_handler;
} vocs_info_t;

/* API_START */
typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t con_handle;

    // service
    uint16_t start_handle;
    uint16_t end_handle;
    uint8_t index;

    vocs_info_t * info;

    att_service_handler_t    service_handler;
    btstack_context_callback_registration_t  scheduled_tasks_callback;
    uint8_t scheduled_tasks;

    // ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE
    uint16_t volume_offset_state_value_handle;
    uint8_t  volume_offset_state_change_counter;

    uint16_t volume_offset_state_client_configuration_handle;
    uint16_t volume_offset_state_client_configuration;
    btstack_context_callback_registration_t volume_offset_state_callback;

    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION
    uint16_t audio_location_value_handle;
   
    uint16_t audio_location_client_configuration_handle;
    uint16_t audio_location_client_configuration;
    btstack_context_callback_registration_t audio_location_callback;

    // ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_OFFSET_CONTROL_POINT
    uint16_t volume_offset_control_point_value_handle;

    // ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION
    uint16_t audio_output_description_value_handle;
    uint8_t audio_output_description_len;

    uint16_t audio_output_description_client_configuration_handle;
    uint16_t audio_output_description_client_configuration;
    btstack_context_callback_registration_t audio_output_description_callback;
} volume_offset_control_service_server_t;


/**
 * @brief Init Volume Offset Control Service Server with ATT DB
 * @param vocs service storage
 */
void volume_offset_control_service_server_init(volume_offset_control_service_server_t * vocs);

/**
 * @brief Set volume offset of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param volume_offset 
 * @return status ERROR_CODE_SUCCESS if successful
 */
uint8_t volume_offset_control_service_server_set_volume_offset(volume_offset_control_service_server_t * vocs, int16_t volume_offset);

/**
 * @brief Set audio location of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param audio_location see VOCS_AUDIO_LOCATION_* defines above
 * @return status ERROR_CODE_SUCCESS if successful
 */
uint8_t volume_offset_control_service_server_set_audio_location(volume_offset_control_service_server_t * vocs, uint32_t audio_location);

/**
 * @brief Set audio output description of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param audio_output_desc
 * @return status ERROR_CODE_SUCCESS if successful
 */
void volume_offset_control_service_server_set_audio_output_description(volume_offset_control_service_server_t * vocs, const char * audio_output_desc);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


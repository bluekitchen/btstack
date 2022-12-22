/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * @title Volume Control Service Server
 * 
 */

#ifndef VOLUME_CONTROL_SERVICE_SERVER_H
#define VOLUME_CONTROL_SERVICE_SERVER_H

#include <stdint.h>

#include "le-audio/gatt-service/audio_input_control_service_server.h"
#include "le-audio/gatt-service/volume_offset_control_service_server.h"

#if defined __cplusplus
extern "C" {
#endif

#define VOLUME_CONTROL_INVALID_CHANGE_COUNTER 0x80
#define VOLUME_CONTROL_OPCODE_NOT_SUPPORTED 0x81

#define VCS_VOLUME_FLAGS_SETTING_PERSISTED_BIT_POS      0

// Volume flags bit position
#define VCS_VOLUME_FLAGS_SETTING_PERSISTED_MASK         (1 << VCS_VOLUME_FLAGS_SETTING_PERSISTED_BIT_POS)

// Volume Setting Persisted
#define VCS_VOLUME_FLAGS_SETTING_PERSISTED_RESET        (0 << VCS_VOLUME_FLAGS_SETTING_PERSISTED_BIT_POS)
#define VCS_VOLUME_FLAGS_SETTING_PERSISTED_USER_SET     (1 << VCS_VOLUME_FLAGS_SETTING_PERSISTED_BIT_POS)

typedef enum {
    VCS_MUTE_OFF = 0,
    VCS_MUTE_ON
} vcs_mute_t;


/**
 * @text The Volume Control Service (VCS) enables a device to expose the controls and state of its audio volume.
 * 
 * To use with your application, add `#import <volume_control_service.gatt>` to your .gatt file. 
 * After adding it to your .gatt file, you call *volume_control_service_server_init()* 
 * 
 * VCS may include zero or more instances of VOCS and zero or more instances of AICS
 * 
 */

/* API_START */

/**
 * @brief Init Volume Control Service Server with ATT DB
 * @param volume_setting        range [0,255]
 * @param mute                  see vcs_mute_t
 * @param volume_change_step
 * @param aics_info_num
 * @param aics_info
 * @param vocs_info_num
 * @param vocs_info
 */
void volume_control_service_server_init(uint8_t volume_setting, vcs_mute_t mute,
    uint8_t aics_info_num, aics_info_t * aics_info, 
    uint8_t vocs_info_num, vocs_info_t * vocs_info);

/**
 * @brief Register callback to receive updates of volume state and volume flags GATTSERVICE_SUBEVENT_VCS_VOLUME_STATE and GATTSERVICE_SUBEVENT_VCS_VOLUME_FLAGS events respectively.
 * @param callback
 */
void volume_control_service_server_register_packet_handler(btstack_packet_handler_t callback);


/**
 * @brief Set volume state.
 * @param volume_setting        range [0,255]
 * @param mute                  see vcs_mute_t
 */
void volume_control_service_server_set_volume_state(uint8_t volume_setting, vcs_mute_t mute);

/**
 * @brief Set volume change step.
 * @param volume_change_step    
 */
void volume_control_service_server_set_volume_change_step(uint8_t volume_change_step);

/**
 * @brief Set mute and gain mode, as well as gain setting of the AICS service identified by aics_index.
 * @param aics_index
 * @param audio_input_state see aics_audio_input_state_t in audio_input_control_service_server.h
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range, or ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS if gain setting is out of valid range.
 */
uint8_t volume_control_service_server_set_audio_input_state_for_aics(uint8_t aics_index, aics_audio_input_state_t * audio_input_state);

/**
 * @brief Set audio input description of the AICS service identified by aics_index.
 * @param aics_index
 * @param audio_input_state see aics_audio_input_state_t in audio_input_control_service_server.h
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range.
 */
uint8_t volume_control_service_server_set_audio_input_description_for_aics(uint8_t aics_index, const char * audio_input_desc);

/**
 * @brief Set audio input status of the AICS service identified by aics_index.
 * @param aics_index
 * @param audio_input_status see aics_audio_input_status_t in audio_input_control_service_server.h
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range.
 */
uint8_t volume_control_service_server_set_audio_input_status_for_aics(uint8_t aics_index, aics_audio_input_status_t audio_input_status);


/**
 * @brief Set volume offset location of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param volume_offset
 * @return ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range.
 */
uint8_t volume_control_service_server_set_volume_offset_for_vocs(uint8_t vocs_index, int16_t volume_offset);

/**
 * @brief Set audio location of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param audio_location see VOCS_AUDIO_LOCATION_* defines in volume_offset_control_service_server.h
 * @return ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range.
 */
uint8_t volume_control_service_server_set_audio_location_for_vocs(uint8_t vocs_index, uint32_t audio_location);

/**
 * @brief Set audio output description of the VOCS service. If successful, all registered clients will be notified of change.
 * @param vocs service
 * @param audio_output_desc
 * @return ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if aics_index is out of range.
 */
uint8_t volume_control_service_server_set_audio_output_description_for_vocs(uint8_t vocs_index, const char * audio_output_desc);


/* API_END */

#if defined __cplusplus
}
#endif

#endif


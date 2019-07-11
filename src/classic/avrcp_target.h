/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

/*
 * avrcp_target.h
 * 
 * Audio/Video Remote Control Profile
 *
 */

#ifndef AVRCP_TARGET_H
#define AVRCP_TARGET_H

#include <stdint.h>
#include "classic/avrcp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef enum {
    AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER = 0,
    AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_MONITOR_OR_AMPLIFIER,
    AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_TUNER,
    AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_MENU,
    AVRCP_TARGET_SUPPORTED_FEATURE_PLAYER_APPLICATION_SETTINGS, // AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER must be 1 for this feature to be set
    AVRCP_TARGET_SUPPORTED_FEATURE_RESERVED_GROUP_NAVIGATION,   // AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER must be 1 for this feature to be set
    AVRCP_TARGET_SUPPORTED_FEATURE_BROWSING,
    AVRCP_TARGET_SUPPORTED_FEATURE_MULTIPLE_MEDIA_PLAYE_APPLICATIONS
} avrcp_target_supported_feature_t;

/**
 * @brief AVRCP Target service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see avrcp_target_supported_feature_t
 * @param service_name
 * @param service_provider_name
 */
void    avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

void    avrcp_target_init(void);

void    avrcp_target_register_packet_handler(btstack_packet_handler_t callback);

uint8_t avrcp_target_connect(bd_addr_t bd_addr, uint16_t * avrcp_cid);

uint8_t avrcp_target_disconnect(uint16_t avrcp_cid);

uint8_t avrcp_target_supported_companies(uint16_t avrcp_cid, uint8_t capabilities_length, uint8_t * capabilities, uint8_t size);
uint8_t avrcp_target_supported_events(uint16_t avrcp_cid, uint8_t capabilities_length, uint8_t * capabilities, uint8_t size);

uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_playback_status_t status); 

void avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t * current_track, uint16_t total_tracks);
uint8_t avrcp_target_set_playback_status(uint16_t avrcp_cid, avrcp_playback_status_t playback_status);
void avrcp_target_set_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id);
void avrcp_target_set_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, const uint8_t * subunit_info_data, uint16_t subunit_info_data_size);

uint8_t avrcp_target_playing_content_changed(uint16_t avrcp_cid);
uint8_t avrcp_target_addressed_player_changed(uint16_t avrcp_cid, uint16_t player_id, uint16_t uid_counter);

uint8_t avrcp_target_battery_status_changed(uint16_t avrcp_cid, avrcp_battery_status_t battery_status);
uint8_t avrcp_target_volume_changed(uint16_t avrcp_cid, uint8_t volume_percentage);
uint8_t avrcp_target_track_changed(uint16_t avrcp_cid, uint8_t * trackID);

// uint8_t avrcp_target_trigger_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);


uint8_t avrcp_target_operation_rejected(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);
uint8_t avrcp_target_operation_accepted(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);
uint8_t avrcp_target_operation_not_implemented(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/* API_END */

 // Used by AVRCP target and AVRCP browsing target
extern avrcp_context_t avrcp_target_context;

#if defined __cplusplus
}
#endif

#endif // AVRCP_TARGET_H

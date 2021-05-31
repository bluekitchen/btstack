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
 * @title AVRCP Target
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
 * @param supported_features 16-bit bitmap, see AVRCP_FEATURE_MASK_* in avrcp.h
 * @param service_name
 * @param service_provider_name
 */
void    avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Set up AVRCP Target service.
 */
void    avrcp_target_init(void);

/**
 * @brief Register callback for the AVRCP Target client. 
 * @param callback
 */
void    avrcp_target_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Select Player that is controlled by Controller
 * @param callback
 * @note Callback should return if selected player is valid
 */
void avrcp_target_register_set_addressed_player_handler(bool (*callback)(uint16_t player_id));

/**
 * @brief Send a list of Company IDs supported by target. 
 * @note  The avrcp_target_packet_handler will receive AVRCP_SUBEVENT_COMPANY_IDS_QUERY event. Use this function to respond.
 * @param avrcp_cid
 * @param num_company_ids
 * @param company_ids
 * @param company_ids_size
 * @returns status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_supported_companies(uint16_t avrcp_cid, uint8_t num_company_ids, const uint8_t *company_ids, uint8_t company_ids_size);

/**
 * @brief Send a list of Events supported by target.
 * @note  The avrcp_target_packet_handler will receive AVRCP_SUBEVENT_EVENT_IDS_QUERY event. Use this function to respond.
 * @param avrcp_cid
 * @param num_event_ids
 * @param event_ids
 * @param event_ids_size
 * @returns status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_supported_events(uint16_t avrcp_cid, uint8_t num_event_ids, const uint8_t *event_ids, uint8_t event_ids_size);

/**
 * @brief Send a play status.
 * @note  The avrcp_target_packet_handler will receive AVRCP_SUBEVENT_PLAY_STATUS_QUERY event. Use this function to respond.
 * @param avrcp_cid
 * @param song_length_ms
 * @param song_position_ms
 * @returns status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_playback_status_t status); 

/**
 * @param Set Now Playing Info that is send to Controller if notifications are enabled
 * @param avrcp_cid
 * @param current_track
 * @param total_tracks
 */
void avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t * current_track, uint16_t total_tracks);

/**
 * @param Set Playing status and send to Controller
 * @param avrcp_cid
 * @param playback_status
 * @return
 */
uint8_t avrcp_target_set_playback_status(uint16_t avrcp_cid, avrcp_playback_status_t playback_status);

/**
 * @param Set Unit Info
 * @param avrcp_cid
 * @param unit_type
 * @param company_id
 */
void avrcp_target_set_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id);

/**
 * @param Set Subunit Info
 * @param avrcp_cid
 * @param subunit_type
 * @param subunit_info_data
 * @param subunit_info_data_size
 */
void avrcp_target_set_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, const uint8_t * subunit_info_data, uint16_t subunit_info_data_size);

/**
 * @param Send Playing Content Changed Notification if enabled
 * @param avrcp_cid
 * @return
 */
uint8_t avrcp_target_playing_content_changed(uint16_t avrcp_cid);

/**
 * @param Send Addressed Player Changed Notification if enabled
 * @param avrcp_cid
 * @param player_id
 * @param uid_counter
 * @return
 */
uint8_t avrcp_target_addressed_player_changed(uint16_t avrcp_cid, uint16_t player_id, uint16_t uid_counter);

/**
 * @param Set Battery Status Changed and send notification if enabled
 * @param avrcp_cid
 * @param battery_status
 * @return
 */
uint8_t avrcp_target_battery_status_changed(uint16_t avrcp_cid, avrcp_battery_status_t battery_status);

/**
 * @param Set Volume and send notification if enabled
 * @param avrcp_cid
 * @param volume_percentage
 * @return
 */
uint8_t avrcp_target_volume_changed(uint16_t avrcp_cid, uint8_t volume_percentage);

/**
 * @param Set Track and send notification if enabled
 * @param avrcp_cid
 * @param trackID
 * @return
 */
uint8_t avrcp_target_track_changed(uint16_t avrcp_cid, uint8_t * trackID);

/**
 * @param Send Operation Rejected message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return
 */
uint8_t avrcp_target_operation_rejected(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/**
 * @param Send Operation Accepted message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return
 */
uint8_t avrcp_target_operation_accepted(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/**
 * @param Send Operation Not Implemented message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return
 */
uint8_t avrcp_target_operation_not_implemented(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/**
 * @brief De-Init AVRCP Browsing Target
 */
void avrcp_target_deinit(void);

/* API_END */

 // Used by AVRCP target and AVRCP browsing target
extern avrcp_context_t avrcp_target_context;

#if defined __cplusplus
}
#endif

#endif // AVRCP_TARGET_H

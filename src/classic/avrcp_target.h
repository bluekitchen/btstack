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
    AVRCP_TARGET_SUPPORTED_FEATURE_GROUP_NAVIGATION,            // AVRCP_TARGET_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER must be 1 for this feature to be set
    AVRCP_TARGET_SUPPORTED_FEATURE_BROWSING,
    AVRCP_TARGET_SUPPORTED_FEATURE_MULTIPLE_MEDIA_PLAYER_APPLICATIONS,
    AVRCP_TARGET_SUPPORTED_FEATURE_COVER_ART,
} avrcp_target_supported_feature_t;

/**
 * @brief AVRCP Target service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVRCP_FEATURE_MASK_* in avrcp.h
 * @param service_name or NULL for default value. Provide "" (empty string) to skip attribute
 * @param service_provider_name or NULL for default value. Provide "" (empty string) to skip attribute
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
 * @brief Register a list of Company IDs supported by target. 
 * @param avrcp_cid
 * @param num_companies
 * @param companies
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_support_companies(uint16_t avrcp_cid, uint8_t num_companies, const uint32_t *companies);

/**
 * @brief Register event ID supported by target.
 * @param avrcp_cid
 * @param event_id 
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE for unsupported event id, otherwise ERROR_CODE_SUCCESS, 
 */
uint8_t avrcp_target_support_event(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);

/**
 * @brief Send a play status.
 * @note  The avrcp_target_packet_handler will receive AVRCP_SUBEVENT_PLAY_STATUS_QUERY event. Use this function to respond.
 * @param avrcp_cid
 * @param song_length_ms
 * @param song_position_ms
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_playback_status_t status); 

/**
 * @brief Set Now Playing Info that is send to Controller if notifications are enabled
 * @param avrcp_cid
 * @param current_track
 * @param total_tracks
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, ERROR_CODE_COMMAND_DISALLOWED if no track is provided, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t * current_track, uint16_t total_tracks);

/**
 * @brief Set Playing status and send to Controller
 * @param avrcp_cid
 * @param playback_status
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_set_playback_status(uint16_t avrcp_cid, avrcp_playback_status_t playback_status);

/**
 * @brief Set Unit Info
 * @param avrcp_cid
 * @param unit_type
 * @param company_id
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_set_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id);

/**
 * @brief Set Subunit Info
 * @param avrcp_cid
 * @param subunit_type
 * @param subunit_info_data
 * @param subunit_info_data_size
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_set_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, const uint8_t * subunit_info_data, uint16_t subunit_info_data_size);

/**
 * @brief Send Playing Content Changed Notification if enabled
 * @param avrcp_cid
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_playing_content_changed(uint16_t avrcp_cid);

/**
 * @brief Send Addressed Player Changed Notification if enabled
 * @param avrcp_cid
 * @param player_id
 * @param uid_counter
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_addressed_player_changed(uint16_t avrcp_cid, uint16_t player_id, uint16_t uid_counter);

/**
 * @brief Set Battery Status Changed and send notification if enabled
 * @param avrcp_cid
 * @param battery_status
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_battery_status_changed(uint16_t avrcp_cid, avrcp_battery_status_t battery_status);

/**
 * @brief Overwrite the absolute volume requested by controller with the actual absolute volume. 
 * This function can only be called on AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED event, which indicates a set absolute volume request by controller. 
 * If the absolute volume requested by controller does not match the granularity of volume control the TG provides, you can use this function to adjust the actual value.
 *
 * @param avrcp_cid
 * @param absolute_volume
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_adjust_absolute_volume(uint16_t avrcp_cid, uint8_t absolute_volume);


/**
 * @brief Set Absolute Volume and send notification if enabled 
 * @param avrcp_cid
 * @param absolute_volume
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_volume_changed(uint16_t avrcp_cid, uint8_t absolute_volume);

/**
 * @brief Set Track and send notification if enabled
 * @param avrcp_cid
 * @param trackID
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_track_changed(uint16_t avrcp_cid, uint8_t * trackID);

/**
 * @brief Send Operation Rejected message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_operation_rejected(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/**
 * @brief Send Operation Accepted message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
 */
uint8_t avrcp_target_operation_accepted(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand);

/**
 * @brief Send Operation Not Implemented message
 * @param avrcp_cid
 * @param opid
 * @param operands_length
 * @param operand
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection is not found, otherwise ERROR_CODE_SUCCESS
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

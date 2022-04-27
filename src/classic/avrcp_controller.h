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

/**
 * @title AVRCP Controller
 *
 */

#ifndef AVRCP_CONTROLLER_H
#define AVRCP_CONTROLLER_H

#include <stdint.h>
#include "btstack_run_loop.h"
#include "btstack_linked_list.h"
#include "classic/avrcp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef enum {
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER = 0,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_MONITOR_OR_AMPLIFIER,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_TUNER,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_MENU,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_RESERVED_4,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_RESERVED_5,
    AVRCP_CONTROLLER_SUPPORTED_FEATURE_BROWSING
} avrcp_controller_supported_feature_t;

/**
 * @brief AVRCP Controller service record.
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVRCP_FEATURE_MASK_* in avrcp.h
 * @param service_name
 * @param service_provider_name
 */
void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Set up AVRCP Controller service.
 */
void avrcp_controller_init(void);

/**
 * @brief Register callback for the AVRCP Controller client.
 * @param callback
 */
void avrcp_controller_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Set max num fragments in whuch message can be transmited.
 * @param avrcp_cid
 * @param max_num_fragments
 * @return status
 */
uint8_t avrcp_controller_set_max_num_fragments(uint16_t avrcp_cid, uint8_t max_num_fragments);


/**
 * @brief Unit info.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_unit_info(uint16_t avrcp_cid);

/**
 * @brief Subunit info.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_subunit_info(uint16_t avrcp_cid);

/**
 * @brief Get capabilities.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_supported_company_ids(uint16_t avrcp_cid);

/**
 * @brief Get supported Events.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_supported_events(uint16_t avrcp_cid);


/**
 * @brief Start continuous cmd (play, pause, volume up, ...). Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_start_press_and_hold_cmd(uint16_t avrcp_cid, avrcp_operation_id_t operation_id);

/**
 * @brief Stops continuous cmd (play, pause, volume up, ...). Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_release_press_and_hold_cmd(uint16_t avrcp_cid);

/**
 * @brief Play. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_play(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_play(uint16_t avrcp_cid);

/**
 * @brief Stop. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_stop(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_stop(uint16_t avrcp_cid);

/**
 * @brief Pause. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_pause(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_pause(uint16_t avrcp_cid);

/**
 * @brief Single step - fast forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_fast_forward(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_fast_forward(uint16_t avrcp_cid);


/**
 * @brief Single step rewind. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_rewind(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_rewind(uint16_t avrcp_cid);

/**
 * @brief Forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_forward(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_forward(uint16_t avrcp_cid);

/**
 * @brief Backward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_backward(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_backward(uint16_t avrcp_cid);

/**
 * @brief Turns the volume to high. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_volume_up(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_volume_up(uint16_t avrcp_cid);
/**
 * @brief Turns the volume to low. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_volume_down(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_volume_down(uint16_t avrcp_cid);

/**
 * @brief Puts the sound out. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_mute(uint16_t avrcp_cid);
uint8_t avrcp_controller_press_and_hold_mute(uint16_t avrcp_cid);

/**
 * @brief Get play status. Returns event of type AVRCP_SUBEVENT_PLAY_STATUS (length, position, play_status).
 * If TG does not support SongLength And SongPosition on TG, then TG shall return 0xFFFFFFFF.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_play_status(uint16_t avrcp_cid);

/**
 * @brief Enable notification. Response via AVRCP_SUBEVENT_NOTIFICATION_STATE.
 * @param avrcp_cid
 * @param event_id
 * @return status
 */
uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);

/**
 * @brief Disable notification. Response via AVRCP_SUBEVENT_NOTIFICATION_STATE.
 * @param avrcp_cid
 * @param event_id
 * @return status
 */
uint8_t avrcp_controller_disable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);

/**
 * @brief Get info on now playing media using subset of attribute IDs
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_element_attributes(uint16_t avrcp_cid, uint8_t num_attributes, avrcp_media_attribute_id_t * attributes);

/**
 * @brief Get info on now playing media using all IDs.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_now_playing_info(uint16_t avrcp_cid);

/**
 * @brief Get info on now playing media using specific media attribute ID.
 * @param media_attribute_id
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_get_now_playing_info_for_media_attribute_id(uint16_t avrcp_cid, avrcp_media_attribute_id_t media_attribute_id);

/**
 * @brief Set absolute volume 0-127 (corresponds to 0-100%). Response via AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_set_absolute_volume(uint16_t avrcp_cid, uint8_t volume);


/**
 * @brief Skip to next playing media. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_skip(uint16_t avrcp_cid);

/**
 * @brief Query repeat and shuffle mode. Response via AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_query_shuffle_and_repeat_modes(uint16_t avrcp_cid);

/**
 * @brief Set shuffle mode. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_set_shuffle_mode(uint16_t avrcp_cid, avrcp_shuffle_mode_t mode);

/**
 * @brief Set repeat mode. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @return status
 */
uint8_t avrcp_controller_set_repeat_mode(uint16_t avrcp_cid, avrcp_repeat_mode_t mode);

/**
 * @brief The PlayItem command starts playing an item indicated by the UID. It is routed to the Addressed Player.
 * @param avrcp_cid
 * @param uid
 * @param uid_counter
 * @param scope
 **/
uint8_t avrcp_controller_play_item_for_scope(uint16_t avrcp_cid, uint8_t * uid, uint16_t uid_counter, avrcp_browsing_scope_t scope);

/**
 * @brief Adds an item indicated by the UID to the Now Playing queue.
 * @param avrcp_cid
 * @param uid
 * @param uid_counter
 * @param scope
 **/
uint8_t avrcp_controller_add_item_from_scope_to_now_playing_list(uint16_t avrcp_cid, uint8_t * uid, uint16_t uid_counter, avrcp_browsing_scope_t scope);

/**
 * @brief Set addressed player.
 * @param avrcp_cid
 * @param addressed_player_id
 */
uint8_t avrcp_controller_set_addressed_player(uint16_t avrcp_cid, uint16_t addressed_player_id);

/**
 * @brief Send custom command
 * @param avrcp_cid
 * @param command_type
 * @param subunit_type
 * @param subunit_id
 * @param pdu_id
 * @param company_id
 * @param data
 * @param data_len
 */
uint8_t avrcp_controller_send_custom_command(uint16_t avrcp_cid,
    avrcp_command_type_t command_type,
    avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id,
    avrcp_pdu_id_t pdu_id, uint32_t company_id,
    const uint8_t * data, uint16_t data_len);

/**
 * @brief De-Init AVRCP Controller
 */
void avrcp_controller_deinit(void);

/* API_END */

// Used by AVRCP controller and AVRCP browsing controller
extern avrcp_context_t avrcp_controller_context;

#if defined __cplusplus
}
#endif

#endif // AVRCP_CONTROLLER_H

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
 * avrcp.h
 * 
 * Audio/Video Remote Control Profile
 *
 */

#ifndef __AVRCP_CONTROLLER_H
#define __AVRCP_CONTROLLER_H

#include <stdint.h>
#include "btstack_run_loop.h"
#include "btstack_linked_list.h"
#include "avrcp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief AVDTP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param browsing  1 - supported, 0 - not supported
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Set up AVDTP Sink device.
 */
void avrcp_controller_init(void);

/**
 * @brief Register callback for the AVRCP Sink client. 
 * @param callback
 */
void avrcp_controller_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a Bluetooth address.
 * @param bd_addr
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_connect(bd_addr_t bd_addr, uint16_t * avrcp_cid);

/**
 * @brief Disconnect from AVRCP target
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_disconnect(uint16_t avrcp_cid);

/**
 * @brief Unit info.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_unit_info(uint16_t avrcp_cid);

/**
 * @brief Get capabilities.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_get_supported_company_ids(uint16_t avrcp_cid);

/**
 * @brief Get supported Events.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_get_supported_events(uint16_t avrcp_cid);

/**
 * @brief Play. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_play(uint16_t avrcp_cid);

/**
 * @brief Stop. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_stop(uint16_t avrcp_cid);

/**
 * @brief Pause. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_pause(uint16_t avrcp_cid);

/**
 * @brief Start Fast Forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_start_fast_forward(uint16_t avrcp_cid);

/**
 * @brief Stop Fast Forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_stop_fast_forward(uint16_t avrcp_cid);

/**
 * @brief Single step - fast forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_fast_forward(uint16_t avrcp_cid);

/**
 * @brief Stop Rewind. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_start_rewind(uint16_t avrcp_cid);

/**
 * @brief Stop Rewind. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_stop_rewind(uint16_t avrcp_cid);

/**
 * @brief Single step rewind. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_rewind(uint16_t avrcp_cid);

/**
 * @brief Forward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_forward(uint16_t avrcp_cid); 

/**
 * @brief Backward. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_backward(uint16_t avrcp_cid);


/**
 * @brief Get play status. Returns event of type AVRCP_SUBEVENT_PLAY_STATUS (length, position, play_status).
 * If TG does not support SongLength And SongPosition on TG, then TG shall return 0xFFFFFFFF.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_get_play_status(uint16_t avrcp_cid);

/**
 * @brief Enable notification. Response via AVRCP_SUBEVENT_ENABLE_NOTIFICATION_COMPLETE.
 * @param avrcp_cid
 * @param event_id
 * @returns status
 */
uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);

/**
 * @brief Disable notification. Response via AVRCP_SUBEVENT_ENABLE_NOTIFICATION_COMPLETE.
 * @param avrcp_cid
 * @param event_id
 * @returns status
 */
uint8_t avrcp_controller_disable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id);

/**
 * @brief Get info on now playing media.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_get_now_playing_info(uint16_t avrcp_cid);

/**
 * @brief Set absolute volume 0-127 (corresponds to 0-100%). Response via AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_set_absolute_volume(uint16_t avrcp_cid, uint8_t volume);

/**
 * @brief Turns the volume to high. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_volume_up(uint16_t avrcp_cid);

/**
 * @brief Turns the volume to low. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_volume_down(uint16_t avrcp_cid);

/**
 * @brief Puts the sound out. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_mute(uint16_t avrcp_cid);

/**
 * @brief Skip to next playing media. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_skip(uint16_t avrcp_cid);

/**
 * @brief Query repeat and shuffle mode. Response via AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_query_shuffle_and_repeat_modes(uint16_t avrcp_cid);

/**
 * @brief Set shuffle mode. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_set_shuffle_mode(uint16_t avrcp_cid, avrcp_shuffle_mode_t mode);

/**
 * @brief Set repeat mode. Event AVRCP_SUBEVENT_OPERATION_COMPLETE returns operation id and status.
 * @param avrcp_cid
 * @returns status
 */
uint8_t avrcp_controller_set_repeat_mode(uint16_t avrcp_cid, avrcp_repeat_mode_t mode);

/* API_END */
#if defined __cplusplus
}
#endif

#endif // __AVRCP_CONTROLLER_H
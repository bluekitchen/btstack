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
 * @title Media Control Service Server
 * 
 */

#ifndef MEDIA_CONTROL_SERVICE_SERVER_H
#define MEDIA_CONTROL_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/media_control_service_util.h"
#include "le-audio/gatt-service/object_transfer_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Media Control Service  provides the client with the ability to control and interact with media players.
 * 
 * To use with your application, add `#import <media_control_service.gatt>` to your .gatt file. 
 */

#define MEDIA_CONTROL_SERVICE_MEDIA_PLAYER_NAME_MAX_LENGTH          50
#define MEDIA_CONTROL_SERVICE_ICON_URL_MAX_LENGTH                   50
#define MEDIA_CONTROL_SERVICE_TRACK_TITLE_MAX_LENGTH                50

typedef enum {
    MEDIA_PLAYER_NAME = 0,                    
    MEDIA_PLAYER_ICON_OBJECT_ID,
    MEDIA_PLAYER_ICON_URL,
    TRACK_CHANGED,
    TRACK_TITLE,
    TRACK_DURATION,                     // 5
    TRACK_POSITION,
    PLAYBACK_SPEED,
    SEEKING_SPEED,
    CURRENT_TRACK_SEGMENTS_OBJECT_ID,
    CURRENT_TRACK_OBJECT_ID,            // 10
    NEXT_TRACK_OBJECT_ID,
    PARENT_GROUP_OBJECT_ID,
    CURRENT_GROUP_OBJECT_ID,
    PLAYING_ORDER,
    PLAYING_ORDERS_SUPPORTED,           // 15
    MEDIA_STATE,
    MEDIA_CONTROL_POINT,
    MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
    SEARCH_RESULTS_OBJECT_ID,
    SEARCH_CONTROL_POINT,
    CONTENT_CONTROL_ID,
    NUM_MCS_CHARACTERISTICS,
} msc_characteristic_id_t;

typedef enum {
    MCS_MEDIA_STATE_INACTIVE = 0,
    MCS_MEDIA_STATE_PLAYING,
    MCS_MEDIA_STATE_PAUSED,
    MCS_MEDIA_STATE_SEEKING,
    MCS_MEDIA_STATE_RFU
} mcs_media_state_t;


typedef struct {
    // uint8_t * data;
    // uint16_t  len;
    uint16_t  value_handle;
    uint16_t  client_configuration_handle;
    uint16_t  client_configuration;
} msc_characteristic_t;


/* API_START */
typedef struct {
    char * name;
    bool name_value_changed;
    
    char track_title[MEDIA_CONTROL_SERVICE_TRACK_TITLE_MAX_LENGTH];
    bool track_title_value_changed;

    char icon_url[MEDIA_CONTROL_SERVICE_ICON_URL_MAX_LENGTH];
    
    uint32_t track_duration_10ms;               // 0xFFFFFFFF unknown, or not set
    
    uint32_t track_position_10ms;               // 0xFFFFFFFF unknown, or not set

    int8_t playback_speed;
    int8_t seeking_speed;

    ots_object_id_t icon_object_id;
    ots_object_id_t current_track_segments_object_id;
    ots_object_id_t current_track_object_id;
    ots_object_id_t next_track_object_id;
    ots_object_id_t parent_group_object_id;
    ots_object_id_t current_group_object_id;
    
    uint8_t content_control_id;

    ots_object_id_t search_results_object_id;
    uint8_t search_results_object_id_len;
    
    playing_order_t playing_order;
    uint16_t playing_orders_supported;

    uint32_t media_control_point_opcodes_supported;
    media_control_point_opcode_t      media_control_point_requested_opcode;
    media_control_point_error_code_t  media_control_point_result_code;
    uint8_t media_control_point_data[4];
    uint8_t media_control_poind_data_length;

    search_control_point_error_code_t search_control_point_result_code;

    mcs_media_state_t media_state;
} mcs_media_player_data_t;


typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t            con_handle;
    
    att_service_handler_t       service;
    uint16_t                    player_id;
    msc_characteristic_t        characteristics[NUM_MCS_CHARACTERISTICS];

    uint32_t                    scheduled_tasks;
    btstack_context_callback_registration_t scheduled_tasks_callback; 

    btstack_packet_handler_t event_callback;
    mcs_media_player_data_t  data;
    btstack_timer_source_t   seeking_speed_timer;
} media_control_service_server_t;

typedef struct {
    uint32_t duration_10ms;               // 0xFFFFFFFF unknown, or not set
    uint32_t position_10ms;               // 0xFFFFFFFF unknown, or not set
    ots_object_id_t object_id;
    char * title;
} msc_track_segment_t;

typedef struct {
    // TODO make const
    uint32_t track_duration_10ms;               // 0xFFFFFFFF unknown, or not set
    // TODO move global for current track
    uint32_t track_position_10ms;               // 0xFFFFFFFF unknown, or not set

    ots_object_id_t object_id;

    char * title;
    
    ots_object_id_t icon_object_id;
    char * icon_url;

    ots_object_id_t segments_object_id;
    uint16_t segments_num;
    msc_track_segment_t segments[15];
} mcs_track_t;

/**
 * @brief Init media Control Service Server with ATT DB
 */

/**
 * @brief Initialize Media Control Service (MCS) Server
 */
void media_control_service_server_init(void);

/**
 * @brief Register generic media player
 * @param generic_player
 * @param packet_handler
 * @param media_control_point_opcodes_supported
 * @param out_generic_player_id
 * @return ERROR_CODE_SUCCESS on success, otherwise:
 *       - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no is service found
 *       - ERROR_CODE_REPEATED_ATTEMPTS if media player is already registered
 */
uint8_t media_control_service_server_register_generic_player(
    media_control_service_server_t * generic_player,
    btstack_packet_handler_t packet_handler, 
    uint32_t media_control_point_opcodes_supported, 
    uint16_t * out_generic_player_id);

/**
 * @brief Register media player
 * @param player
 * @param packet_handler
 * @param media_control_point_opcodes_supported
 * @param out_player_id
 * @return ERROR_CODE_SUCCESS on success, otherwise:
 *       - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no is service found
 *       - ERROR_CODE_REPEATED_ATTEMPTS if media player is already registered
 */
uint8_t media_control_service_server_register_player(
    media_control_service_server_t * player,
    btstack_packet_handler_t packet_handler, 
    uint32_t media_control_point_opcodes_supported, 
    uint16_t * out_player_id);

/**
 *
 * @param media_player_id
 * @param track
 * @return
 */
uint8_t media_control_service_server_update_current_track_info(uint16_t media_player_id, mcs_track_t * track);

/**
 *
 * @param media_player_id
 * @param name
 * @return
 */
uint8_t media_control_service_server_set_media_player_name(uint16_t media_player_id, char * name);

/**
 *
 * @param media_player_id
 * @param icon_object_id
 * @return
 */
uint8_t media_control_service_server_set_icon_object_id(uint16_t media_player_id, const ots_object_id_t * icon_object_id);

/**
 *
 * @param media_player_id
 * @param icon_url
 * @return
 */
uint8_t media_control_service_server_set_icon_url(uint16_t media_player_id, const char * icon_url);

/**
 *
 * @param media_player_id
 * @param object_id
 * @return
 */
uint8_t media_control_service_server_set_parent_group_object_id(uint16_t media_player_id, const ots_object_id_t * object_id);

/**
 *
 * @param media_player_id
 * @param object_id
 * @return
 */
uint8_t media_control_service_server_set_current_group_object_id(uint16_t media_player_id, const ots_object_id_t * object_id);

/**
 *
 * @param media_player_id
 * @param object_id
 * @return
 */
uint8_t media_control_service_server_set_current_track_id(uint16_t media_player_id, const ots_object_id_t * object_id);

/**
 *
 * @param media_player_id
 * @param object_id
 * @return
 */
uint8_t media_control_service_server_set_next_track_id(uint16_t media_player_id, const ots_object_id_t * object_id);

/**
 *
 * @param media_player_id
 * @param object_id
 * @return
 */
uint8_t media_control_service_server_set_current_track_segments_id(uint16_t media_player_id, const ots_object_id_t * object_id);

/**
 *
 * @param media_player_id
 * @return
 */
uint8_t media_control_service_server_set_media_track_changed(uint16_t media_player_id);

/**
 *
 * @param media_player_id
 * @param track_title
 * @return
 */
uint8_t media_control_service_server_set_track_title(uint16_t media_player_id, const char * track_title);

/**
 *
 * @param media_player_id
 * @param track_duration_10ms
 * @return
 */
uint8_t media_control_service_server_set_track_duration(uint16_t media_player_id, uint32_t track_duration_10ms);

/**
 *
 * @param media_player_id
 * @param track_position_10ms
 * @return
 */
uint8_t media_control_service_server_set_track_position(uint16_t media_player_id, int32_t track_position_10ms);

/**
 *
 * @param media_player_id
 * @param playback_speed
 * @return
 */
uint8_t media_control_service_server_set_playback_speed(uint16_t media_player_id, int8_t playback_speed);

/**
 *
 * @param media_player_id
 * @param seeking_speed
 * @return
 */
uint8_t media_control_service_server_set_seeking_speed( uint16_t media_player_id, int8_t seeking_speed);

/**
 *
 * @param media_player_id
 * @param playing_orders_supported
 * @return
 */
uint8_t media_control_service_server_set_playing_orders_supported(uint16_t media_player_id, uint16_t playing_orders_supported);

/**
 *
 * @param media_player_id
 * @param playing_order
 * @return
 */
uint8_t media_control_service_server_support_playing_order(uint16_t media_player_id, playing_order_t playing_order);

/**
 *
 * @param media_player_id
 * @param playing_order
 * @return
 */
uint8_t media_control_service_server_set_playing_order(uint16_t media_player_id, playing_order_t playing_order);

/**
 *
 * @param media_player_id
 * @return
 */
mcs_media_state_t media_control_service_server_get_media_state(uint16_t media_player_id);

/**
 *
 * @param media_player_id
 * @param media_state
 * @return
 */
uint8_t media_control_service_server_set_media_state(uint16_t media_player_id, mcs_media_state_t media_state);

/**
 *
 * @param media_player
 * @return
 */
uint8_t media_control_service_server_unregister_media_player(media_control_service_server_t * media_player);

/**
 *
 * @param media_player_id
 * @param media_control_point_opcode
 * @param media_control_point_result_code
 * @return
 */
uint8_t media_control_service_server_respond_to_media_control_point_command(
    uint16_t media_player_id, 
    media_control_point_opcode_t      media_control_point_opcode,
    media_control_point_error_code_t  media_control_point_result_code);

uint8_t media_control_service_server_search_control_point_response(
    uint16_t media_player_id, 
    uint8_t * search_results_object_id);

char * mcs_server_media_control_opcode2str(media_control_point_opcode_t opcode);
char * mcs_server_media_state2str(mcs_media_state_t media_state);
char * mcs_server_characteristic2str(msc_characteristic_id_t msc_characteristic);
/* API_END */

#if defined __cplusplus
}
#endif

#endif


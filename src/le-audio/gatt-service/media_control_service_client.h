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
 * @title Media Control Service Client
 * 
 */

#ifndef MEDIA_CONTROL_SERVICE_CLIENT_H
#define MEDIA_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/media_control_service_util.h"
#include "le-audio/gatt-service/object_transfer_service_client.h"

#if defined __cplusplus
extern "C" {
#endif

// number of characteristics in Media Control Service
#define MEDIA_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS 22

/** 
 * @text The Media Control Service Client 
 */
typedef enum {
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_IDLE = 0,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICE_CONNECTED,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_READY,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE    
} media_service_client_state_t;

typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    media_service_client_state_t state;
    ots_client_connection_t ots_connection;
    uint8_t ots_connections_num;

    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic
    union {
        uint8_t  data_8;
        uint16_t data_16;
        uint32_t data_32;
        const char * data_string;
    } data;

    // Used to store param of media control point command
    int32_t  media_control_command_param;

    uint8_t write_buffer[MCS_SEARCH_CONTROL_POINT_COMMAND_MAX_LENGTH];
    uint8_t write_buffer_length;
} mcs_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Media Control Service. 
 */
void media_control_service_client_init(void);

 /**
  * @brief Connect to Generic Media Control Service of remote device. The client will automatically register for notifications.
  * The mute state is received via LEAUDIO_SUBEVENT_MCS_CLIENT_MUTE event.
  * The mute state can be 0 - off, 1 - on, 2 - disabeled and it is valid if the ATT status is equal to ATT_ERROR_SUCCESS,
  * see ATT errors (see bluetooth.h) for other values.
  *
  * Event LEAUDIO_SUBEVENT_MCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
  * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h).
  *
  * @param con_handle
  * @param connection
  * @param packet_handler
  * @param characteristics       storage for characteristics
  * @param characteristics_num >= MEDIA_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS
  * @param mcs_cid
  * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
  */
 uint8_t media_control_service_client_connect_generic_player(
     hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler,
     mcs_client_connection_t * connection,
     gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
     uint16_t * mcs_cid);

 /**
 * @brief Connect to a Media Control Service instance of remote device. The client will automatically register for notifications.
 * The mute state is received via LEAUDIO_SUBEVENT_MCS_CLIENT_MUTE event.
 * The mute state can be 0 - off, 1 - on, 2 - disabeled and it is valid if the ATT status is equal to ATT_ERROR_SUCCESS,
 * see ATT errors (see bluetooth.h) for other values.
 *   
 * Event LEAUDIO_SUBEVENT_MCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param service_index         index o media player to connect to
 * @param packet_handler
 * @param connection
 * @param characteristics       storage for characteristics
 * @param characteristics_num >= MEDIA_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS
 * @param mcs_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t media_control_service_client_connect_media_player(
    hci_con_handle_t con_handle, uint8_t service_index, btstack_packet_handler_t packet_handler,
    mcs_client_connection_t * connection,
    gatt_service_client_characteristic_t * characteristics, uint8_t characteristics_num,
    uint16_t * mcs_cid);

/**
 * @brief Get the name of the media player application that is providing media tracks. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_NAME event.  
 * If the name size is greater than ATT_MTU-3, then the first ATT_MTU-3 octets shall be returned. 
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_media_player_name(uint16_t mcs_cid);

/**
 * @brief Get the media player icon object id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_OBJECT_ID event.  
 * If the name size is greater than ATT_MTU-3, then the first ATT_MTU-3 octets shall be returned. 
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_media_player_icon_object_id(uint16_t mcs_cid);

/**
 * @brief Get the media player icon object url. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_PLAYER_ICON_URI event.  
 * If the name size is greater than ATT_MTU-3, then the first ATT_MTU-3 octets shall be returned. 
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_media_player_icon_uri(uint16_t mcs_cid);

/**
 * @brief Get the track title. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_TITLE event.  
 * If the name size is greater than ATT_MTU-3, then the first ATT_MTU-3 octets shall be returned. 
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_track_title(uint16_t mcs_cid);

/**
 * @brief Get the track duration. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_DURATION event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_track_duration(uint16_t mcs_cid);

/**
 * @brief Get the track position. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_TRACK_POSITION event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_track_position(uint16_t mcs_cid);

/**
 * @brief Get the playback speed. 
 * The value is reported via the LEAUDIO_SUBEVENT_MSC_CLIENT_PLAYBACK_SPEED event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_playback_speed(uint16_t mcs_cid);

/**
 * @brief Get the seeking speed. 
 * The value is reported via the LEAUDIO_SUBEVENT_MSC_CLIENT_SEEKING_SPEED event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_seeking_speed(uint16_t mcs_cid);

/**
 * @brief Get current track segments object id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_SEGMENTS_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_current_track_segments_object_id(uint16_t mcs_cid);

/**
 * @brief Get the current track object id.  
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_TRACK_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_current_track_object_id(uint16_t mcs_cid);

/**
 * @brief Get the next track object id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_NEXT_TRACK_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_next_track_object_id(uint16_t mcs_cid);

/**
 * @brief Get the parent group object id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_PARENT_GROUP_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_parent_group_object_id(uint16_t mcs_cid);

/**
 * @brief Get the current group object id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_CURRENT_GROUP_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_current_group_object_id(uint16_t mcs_cid);

/**
 * @brief Get the playing order. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_PLAYING_ORDER event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_playing_order(uint16_t mcs_cid);

/**
 * @brief Get the playing orders supported. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_PLAYING_ORDER_SUPPORTED event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_playing_orders_supported(uint16_t mcs_cid);

/**
 * @brief Get the media state. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_MEDIA_STATE event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_media_state(uint16_t mcs_cid);

/**
 * @brief Get the supported media control point opcodes. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_CONTROL_POINT_OPCODES_SUPPORTED event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_media_control_point_opcodes_supported(uint16_t mcs_cid);

/**
 * @brief Get the name of the media player application that is providing media tracks. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_SEARCH_RESULT_OBJECT_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_search_results_object_id(uint16_t mcs_cid);

/**
 * @brief Get the content control id. 
 * The value is reported via the LEAUDIO_SUBEVENT_MCS_CLIENT_CONTENT_CONTROL_ID event.  
 *
 * @param mcs_cid
 * @return status  ERROR_CODE_SUCCESS on success, otherwise:
 *      - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if there is no client with mcs_cid,
 *      - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous queries, or
 *      - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if the corresponding characteristic is not found on server side.
 */
uint8_t media_control_service_client_get_content_control_id(uint16_t mcs_cid);


uint8_t media_control_service_client_set_track_position(uint16_t mcs_cid, uint32_t position_10ms);
uint8_t media_control_service_client_set_playback_speed(uint16_t mcs_cid, uint16_t playback_speed);
uint8_t media_control_service_client_set_current_track_object_id(uint16_t mcs_cid, const char * object_id);
uint8_t media_control_service_client_set_next_track_object_id(uint16_t mcs_cid, const char * object_id);
uint8_t media_control_service_client_set_current_group_object_id(uint16_t mcs_cid, const char * object_id);
uint8_t media_control_service_client_set_playing_order(uint16_t mcs_cid, uint8_t playing_order);

uint8_t media_control_service_client_command_play(uint16_t mcs_cid);
uint8_t media_control_service_client_command_pause(uint16_t mcs_cid);
uint8_t media_control_service_client_command_fast_rewind(uint16_t mcs_cid);
uint8_t media_control_service_client_command_fast_forward(uint16_t mcs_cid);
uint8_t media_control_service_client_command_stop(uint16_t mcs_cid);
uint8_t media_control_service_client_command_move_relative(uint16_t mcs_cid, int32_t track_position_offset_10ms);
uint8_t media_control_service_client_command_previous_segment(uint16_t mcs_cid);
uint8_t media_control_service_client_command_next_segment(uint16_t mcs_cid);
uint8_t media_control_service_client_command_first_segment(uint16_t mcs_cid);
uint8_t media_control_service_client_command_last_segment(uint16_t mcs_cid);
uint8_t media_control_service_client_command_goto_segment(uint16_t mcs_cid, int32_t segment_nr);
uint8_t media_control_service_client_command_previous_track(uint16_t mcs_cid);
uint8_t media_control_service_client_command_next_track(uint16_t mcs_cid);
uint8_t media_control_service_client_command_first_track(uint16_t mcs_cid);
uint8_t media_control_service_client_command_last_track(uint16_t mcs_cid);
uint8_t media_control_service_client_command_goto_track(uint16_t mcs_cid, int32_t track_nr);
uint8_t media_control_service_client_command_previous_group(uint16_t mcs_cid);
uint8_t media_control_service_client_command_next_group(uint16_t mcs_cid);
uint8_t media_control_service_client_command_first_group(uint16_t mcs_cid);
uint8_t media_control_service_client_command_last_group(uint16_t mcs_cid);
uint8_t media_control_service_client_command_goto_group(uint16_t mcs_cid, int32_t group_nr);


uint8_t media_control_service_client_search_control_command_init(uint16_t mcs_cid);
uint8_t media_control_service_client_search_control_command_add(uint16_t mcs_cid, search_control_point_type_t type, const char * data);
uint8_t media_control_service_client_search_control_command_execute(uint16_t mcs_cid);

/**
 * @brief Disconnect.
 * @param mcs_cid
 * @return status
 */
uint8_t media_control_service_client_disconnect(uint16_t mcs_cid);

/**
 * @brief De-initialize Media Control Service. 
 */
void media_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

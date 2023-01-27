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

#if defined __cplusplus
extern "C" {
#endif

#define ATT_ERROR_RESPONSE_MEDIA_CONTROL_VALUE_CHANGED_DURING_READ_LONG    0x80

/**
 * @text The Media Control Service  provides the client with the ability to control and interact with media players.
 * 
 * To use with your application, add `#import <media_control_service.gatt>` to your .gatt file. 
 */

#define MEDIA_CONTROL_SERVICE_MEDIA_PLAYER_NAME_MAX_LENGTH          20
#define MEDIA_CONTROL_SERVICE_ICON_URL_MAX_LENGTH                   30
#define MEDIA_CONTROL_SERVICE_TRACK_TITLE_MAX_LENGTH                30

typedef enum {
    MEDIA_PLAYER_NAME = 0,                    
    MEDIA_PLAYER_ICON_OBJECT_ID,
    MEDIA_PLAYER_ICON_URL,
    TRACK_CHANGED,
    TRACK_TITLE,
    TRACK_DURATION,
    TRACK_POSITION,
    PLAYBACK_SPEED,
    SEEKING_SPEED,
    CURRENT_TRACK_SEGMENTS_OBJECT_ID,
    CURRENT_TRACK_OBJECT_ID,
    NEXT_TRACK_OBJECT_ID,
    PARENT_GROUP_OBJECT_ID,
    CURRENT_GROUP_OBJECT_ID,
    PLAYING_ORDER,
    PLAYING_ORDERS_SUPPORTED,
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
    char name[MEDIA_CONTROL_SERVICE_MEDIA_PLAYER_NAME_MAX_LENGTH];

    uint8_t icon_object_id_len;
    uint8_t icon_object_id[6];

    char icon_url[MEDIA_CONTROL_SERVICE_ICON_URL_MAX_LENGTH];
    char track_title[MEDIA_CONTROL_SERVICE_TRACK_TITLE_MAX_LENGTH];

    uint32_t track_duration_10ms;        // 0xFFFFFFFF unknown, or not set
    
    uint32_t track_position_10ms;        // 0xFFFFFFFF unknown, or not set
    uint32_t track_position_offset_10ms;        // 0xFFFFFFFF unknown, or not set

    int8_t playback_speed;
    int8_t seeking_speed;

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

    mcs_media_player_data_t data;
} media_control_service_server_t;

/**
 * @brief Init media Control Service Server with ATT DB
 */
void media_control_service_server_init(void);

void media_control_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

uint8_t media_control_service_server_register_media_player(media_control_service_server_t * media_player, uint16_t * media_player_id);

uint8_t media_control_service_server_set_media_player_name(uint16_t media_player_id, char * name);

uint8_t media_control_service_server_set_icon_object_id(uint16_t media_player_id, const uint8_t * icon_object_id, uint8_t icon_object_id_len);

uint8_t media_control_service_server_set_icon_url(uint16_t media_player_id, const char * icon_url);

uint8_t media_control_service_server_set_media_track_changed(uint16_t media_player_id);

uint8_t media_control_service_server_set_track_title(uint16_t media_player_id, const char * track_title);

uint8_t media_control_service_server_set_track_duration(uint16_t media_player_id, uint32_t track_duration_10ms);
uint8_t media_control_service_server_set_track_position_offset(uint16_t media_player_id, int32_t track_position_offset_10ms);
uint8_t media_control_service_server_set_playback_speed(uint16_t media_player_id, int8_t playback_speed);
uint8_t media_control_service_server_set_seeking_speed( uint16_t media_player_id, int8_t seeking_speed);

uint8_t media_control_service_server_set_media_state(uint16_t media_player_id, mcs_media_state_t media_state);

uint8_t media_control_service_server_unregister_media_player(media_control_service_server_t * media_player);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


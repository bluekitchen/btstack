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

#ifndef __AVRCP_H
#define __AVRCP_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define BT_SIG_COMPANY_ID 0x001958
/* API_START */

typedef enum {
    AVRCP_CAPABILITY_ID_COMPANY = 0x02,
    AVRCP_CAPABILITY_ID_EVENT = 0x03
} avrcp_capability_id_t;

typedef enum {
    AVRCP_MEDIA_ATTR_TITLE = 1,
    AVRCP_MEDIA_ATTR_ARTIST,
    AVRCP_MEDIA_ATTR_ALBUM,
    AVRCP_MEDIA_ATTR_TOTAL_TRACKS,
    AVRCP_MEDIA_ATTR_GENRE,
    AVRCP_MEDIA_ATTR_SONG_LENGTH
} avrcp_media_attribute_id_t;

typedef enum {
    AVRCP_PDU_ID_GET_CAPABILITIES = 0x10,
    AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES = 0x20,
    AVRCP_PDU_ID_GET_PLAY_STATUS = 0x30,
    AVRCP_PDU_ID_REGISTER_NOTIFICATION = 0x31,
    AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME = 0x50
} avrcp_pdu_id_t;

typedef enum {
    AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED = 0x01,            // Change in playback status of the current track.
    AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED = 0x02,                      // Change of current track
    AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END = 0x03,                  // Reached end of a track
    AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START = 0x04,                // Reached start of a track
    AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED = 0x05,               // Change in playback position. Returned after the specified playback notification change notification interval
    AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED = 0x06,                // Change in battery status
    AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED = 0x07,              // Change in system status
    AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED = 0x08, // Change in player application setting
    AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED = 0x09,        // The content of the Now Playing list has changed, see 6.9.5.
    AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED = 0x0a,          // The available players have changed, see 6.9.4.
    AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED = 0x0b,           // The Addressed Player has been changed, see 6.9.2.
    AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED = 0x0c,                       // The UIDs have changed, see 6.10.3.3.
    AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED = 0x0d                      // The volume has been changed locally on the TG, see 6.13.3.
} avrcp_notification_event_id_t;

typedef enum {
    AVRCP_CTYPE_CONTROL = 0,
    AVRCP_CTYPE_STATUS,
    AVRCP_CTYPE_SPECIFIC_INQUIRY,
    AVRCP_CTYPE_NOTIFY,
    AVRCP_CTYPE_GENERAL_INQUIRY,
    AVRCP_CTYPE_RESERVED5,
    AVRCP_CTYPE_RESERVED6,
    AVRCP_CTYPE_RESERVED7,
    AVRCP_CTYPE_RESPONSE_NOT_IMPLEMENTED = 8,
    AVRCP_CTYPE_RESPONSE_ACCEPTED,
    AVRCP_CTYPE_RESPONSE_REJECTED,
    AVRCP_CTYPE_RESPONSE_IN_TRANSITION, // target state is in transition. A subsequent STATUS command, may result in the return of a STABLE status
    AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE,
    AVRCP_CTYPE_RESPONSE_CHANGED_STABLE,
    AVRCP_CTYPE_RESPONSE_RESERVED,
    AVRCP_CTYPE_RESPONSE_INTERIM            // target is unable to respond with either ACCEPTED or REJECTED within 100 millisecond
} avrcp_command_type_t;

// control command response: accepted, rejected, interim
// status command response: not implemented, rejected, in transiiton, stable
// notify command response: not implemented, rejected, changed
typedef enum {
    AVRCP_SUBUNIT_TYPE_MONITOR = 0,
    AVRCP_SUBUNIT_TYPE_AUDIO = 1,
    AVRCP_SUBUNIT_TYPE_PRINTER,
    AVRCP_SUBUNIT_TYPE_DISC,
    AVRCP_SUBUNIT_TYPE_TAPE_RECORDER_PLAYER,
    AVRCP_SUBUNIT_TYPE_TUNER,
    AVRCP_SUBUNIT_TYPE_CA,
    AVRCP_SUBUNIT_TYPE_CAMERA,
    AVRCP_SUBUNIT_TYPE_RESERVED,
    AVRCP_SUBUNIT_TYPE_PANEL = 9,
    AVRCP_SUBUNIT_TYPE_BULLETIN_BOARD,
    AVRCP_SUBUNIT_TYPE_CAMERA_STORAGE,
    AVRCP_SUBUNIT_TYPE_VENDOR_UNIQUE = 0x1C,
    AVRCP_SUBUNIT_TYPE_RESERVED_FOR_ALL_SUBUNIT_TYPES,
    AVRCP_SUBUNIT_TYPE_EXTENDED_TO_NEXT_BYTE, // The unit_type field may take value 1E16, which means that the field is extended to the following byte. In that case, an additional byte for extended_unit_type will be added immediately following operand[1].
                                              // Further extension is possible when the value of extended_unit_type is FF16, in which case another byte will be added.
    AVRCP_SUBUNIT_TYPE_UNIT = 0x1F
} avrcp_subunit_type_t;

typedef enum {
    AVRCP_SUBUNIT_ID = 0,
    AVRCP_SUBUNIT_ID_IGNORE = 7
} avrcp_subunit_id_t;

typedef enum {
    AVRCP_CMD_OPCODE_VENDOR_DEPENDENT = 0x00,
    AVRCP_CMD_OPCODE_RESERVE = 0x01,
    AVRCP_CMD_OPCODE_UNIT_INFO = 0x30,
    AVRCP_CMD_OPCODE_SUBUNIT_INFO = 0x31,
    AVRCP_CMD_OPCODE_PASS_THROUGH = 0x7C,
    AVRCP_CMD_OPCODE_VERSION = 0xB0,
    AVRCP_CMD_OPCODE_POWER = 0xB2
} avrcp_command_opcode_t;

typedef enum {
    AVRCP_OPERATION_ID_SKIP = 0x3C,
    AVRCP_OPERATION_ID_VOLUME_UP = 0x41,
    AVRCP_OPERATION_ID_VOLUME_DOWN = 0x42,
    AVRCP_OPERATION_ID_MUTE = 0x43,
    
    AVRCP_OPERATION_ID_PLAY = 0x44,
    AVRCP_OPERATION_ID_STOP = 0x45,
    AVRCP_OPERATION_ID_PAUSE = 0x46,
    AVRCP_OPERATION_ID_REWIND = 0x48,
    AVRCP_OPERATION_ID_FAST_FORWARD = 0x49,
    AVRCP_OPERATION_ID_FORWARD = 0x4B,
    AVRCP_OPERATION_ID_BACKWARD = 0x4C
} avrcp_operation_id_t;

typedef enum {
    AVCTP_CONNECTION_IDLE,
    AVCTP_CONNECTION_W4_L2CAP_CONNECTED,
    AVCTP_CONNECTION_OPENED,
    AVCTP_W2_SEND_PRESS_COMMAND,
    AVCTP_W2_SEND_RELEASE_COMMAND,
    AVCTP_W4_STOP,
    AVCTP_W2_SEND_COMMAND,
    AVCTP_W2_RECEIVE_PRESS_RESPONSE,
    AVCTP_W2_RECEIVE_RESPONSE,
    AVCTP_CONNECTION_W4_L2CAP_DISCONNECTED
} avctp_connection_state_t;

typedef struct {
    btstack_linked_item_t    item;
    bd_addr_t remote_addr;
    hci_con_handle_t con_handle;
    uint16_t l2cap_signaling_cid;
    
    avctp_connection_state_t state;
    uint8_t wait_to_send;

    // command
    uint8_t transaction_label;
    avrcp_command_opcode_t cmd_to_send;
    avrcp_command_type_t command_type;
    avrcp_subunit_type_t subunit_type;
    avrcp_subunit_id_t   subunit_id;
    uint8_t cmd_operands[20];
    uint8_t cmd_operands_lenght;
    btstack_timer_source_t press_and_hold_cmd_timer;

    uint16_t notifications_enabled;
    uint16_t notifications_to_register;
    uint16_t notifications_to_deregister; 
} avrcp_connection_t;

 typedef enum {
    AVRCP_PLAY_STATUS_STOPPED = 0x00,
    AVRCP_PLAY_STATUS_PLAYING,
    AVRCP_PLAY_STATUS_PAUSED,
    AVRCP_PLAY_STATUS_FWD_SEEK, 
    AVRCP_PLAY_STATUS_REV_SEEK, 
    AVRCP_PLAY_STATUS_ERROR = 0xFF
 } avrcp_play_status_t;
 
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
 * @brief AVDTP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param browsing  1 - supported, 0 - not supported
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);


/**
 * @brief Set up AVDTP Sink device.
 */
void avrcp_init(void);

/**
 * @brief Register callback for the AVDTP Sink client. 
 * @param callback
 */
void avrcp_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address.
 * @param bd_addr
 */
void avrcp_connect(bd_addr_t bd_addr);

/**
 * @brief Unit info.
 * @param con_handle
 */
void avrcp_unit_info(uint16_t con_handle);

/**
 * @brief Get capabilities.
 * @param con_handle
 */
void avrcp_get_supported_company_ids(uint16_t con_handle);
void avrcp_get_supported_events(uint16_t con_handle);


/**
 * @brief Play.
 * @param con_handle
 */
void avrcp_play(uint16_t con_handle);

/**
 * @brief Stop.
 * @param con_handle
 */
void avrcp_stop(uint16_t con_handle);

/**
 * @brief Pause.
 * @param con_handle
 */
void avrcp_pause(uint16_t con_handle);

/**
 * @brief Fast forward.
 * @param con_handle
 */
void avrcp_start_fast_forward(uint16_t con_handle);
void avrcp_stop_fast_forward(uint16_t con_handle);

/**
 * @brief Rewind.
 * @param con_handle
 */
void avrcp_start_rewind(uint16_t con_handle);
void avrcp_stop_rewind(uint16_t con_handle);

/**
 * @brief Forward.
 * @param con_handle
 */
void avrcp_forward(uint16_t con_handle); 

/**
 * @brief Backward.
 * @param con_handle
 */
void avrcp_backward(uint16_t con_handle);


/**
 * @brief Get play status. Returns event of type AVRCP_SUBEVENT_PLAY_STATUS (length, position, play_status).
 * If TG does not support SongLength And SongPosition on TG, then TG shall return 0xFFFFFFFF.
 * @param con_handle
 */
void avrcp_get_play_status(uint16_t con_handle);

/**
 * @brief Register notification.
 * @param con_handle
 * @param event_id
 */
void avrcp_enable_notification(uint16_t con_handle, avrcp_notification_event_id_t event_id, uint32_t playback_interval_in_seconds);
void avrcp_disable_notification(uint16_t con_handle, avrcp_notification_event_id_t event_id);

/**
 * @brief Get info on now playing media.
 * @param con_handle
 */
void avrcp_get_now_playing_info(uint16_t con_handle);

/**
 * @brief Set absolute volume 0-127 (corresponds to 0-100%)
 * @param con_handle
 */
void avrcp_set_absolute_volume(uint16_t con_handle, uint8_t volume);

/**
 * @brief Turns the volume to high.
 * @param con_handle
 */
void avrcp_volume_up(uint16_t con_handle);

/**
 * @brief Turns the volume to low.
 * @param con_handle
 */
void avrcp_start_volume_down(uint16_t con_handle);

/**
 * @brief Puts the sound out.
 * @param con_handle
 */
void avrcp_start_mute(uint16_t con_handle);

/**
 * @brief Skip to next playing media.
 * @param con_handle
 */
void avrcp_start_skip(uint16_t con_handle);

/* API_END */
#if defined __cplusplus
}
#endif

#endif // __AVRCP_H
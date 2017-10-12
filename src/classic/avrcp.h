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
#include "btstack_run_loop.h"
#include "btstack_linked_list.h"

#if defined __cplusplus
extern "C" {
#endif

#define BT_SIG_COMPANY_ID 0x001958
#define AVRCP_MEDIA_ATTR_COUNT 7
#define AVRCP_MAX_ATTRIBUTTE_SIZE 100
#define AVRCP_ATTRIBUTE_HEADER_LEN  8

typedef enum {
    AVRCP_STATUS_INVALID_COMMAND = 0,           // sent if TG received a PDU that it did not understand.
    AVRCP_STATUS_INVALID_PARAMETER,             // Sent if the TG received a PDU with a parameter ID that it did not understand, or, if there is only one parameter ID in the PDU.
    AVRCP_STATUS_SPECIFIED_PARAMETER_NOT_FOUND, // sent if the parameter ID is understood, but content is wrong or corrupted.
    AVRCP_STATUS_INTERNAL_ERROR,                // sent if there are error conditions not covered by a more specific error code.
    AVRCP_STATUS_SUCCESS,                       // sent if the operation was successful. 
    AVRCP_STATUS_UID_CHANGED,                   // sent if the UIDs on the device have changed.
    AVRCP_STATUS_RESERVED_6,
    AVRCP_STATUS_INVALID_DIRECTION,             // The Direction parameter is invalid. Valid for command: Change Path
    AVRCP_STATUS_NOT_A_DIRECTORY,               // The UID provided does not refer to a folder item. Valid for command: Change Path
    AVRCP_STATUS_DOES_NOT_EXIST,                // The UID provided does not refer to any currently valid. Valid for command: Change Path, PlayItem, AddToNowPlaying, GetItemAttributes
    AVRCP_STATUS_INVALID_SCOPE,                 // The scope parameter is invalid. Valid for command: GetFolderItems, PlayItem, AddToNowPlayer, GetItemAttributes,
    AVRCP_STATUS_RANGE_OUT_OF_BOUNDS,           // The start of range provided is not valid. Valid for command: GetFolderItems
    AVRCP_STATUS_UID_IS_A_DIRECTORY,            // The UID provided refers to a directory, which cannot be handled by this media player. Valid for command: PlayItem, AddToNowPlaying
    AVRCP_STATUS_MEDIA_IN_USE,                  // The media is not able to be used for this operation at this time. Valid for command: PlayItem, AddToNowPlaying
    AVRCP_STATUS_NOW_PLAYING_LIST_FULL,         // No more items can be added to the Now Playing List. Valid for command: AddToNowPlaying
    AVRCP_STATUS_SEARCH_NOT_SUPPORTED,          // The Browsed Media Player does not support search. Valid for command: Search
    AVRCP_STATUS_SEARCH_IN_PROGRESS,            // A search operation is already in progress. Valid for command: Search
    AVRCP_STATUS_INVALID_PLAYER_ID,             // The specified Player Id does not refer to a valid player. Valid for command: SetAddressedPlayer, SetBrowsedPlayer
    AVRCP_STATUS_PLAYER_NOT_BROWSABLE,          // The Player Id supplied refers to a Media Player which does not support browsing. Valid for command: SetBrowsedPlayer
    AVRCP_STATUS_PLAYER_NOT_ADDRESSED,          // The Player Id supplied refers to a player which is not currently addressed, and the command is not able to be performed if the player is not set as addressed. Valid for command: Search SetBrowsedPlayer
    AVRCP_STATUS_NO_VALID_SEARCH_RESULTS,       // The Search result list does not contain valid entries, e.g. after being invalidated due to change of browsed player. Valid for command: GetFolderItems
    AVRCP_STATUS_NO_AVAILABLE_PLAYERS,
    AVRCP_STATUS_ADDRESSED_PLAYER_CHANGED,       // Valid for command: Register Notification
    AVRCP_STATUS_RESERVED                       // 0x17 - 0xFF
} avrcp_status_code_t;

typedef enum {
    AVRCP_SINGLE_PACKET= 0,
    AVRCP_START_PACKET    ,
    AVRCP_CONTINUE_PACKET ,
    AVRCP_END_PACKET
} avrcp_packet_type_t;

typedef enum {
    AVRCP_COMMAND_FRAME = 0,
    AVRCP_RESPONSE_FRAME    
} avrcp_frame_type_t;


typedef enum {
    AVRCP_CAPABILITY_ID_COMPANY = 0x02,
    AVRCP_CAPABILITY_ID_EVENT = 0x03
} avrcp_capability_id_t;

typedef enum {
    AVRCP_MEDIA_ATTR_NONE = 0,
    AVRCP_MEDIA_ATTR_TITLE,
    AVRCP_MEDIA_ATTR_ARTIST,
    AVRCP_MEDIA_ATTR_ALBUM,
    AVRCP_MEDIA_ATTR_TRACK,
    AVRCP_MEDIA_ATTR_TOTAL_TRACKS,
    AVRCP_MEDIA_ATTR_GENRE,
    AVRCP_MEDIA_ATTR_SONG_LENGTH
} avrcp_media_attribute_id_t;

typedef enum {
    AVRCP_PDU_ID_GET_CAPABILITIES = 0x10,
    AVRCP_PDU_ID_GetCurrentPlayerApplicationSettingValue = 0x13,
    AVRCP_PDU_ID_SetPlayerApplicationSettingValue = 0x14,
    AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES = 0x20,
    AVRCP_PDU_ID_GET_PLAY_STATUS = 0x30,
    AVRCP_PDU_ID_REGISTER_NOTIFICATION = 0x31,
    AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE = 0x40,
    AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE = 0x41,
    AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME = 0x50,
    AVRCP_PDU_ID_UNDEFINED = 0xFF
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

// control command response: accepted, rejected, interim
// status  command response: not implemented, rejected, in transition, stable
// notify  command response: not implemented, rejected, changed

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
    // AVRCP_CMD_OPCODE_RESERVE = 0x01,
    AVRCP_CMD_OPCODE_UNIT_INFO = 0x30,
    AVRCP_CMD_OPCODE_SUBUNIT_INFO = 0x31,
    AVRCP_CMD_OPCODE_PASS_THROUGH = 0x7C,
    // AVRCP_CMD_OPCODE_VERSION = 0xB0,
    // AVRCP_CMD_OPCODE_POWER = 0xB2,
    AVRCP_CMD_OPCODE_UNDEFINED = 0xFF
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
    AVRCP_OPERATION_ID_BACKWARD = 0x4C,
    AVRCP_OPERATION_ID_UNDEFINED = 0xFF
} avrcp_operation_id_t;

typedef enum{
    AVRCP_PLAYBACK_STATUS_STOPPED = 0x00,
    AVRCP_PLAYBACK_STATUS_PLAYING,
    AVRCP_PLAYBACK_STATUS_PAUSED,
    AVRCP_PLAYBACK_STATUS_FWD_SEEK,
    AVRCP_PLAYBACK_STATUS_REV_SEEK,
    AVRCP_PLAYBACK_STATUS_ERROR = 0xFF
} avrcp_playback_status_t;

typedef enum{
    AVRCP_BATTERY_STATUS_NORMAL = 0x00, // Battery operation is in normal state
    AVRCP_BATTERY_STATUS_WARNING,       // unable to operate soon. Is provided when the battery level is going down.
    AVRCP_BATTERY_STATUS_CRITICAL,      // can not operate any more. Is provided when the battery level is going down.
    AVRCP_BATTERY_STATUS_EXTERNAL,      // Plugged to external power supply
    AVRCP_BATTERY_STATUS_FULL_CHARGE    // when the device is completely charged from the external power supply
} avrcp_battery_status_t;


typedef enum {
    AVCTP_CONNECTION_IDLE,
    AVCTP_SIGNALING_W4_SDP_QUERY_COMPLETE,
    AVCTP_CONNECTION_W4_L2CAP_CONNECTED,
    AVCTP_CONNECTION_OPENED,
    AVCTP_W2_SEND_PRESS_COMMAND,
    AVCTP_W2_SEND_RELEASE_COMMAND,
    AVCTP_W4_STOP,
    AVCTP_W2_SEND_COMMAND,
    AVCTP_W2_SEND_RESPONSE,
    AVCTP_W2_RECEIVE_PRESS_RESPONSE,
    AVCTP_W2_RECEIVE_RESPONSE
} avctp_connection_state_t;

typedef struct {
    uint16_t len;
    uint8_t  * value;
} avrcp_now_playing_info_item_t;

typedef struct {
    uint8_t track_id[8];
    uint16_t track_nr;
    char * title;
    char * artist;
    char * album;
    char * genre;
    uint32_t song_length_ms;
    uint32_t song_position_ms;
} avrcp_track_t;

typedef enum {
    AVRCP_PARSER_IDLE = 0,
    AVRCP_PARSER_GET_ATTRIBUTE_HEADER,       // 8 bytes
    AVRCP_PARSER_GET_ATTRIBUTE_VALUE,
    AVRCP_PARSER_IGNORE_ATTRIBUTE_VALUE
} avrcp_parser_state_t;

typedef struct {
    btstack_linked_item_t    item;
    bd_addr_t remote_addr;
    uint16_t l2cap_signaling_cid;
    uint16_t avrcp_cid;

    avctp_connection_state_t state;
    uint8_t wait_to_send;

    // command
    uint8_t transaction_label;
    avrcp_command_opcode_t command_opcode;
    avrcp_command_type_t command_type;
    avrcp_subunit_type_t subunit_type;
    avrcp_subunit_id_t   subunit_id;
    avrcp_packet_type_t  packet_type;

    uint8_t cmd_operands[20];
    uint8_t cmd_operands_length;
    btstack_timer_source_t press_and_hold_cmd_timer;
    uint8_t  continuous_fast_forward_cmd;
    uint16_t notifications_enabled;
    uint16_t notifications_to_register;
    uint16_t notifications_to_deregister; 

    avrcp_subunit_type_t unit_type;
    uint32_t company_id;
    avrcp_subunit_type_t subunit_info_type;
    const uint8_t * subunit_info_data;
    uint16_t subunit_info_data_size;

    avrcp_now_playing_info_item_t now_playing_info[AVRCP_MEDIA_ATTR_COUNT];
    uint8_t  track_id[8];
    uint32_t song_length_ms;
    uint32_t song_position_ms;
    int total_tracks;
    int track_nr;
    uint8_t track_selected;
    uint8_t track_changed;
    
    avrcp_playback_status_t playback_status;
    uint8_t playback_status_changed;

    uint8_t playing_content_changed;
    
    avrcp_battery_status_t battery_status;
    uint8_t battery_status_changed;
    uint8_t volume_percentage;
    uint8_t volume_percentage_changed;
    uint8_t now_playing_info_response;
    uint8_t now_playing_info_attr_bitmap;
    uint8_t abort_continue_response;
    
    // used for fragmentation
    avrcp_media_attribute_id_t next_attr_id;
    
    avrcp_parser_state_t parser_state;
    uint8_t  parser_attribute_header[AVRCP_ATTRIBUTE_HEADER_LEN];
    uint8_t  parser_attribute_header_pos;

    uint16_t list_size;
    uint16_t list_offset;
    uint8_t  attribute_value[AVRCP_MAX_ATTRIBUTTE_SIZE];
    uint16_t attribute_value_len;
    uint16_t attribute_value_offset;
    
    uint32_t attribute_id;
    
    uint8_t  num_attributes;
    uint8_t  num_parsed_attributes;
} avrcp_connection_t;

typedef enum {
    AVRCP_SHUFFLE_MODE_INVALID,
    AVRCP_SHUFFLE_MODE_OFF,
    AVRCP_SHUFFLE_MODE_ALL_TRACKS,
    AVRCP_SHUFFLE_MODE_GROUP
} avrcp_shuffle_mode_t;

typedef enum {
    AVRCP_REPEAT_MODE_INVALID,
    AVRCP_REPEAT_MODE_OFF,
    AVRCP_REPEAT_MODE_SINGLE_TRACK,
    AVRCP_REPEAT_MODE_ALL_TRACKS,
    AVRCP_REPEAT_MODE_GROUP
} avrcp_repeat_mode_t;

typedef enum{
    AVRCP_CONTROLLER = 0,
    AVRCP_TARGET
} avrcp_role_t;

typedef enum {
    UTF8 = 106
} rfc2978_charset_mib_enumid_t;

typedef struct {
    avrcp_role_t role;
    btstack_linked_list_t connections;
    btstack_packet_handler_t avrcp_callback;
    btstack_packet_handler_t packet_handler;

    // SDP query
    uint16_t avrcp_cid;
    uint16_t avrcp_l2cap_psm;
    uint16_t avrcp_version;
    uint16_t avrcp_browsing_l2cap_psm;
    uint16_t avrcp_browsing_version;
    uint8_t  role_supported;
} avrcp_context_t; 

const char * avrcp_subunit2str(uint16_t index);
const char * avrcp_event2str(uint16_t index);
const char * avrcp_operation2str(uint8_t index);
const char * avrcp_attribute2str(uint8_t index);
const char * avrcp_play_status2str(uint8_t index);
const char * avrcp_ctype2str(uint8_t index);
const char * avrcp_repeat2str(uint8_t index);
const char * avrcp_shuffle2str(uint8_t index);

void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);

void avrcp_create_sdp_record(uint8_t controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);
uint8_t avrcp_connect(bd_addr_t bd_addr, avrcp_context_t * context, uint16_t * avrcp_cid);
void avrcp_emit_connection_established(btstack_packet_handler_t callback, uint16_t avrcp_cid, bd_addr_t addr, uint8_t status);
void avrcp_emit_connection_closed(btstack_packet_handler_t callback, uint16_t avrcp_cid);

uint8_t avrcp_cmd_opcode(uint8_t *packet, uint16_t size);
avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid, avrcp_context_t * context);
avrcp_connection_t * get_avrcp_connection_for_avrcp_cid(uint16_t avrcp_cid, avrcp_context_t * context);
void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid);

#if defined __cplusplus
}
#endif

#endif // __AVRCP_H
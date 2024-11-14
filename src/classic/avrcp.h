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
 * Audio/Video Remote Control Profile (AVRCP)
 *
 */

#ifndef AVRCP_H
#define AVRCP_H

#include <stdint.h>

#include "btstack_bool.h"
#include "btstack_run_loop.h"
#include "btstack_linked_list.h"
#include "l2cap.h"

#if defined __cplusplus
extern "C" {
#endif

#define PSM_AVCTP_BROWSING              0x001b

#define AVRCP_BROWSING_ITEM_HEADER_LEN 3
#define AVRCP_BROWSING_MAX_NUM_ATTR_IDS 8
#define AVRCP_DISPLAYABLE_NAME_MAX_LENGTH  20
#define AVRCP_ATTRIBUTES_MAX_NUM           10
#define AVRCP_SEARCH_STRING_MAX_LENGTH     20

#define AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE 512

#define AVRCP_MAX_COMMAND_PARAMETER_LENGTH 11
#define BT_SIG_COMPANY_ID 0x001958
#define AVRCP_MEDIA_ATTR_COUNT 7
#define AVRCP_MAX_ATTRIBUTE_SIZE 130
#define AVRCP_ATTRIBUTE_HEADER_LEN  8
#define AVRCP_MAX_FOLDER_NAME_SIZE      20

#define AVRCP_NO_TRACK_SELECTED_PLAYBACK_POSITION_CHANGED    0xFFFFFFFF

#define AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER          0x0001u
#define AVRCP_FEATURE_MASK_CATEGORY_MONITOR_OR_AMPLIFIER        0x0002u
#define AVRCP_FEATURE_MASK_CATEGORY_TUNER                       0x0004u
#define AVRCP_FEATURE_MASK_CATEGORY_MENU                        0x0008u
#define AVRCP_FEATURE_MASK_PLAYER_APPLICATION_SETTINGS          0x0010u   // AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER must be 1 for this feature to be set
#define AVRCP_FEATURE_MASK_GROUP_NAVIGATION                     0x0020u   // AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER must be 1 for this feature to be set
#define AVRCP_FEATURE_MASK_BROWSING                             0x0040u
#define AVRCP_FEATURE_MASK_MULTIPLE_MEDIA_PLAYE_APPLICATIONS    0x0080u

#define AVRCP_MAJOR_PLAYER_TYPE_FEATURE_MASK_AUDIO                       0x01u
#define AVRCP_MAJOR_PLAYER_TYPE_FEATURE_MASK_VIDEO                       0x02u
#define AVRCP_MAJOR_PLAYER_TYPE_FEATURE_MASK_BROADCASTING_AUDIO          0x04u
#define AVRCP_MAJOR_PLAYER_TYPE_FEATURE_MASK_BROADCASTING_VIDEO          0x08u

#define AVRCP_PLAYER_SUBTYPE_FEATURE_MASK_AUDIO_BOOK                       0x01u
#define AVRCP_PLAYER_SUBTYPE_FEATURE_MASK_PODCAST                          0x02u

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
    AVCTP_SINGLE_PACKET= 0,
    AVCTP_START_PACKET    ,
    AVCTP_CONTINUE_PACKET ,
    AVCTP_END_PACKET
} avctp_packet_type_t;

typedef enum {
    AVRCP_COMMAND_FRAME = 0,
    AVRCP_RESPONSE_FRAME    
} avrcp_frame_type_t;


typedef enum {
    AVRCP_CAPABILITY_ID_COMPANY = 0x02,
    AVRCP_CAPABILITY_ID_EVENT = 0x03
} avrcp_capability_id_t;

typedef enum {
    AVRCP_MEDIA_ATTR_ALL = 0x0000,
    AVRCP_MEDIA_ATTR_TITLE,
    AVRCP_MEDIA_ATTR_ARTIST,
    AVRCP_MEDIA_ATTR_ALBUM,
    AVRCP_MEDIA_ATTR_TRACK,
    AVRCP_MEDIA_ATTR_TOTAL_NUM_ITEMS,
    AVRCP_MEDIA_ATTR_GENRE,
    AVRCP_MEDIA_ATTR_SONG_LENGTH_MS,
    AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART,
    AVRCP_MEDIA_ATTR_NUM = 0x0009,
    AVRCP_MEDIA_ATTR_NONE = 0x7FFF
} avrcp_media_attribute_id_t;

typedef enum {
    AVRCP_PDU_ID_GET_CAPABILITIES = 0x10,
    AVRCP_PDU_ID_LIST_PLAYER_APPLICATION_SETTING_ATTRIBUTES, 
    AVRCP_PDU_ID_LIST_PLAYER_APPLICATION_SETTING_VALUES,
    AVRCP_PDU_ID_GET_CURRENT_PLAYER_APPLICATION_SETTING_VALUE = 0x13,
    AVRCP_PDU_ID_SET_PLAYER_APPLICATION_SETTING_VALUE = 0x14,
    AVRCP_PDU_ID_GET_PLAYER_APPLICATION_SETTING_ATTRIBUTE_TEXT,
    AVRCP_PDU_ID_GET_PLAYER_APPLICATION_SETTING_VALUE_TEXT,
    AVRCP_PDU_ID_INFORM_DISPLAYABLE_CHARACTERSET,
    AVRCP_PDU_ID_INFORM_BATTERY_STATUS_OF_CT,
    AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES = 0x20,
    AVRCP_PDU_ID_GET_PLAY_STATUS = 0x30,
    AVRCP_PDU_ID_REGISTER_NOTIFICATION = 0x31,
    AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE = 0x40,
    AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE = 0x41,
    AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME = 0x50,
    AVRCP_PDU_ID_SET_ADDRESSED_PLAYER = 0x60,
    AVRCP_PDU_ID_SET_BROWSED_PLAYER = 0x70,
    AVRCP_PDU_ID_GET_FOLDER_ITEMS = 0x71,
    AVRCP_PDU_ID_CHANGE_PATH = 0x72,
    AVRCP_PDU_ID_GET_ITEM_ATTRIBUTES = 0x73,
    AVRCP_PDU_ID_PLAY_ITEM = 0x74,
    AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS = 0x75,
    AVRCP_PDU_ID_SEARCH = 0x80,
    AVRCP_PDU_ID_ADD_TO_NOW_PLAYING = 0x90,
    AVRCP_PDU_ID_GENERAL_REJECT = 0xA0,
    
    AVRCP_PDU_ID_UNDEFINED = 0xFF
} avrcp_pdu_id_t;

typedef enum {
    AVRCP_NOTIFICATION_EVENT_NONE = 0,
    AVRCP_NOTIFICATION_EVENT_FIRST_INDEX = 0x01,
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
    AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED = 0x0d,                     // The volume has been changed locally on the TG, see 6.13.3.
    AVRCP_NOTIFICATION_EVENT_MAX_VALUE = 0x0e,
    AVRCP_NOTIFICATION_EVENT_LAST_INDEX = 0x0e
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
    AVRCP_SUBUNIT_TYPE_EXTENDED_TO_NEXT_BYTE, // The target_unit_type field may take value 1E16, which means that the field is extended to the following byte. In that case, an additional byte for extended_unit_type will be added immediately following operand[1].
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

// See "AVC-Panel Subunit.pdf", Chapter 9.4 "PASS THROUGH control command"
// Using subset defined in "AVRCP_v1.6.3.pdf", Chapter 4.6.1 "Support Level in TG"

typedef enum {
    AVRCP_GROUP_OPERATION_ID_GOTO_NEXT     = 0x00,
    AVRCP_GROUP_OPERATION_ID_GOTO_PREVIOUS = 0x01
} avrcp_group_operation_id_t;

typedef enum {
    AVRCP_OPERATION_ID_SELECT = 0x00, // Next Group
    AVRCP_OPERATION_ID_UP = 0x01,     // Previous Group
    AVRCP_OPERATION_ID_DOWN = 0x02,
    AVRCP_OPERATION_ID_LEFT = 0x03,
    AVRCP_OPERATION_ID_RIGHT = 0x04,
    AVRCP_OPERATION_ID_RIGHT_UP = 0x05,
    AVRCP_OPERATION_ID_RIGHT_DOWN = 0x06,
    AVRCP_OPERATION_ID_LEFT_UP = 0x07,
    AVRCP_OPERATION_ID_LEFT_DOWN = 0x08,
    AVRCP_OPERATION_ID_ROOT_MENU = 0x09,
    AVRCP_OPERATION_ID_SETUP_MENU = 0x0A,
    AVRCP_OPERATION_ID_CONTENTS_MENU = 0x0B,
    AVRCP_OPERATION_ID_FAVORITE_MENU = 0x0C,
    AVRCP_OPERATION_ID_EXIT = 0x0D,
    AVRCP_OPERATION_ID_RESERVED_1 = 0x0E,

    AVRCP_OPERATION_ID_0 = 0x20,
    AVRCP_OPERATION_ID_1 = 0x21,
    AVRCP_OPERATION_ID_2 = 0x22,
    AVRCP_OPERATION_ID_3 = 0x23,
    AVRCP_OPERATION_ID_4 = 0x24,
    AVRCP_OPERATION_ID_5 = 0x25,
    AVRCP_OPERATION_ID_6 = 0x26,
    AVRCP_OPERATION_ID_7 = 0x27,
    AVRCP_OPERATION_ID_8 = 0x28,
    AVRCP_OPERATION_ID_9 = 0x29,
    AVRCP_OPERATION_ID_DOT   = 0x2A,
    AVRCP_OPERATION_ID_ENTER = 0x2B,
    AVRCP_OPERATION_ID_CLEAR = 0x2C,
    AVRCP_OPERATION_ID_RESERVED_2 = 0x2D,

    AVRCP_OPERATION_ID_CHANNEL_UP = 0x30,
    AVRCP_OPERATION_ID_CHANNEL_DOWN = 0x31,
    AVRCP_OPERATION_ID_PREVIOUS_CHANNEL = 0x32,
    AVRCP_OPERATION_ID_SOUND_SELECT = 0x33,
    AVRCP_OPERATION_ID_INPUT_SELECT = 0x34,
    AVRCP_OPERATION_ID_DISPLAY_INFORMATION = 0x35,
    AVRCP_OPERATION_ID_HELP = 0x36,
    AVRCP_OPERATION_ID_PAGE_UP = 0x37,
    AVRCP_OPERATION_ID_PAGE_DOWN = 0x38,
    AVRCP_OPERATION_ID_RESERVED_3 = 0x39,
    
    AVRCP_OPERATION_ID_SKIP = 0x3C,
    
    AVRCP_OPERATION_ID_POWER = 0x40,
    AVRCP_OPERATION_ID_VOLUME_UP = 0x41,
    AVRCP_OPERATION_ID_VOLUME_DOWN = 0x42,
    AVRCP_OPERATION_ID_MUTE = 0x43,
    AVRCP_OPERATION_ID_PLAY = 0x44,
    AVRCP_OPERATION_ID_STOP = 0x45,
    AVRCP_OPERATION_ID_PAUSE = 0x46,
    AVRCP_OPERATION_ID_RECORD = 0x47,
    AVRCP_OPERATION_ID_REWIND = 0x48,
    AVRCP_OPERATION_ID_FAST_FORWARD = 0x49,
    AVRCP_OPERATION_ID_EJECT = 0x4A,
    AVRCP_OPERATION_ID_FORWARD = 0x4B,
    AVRCP_OPERATION_ID_BACKWARD = 0x4C,
    AVRCP_OPERATION_ID_RESERVED_4 = 0x4D,

    AVRCP_OPERATION_ID_ANGLE = 0x50,
    AVRCP_OPERATION_ID_SUBPICTURE = 0x51,
    AVRCP_OPERATION_ID_RESERVED_5 = 0x52,

    AVRCP_OPERATION_ID_F1 = 0x71,
    AVRCP_OPERATION_ID_F2 = 0x72,
    AVRCP_OPERATION_ID_F3 = 0x73,
    AVRCP_OPERATION_ID_F4 = 0x74,
    AVRCP_OPERATION_ID_F5 = 0x75,
    AVRCP_OPERATION_ID_RESERVED_6 = 0x76,

    AVRCP_OPERATION_ID_VENDOR_UNIQUE = 0x7E,
    AVRCP_OPERATION_ID_RESERVED_7 = 0x7F
} avrcp_operation_id_t;

typedef enum{
    AVRCP_PLAYBACK_STATUS_STOPPED = 0x00,
    AVRCP_PLAYBACK_STATUS_PLAYING,
    AVRCP_PLAYBACK_STATUS_PAUSED,
    AVRCP_PLAYBACK_STATUS_FWD_SEEK,
    AVRCP_PLAYBACK_STATUS_REV_SEEK,
    AVRCP_PLAYBACK_STATUS_ERROR = 0xFF
} avrcp_playback_status_t;

typedef enum {
    AVRCP_BATTERY_STATUS_NORMAL = 0x00, // Battery operation is in normal state
    AVRCP_BATTERY_STATUS_WARNING,       // unable to operate soon. Is provided when the battery level is going down.
    AVRCP_BATTERY_STATUS_CRITICAL,      // can not operate any more. Is provided when the battery level is going down.
    AVRCP_BATTERY_STATUS_EXTERNAL,      // Plugged to external power supply
    AVRCP_BATTERY_STATUS_FULL_CHARGE    // when the device is completely charged from the external power supply
} avrcp_battery_status_t;

typedef enum {
    AVRCP_SYSTEM_STATUS_POWER_ON = 0x00,
    AVRCP_SYSTEM_STATUS_POWER_OFF,
    AVRCP_SYSTEM_STATUS_UNPLUGGED
} avrcp_system_status_t;

typedef enum {
    AVRCP_PLAYER_APPLICATION_SETTING_ATTRIBUTE_ID_ILLEGAL = 0x00,       // ValueIDs with descriptions:
    AVRCP_PLAYER_APPLICATION_SETTING_ATTRIBUTE_ID_EQUALIZER_STATUS,     // 1 - off, 2 - on
    AVRCP_PLAYER_APPLICATION_SETTING_ATTRIBUTE_ID_REPEAT_MODE_STATUS,   // 1 - off, 2 - single track repeat, 3 - all tracks repeat, 4 - group repeat
    AVRCP_PLAYER_APPLICATION_SETTING_ATTRIBUTE_ID_SHUFFLE_STATUS,       // 1 - off, 2 - all tracks shuffle , 3 - group shuffle
    AVRCP_PLAYER_APPLICATION_SETTING_ATTRIBUTE_ID_SCAN_STATUS           // 1 - off, 2 - all tracks scan    , 3 - group scan
} avrcp_player_application_setting_attribute_id_t;

typedef enum {
    AVCTP_CONNECTION_IDLE,
    AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE,
    AVCTP_CONNECTION_W4_ERTM_CONFIGURATION,
    AVCTP_CONNECTION_W2_L2CAP_CONNECT,
    AVCTP_CONNECTION_W4_L2CAP_CONNECTED,
    AVCTP_CONNECTION_W2_L2CAP_RETRY,
    AVCTP_CONNECTION_OPENED,
    AVCTP_W2_SEND_PRESS_COMMAND,
    AVCTP_W2_SEND_RELEASE_COMMAND,
    AVCTP_W4_STOP,
    AVCTP_W2_SEND_COMMAND,
    AVCTP_W2_SEND_RESPONSE,
    AVCTP_W2_CHECK_DATABASE,
    AVCTP_W2_RECEIVE_PRESS_RESPONSE,
    AVCTP_W2_RECEIVE_RESPONSE,
    AVCTP_W2_SEND_GET_ELEMENT_ATTRIBUTES_REQUEST,
    //AVCTP_W2_SEND_AVCTP_FRAGMENTED_MESSAGE
} avctp_connection_state_t;

typedef struct {
    uint16_t len;
    uint8_t  * value;
} avrcp_now_playing_info_item_t;

typedef enum {
    AVRCP_PARSER_GET_ATTRIBUTE_HEADER = 0,       // 8 bytes
    AVRCP_PARSER_GET_ATTRIBUTE_VALUE,
    AVRCP_PARSER_IGNORE_REST_OF_ATTRIBUTE_VALUE
} avrcp_parser_state_t;

typedef enum{
    AVRCP_CONTROLLER = 0,
    AVRCP_TARGET
} avrcp_role_t;

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

typedef enum {
    RFC2978_CHARSET_MIB_UTF8 = 106
} rfc2978_charset_mib_enumid_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_PLAYER_LIST = 0x00,
    AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM,
    AVRCP_BROWSING_SEARCH,
    AVRCP_BROWSING_NOW_PLAYING,
    AVRCP_BROWSING_RFU
} avrcp_browsing_scope_t;

typedef enum {
    AVRCP_BROWSING_DIRECTION_FOLDER_UP = 0x00,
    AVRCP_BROWSING_DIRECTION_FOLDER_DOWN,
    AVRCP_BROWSING_DIRECTION_FOLDER_RFU
} avrcp_browsing_direction_t;

typedef enum {
    AVRCP_REMOTE_CAPABILITIES_NONE = 0,
    AVRCP_REMOTE_CAPABILITIES_W4_QUERY_RESULT,
    AVRCP_REMOTE_CAPABILITIES_KNOWN
} avrcp_remote_capabilities_state_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_PLAYER_ITEM = 0x01,
    AVRCP_BROWSING_FOLDER_ITEM,
    AVRCP_BROWSING_MEDIA_ELEMENT_ITEM,
    AVRCP_BROWSING_MEDIA_ROOT_FOLDER,
    AVRCP_BROWSING_MEDIA_ELEMENT_ITEM_ATTRIBUTE
} avrcp_browsing_item_type_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_PLAYER_MAJOR_TYPE_AUDIO = 1,
    AVRCP_BROWSING_MEDIA_PLAYER_MAJOR_TYPE_VIDEO = 2,
    AVRCP_BROWSING_MEDIA_PLAYER_MAJOR_TYPE_BROADCASTING_AUDIO = 4,
    AVRCP_BROWSING_MEDIA_PLAYER_MAJOR_TYPE_BROADCASTING_VIDEO = 8
} avrcp_browsing_media_player_major_type_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_PLAYER_SUBTYPE_AUDIO_BOOK = 1,
    AVRCP_BROWSING_MEDIA_PLAYER_SUBTYPE_POADCAST   = 2
} avrcp_browsing_media_player_subtype_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_STOPPED = 0,
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_PLAYING,
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_PAUSED,
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_FWD_SEEK,
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_REV_SEEK,
    AVRCP_BROWSING_MEDIA_PLAYER_STATUS_ERROR = 0xFF
} avrcp_browsing_media_player_status_t;

typedef enum {
    AVRCP_BROWSING_FOLDER_TYPE_MIXED = 0x00,
    AVRCP_BROWSING_FOLDER_TYPE_TITLES,
    AVRCP_BROWSING_FOLDER_TYPE_ALBUMS,
    AVRCP_BROWSING_FOLDER_TYPE_ARTISTS,
    AVRCP_BROWSING_FOLDER_TYPE_GENRES,
    AVRCP_BROWSING_FOLDER_TYPE_PLAYLISTS,
    AVRCP_BROWSING_FOLDER_TYPE_YEARS
} avrcp_browsing_folder_type_t;

typedef enum {
    AVRCP_BROWSING_MEDIA_TYPE_AUDIO = 0x00,
    AVRCP_BROWSING_MEDIA_TYPE_VIDEO
} avrcp_browsing_media_type_t;

typedef struct {
    avrcp_media_attribute_id_t attribute_id; // 4B
    uint16_t characterset;   // RFC2978_CHARSET_MIB_UTF8
    uint16_t displayable_name_len;
    char displayable_name[AVRCP_DISPLAYABLE_NAME_MAX_LENGTH];
} avrcp_browsing_attribute_t ;

typedef struct {
    uint16_t player_id;
    uint8_t major_player_type_bitmap;
    uint32_t player_subtype_bitmap;
    avrcp_playback_status_t play_status;
    uint8_t feature_bitmap[16];
    uint16_t characterset;   // RFC2978_CHARSET_MIB_UTF8
    uint16_t displayable_name_len;
    char displayable_name[AVRCP_DISPLAYABLE_NAME_MAX_LENGTH];
} avrcp_browsing_media_player_t;

typedef struct {
    // avrcp_browsing_item_type_t item_type = AVRCP_BROWSING_Item_type_Media_Player;
    // uint16_t item_length = 28 + displayable_name_len;
    uint8_t id[8];
    avrcp_browsing_folder_type_t type;
    uint8_t is_playable;
    uint16_t characterset;   // RFC2978_CHARSET_MIB_UTF8
    uint16_t displayable_name_len;
    char displayable_name[AVRCP_DISPLAYABLE_NAME_MAX_LENGTH];
} avrcp_browsing_folder_item_t;

typedef struct {
    // avrcp_browsing_item_type_t item_type = AVRCP_BROWSING_Item_type_Media_Player;
    // uint16_t item_length = 28 + displayable_name_len;
    uint8_t id[8];
    avrcp_browsing_media_type_t type;
    uint16_t characterset;   // RFC2978_CHARSET_MIB_UTF8
    uint16_t displayable_name_len;
    char displayable_name[AVRCP_DISPLAYABLE_NAME_MAX_LENGTH];
    uint8_t num_attributes;
    avrcp_browsing_attribute_t attributes[AVRCP_ATTRIBUTES_MAX_NUM];
} avrcp_browsing_media_element_item_t;

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

// BROWSING 
typedef struct {
    uint16_t l2cap_browsing_cid;

    avctp_connection_state_t state;
    bool     wait_to_send;
    uint8_t  transaction_label;
    // used for fragmentation
    uint8_t  num_packets;
    uint16_t bytes_to_send;

    uint8_t *ertm_buffer;
    uint32_t ertm_buffer_size;
    l2cap_ertm_config_t ertm_config;

    // players
    uint8_t  set_browsed_player_id;
    uint16_t browsed_player_id;

    avrcp_browsing_scope_t  scope;
    uint8_t  item_uid[8]; // or media element
    uint16_t uid_counter;

    // get folder item
    uint8_t  get_folder_items;
    uint32_t start_item;
    uint32_t end_item;
    uint32_t attr_bitmap;

    // item attrs
    uint8_t get_item_attributes;

    // change_path
    uint8_t  change_path;
    uint8_t  direction;
    
    // search str
    uint16_t search_str_len;
    uint8_t  search_str[20];
    uint8_t  search;

    // search str
    uint16_t target_search_str_len;
    char *  target_search_str;
    uint16_t target_search_characterset;

    // get_item_attributes
    uint8_t  get_total_nr_items;
    avrcp_browsing_scope_t get_total_nr_items_scope;
    
    avrcp_pdu_id_t pdu_id;
    uint8_t browsing_status;
    uint16_t num_items;

    avrcp_parser_state_t parser_state;
    uint8_t  parser_attribute_header[AVRCP_BROWSING_ITEM_HEADER_LEN];
    uint8_t  parser_attribute_header_pos;
    uint8_t  parsed_attribute_value[AVRCP_MAX_ATTRIBUTE_SIZE];
    uint16_t parsed_attribute_value_len;
    uint16_t parsed_attribute_value_offset;
    uint8_t  parsed_num_attributes;

    // get folder items data
    uint8_t * attr_list;
    uint16_t attr_list_size;
    // command
    // uint8_t transaction_label;
    avrcp_command_opcode_t command_opcode;
    avrcp_command_type_t command_type;
    avrcp_subunit_type_t subunit_type;
    avrcp_subunit_id_t   subunit_id;
    avrcp_packet_type_t  packet_type;
    uint8_t cmd_operands[400];
    uint16_t cmd_operands_length;

    bool incoming_declined;
} avrcp_browsing_connection_t;

typedef struct {
    btstack_linked_item_t    item;
    
    avrcp_role_t role;
    bd_addr_t remote_addr;
    hci_con_handle_t con_handle;

    uint16_t l2cap_signaling_cid;
    uint16_t l2cap_mtu;
    uint16_t avrcp_cid;
    uint16_t avrcp_browsing_cid;

    bool incoming_declined;

    bool    trigger_sdp_query;

    // SDP results
    uint16_t avrcp_l2cap_psm;
    uint16_t browsing_l2cap_psm;
    uint16_t browsing_version;
#ifdef ENABLE_AVRCP_COVER_ART
    uint16_t cover_art_psm;
#endif

    avrcp_browsing_connection_t * browsing_connection;

    avctp_connection_state_t state;
    bool wait_to_send;


    // transaction id 
    uint8_t transaction_id_counter;

    btstack_timer_source_t retry_timer;

    // AVCTP header
    uint8_t                transaction_id;
    avctp_packet_type_t    avctp_packet_type;
    // AVRCP header
    avrcp_packet_type_t    avrcp_packet_type;
    uint16_t               avrcp_frame_bytes_sent;
    avrcp_subunit_type_t   subunit_type;
    avrcp_subunit_id_t     subunit_id;
    uint32_t               company_id;
    // message (command and response) header
    avrcp_pdu_id_t         pdu_id;
    avrcp_command_opcode_t command_opcode;
    avrcp_command_type_t   command_type;
    // needed for PASS_THROUGH
    avrcp_operation_id_t   operation_id;

    uint16_t notifications_enabled;
    uint16_t notifications_supported_by_target;

    // message data, used for responses and commands that fit into a single AVCTP packet
    uint8_t   message_body[AVRCP_MAX_COMMAND_PARAMETER_LENGTH];

    // pointer to command and response data
    uint8_t * data;
    uint16_t  data_len;

    // used for fragmentation
    uint16_t  data_offset;
    avrcp_media_attribute_id_t next_attr_id;

    // used for parser in controller, and for fragmentation in target
    uint8_t  attribute_value[AVRCP_MAX_ATTRIBUTE_SIZE];
    uint16_t attribute_value_len;
    uint16_t attribute_value_offset;

    // controller only
    // parser
    avrcp_parser_state_t parser_state;
    uint8_t  parser_attribute_header[AVRCP_ATTRIBUTE_HEADER_LEN];
    uint8_t  parser_attribute_header_pos;
    uint16_t list_size;
    uint16_t list_offset;

    // limit number of pending commands to transaction id window size
    uint8_t controller_last_confirmed_transaction_id;

    btstack_timer_source_t controller_press_and_hold_cmd_timer;
    bool     controller_press_and_hold_cmd_active;
    bool     controller_press_and_hold_cmd_release;

    avrcp_remote_capabilities_state_t remote_capabilities_state;
    bool     controller_notifications_supported_by_target_suppress_emit_result;
    uint16_t controller_initial_status_reported;
    uint16_t controller_notifications_to_register;
    uint16_t controller_notifications_to_deregister;

    // used for avrcp_controller_get_element_attributes
    uint16_t controller_element_attributes;

    // PTS requires definition of max num fragments
    uint8_t controller_max_num_fragments;
    uint8_t controller_num_received_fragments;

    // target only
    // PID check
    bool     target_reject_transport_header;
    uint16_t target_invalid_pid;

    uint8_t  target_notifications_transaction_label[AVRCP_NOTIFICATION_EVENT_MAX_VALUE + 1];

    avrcp_subunit_type_t target_unit_type;
    avrcp_subunit_type_t target_subunit_info_type;
    const uint8_t *  target_subunit_info_data;
    uint16_t         target_subunit_info_data_size;

    const uint32_t * target_supported_companies;
    uint8_t          target_supported_companies_num;


    bool     target_addressed_player_changed;
    uint16_t target_addressed_player_id;
    uint16_t target_uid_counter;
    bool     target_uids_changed;
    avrcp_browsing_scope_t target_scope;

    bool     target_accept_response;

    bool     target_now_playing_info_response;
    bool     target_abort_continue_response;
    bool     target_continue_response;
    uint8_t  target_now_playing_info_attr_bitmap;

    bool     target_playback_status_changed;
    avrcp_playback_status_t target_playback_status;

    bool     target_battery_status_changed;
    avrcp_battery_status_t target_battery_status;

    bool     target_notify_absolute_volume_changed;
    uint8_t  target_absolute_volume;

    bool     target_playing_content_changed;
    bool     target_track_selected;
    bool     target_track_changed;
    avrcp_now_playing_info_item_t target_now_playing_info[AVRCP_MEDIA_ATTR_COUNT];
    uint8_t  target_track_id[8];
    uint32_t target_song_length_ms;
    uint32_t target_song_position_ms;
    uint32_t target_total_tracks;
    uint32_t target_track_nr;

#ifdef ENABLE_AVCTP_FRAGMENTATION
    uint16_t avctp_reassembly_size;
    uint8_t  avctp_reassembly_buffer[200];
#endif

} avrcp_connection_t;

typedef struct {
    avrcp_role_t role;
    btstack_packet_handler_t avrcp_callback;
    btstack_packet_handler_t packet_handler;

    bool (*set_addressed_player_callback)(uint16_t player_id);

    btstack_packet_handler_t browsing_avrcp_callback;
    btstack_packet_handler_t browsing_packet_handler;
} avrcp_context_t; 


const char * avrcp_subunit2str(uint16_t index);
const char * avrcp_event2str(uint16_t index);
const char * avrcp_operation2str(uint8_t index);
const char * avrcp_attribute2str(uint8_t index);
const char * avrcp_play_status2str(uint8_t index);
const char * avrcp_ctype2str(uint8_t index);
const char * avrcp_repeat2str(uint8_t index);
const char * avrcp_shuffle2str(uint8_t index);
const char * avrcp_notification2str(avrcp_notification_event_id_t index);

avctp_packet_type_t avctp_get_packet_type(avrcp_connection_t * connection, uint16_t * max_payload_size);
avrcp_packet_type_t avrcp_get_packet_type(avrcp_connection_t * connection);
uint16_t avctp_get_num_bytes_for_header(avctp_packet_type_t avctp_packet_type);
uint16_t avrcp_get_num_bytes_for_header(avrcp_command_opcode_t command_opcode, avctp_packet_type_t avctp_packet_type);

void avrcp_register_controller_packet_handler(btstack_packet_handler_t avrcp_controller_packet_handler);
void avrcp_register_target_packet_handler(btstack_packet_handler_t avrcp_target_packet_handler);

void avrcp_register_browsing_sdp_query_complete_handler(void (*callback)(avrcp_connection_t * connection, uint8_t status));
void avrcp_register_cover_art_sdp_query_complete_handler(void (*callback)(avrcp_connection_t * connection, uint8_t status));

uint8_t avrcp_cmd_opcode(uint8_t *packet, uint16_t size);

avrcp_connection_t * avrcp_get_connection_for_l2cap_signaling_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid);
avrcp_connection_t * avrcp_get_connection_for_avrcp_cid_for_role(avrcp_role_t role, uint16_t avrcp_cid);
avrcp_connection_t * avrcp_get_connection_for_bd_addr_for_role(avrcp_role_t role, bd_addr_t addr);

avrcp_connection_t * avrcp_get_connection_for_browsing_cid_for_role(avrcp_role_t role, uint16_t browsing_cid);
avrcp_connection_t * avrcp_get_connection_for_browsing_l2cap_cid_for_role(avrcp_role_t role, uint16_t browsing_l2cap_cid);
avrcp_browsing_connection_t * avrcp_get_browsing_connection_for_l2cap_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid);

void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid);
uint16_t avrcp_get_next_cid(avrcp_role_t role);
btstack_linked_list_t avrcp_get_connections(void);

uint16_t avrcp_sdp_query_browsing_l2cap_psm(void);
void avrcp_handle_sdp_client_query_attribute_value(uint8_t *packet);
avrcp_connection_t * get_avrcp_connection_for_browsing_cid_for_role(avrcp_role_t role, uint16_t browsing_cid);
avrcp_connection_t * get_avrcp_connection_for_browsing_l2cap_cid_for_role(avrcp_role_t role, uint16_t browsing_l2cap_cid);
avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_l2cap_cid_for_role(avrcp_role_t role, uint16_t l2cap_cid);
// SDP query
void    avrcp_create_sdp_record(bool controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);
void avrcp_trigger_sdp_query(avrcp_connection_t *connection_controller, avrcp_connection_t *connection_target);


/* API_START */

/**
 * @brief Set up AVRCP service
 */
void avrcp_init(void);

/**
 * @brief Register callback for the AVRCP Controller client. 
 * @param callback
 */
void avrcp_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief   Connect to AVRCP service on a remote device, emits AVRCP_SUBEVENT_CONNECTION_ESTABLISHED with status
 * @param   remote_addr
 * @param   avrcp_cid  outgoing parameter, valid if status == ERROR_CODE_SUCCESS
 * @return status     
 */
uint8_t avrcp_connect(bd_addr_t remote_addr, uint16_t * avrcp_cid);

/**
 * @brief   Disconnect from AVRCP service
 * @param   avrcp_cid
 * @return status
 */
uint8_t avrcp_disconnect(uint16_t avrcp_cid);

/**
 * @brief De-Init AVRCP
 */
void avrcp_deinit(void);

/* API_END */

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void avrcp_init_fuzz(void);
void avrcp_packet_handler_fuzz(uint8_t *packet, uint16_t size);
#endif


#if defined __cplusplus
}
#endif

#endif // AVRCP_H

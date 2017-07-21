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
    AVRCP_MEDIA_ATTR_TITLE = 1,
    AVRCP_MEDIA_ATTR_ARTIST,
    AVRCP_MEDIA_ATTR_ALBUM,
    AVRCP_MEDIA_ATTR_TRACK,
    AVRCP_MEDIA_ATTR_TOTAL_TRACKS,
    AVRCP_MEDIA_ATTR_GENRE,
    AVRCP_MEDIA_ATTR_SONG_LENGTH
} avrcp_media_attribute_id_t;

#define AVRCP_MEDIA_ATTR_COUNT 7

typedef enum {
    AVRCP_PDU_ID_GET_CAPABILITIES = 0x10,
    AVRCP_PDU_ID_GetCurrentPlayerApplicationSettingValue = 0x13,
    AVRCP_PDU_ID_SetPlayerApplicationSettingValue = 0x14,
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
    // AVRCP_CMD_OPCODE_RESERVE = 0x01,
    AVRCP_CMD_OPCODE_UNIT_INFO = 0x30,
    // AVRCP_CMD_OPCODE_SUBUNIT_INFO = 0x31,
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

typedef enum {
    AVCTP_CONNECTION_IDLE,
    AVCTP_SIGNALING_W4_SDP_QUERY_COMPLETE,
    AVCTP_CONNECTION_W4_L2CAP_CONNECTED,
    AVCTP_CONNECTION_OPENED,
    AVCTP_W2_SEND_PRESS_COMMAND,
    AVCTP_W2_SEND_RELEASE_COMMAND,
    AVCTP_W4_STOP,
    AVCTP_W2_SEND_COMMAND,
    AVCTP_W2_RECEIVE_PRESS_RESPONSE,
    AVCTP_W2_RECEIVE_RESPONSE
} avctp_connection_state_t;

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
    uint8_t cmd_operands[20];
    uint8_t cmd_operands_length;
    btstack_timer_source_t press_and_hold_cmd_timer;
    uint8_t  continuous_fast_forward_cmd;
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

avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid, avrcp_context_t * context);
avrcp_connection_t * get_avrcp_connection_for_avrcp_cid(uint16_t avrcp_cid, avrcp_context_t * context);
void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid);

#if defined __cplusplus
}
#endif

#endif // __AVRCP_H
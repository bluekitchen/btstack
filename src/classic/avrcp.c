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

#define __BTSTACK_FILE__ "avrcp.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "classic/avrcp.h"

#define PSM_AVCTP                       BLUETOOTH_PROTOCOL_AVCTP
#define PSM_AVCTP_BROWSING              0x001b

/* 
Category 1: Player/Recorder
Category 2: Monitor/Amplifier
Category 3: Tuner
Category 4: Menu
*/

/* controller supported features
Bit 0 = Category 1
Bit 1 = Category 2
Bit 2 = Category 3
Bit 3 = Category 4
Bit 4-5 = RFA
Bit 6 = Supports browsing
Bit 7-15 = RFA
The bits for supported categories are set to 1. Others are set to 0.
*/

/* target supported features
Bit 0 = Category 1 
Bit 1 = Category 2 
Bit 2 = Category 3 
Bit 3 = Category 4
Bit 4 = Player Application Settings. Bit 0 should be set for this bit to be set.
Bit 5 = Group Navigation. Bit 0 should be set for this bit to be set.
Bit 6 = Supports browsing*4
Bit 7 = Supports multiple media player applications
Bit 8-15 = RFA
The bits for supported categories are set to 1. Others are set to 0.
*/

// TODO: merge with avdtp_packet_type_t
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

static int record_id = -1;
static uint16_t  avrcp_cid_counter = 0;
static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

static const char * default_avrcp_controller_service_name = "BTstack AVRCP Controller Service";
static const char * default_avrcp_controller_service_provider_name = "BTstack AVRCP Controller Service Provider";
static const char * default_avrcp_target_service_name = "BTstack AVRCP Target Service";
static const char * default_avrcp_target_service_provider_name = "BTstack AVRCP Target Service Provider";

static btstack_packet_handler_t avrcp_callback;

static avrcp_context_t avrcp_controller_context;

static const char * avrcp_subunit_type_name[] = {
    "MONITOR", "AUDIO", "PRINTER", "DISC", "TAPE_RECORDER_PLAYER", "TUNER", 
    "CA", "CAMERA", "RESERVED", "PANEL", "BULLETIN_BOARD", "CAMERA_STORAGE", 
    "VENDOR_UNIQUE", "RESERVED_FOR_ALL_SUBUNIT_TYPES",
    "EXTENDED_TO_NEXT_BYTE", "UNIT", "ERROR"
};
const char * avrcp_subunit2str(uint16_t index){
    if (index <= 11) return avrcp_subunit_type_name[index];
    if (index >= 0x1C && index <= 0x1F) return avrcp_subunit_type_name[index - 0x10];
    return avrcp_subunit_type_name[16];
}

static const char * avrcp_event_name[] = {
    "ERROR", "PLAYBACK_STATUS_CHANGED",
    "TRACK_CHANGED", "TRACK_REACHED_END", "TRACK_REACHED_START",
    "PLAYBACK_POS_CHANGED", "BATT_STATUS_CHANGED", "SYSTEM_STATUS_CHANGED",
    "PLAYER_APPLICATION_SETTING_CHANGED", "NOW_PLAYING_CONTENT_CHANGED", 
    "AVAILABLE_PLAYERS_CHANGED", "ADDRESSED_PLAYER_CHANGED", "UIDS_CHANGED", "VOLUME_CHANGED"
};
const char * avrcp_event2str(uint16_t index){
    if (index <= 0x0d) return avrcp_event_name[index];
    return avrcp_event_name[0];
}

static const char * avrcp_operation_name[] = {
    "NOT SUPPORTED", // 0x3B
    "SKIP", "NOT SUPPORTED", "NOT SUPPORTED", "NOT SUPPORTED", "NOT SUPPORTED", 
    "VOLUME_UP", "VOLUME_DOWN", "MUTE", "PLAY", "STOP", "PAUSE", "NOT SUPPORTED",
    "REWIND", "FAST_FORWARD", "NOT SUPPORTED", "FORWARD", "BACKWARD" // 0x4C
};
const char * avrcp_operation2str(uint8_t index){
    if (index >= 0x3B && index <= 0x4C) return avrcp_operation_name[index - 0x3B];
    return avrcp_operation_name[0];
}

static const char * avrcp_media_attribute_id_name[] = {
    "NONE", "TITLE", "ARTIST", "ALBUM", "TRACK", "TOTAL TRACKS", "GENRE", "SONG LENGTH"
};
const char * avrcp_attribute2str(uint8_t index){
    if (index >= 1 && index <= 7) return avrcp_media_attribute_id_name[index];
    return avrcp_media_attribute_id_name[0];
}

static const char * avrcp_play_status_name[] = {
    "STOPPED", "PLAYING", "PAUSED", "FORWARD SEEK", "REVERSE SEEK",
    "ERROR" // 0xFF
};
const char * avrcp_play_status2str(uint8_t index){
    if (index >= 1 && index <= 4) return avrcp_play_status_name[index];
    return avrcp_play_status_name[5];
}

static const char * avrcp_ctype_name[] = {
    "CONTROL",
    "STATUS",
    "SPECIFIC_INQUIRY",
    "NOTIFY",
    "GENERAL_INQUIRY",
    "RESERVED5",
    "RESERVED6",
    "RESERVED7",
    "NOT IMPLEMENTED IN REMOTE",
    "ACCEPTED BY REMOTE",
    "REJECTED BY REMOTE",
    "IN_TRANSITION", 
    "IMPLEMENTED_STABLE",
    "CHANGED_STABLE",
    "RESERVED",
    "INTERIM"            
};
const char * avrcp_ctype2str(uint8_t index){
    if (index < sizeof(avrcp_ctype_name)){
        return avrcp_ctype_name[index];
    }
    return "NONE";
}

static const char * avrcp_shuffle_mode_name[] = {
    "SHUFFLE OFF",
    "SHUFFLE ALL TRACKS",
    "SHUFFLE GROUP"
};

const char * avrcp_shuffle2str(uint8_t index){
    if (index >= 1 && index <= 3) return avrcp_shuffle_mode_name[index-1];
    return "NONE";
}

static const char * avrcp_repeat_mode_name[] = {
    "REPEAT OFF",
    "REPEAT SINGLE TRACK",
    "REPEAT ALL TRACKS",
    "REPEAT GROUP"
};

const char * avrcp_repeat2str(uint8_t index){
    if (index >= 1 && index <= 4) return avrcp_repeat_mode_name[index-1];
    return "NONE";
}
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);

static avrcp_sdp_query_context_t sdp_query_context;
static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void avrcp_create_sdp_record(uint8_t controller, uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        if (controller){
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL); 
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER); 
        } else {
            de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET); 
        }
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVCTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avctpProtocol = de_push_sequence(attribute);
        {
            de_add_number(avctpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVCTP);  // avctpProtocol_service
            de_add_number(avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
        }
        de_pop_sequence(attribute, avctpProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *avrcProfile = de_push_sequence(attribute);
        {
            de_add_number(avrcProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL); 
            de_add_number(avrcProfile,  DE_UINT, DE_SIZE_16, 0x0105); 
        }
        de_pop_sequence(attribute, avrcProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x000d "Additional Bluetooth Profile Descriptor List"
    if (browsing){
        de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS);
        attribute = de_push_sequence(service);
        {
            uint8_t * des = de_push_sequence(attribute);
            {
                uint8_t* browsing_l2cpProtocol = de_push_sequence(des);
                {
                    de_add_number(browsing_l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
                    de_add_number(browsing_l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_AVCTP_BROWSING);  
                }
                de_pop_sequence(des, browsing_l2cpProtocol);
                
                uint8_t* browsing_avctpProtocol = de_push_sequence(des);
                {
                    de_add_number(browsing_avctpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVCTP);  // browsing_avctpProtocol_service
                    de_add_number(browsing_avctpProtocol,  DE_UINT, DE_SIZE_16,  0x0103);    // version
                }
                de_pop_sequence(des, browsing_avctpProtocol);
            }
            de_pop_sequence(attribute, des);
        }
        de_pop_sequence(service, attribute);
    }


    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (service_name){
        de_add_data(service,  DE_STRING, strlen(service_name), (uint8_t *) service_name);
    } else {
        if (controller){
            de_add_data(service,  DE_STRING, strlen(default_avrcp_controller_service_name), (uint8_t *) default_avrcp_controller_service_name);
        } else {
            de_add_data(service,  DE_STRING, strlen(default_avrcp_target_service_name), (uint8_t *) default_avrcp_target_service_name);
        }
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        if (controller){
            de_add_data(service,  DE_STRING, strlen(default_avrcp_controller_service_provider_name), (uint8_t *) default_avrcp_controller_service_provider_name);
        } else {
            de_add_data(service,  DE_STRING, strlen(default_avrcp_target_service_provider_name), (uint8_t *) default_avrcp_target_service_provider_name);
        }
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(1, service, service_record_handle, browsing, supported_features, service_name, service_provider_name);
}

void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(0, service, service_record_handle, browsing, supported_features, service_name, service_provider_name);
}

static void avrcp_emit_connection_established(btstack_packet_handler_t callback, uint16_t avrcp_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_repeat_and_shuffle_mode(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t ctype, avrcp_repeat_mode_t repeat_mode, avrcp_shuffle_mode_t shuffle_mode){
    if (!callback) return;
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    event[pos++] = repeat_mode;
    event[pos++] = shuffle_mode;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_operation_status(btstack_packet_handler_t callback, uint8_t subevent, uint16_t avrcp_cid, uint8_t ctype, uint8_t operation_id){
    if (!callback) return;
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    event[pos++] = operation_id;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_connection_closed(btstack_packet_handler_t callback, uint16_t avrcp_cid){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static avrcp_connection_t * get_avrcp_connection_for_bd_addr(bd_addr_t addr, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static avrcp_connection_t * get_avrcp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_signaling_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static avrcp_connection_t * get_avrcp_connection_for_avrcp_cid(uint16_t l2cap_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->avrcp_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static void avrcp_request_can_send_now(avrcp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}

static void avrcp_press_and_hold_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    avrcp_connection_t * connection = btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&connection->press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->press_and_hold_cmd_timer);
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

static void avrcp_press_and_hold_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->press_and_hold_cmd_timer);
    btstack_run_loop_set_timer_handler(&connection->press_and_hold_cmd_timer, avrcp_press_and_hold_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->press_and_hold_cmd_timer, connection);
    btstack_run_loop_set_timer(&connection->press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->press_and_hold_cmd_timer);
}

static void avrcp_press_and_hold_timer_stop(avrcp_connection_t * connection){
    connection->continuous_fast_forward_cmd = 0;   
    btstack_run_loop_remove_timer(&connection->press_and_hold_cmd_timer);
} 

static uint8_t request_pass_through_release_control_cmd(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
    if (connection->continuous_fast_forward_cmd){
        avrcp_press_and_hold_timer_stop(connection);
    }
    connection->cmd_operands[0] = 0x80 | connection->cmd_operands[0];
    connection->transaction_label++;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static inline uint8_t request_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed, uint8_t continuous_fast_forward_cmd, avrcp_context_t * context){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, context);
    if (!connection){
        log_error("avrcp: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    connection->command_opcode =  AVRCP_CMD_OPCODE_PASS_THROUGH;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id =   AVRCP_SUBUNIT_ID;
    connection->cmd_operands_length = 0;

    connection->continuous_fast_forward_cmd = continuous_fast_forward_cmd;
    connection->cmd_operands_length = 2;
    connection->cmd_operands[0] = opid;
    if (playback_speed > 0){
        connection->cmd_operands[2] = playback_speed;
        connection->cmd_operands_length++;
    }
    connection->cmd_operands[1] = connection->cmd_operands_length - 2;
    
    if (connection->continuous_fast_forward_cmd){
        avrcp_press_and_hold_timer_start(connection);
    }

    connection->transaction_label++;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t request_single_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, 0, &avrcp_controller_context);
}

static uint8_t request_continuous_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, 1, &avrcp_controller_context);
}

static int avrcp_send_cmd(uint16_t cid, avrcp_connection_t * connection){
    uint8_t command[30];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    command[pos++] = connection->command_type;
    // subunit_type | subunit ID
    command[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    command[pos++] = (uint8_t)connection->command_opcode;
    // operands
    memcpy(command+pos, connection->cmd_operands, connection->cmd_operands_length);
    pos += connection->cmd_operands_length;

    return l2cap_send(cid, command, pos);
}

static int avrcp_register_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t event_id){
    if (connection->notifications_to_deregister & (1 << event_id)) return 0;
    if (connection->notifications_enabled & (1 << event_id)) return 0;
    if (connection->notifications_to_register & (1 << event_id)) return 0;
    connection->notifications_to_register |= (1 << event_id);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return 1;
}

static void avrcp_prepare_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t event_id){
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_NOTIFY;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_REGISTER_NOTIFICATION;
    connection->cmd_operands[pos++] = 0;                     // reserved(upper 6) | packet_type -> 0
    big_endian_store_16(connection->cmd_operands, pos, 5);     // parameter length
    pos += 2;
    connection->cmd_operands[pos++] = event_id; 
    big_endian_store_32(connection->cmd_operands, pos, 0);
    pos += 4;
    connection->cmd_operands_length = pos;
    // AVRCP_SPEC_V14.pdf 166
    // answer page 61
}

static uint8_t avrcp_cmd_opcode(uint8_t *packet, uint16_t size){
    uint8_t cmd_opcode_index = 5;
    if (cmd_opcode_index > size) return AVRCP_CMD_OPCODE_UNDEFINED;
    return packet[cmd_opcode_index];
}

static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (!sdp_query_context.connection) return;
    if (sdp_query_context.connection->state != AVCTP_SIGNALING_W4_SDP_QUERY_COMPLETE) return;
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    
    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    // uint32_t avdtp_remote_uuid    = 0;
    
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                // log_info("SDP Record: Nr: %d", record_id);
            }

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                            if (de_get_element_type(attribute_value) != DE_DES) break;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                                uint8_t * element = des_iterator_get_element(&des_list_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint32_t uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET:
                                        if (sdp_query_context.avrcp_context->role == AVRCP_CONTROLLER) {
                                            sdp_query_context.role_supported = 1;
                                            break;
                                        }
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL:
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER:
                                        if (sdp_query_context.avrcp_context->role == AVRCP_TARGET) {
                                            sdp_query_context.role_supported = 1;
                                            break;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context.avrcp_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context.avrcp_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS: {
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                if (de_get_element_type(attribute_value) != DE_DES) break;

                                des_iterator_t des_list_0_it;
                                uint8_t       *element_0; 

                                des_iterator_init(&des_list_0_it, attribute_value);
                                element_0 = des_iterator_get_element(&des_list_0_it);

                                for (des_iterator_init(&des_list_it, element_0); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);

                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context.avrcp_browsing_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context.avrcp_browsing_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                log_error("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            log_info("General query done with status %d, role supported %d.\n", sdp_event_query_complete_get_status(packet), sdp_query_context.role_supported);
            if (!sdp_query_context.role_supported || !sdp_query_context.avrcp_l2cap_psm){
                sdp_query_context.connection->state = AVCTP_CONNECTION_IDLE;
                avrcp_emit_connection_established(sdp_query_context.avrcp_context->avrcp_callback, sdp_query_context.connection->avrcp_cid, sdp_query_context.connection->remote_addr, SDP_SERVICE_NOT_FOUND);
                break;                
            } 
            log_info("AVRCP Control PSM 0x%02x, Browsing PSM 0x%02x", sdp_query_context.avrcp_l2cap_psm, sdp_query_context.avrcp_browsing_l2cap_psm);
            sdp_query_context.connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            l2cap_create_channel(sdp_query_context.avrcp_context->packet_handler, sdp_query_context.connection->remote_addr, sdp_query_context.avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
            break;
    }
}

static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t *packet, uint16_t size){
    uint8_t operands[20];
    uint8_t opcode;
    int     pos = 3;
    // uint8_t transport_header = packet[0];
    // uint8_t transaction_label = transport_header >> 4;
    // uint8_t packet_type = (transport_header & 0x0F) >> 2;
    // uint8_t frame_type = (transport_header & 0x03) >> 1;
    // uint8_t ipid = transport_header & 0x01;
    // uint8_t byte_value = packet[2];
    // uint16_t pid = (byte_value << 8) | packet[2];
    
    avrcp_command_type_t ctype = (avrcp_command_type_t) packet[pos++];
    uint8_t byte_value = packet[pos++];
    avrcp_subunit_type_t subunit_type = (avrcp_subunit_type_t) (byte_value >> 3);
    avrcp_subunit_type_t subunit_id = (avrcp_subunit_type_t)   (byte_value & 0x07);
    opcode = packet[pos++];
    
    // printf("    Transport header 0x%02x (transaction_label %d, packet_type %d, frame_type %d, ipid %d), pid 0x%4x\n", 
    //     transport_header, transaction_label, packet_type, frame_type, ipid, pid);
    // // printf_hexdump(packet+pos, size-pos);
    
    uint8_t pdu_id;
    uint16_t param_length;
    switch (avrcp_cmd_opcode(packet,size)){
        case AVRCP_CMD_OPCODE_UNIT_INFO:{
            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE) return;
            connection->state = AVCTP_CONNECTION_OPENED;
            
            // operands:
            memcpy(operands, packet+pos, 5);
            uint8_t unit_type = operands[1] >> 3;
            uint8_t unit = operands[1] & 0x07;
            uint32_t company_id = operands[2] << 16 | operands[3] << 8 | operands[4];
            log_info("    UNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), unit_type 0x%02x, unit %d, company_id 0x%06x",
                ctype, subunit_type, subunit_id, opcode, unit_type, unit, company_id );
            break;
        }
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
            if (size - pos < 7) {
                log_error("avrcp: wrong packet size");
                return;
            };
            // operands:
            memcpy(operands, packet+pos, 7);
            pos += 7;
            // uint32_t company_id = operands[0] << 16 | operands[1] << 8 | operands[2];
            pdu_id = operands[3];

            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE && pdu_id != AVRCP_PDU_ID_REGISTER_NOTIFICATION){
                log_info("AVRCP_CMD_OPCODE_VENDOR_DEPENDENT state %d", connection->state);
                return;
            } 
            connection->state = AVCTP_CONNECTION_OPENED;

            
            // uint8_t unit_type = operands[4] >> 3;
            // uint8_t unit = operands[4] & 0x07;
            param_length = big_endian_read_16(operands, 5);

            // printf("    VENDOR DEPENDENT response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), unit_type 0x%02x, unit %d, company_id 0x%06x\n",
            //     ctype, subunit_type, subunit_id, opcode, unit_type, unit, company_id );

            // if (ctype == AVRCP_CTYPE_RESPONSE_INTERIM) return;
            log_info("        VENDOR DEPENDENT response: pdu id 0x%02x, param_length %d, status %s", pdu_id, param_length, avrcp_ctype2str(ctype));
            switch (pdu_id){
                case AVRCP_PDU_ID_GetCurrentPlayerApplicationSettingValue:{
                    uint8_t num_attributes = packet[pos++];
                    int i;
                    avrcp_repeat_mode_t  repeat_mode =  AVRCP_REPEAT_MODE_INVALID;
                    avrcp_shuffle_mode_t shuffle_mode = AVRCP_SHUFFLE_MODE_INVALID;
                    for (i = 0; i < num_attributes; i++){
                        uint8_t attribute_id    = packet[pos++];
                        uint8_t value = packet[pos++];
                        switch (attribute_id){
                            case 0x02:
                                repeat_mode = (avrcp_repeat_mode_t) value;
                                break;
                            case 0x03:
                                shuffle_mode = (avrcp_shuffle_mode_t) value;
                                break;
                            default:
                                break;
                        }
                    }
                    avrcp_emit_repeat_and_shuffle_mode(avrcp_callback, connection->avrcp_cid, ctype, repeat_mode, shuffle_mode);
                    break;
                }
                case AVRCP_PDU_ID_SetPlayerApplicationSettingValue:{
                    uint8_t event[6];
                    int offset = 0;
                    event[offset++] = HCI_EVENT_AVRCP_META;
                    event[offset++] = sizeof(event) - 2;
                    event[offset++] = AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE;
                    little_endian_store_16(event, offset, connection->avrcp_cid);
                    offset += 2;
                    event[offset++] = ctype;
                    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                case AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME:{
                    uint8_t event[7];
                    int offset = 0;
                    event[offset++] = HCI_EVENT_AVRCP_META;
                    event[offset++] = sizeof(event) - 2;
                    event[offset++] = AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE;
                    little_endian_store_16(event, offset, connection->avrcp_cid);
                    offset += 2;
                    event[offset++] = ctype;
                    event[offset++] = packet[pos++];
                    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                case AVRCP_PDU_ID_GET_CAPABILITIES:{
                    avrcp_capability_id_t capability_id = (avrcp_capability_id_t) packet[pos++];
                    uint8_t capability_count = packet[pos++];
                    int i;
                    switch (capability_id){
                        case AVRCP_CAPABILITY_ID_COMPANY:
                            // log_info("Supported companies %d: ", capability_count);
                            for (i = 0; i < capability_count; i++){
                                uint32_t company_id = big_endian_read_24(packet, pos);
                                pos += 3;
                                log_info("  0x%06x, ", company_id);
                            }
                            break;
                        case AVRCP_CAPABILITY_ID_EVENT:
                            // log_info("Supported events %d: ", capability_count);
                            for (i = 0; i < capability_count; i++){
                                uint8_t event_id = packet[pos++];
                                log_info("  0x%02x %s", event_id, avrcp_event2str(event_id));
                            }
                            break;
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_PLAY_STATUS:{
                    uint32_t song_length = big_endian_read_32(packet, pos);
                    pos += 4;
                    uint32_t song_position = big_endian_read_32(packet, pos);
                    pos += 4;
                    uint8_t play_status = packet[pos];
                    // log_info("        GET_PLAY_STATUS length 0x%04X, position 0x%04X, status %s", song_length, song_position, avrcp_play_status2str(play_status));

                    uint8_t event[15];
                    int offset = 0;
                    event[offset++] = HCI_EVENT_AVRCP_META;
                    event[offset++] = sizeof(event) - 2;
                    event[offset++] = AVRCP_SUBEVENT_PLAY_STATUS;
                    little_endian_store_16(event, offset, connection->avrcp_cid);
                    offset += 2;
                    event[offset++] = ctype;
                    little_endian_store_32(event, offset, song_length);
                    offset += 4;
                    little_endian_store_32(event, offset, song_position);
                    offset += 4;
                    event[offset++] = play_status;
                    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                case AVRCP_PDU_ID_REGISTER_NOTIFICATION:{
                    avrcp_notification_event_id_t  event_id = (avrcp_notification_event_id_t) packet[pos++];
                    uint16_t event_mask = (1 << event_id);
                    uint16_t reset_event_mask = ~event_mask;
                    switch (ctype){
                        case AVRCP_CTYPE_RESPONSE_INTERIM:
                            // register as enabled
                            connection->notifications_enabled |= event_mask;
                            // printf("INTERIM notifications_enabled 0x%2x, notifications_to_register 0x%2x\n", connection->notifications_enabled,  connection->notifications_to_register);
                            break;
                        case AVRCP_CTYPE_RESPONSE_CHANGED_STABLE:
                            // received change, event is considered deregistered
                            // we are re-enabling it automatically, if it is not 
                            // explicitly disabled
                            connection->notifications_enabled &= reset_event_mask;
                            if (! (connection->notifications_to_deregister & event_mask)){
                                avrcp_register_notification(connection, event_id);
                                // printf("CHANGED_STABLE notifications_enabled 0x%2x, notifications_to_register 0x%2x\n", connection->notifications_enabled,  connection->notifications_to_register);
                            } else {
                                connection->notifications_to_deregister &= reset_event_mask;
                            }
                            break;
                        default:
                            connection->notifications_to_register &= reset_event_mask;
                            connection->notifications_enabled &= reset_event_mask;
                            connection->notifications_to_deregister &= reset_event_mask;
                            break;
                    }
                    
                    switch (event_id){
                        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED:{
                            uint8_t event[7];
                            int offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            event[offset++] = packet[pos];
                            (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;
                        }
                        case AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED:{
                            uint8_t event[6];
                            int offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;
                        }
                        case AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED:{
                            uint8_t event[6];
                            int offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;
                        }
                        case AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED:{
                            uint8_t event[6];
                            int offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;
                        }
                        case AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED:{
                            uint8_t event[7];
                            int offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            event[offset++] = packet[pos++] & 0x7F;
                            (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;
                        }
                        // case AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED:{
                        //     uint8_t num_PlayerApplicationSettingAttributes = packet[pos++];
                        //     int i;
                        //     for (i = 0; i < num_PlayerApplicationSettingAttributes; i++){
                        //         uint8_t PlayerApplicationSetting_AttributeID = packet[pos++];
                        //         uint8_t PlayerApplicationSettingValueID = packet[pos++];
                        //     }
                        //     break;
                        // }
                        // case AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED:
                        //     uint16_t player_id = big_endian_read_16(packet, pos);
                        //     pos += 2;
                        //     uint16_t uid_counter = big_endian_read_16(packet, pos);
                        //     pos += 2;
                        //     break;
                        // case AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED:
                        //     uint16_t uid_counter = big_endian_read_16(packet, pos);
                        //     pos += 2;
                        //     break;
                        default:
                            log_info("avrcp: not implemented");
                            break;
                    }
                    if (connection->notifications_to_register != 0){
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    }
                    break;
                }

                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:{
                    uint8_t num_attributes = packet[pos++];
                    int i;
                    struct item {
                        uint16_t len;
                        uint8_t  * value;
                    } items[AVRCP_MEDIA_ATTR_COUNT];
                    memset(items, 0, sizeof(items));

                    uint16_t string_attributes_len = 0;
                    uint8_t  num_string_attributes = 0;
                    uint16_t total_event_payload_for_string_attributes = HCI_EVENT_PAYLOAD_SIZE-2;
                    uint16_t max_string_attribute_value_len = 0;
                    if (ctype == AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE || ctype == AVRCP_CTYPE_RESPONSE_CHANGED_STABLE){
                        for (i = 0; i < num_attributes; i++){
                            avrcp_media_attribute_id_t attr_id = (avrcp_media_attribute_id_t) big_endian_read_32(packet, pos);
                            pos += 4;
                            // uint16_t character_set = big_endian_read_16(packet, pos);
                            pos += 2;
                            uint16_t attr_value_length = big_endian_read_16(packet, pos);
                            pos += 2;

                            // debug - to remove later
                            uint8_t  value[100];
                            uint16_t value_len = sizeof(value) <= attr_value_length? sizeof(value) - 1 : attr_value_length;
                            memcpy(value, packet+pos, value_len);
                            value[value_len] = 0;
                            // printf("Now Playing Info %s: %s \n", attribute2str(attr_id), value);
                            // end debug

                            if ((attr_id >= 1) || (attr_id <= AVRCP_MEDIA_ATTR_COUNT)) {
                                items[attr_id-1].len = attr_value_length;
                                items[attr_id-1].value = &packet[pos];
                                switch (attr_id){
                                    case AVRCP_MEDIA_ATTR_TITLE:
                                    case AVRCP_MEDIA_ATTR_ARTIST:
                                    case AVRCP_MEDIA_ATTR_ALBUM:
                                    case AVRCP_MEDIA_ATTR_GENRE:
                                        num_string_attributes++;
                                        string_attributes_len += attr_value_length;
                                        if (max_string_attribute_value_len < attr_value_length){
                                            max_string_attribute_value_len = attr_value_length;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                            pos += attr_value_length;
                        }
                    }

                    // subtract space for fixed fields
                    total_event_payload_for_string_attributes -= 14 + 4;    // 4 for '\0'

                    // @TODO optimize space by repeatedly decreasing max_string_attribute_value_len until it fits into buffer instead of crude divion
                    uint16_t max_value_len = total_event_payload_for_string_attributes > string_attributes_len? max_string_attribute_value_len : total_event_payload_for_string_attributes/(string_attributes_len+1) - 1;
                    // printf("num_string_attributes %d, string_attributes_len %d, total_event_payload_for_string_attributes %d, max_value_len %d \n", num_string_attributes, string_attributes_len, total_event_payload_for_string_attributes, max_value_len);

                    const uint8_t attribute_order[] = {
                        AVRCP_MEDIA_ATTR_TRACK,
                        AVRCP_MEDIA_ATTR_TOTAL_TRACKS,
                        AVRCP_MEDIA_ATTR_SONG_LENGTH,
                        AVRCP_MEDIA_ATTR_TITLE,
                        AVRCP_MEDIA_ATTR_ARTIST,
                        AVRCP_MEDIA_ATTR_ALBUM,
                        AVRCP_MEDIA_ATTR_GENRE
                    };

                    uint8_t event[HCI_EVENT_BUFFER_SIZE];
                    event[0] = HCI_EVENT_AVRCP_META;
                    pos = 2;
                    event[pos++] = AVRCP_SUBEVENT_NOW_PLAYING_INFO;
                    little_endian_store_16(event, pos, connection->avrcp_cid);
                    pos += 2;
                    event[pos++] = ctype;
                    for (i = 0; i < sizeof(attribute_order); i++){
                        avrcp_media_attribute_id_t attr_id = (avrcp_media_attribute_id_t) attribute_order[i];
                        uint16_t value_len = 0;
                        switch (attr_id){
                            case AVRCP_MEDIA_ATTR_TITLE:
                            case AVRCP_MEDIA_ATTR_ARTIST:
                            case AVRCP_MEDIA_ATTR_ALBUM:
                            case AVRCP_MEDIA_ATTR_GENRE:
                                if (items[attr_id-1].value){
                                    value_len = items[attr_id-1].len <= max_value_len ? items[attr_id-1].len : max_value_len;
                                }
                                event[pos++] = value_len + 1;
                                if (value_len){
                                    memcpy(event+pos, items[attr_id-1].value, value_len);
                                    pos += value_len;
                                }
                                event[pos++] = 0;
                                break;
                            case AVRCP_MEDIA_ATTR_SONG_LENGTH:
                                if (items[attr_id-1].value){
                                    little_endian_store_32(event, pos, btstack_atoi((char *)items[attr_id-1].value));
                                } else {
                                    little_endian_store_32(event, pos, 0);
                                }
                                pos += 4;
                                break;
                            case AVRCP_MEDIA_ATTR_TRACK:
                            case AVRCP_MEDIA_ATTR_TOTAL_TRACKS:
                                if (items[attr_id-1].value){
                                    event[pos++] = btstack_atoi((char *)items[attr_id-1].value);
                                } else {
                                    event[pos++] = 0;
                                }
                                break;
                        }
                    }
                    event[1] = pos - 2;
                    // printf_hexdump(event, pos);
                    (*avrcp_callback)(HCI_EVENT_PACKET, 0, event, pos);
                    break;
                }
                default:
                    break;
            }
            break;
        case AVRCP_CMD_OPCODE_PASS_THROUGH:{
            // 0x80 | connection->cmd_operands[0]
            uint8_t operation_id = packet[pos++];
            switch (connection->state){
                case AVCTP_W2_RECEIVE_PRESS_RESPONSE:
                    if (connection->continuous_fast_forward_cmd){
                        connection->state = AVCTP_W4_STOP;
                    } else {
                        connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
                    }
                    break;
                case AVCTP_W2_RECEIVE_RESPONSE:
                    connection->state = AVCTP_CONNECTION_OPENED;
                    break;
                default:
                    // check for notifications? move state transition down
                    // log_info("AVRCP_CMD_OPCODE_PASS_THROUGH state %d\n", connection->state);
                    break;
            }
            if (connection->state == AVCTP_W4_STOP){
                avrcp_emit_operation_status(avrcp_callback, AVRCP_SUBEVENT_OPERATION_START, connection->avrcp_cid, ctype, operation_id);
            }
            if (connection->state == AVCTP_CONNECTION_OPENED) {
                // RELEASE response
                operation_id = operation_id & 0x7F;
                avrcp_emit_operation_status(avrcp_callback, AVRCP_SUBEVENT_OPERATION_COMPLETE, connection->avrcp_cid, ctype, operation_id);
            }
            if (connection->state == AVCTP_W2_SEND_RELEASE_COMMAND){
                // PRESS response
                request_pass_through_release_control_cmd(connection);
            } 
            break;
        }
        default:
            break;
    }
}

static void avrcp_handle_can_send_now(avrcp_connection_t * connection){
    int i;
    switch (connection->state){
        case AVCTP_W2_SEND_PRESS_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_PRESS_RESPONSE;
            avrcp_send_cmd(connection->l2cap_signaling_cid, connection);
            break;
        case AVCTP_W2_SEND_COMMAND:
        case AVCTP_W2_SEND_RELEASE_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_RESPONSE;
            avrcp_send_cmd(connection->l2cap_signaling_cid, connection);
            break;
        case AVCTP_CONNECTION_OPENED:
            if (connection->notifications_to_register != 0){
                for (i = 1; i <= AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED; i++){
                    if (connection->notifications_to_register & (1<<i)){
                        connection->notifications_to_register &= ~ (1 << i);
                        avrcp_prepare_notification(connection, (avrcp_notification_event_id_t) i);
                        connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                        avrcp_send_cmd(connection->l2cap_signaling_cid, connection);
                        return;    
                    }
                }
            }
            return;
        default:
            return;
    }
}

static uint16_t avdtp_get_next_avrcp_cid(){
    avrcp_cid_counter++;
    if (avrcp_cid_counter == 0){
        avrcp_cid_counter = 1;
    }
    return avrcp_cid_counter;
}

static avrcp_connection_t * avrcp_create_connection(bd_addr_t remote_addr, avrcp_context_t * context){
    avrcp_connection_t * connection = btstack_memory_avrcp_connection_get();
    memset(connection, 0, sizeof(avrcp_connection_t));
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    connection->avrcp_cid = avdtp_get_next_avrcp_cid();
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&context->connections, (btstack_linked_item_t *) connection);
    return connection;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context){
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    avrcp_connection_t * connection = NULL;
    
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avrcp_connection_for_l2cap_signaling_cid(channel, context);
            if (!connection) break;
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            break;

        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit test app
                    // status = hci_event_disconnection_complete_get_status(packet);
                    avrcp_emit_connection_closed(avrcp_callback, 0);
                    break;
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    connection = avrcp_create_connection(event_addr, context);
                    if (!connection) {
                        log_error("Failed to alloc connection structure");
                        l2cap_decline_connection(local_cid);
                        break;
                    }
                    connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
                    connection->l2cap_signaling_cid = local_cid;
                    log_info("L2CAP_EVENT_INCOMING_CONNECTION avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x", connection->avrcp_cid, connection->l2cap_signaling_cid);
                    l2cap_accept_connection(local_cid);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    status = l2cap_event_channel_opened_get_status(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    
                    connection = get_avrcp_connection_for_bd_addr(event_addr, context);
                    if (!connection){
                        log_error("Failed to alloc AVRCP connection structure");
                        avrcp_emit_connection_established(avrcp_callback, connection->avrcp_cid, event_addr, BTSTACK_MEMORY_ALLOC_FAILED);
                        l2cap_disconnect(local_cid, 0); // reason isn't used
                        break;
                    }
                    if (status != ERROR_CODE_SUCCESS){
                        log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                        avrcp_emit_connection_established(avrcp_callback, connection->avrcp_cid, event_addr, status);
                        btstack_linked_list_remove(&context->connections, (btstack_linked_item_t*) connection); 
                        btstack_memory_avrcp_connection_free(connection);
                        break;
                    }
                    connection->l2cap_signaling_cid = local_cid;
                    log_info("L2CAP_EVENT_CHANNEL_OPENED avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x", connection->avrcp_cid, connection->l2cap_signaling_cid);
                    connection->state = AVCTP_CONNECTION_OPENED;
                    avrcp_emit_connection_established(avrcp_callback, connection->avrcp_cid, event_addr, ERROR_CODE_SUCCESS);
                    break;
                
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(channel, context);
                    if (!connection) break;
                    avrcp_handle_can_send_now(connection);
                    break;

                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(local_cid, context);
                    if (connection){
                        avrcp_emit_connection_closed(avrcp_callback, connection->avrcp_cid);
                        // free connection
                        btstack_linked_list_remove(&context->connections, (btstack_linked_item_t*) connection); 
                        btstack_memory_avrcp_connection_free(connection);
                        break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    packet_handler(packet_type, channel, packet, size, &avrcp_controller_context);
}

void avrcp_controller_init(void){
    avrcp_controller_context.role = AVRCP_CONTROLLER;
    avrcp_controller_context.connections = NULL;
    avrcp_controller_context.avrcp_callback = avrcp_callback;
    avrcp_controller_context.packet_handler = avrcp_controller_packet_handler;
    l2cap_register_service(&avrcp_controller_packet_handler, BLUETOOTH_PROTOCOL_AVCTP, 0xffff, LEVEL_0);
}

void avrcp_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_callback = callback;
    avrcp_controller_context.avrcp_callback = avrcp_callback;
}

uint8_t avrcp_connect(bd_addr_t bd_addr, avrcp_context_t * context, uint16_t * avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_bd_addr(bd_addr, context);
    if (connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    connection = avrcp_create_connection(bd_addr, context);
    if (!connection){
        log_error("avrcp: could not allocate connection struct.");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    if (!avrcp_cid) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;

    *avrcp_cid = connection->avrcp_cid; 
    connection->state = AVCTP_SIGNALING_W4_SDP_QUERY_COMPLETE;
    sdp_query_context.avrcp_context = context;
    sdp_query_context.avrcp_l2cap_psm = 0;
    sdp_query_context.avrcp_version = 0;
    sdp_query_context.connection = connection;

    sdp_client_query_uuid16(&avrcp_handle_sdp_client_query_result, bd_addr, BLUETOOTH_PROTOCOL_AVCTP);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_connect(bd_addr_t bd_addr, uint16_t * avrcp_cid){
    return avrcp_connect(bd_addr, &avrcp_controller_context, avrcp_cid);
}

uint8_t avrcp_unit_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_UNIT_INFO;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    memset(connection->cmd_operands, 0xFF, connection->cmd_operands_length);
    connection->cmd_operands_length = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_get_capabilities(uint16_t avrcp_cid, uint8_t capability_id){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    big_endian_store_24(connection->cmd_operands, 0, BT_SIG_COMPANY_ID);
    connection->cmd_operands[3] = AVRCP_PDU_ID_GET_CAPABILITIES; // PDU ID
    connection->cmd_operands[4] = 0;
    big_endian_store_16(connection->cmd_operands, 5, 1); // parameter length
    connection->cmd_operands[7] = capability_id;                  // capability ID
    connection->cmd_operands_length = 8;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_get_supported_company_ids(uint16_t avrcp_cid){
    return avrcp_get_capabilities(avrcp_cid, AVRCP_CAPABILITY_ID_COMPANY);
}

uint8_t avrcp_get_supported_events(uint16_t avrcp_cid){
    return avrcp_get_capabilities(avrcp_cid, AVRCP_CAPABILITY_ID_EVENT);
}


uint8_t avrcp_play(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PLAY, 0);
}

uint8_t avrcp_stop(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_STOP, 0);
}

uint8_t avrcp_pause(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PAUSE, 0);
}

uint8_t avrcp_forward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FORWARD, 0);
} 

uint8_t avrcp_backward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_BACKWARD, 0);
}

uint8_t avrcp_start_rewind(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_REWIND, 0);
}

uint8_t avrcp_volume_up(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_UP, 0);
}

uint8_t avrcp_volume_down(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_DOWN, 0);
}

uint8_t avrcp_mute(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_MUTE, 0);
}

uint8_t avrcp_skip(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_SKIP, 0);
}

uint8_t avrcp_stop_rewind(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_stop_rewind: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_W4_STOP) return ERROR_CODE_COMMAND_DISALLOWED;
    return request_pass_through_release_control_cmd(connection);
}

uint8_t avrcp_start_fast_forward(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FAST_FORWARD, 0);
}

uint8_t avrcp_fast_forward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FAST_FORWARD, 0);
}

uint8_t avrcp_rewind(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_REWIND, 0);
}


uint8_t avrcp_stop_fast_forward(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_stop_fast_forward: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_W4_STOP) return ERROR_CODE_COMMAND_DISALLOWED;
    return request_pass_through_release_control_cmd(connection);
}

uint8_t avrcp_get_play_status(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    big_endian_store_24(connection->cmd_operands, 0, BT_SIG_COMPANY_ID);
    connection->cmd_operands[3] = AVRCP_PDU_ID_GET_PLAY_STATUS;
    connection->cmd_operands[4] = 0;                     // reserved(upper 6) | packet_type -> 0
    big_endian_store_16(connection->cmd_operands, 5, 0); // parameter length
    connection->cmd_operands_length = 7;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_enable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_register_notification(connection, event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_disable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->notifications_to_deregister |= (1 << event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_get_now_playing_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES; // PDU ID
    connection->cmd_operands[pos++] = 0;
    
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 9);
    pos += 2;

    // write 8 bytes value
    memset(connection->cmd_operands + pos, 0, 8); // identifier: PLAYING
    pos += 8;

    connection->cmd_operands[pos++] = 0; // attribute count, if 0 get all attributes
    // every attribute is 4 bytes long

    connection->cmd_operands_length = pos;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_set_absolute_volume(uint16_t avrcp_cid, uint8_t volume){
     avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME; // PDU ID
    connection->cmd_operands[pos++] = 0;
    
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = volume;

    connection->cmd_operands_length = pos;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_query_shuffle_and_repeat_modes(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    big_endian_store_24(connection->cmd_operands, 0, BT_SIG_COMPANY_ID);
    connection->cmd_operands[3] = AVRCP_PDU_ID_GetCurrentPlayerApplicationSettingValue; // PDU ID
    connection->cmd_operands[4] = 0;
    big_endian_store_16(connection->cmd_operands, 5, 5); // parameter length
    connection->cmd_operands[7] = 4;                     // NumPlayerApplicationSettingAttributeID
    // PlayerApplicationSettingAttributeID1 AVRCP Spec, Appendix F, 133
    connection->cmd_operands[8]  = 0x01;    // equalizer  (1-OFF, 2-ON)     
    connection->cmd_operands[9]  = 0x02;    // repeat     (1-off, 2-single track, 3-all tracks, 4-group repeat)
    connection->cmd_operands[10] = 0x03;    // shuffle    (1-off, 2-all tracks, 3-group shuffle)
    connection->cmd_operands[11] = 0x04;    // scan       (1-off, 2-all tracks, 3-group scan)
    connection->cmd_operands_length = 12;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_set_current_player_application_setting_value(uint16_t avrcp_cid, uint8_t attr_id, uint8_t attr_value){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_label++;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SetPlayerApplicationSettingValue; // PDU ID
    connection->cmd_operands[pos++] = 0;
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 3);
    pos += 2;
    connection->cmd_operands[pos++] = 2;
    connection->cmd_operands_length = pos;
    connection->cmd_operands[pos++]  = attr_id;
    connection->cmd_operands[pos++]  = attr_value;
    connection->cmd_operands_length = pos;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_set_shuffle_mode(uint16_t avrcp_cid, avrcp_shuffle_mode_t mode){
    if (mode < AVRCP_SHUFFLE_MODE_OFF || mode > AVRCP_SHUFFLE_MODE_GROUP) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    return avrcp_set_current_player_application_setting_value(avrcp_cid, 0x03, mode);
}

uint8_t avrcp_set_repeat_mode(uint16_t avrcp_cid, avrcp_repeat_mode_t mode){
    if (mode < AVRCP_REPEAT_MODE_OFF || mode > AVRCP_REPEAT_MODE_GROUP) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    return avrcp_set_current_player_application_setting_value(avrcp_cid, 0x02, mode);
}

uint8_t avrcp_disconnect(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_controller_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    l2cap_disconnect(connection->l2cap_signaling_cid, 0);
    return ERROR_CODE_SUCCESS;
}
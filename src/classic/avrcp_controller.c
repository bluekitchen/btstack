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

#define BTSTACK_FILE__ "avrcp_controller.c"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "classic/avrcp.h"
#include "classic/avrcp_controller.h"

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "l2cap.h"

#define AVRCP_CMD_BUFFER_SIZE 30

// made public in avrcp_controller.h
avrcp_context_t avrcp_controller_context;

static uint8_t avrcp_controller_calc_next_transaction_label(uint8_t current_transaction_label){
    current_transaction_label++;
    if (current_transaction_label == 16){
        current_transaction_label = 1;
    }
    return current_transaction_label;
}

static uint8_t avrcp_controller_get_next_transaction_label(avrcp_connection_t * connection){
    connection->transaction_id_counter = avrcp_controller_calc_next_transaction_label(connection->transaction_id_counter);
    return connection->transaction_id_counter;
}

static bool avrcp_controller_is_transaction_id_valid(avrcp_connection_t * connection, uint8_t transaction_id){
    uint8_t delta = ((int8_t) transaction_id - connection->last_confirmed_transaction_id) & 0x0f;
    return delta < 15;
}

static uint16_t avrcp_get_max_payload_size_for_packet_type(avrcp_packet_type_t packet_type){
    switch (packet_type){
        case AVRCP_SINGLE_PACKET:
            return AVRCP_CMD_BUFFER_SIZE - 3;
        case AVRCP_START_PACKET:
            return AVRCP_CMD_BUFFER_SIZE - 4;
        case AVRCP_CONTINUE_PACKET:
        case AVRCP_END_PACKET:
            return AVRCP_CMD_BUFFER_SIZE - 1;
        default:
            btstack_assert(false);
            return 0;
    }
}

static int avrcp_controller_supports_browsing(uint16_t controller_supported_features){
    return controller_supported_features & AVRCP_FEATURE_MASK_BROWSING;
}

static void avrcp_controller_emit_notification_for_event_id(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id,
                                                            avrcp_command_type_t ctype, const uint8_t *payload,
                                                            uint16_t size) {
    switch (event_id){
        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED:{
            if (size < 4) break;
            uint32_t song_position = big_endian_read_32(payload, 0);
            uint16_t offset = 0;
            uint8_t event[10];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_POS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            little_endian_store_32(event, offset, song_position);
            offset += 4;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED:{
            if (size < 1) break;
            uint16_t offset = 0;
            uint8_t event[7];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            event[offset++] = payload[0];
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED:{
            uint16_t offset = 0;
            uint8_t event[6];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED:{
            uint16_t offset = 0;
            uint8_t event[6];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED:{
            uint16_t offset = 0;
            uint8_t event[6];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED:{
            if (size < 1) break;
            uint16_t offset = 0;
            uint8_t event[7];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            event[offset++] = payload[0] & 0x7F;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED:{
            if (size < 2) break;
            uint8_t event[8];
            uint16_t offset = 0;
            uint16_t uuid = big_endian_read_16(payload, 0);
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_EVENT_UIDS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            little_endian_store_16(event, offset, uuid);
            offset += 2;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }

        case AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END:{
            uint16_t offset = 0;
            uint8_t event[6];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_EVENT_TRACK_REACHED_END;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START:{
            uint16_t offset = 0;
            uint8_t event[6];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_EVENT_TRACK_REACHED_START;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED:{
            if (size < 1) break;
            uint16_t offset = 0;
            uint8_t event[7];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_EVENT_BATT_STATUS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            event[offset++] = payload[0];
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }

        case AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED:{
            if (size < 1) break;
            uint16_t offset = 0;
            uint8_t event[7];
            event[offset++] = HCI_EVENT_AVRCP_META;
            event[offset++] = sizeof(event) - 2;
            event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED;
            little_endian_store_16(event, offset, avrcp_cid);
            offset += 2;
            event[offset++] = ctype;
            event[offset++] = payload[0];
            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }

        case AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED:
        default:
            log_info("avrcp: not implemented");
            break;
    }
}

static void avrcp_controller_emit_repeat_and_shuffle_mode(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t ctype, avrcp_repeat_mode_t repeat_mode, avrcp_shuffle_mode_t shuffle_mode){
    btstack_assert(callback != NULL);
    
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

static void avrcp_controller_emit_now_playing_info_event_done(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t ctype, uint8_t status){
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_NOW_PLAYING_INFO_DONE;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void avrcp_controller_emit_now_playing_info_event(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t ctype, avrcp_media_attribute_id_t attr_id, uint8_t * value, uint16_t value_len){
    uint8_t event[HCI_EVENT_BUFFER_SIZE];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    // reserve one byte for subevent type and data len
    int data_len_pos = pos;
    pos++;
    int subevent_type_pos = pos;
    pos++;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    
    switch (attr_id){
        case AVRCP_MEDIA_ATTR_TITLE:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO;
            event[pos++] = value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_ARTIST:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO;
            event[pos++] = value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_ALBUM:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO;
            event[pos++] = value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_GENRE:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO;
            event[pos++] = value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_SONG_LENGTH_MS:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_SONG_LENGTH_MS_INFO;
            if (value){
                little_endian_store_32(event, pos, btstack_atoi((char *)value));
            } else {
                little_endian_store_32(event, pos, 0);
            }
            pos += 4;
            break;
        case AVRCP_MEDIA_ATTR_TRACK:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO;
            if (value){
                event[pos++] = btstack_atoi((char *)value);
            } else {
                event[pos++] = 0;
            }
            break;
        case AVRCP_MEDIA_ATTR_TOTAL_NUM_ITEMS:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO;
            if (value){
                event[pos++] = btstack_atoi((char *)value);
            } else {
                event[pos++] = 0;
            }
            break;
        default:
            break;
    }
    event[data_len_pos] = pos - 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void avrcp_controller_emit_operation_status(btstack_packet_handler_t callback, uint8_t subevent, uint16_t avrcp_cid, uint8_t ctype, uint8_t operation_id){
    btstack_assert(callback != NULL);
    
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

static void avrcp_parser_reset(avrcp_connection_t * connection){
    connection->list_offset = 0;
    connection->num_attributes = 0;
    connection->num_parsed_attributes = 0;
    connection->parser_attribute_header_pos = 0;
    connection->num_received_fragments = 0;
    connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_HEADER;
}

static void avrcp_parser_process_byte(uint8_t byte, avrcp_connection_t * connection, avrcp_command_type_t ctype){
    uint16_t attribute_total_value_len;
    uint32_t attribute_id;
    switch(connection->parser_state){
        case AVRCP_PARSER_GET_ATTRIBUTE_HEADER:
            connection->parser_attribute_header[connection->parser_attribute_header_pos++] = byte;
            connection->list_offset++;

            if (connection->parser_attribute_header_pos < AVRCP_ATTRIBUTE_HEADER_LEN) return;

            attribute_total_value_len = big_endian_read_16(connection->parser_attribute_header, 6);
            connection->attribute_value_len = btstack_min(attribute_total_value_len, AVRCP_MAX_ATTRIBUTTE_SIZE);
            if (connection->attribute_value_len > 0){
                // get ready for attribute value
                connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_VALUE;
                return;
            }
            
            // emit empty attribute
            attribute_id = big_endian_read_32(connection->parser_attribute_header, 0);
            avrcp_controller_emit_now_playing_info_event(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, (avrcp_media_attribute_id_t) attribute_id, connection->attribute_value, connection->attribute_value_len);

            // done, see below
            break;

        case AVRCP_PARSER_GET_ATTRIBUTE_VALUE:
            connection->attribute_value[connection->attribute_value_offset++] = byte;
            connection->list_offset++;

            if (connection->attribute_value_offset < connection->attribute_value_len) return;
            
            // emit (potentially partial) attribute
            attribute_id = big_endian_read_32(connection->parser_attribute_header, 0);
            avrcp_controller_emit_now_playing_info_event(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, (avrcp_media_attribute_id_t) attribute_id, connection->attribute_value, connection->attribute_value_len);

            attribute_total_value_len = big_endian_read_16(connection->parser_attribute_header, 6);
            if (connection->attribute_value_offset < attribute_total_value_len){
                // ignore rest of attribute
                connection->parser_state = AVRCP_PARSER_IGNORE_REST_OF_ATTRIBUTE_VALUE;
                return;
            }

            // done, see below
            break;

        case AVRCP_PARSER_IGNORE_REST_OF_ATTRIBUTE_VALUE:
            connection->attribute_value_offset++;
            connection->list_offset++;

            attribute_total_value_len = big_endian_read_16(connection->parser_attribute_header, 6);
            if (connection->attribute_value_offset < attribute_total_value_len) return;

            // done, see below
            break;

        default:
            return;
    }

    // attribute fully read, check if more to come
    if (connection->list_offset < connection->list_size){
        // more to come, reset parser
        connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_HEADER;
        connection->parser_attribute_header_pos = 0;
        connection->attribute_value_offset = 0;
    } else {
        // fully done
        avrcp_parser_reset(connection);
        avrcp_controller_emit_now_playing_info_event_done(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, 0);
    }
}

static void avrcp_controller_parse_and_emit_element_attrs(uint8_t * packet, uint16_t num_bytes_to_read, avrcp_connection_t * connection, avrcp_command_type_t ctype){
    int i;
    for (i=0;i<num_bytes_to_read;i++){
        avrcp_parser_process_byte(packet[i], connection, ctype);
    }
}


static int avrcp_send_cmd(avrcp_connection_t * connection, avrcp_packet_type_t packet_type){
    uint8_t  command[AVRCP_CMD_BUFFER_SIZE];
    uint16_t pos = 0; 

    // non-fragmented: transport header (1) + PID (2)
    // fragmented:     transport header (1) + num packets (1) + PID (2)

    // transport header : transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_id << 4) | (packet_type << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;

    if (packet_type == AVRCP_START_PACKET){
        // num packets: (3 bytes overhead (PID, num packets) + command) / (MTU - transport header). 
        // to get number of packets using integer division, we subtract 1 from the data e.g. len = 5, packet size 5 => need 1 packet
        command[pos++] = ((connection->cmd_operands_fragmented_len + 3 - 1) / (AVRCP_CMD_BUFFER_SIZE - 1)) + 1;
    }

    if ((packet_type == AVRCP_SINGLE_PACKET) || (packet_type == AVRCP_START_PACKET)){
        // Profile IDentifier (PID)
        command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
        command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

        // command_type
        command[pos++] = connection->command_type;
        // subunit_type | subunit ID
        command[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
        // opcode
        command[pos++] = (uint8_t)connection->command_opcode;
    }

    if (packet_type == AVRCP_SINGLE_PACKET){
        // operands
        (void)memcpy(command + pos, connection->cmd_operands,
                     connection->cmd_operands_length);
        pos += connection->cmd_operands_length;
    } else {
        uint16_t bytes_free = AVRCP_CMD_BUFFER_SIZE - pos;
        uint16_t bytes_to_store = connection->cmd_operands_fragmented_len-connection->cmd_operands_fragmented_pos;
        uint16_t bytes_to_copy = btstack_min(bytes_to_store, bytes_free);
        (void)memcpy(command + pos,
                     &connection->cmd_operands_fragmented_buffer[connection->cmd_operands_fragmented_pos],
                     bytes_to_copy);
        pos += bytes_to_copy;
        connection->cmd_operands_fragmented_pos += bytes_to_copy;
    }

    return l2cap_send(connection->l2cap_signaling_cid, command, pos);
}

static int avrcp_send_register_notification(avrcp_connection_t * connection, uint8_t event_id){
    uint8_t command[18];
    uint16_t pos = 0; 
    // transport header : transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    command[pos++] = (connection->transaction_id << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_CTYPE_NOTIFY;
    command[pos++] = (AVRCP_SUBUNIT_TYPE_PANEL << 3) | AVRCP_SUBUNIT_ID;
    command[pos++] = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;

    big_endian_store_24(command, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    command[pos++] = AVRCP_PDU_ID_REGISTER_NOTIFICATION;
    command[pos++] = 0;                       // reserved(upper 6) | packet_type -> 0
    big_endian_store_16(command, pos, 5);     // parameter length
    pos += 2;
    command[pos++] = event_id; 
    big_endian_store_32(command, pos, 1);     // send notification on playback position every second, for other notifications it is ignored
    pos += 4;
    return l2cap_send(connection->l2cap_signaling_cid, command, pos);
}

static void avrcp_press_and_hold_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    avrcp_connection_t * connection = (avrcp_connection_t*) btstack_run_loop_get_timer_context(timer);
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
    connection->press_and_hold_cmd = 0;
    btstack_run_loop_remove_timer(&connection->press_and_hold_cmd_timer);
} 


static uint8_t avrcp_controller_request_pass_through_release_control_cmd(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
    if (connection->press_and_hold_cmd){
        avrcp_press_and_hold_timer_stop(connection);
    }
    connection->cmd_operands[0] = 0x80 | connection->cmd_operands[0];
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static inline uint8_t avrcp_controller_request_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed, bool continuous_fast_forward_cmd){
    log_info("Send command %d", opid);
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("Could not find a connection. avrcp cid 0x%02x", avrcp_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state %d, expected %d. avrcp cid 0x%02x", connection->state, AVCTP_CONNECTION_OPENED, avrcp_cid);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    connection->command_opcode =  AVRCP_CMD_OPCODE_PASS_THROUGH;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id =   AVRCP_SUBUNIT_ID;
    connection->cmd_operands_length = 0;

    connection->press_and_hold_cmd = continuous_fast_forward_cmd;
    connection->cmd_operands_length = 2;
    connection->cmd_operands[0] = opid;
    if (playback_speed > 0){
        connection->cmd_operands[2] = playback_speed;
        connection->cmd_operands_length++;
    }
    connection->cmd_operands[1] = connection->cmd_operands_length - 2;
    
    if (connection->press_and_hold_cmd){
        avrcp_press_and_hold_timer_start(connection);
    }

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t request_single_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return avrcp_controller_request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, false);
}

static uint8_t request_continuous_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return avrcp_controller_request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, true);
}

static int avrcp_controller_register_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t event_id){
    if (connection->notifications_to_deregister & (1 << event_id)) return 0;
    if (connection->notifications_enabled & (1 << event_id)) return 0;
    if (connection->notifications_to_register & (1 << event_id)) return 0;
    connection->notifications_to_register |= (1 << event_id);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return 1;
}

static uint8_t avrcp_controller_request_abort_continuation(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_COMMAND;
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE; // PDU ID
    connection->cmd_operands[pos++] = 0;
    // Parameter Length
    connection->cmd_operands_length = 8;
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}


static uint8_t avrcp_controller_request_continue_response(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_COMMAND;
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE; // PDU ID
    connection->cmd_operands[pos++] = 0;
    // Parameter Length
    connection->cmd_operands_length = 8;
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static void
avrcp_controller_handle_notification(avrcp_connection_t *connection, avrcp_command_type_t ctype, uint8_t *payload, uint16_t size) {
    if (size < 1) return;
    uint16_t pos = 0;
    avrcp_notification_event_id_t event_id = (avrcp_notification_event_id_t) payload[pos++];
    uint16_t event_mask = (1 << event_id);
    uint16_t reset_event_mask = ~event_mask;
    switch (ctype){
        case AVRCP_CTYPE_RESPONSE_INTERIM:
            // register as enabled
            connection->notifications_enabled |= event_mask;
            break;
        case AVRCP_CTYPE_RESPONSE_CHANGED_STABLE:
            // received change, event is considered deregistered
            // we are re-enabling it automatically, if it is not 
            // explicitly disabled
            connection->notifications_enabled &= reset_event_mask;
            if (! (connection->notifications_to_deregister & event_mask)){
                avrcp_controller_register_notification(connection, event_id);
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

    avrcp_controller_emit_notification_for_event_id(connection->avrcp_cid, event_id, ctype, payload + pos, size - pos);
}

#ifdef ENABLE_AVCTP_FRAGMENTATION
static void avctp_reassemble_message(avrcp_connection_t * connection, avctp_packet_type_t packet_type, uint8_t *packet, uint16_t size){
    // after header (transaction label and packet type)
    uint16_t pos;
    uint16_t bytes_to_store;
    
    switch (packet_type){
        case AVCTP_START_PACKET:
            if (size < 2) return;
            
            // store header
            pos = 0;
            connection->avctp_reassembly_buffer[pos] = packet[pos];
            pos++;
            connection->avctp_reassembly_size = pos;

            // NOTE: num packets not needed for reassembly, ignoring it does not pose security risk -> no need to store it
            pos++;
            
            // PID in reassembled packet is at offset 1, it will be read later after the avctp_reassemble_message with AVCTP_END_PACKET is called
            
            bytes_to_store = btstack_min(size - pos, sizeof(connection->avctp_reassembly_buffer) - connection->avctp_reassembly_size);
            memcpy(&connection->avctp_reassembly_buffer[connection->avctp_reassembly_size], &packet[pos], bytes_to_store);
            connection->avctp_reassembly_size += bytes_to_store;
            break;
        
        case AVCTP_CONTINUE_PACKET:
        case AVCTP_END_PACKET:
            if (size < 1) return;
            
            // store remaining data, ignore header
            pos = 1;
            bytes_to_store = btstack_min(size - pos, sizeof(connection->avctp_reassembly_buffer) - connection->avctp_reassembly_size);
            memcpy(&connection->avctp_reassembly_buffer[connection->avctp_reassembly_size], &packet[pos], bytes_to_store);
            connection->avctp_reassembly_size += bytes_to_store;
            break;
        
        default:
            return;
    }
}
#endif

static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t *packet, uint16_t size){
    if (size < 6u) return;
    uint8_t  pdu_id;
    avrcp_packet_type_t  vendor_dependent_packet_type;

    uint16_t pos = 0;
    connection->last_confirmed_transaction_id = packet[pos] >> 4;
    avrcp_frame_type_t  frame_type =  (avrcp_frame_type_t)((packet[pos] >> 1) & 0x01);
    avctp_packet_type_t packet_type = (avctp_packet_type_t)((packet[pos] >> 2) & 0x03);
    pos++;

    if (frame_type != AVRCP_RESPONSE_FRAME) return;
        
    switch (packet_type){
        case AVCTP_SINGLE_PACKET:
            break;
        
#ifdef ENABLE_AVCTP_FRAGMENTATION
        case AVCTP_START_PACKET:
        case AVCTP_CONTINUE_PACKET:
            avctp_reassemble_message(connection, packet_type, packet, size);
            return;
        
        case AVCTP_END_PACKET:
            avctp_reassemble_message(connection, packet_type, packet, size);

            packet = connection->avctp_reassembly_buffer;
            size   = connection->avctp_reassembly_size; 
            break;
#endif
        
        default:
            return;
    }

    pos += 2; // PID

    avrcp_command_type_t ctype = (avrcp_command_type_t) packet[pos++];
    
#ifdef ENABLE_LOG_INFO
    uint8_t byte_value = packet[pos];
    avrcp_subunit_type_t subunit_type = (avrcp_subunit_type_t) (byte_value >> 3);
    avrcp_subunit_type_t subunit_id   = (avrcp_subunit_type_t) (byte_value & 0x07);
#endif
    pos++;
    
    uint8_t opcode = packet[pos++];
    uint16_t param_length;

    switch (opcode){
        case AVRCP_CMD_OPCODE_SUBUNIT_INFO:{
            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE) return;
            connection->state = AVCTP_CONNECTION_OPENED;

#ifdef ENABLE_LOG_INFO
            // page, extention code (1)
            pos++;
            uint8_t unit_type = packet[pos] >> 3;
            uint8_t max_subunit_ID = packet[pos] & 0x07;
            log_info("SUBUNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), unit_type 0x%02x, max_subunit_ID %d", ctype, subunit_type, subunit_id, opcode, unit_type, max_subunit_ID);
#endif
            break;
        }
        case AVRCP_CMD_OPCODE_UNIT_INFO:{
            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE) return;
            connection->state = AVCTP_CONNECTION_OPENED;

#ifdef ENABLE_LOG_INFO
            // byte value 7 (1)
            pos++;
            uint8_t unit_type = packet[pos] >> 3;
            uint8_t unit = packet[pos] & 0x07;
            pos++;
            uint32_t company_id = big_endian_read_24(packet, pos);
            log_info("UNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), unit_type 0x%02x, unit %d, company_id 0x%06" PRIx32,
                ctype, subunit_type, subunit_id, opcode, unit_type, unit, company_id);
#endif
            break;
        }
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:

            if ((size - pos) < 7) return;

            // Company ID (3)
            pos += 3;
            pdu_id = packet[pos++];
            vendor_dependent_packet_type = (avrcp_packet_type_t)(packet[pos++] & 0x03);            
            param_length = big_endian_read_16(packet, pos);
            pos += 2;
            
            if ((size - pos) < param_length) return;

            // handle asynchronous notifications, without changing state
            if (pdu_id == AVRCP_PDU_ID_REGISTER_NOTIFICATION){
                avrcp_controller_handle_notification(connection, ctype, packet + pos, size - pos);
                break;
            }

            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE){
                log_info("AVRCP_CMD_OPCODE_VENDOR_DEPENDENT state %d", connection->state);
                return;
            } 
            connection->state = AVCTP_CONNECTION_OPENED;

            log_info("VENDOR DEPENDENT response: pdu id 0x%02x, param_length %d, status %s", pdu_id, param_length, avrcp_ctype2str(ctype));
            switch (pdu_id){
                case AVRCP_PDU_ID_GET_CURRENT_PLAYER_APPLICATION_SETTING_VALUE:{
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
                    avrcp_controller_emit_repeat_and_shuffle_mode(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, repeat_mode, shuffle_mode);
                    break;
                }
                
                case AVRCP_PDU_ID_SET_PLAYER_APPLICATION_SETTING_VALUE:{
                    uint16_t offset = 0;
                    uint8_t event[6];
                    event[offset++] = HCI_EVENT_AVRCP_META;
                    event[offset++] = sizeof(event) - 2;
                    event[offset++] = AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE;
                    little_endian_store_16(event, offset, connection->avrcp_cid);
                    offset += 2;
                    event[offset++] = ctype;
                    (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                
                case AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME:{
                    uint16_t offset = 0;
                    uint8_t event[7];
                    event[offset++] = HCI_EVENT_AVRCP_META;
                    event[offset++] = sizeof(event) - 2;
                    event[offset++] = AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE;
                    little_endian_store_16(event, offset, connection->avrcp_cid);
                    offset += 2;
                    event[offset++] = ctype;
                    event[offset++] = packet[pos++];
                    (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                
                case AVRCP_PDU_ID_GET_CAPABILITIES:{
                    avrcp_capability_id_t capability_id = (avrcp_capability_id_t) packet[pos++];
                    uint8_t capability_count = packet[pos++];
                    uint16_t i;
                    uint16_t offset = 0;
                    uint8_t event[10];

                    switch (capability_id){

                        case AVRCP_CAPABILITY_ID_COMPANY:
                            for (i = 0; i < capability_count; i++){
                                uint32_t company_id = big_endian_read_24(packet, pos);
                                pos += 3;
                                log_info("  0x%06" PRIx32 ", ", company_id);

                                offset = 0;
                                event[offset++] = HCI_EVENT_AVRCP_META;
                                event[offset++] = sizeof(event) - 2;
                                event[offset++] = AVRCP_SUBEVENT_GET_CAPABILITY_COMPANY_ID;
                                little_endian_store_16(event, offset, connection->avrcp_cid);
                                offset += 2;
                                event[offset++] = ctype;
                                event[offset++] = 0;
                                little_endian_store_24(event, offset, company_id);
                                offset += 3;
                                (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, offset);
                                break;
                            }
                            
                            offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_GET_CAPABILITY_COMPANY_ID_DONE;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            event[offset++] = 0;
                            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, offset);
                            break;

                        case AVRCP_CAPABILITY_ID_EVENT:
                            for (i = 0; i < capability_count; i++){
                                uint8_t event_id = packet[pos++];
                                log_info("  0x%02x %s", event_id, avrcp_event2str(event_id));
                                
                                offset = 0;
                                event[offset++] = HCI_EVENT_AVRCP_META;
                                event[offset++] = sizeof(event) - 2;
                                event[offset++] = AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID;
                                little_endian_store_16(event, offset, connection->avrcp_cid);
                                offset += 2;
                                event[offset++] = ctype;
                                event[offset++] = 0;
                                event[offset++] = event_id;
                                (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, offset);
                            }

                            offset = 0;
                            event[offset++] = HCI_EVENT_AVRCP_META;
                            event[offset++] = sizeof(event) - 2;
                            event[offset++] = AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID_DONE;
                            little_endian_store_16(event, offset, connection->avrcp_cid);
                            offset += 2;
                            event[offset++] = ctype;
                            event[offset++] = 0;
                            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                            break;

                        default:
                            btstack_assert(false);
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
                    (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                    break;
                }
                
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:{
                    switch (vendor_dependent_packet_type){
                        case AVRCP_START_PACKET:
                        case AVRCP_SINGLE_PACKET:
                            avrcp_parser_reset(connection);
                            connection->list_size = param_length;
                            connection->num_attributes = packet[pos++];

                            avrcp_controller_parse_and_emit_element_attrs(packet+pos, size-pos, connection, ctype);
                            if (vendor_dependent_packet_type == AVRCP_START_PACKET){
                                avrcp_controller_request_continue_response(connection);
                            }
                            break;
                        case AVRCP_CONTINUE_PACKET:
                        case AVRCP_END_PACKET:
                            connection->num_received_fragments++;
                            
                            if (connection->num_received_fragments < connection->max_num_fragments){
                                avrcp_controller_parse_and_emit_element_attrs(packet+pos, size-pos, connection, ctype);

                                if (vendor_dependent_packet_type == AVRCP_CONTINUE_PACKET){
                                    avrcp_controller_request_continue_response(connection);
                                } 
                            } else {
                                avrcp_controller_emit_now_playing_info_event_done(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, 1);
                                avrcp_parser_reset(connection);
                                avrcp_controller_request_abort_continuation(connection);
                            }
                            break;
                        default:
                            // TODO check
                            btstack_assert(false);
                            break;
                    }
                }
                default:
                    break;
            }
            break;
        case AVRCP_CMD_OPCODE_PASS_THROUGH:{
            if ((size - pos) < 1) return;
            uint8_t operation_id = packet[pos++];
            switch (connection->state){
                case AVCTP_W2_RECEIVE_PRESS_RESPONSE:
                    if (connection->press_and_hold_cmd){
                        connection->state = AVCTP_W4_STOP;
                    } else {
                        connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
                    }
                    break;
                case AVCTP_W2_RECEIVE_RESPONSE:
                    connection->state = AVCTP_CONNECTION_OPENED;
                    break;
                default:
                    break;
            }
            if (connection->state == AVCTP_W4_STOP){
                avrcp_controller_emit_operation_status(avrcp_controller_context.avrcp_callback, AVRCP_SUBEVENT_OPERATION_START, connection->avrcp_cid, ctype, operation_id);
            }
            if (connection->state == AVCTP_CONNECTION_OPENED) {
                // RELEASE response
                operation_id = operation_id & 0x7F;
                avrcp_controller_emit_operation_status(avrcp_controller_context.avrcp_callback, AVRCP_SUBEVENT_OPERATION_COMPLETE, connection->avrcp_cid, ctype, operation_id);
            }
            if (connection->state == AVCTP_W2_SEND_RELEASE_COMMAND){
                // PRESS response
                avrcp_controller_request_pass_through_release_control_cmd(connection);
            } 
            break;
        }
        default:
            break;
    }

    // trigger pending notification reqistrations
    if ((connection->state == AVCTP_CONNECTION_OPENED) && connection->notifications_to_register){
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
}

static void avrcp_controller_handle_can_send_now(avrcp_connection_t * connection){
    switch (connection->state){
        case AVCTP_W2_SEND_PRESS_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_PRESS_RESPONSE;
            avrcp_send_cmd(connection, AVRCP_SINGLE_PACKET);
            return;
        case AVCTP_W2_SEND_COMMAND:
        case AVCTP_W2_SEND_RELEASE_COMMAND:
            connection->state = AVCTP_W2_RECEIVE_RESPONSE;
            avrcp_send_cmd(connection, AVRCP_SINGLE_PACKET);
            return;
        case AVCTP_W2_SEND_FRAGMENTED_COMMAND:
            if (connection->cmd_operands_fragmented_pos == 0){
                 avrcp_send_cmd(connection, AVRCP_START_PACKET);
                 avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
            } else {
                if ((connection->cmd_operands_fragmented_len - connection->cmd_operands_fragmented_pos) > avrcp_get_max_payload_size_for_packet_type(AVRCP_CONTINUE_PACKET)){
                     avrcp_send_cmd(connection, AVRCP_CONTINUE_PACKET);
                     avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                 } else {
                    connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                    avrcp_send_cmd(connection, AVRCP_END_PACKET);
                 }
            }
            return;
        default:
            break;
    }
    // send register notification if queued
    if (connection->notifications_to_register != 0){
        uint8_t event_id;
        for (event_id = 1; event_id <= AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED; event_id++){
            if (connection->notifications_to_register & (1<<event_id)){
                connection->notifications_to_register &= ~ (1 << event_id);
                avrcp_send_register_notification(connection, event_id);
                return;    
            }
        }
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_connection_t * connection;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, channel);
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            break;
        
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_CONTROLLER, channel);
                    avrcp_controller_handle_can_send_now(connection);
                    break;
            default:
                break;
        }
        default:
            break;
    }
}

void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(1, service, service_record_handle, avrcp_controller_supports_browsing(supported_features), supported_features, service_name, service_provider_name);
}

void avrcp_controller_init(void){
    avrcp_controller_context.role = AVRCP_CONTROLLER;
    avrcp_controller_context.packet_handler = avrcp_controller_packet_handler;
    avrcp_register_controller_packet_handler(&avrcp_controller_packet_handler);
}

void avrcp_controller_deinit(void){
    memset(&avrcp_controller_context, 0, sizeof(avrcp_context_t));
}

void avrcp_controller_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_controller_context.avrcp_callback = callback;
}


uint8_t avrcp_controller_play(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PLAY, 0);
}

uint8_t avrcp_controller_stop(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_STOP, 0);
}

uint8_t avrcp_controller_pause(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PAUSE, 0);
}

uint8_t avrcp_controller_forward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FORWARD, 0);
} 

uint8_t avrcp_controller_backward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_BACKWARD, 0);
}

uint8_t avrcp_controller_volume_up(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_UP, 0);
}

uint8_t avrcp_controller_volume_down(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_DOWN, 0);
}

uint8_t avrcp_controller_mute(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_MUTE, 0);
}

uint8_t avrcp_controller_skip(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_SKIP, 0);
}

uint8_t avrcp_controller_fast_forward(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FAST_FORWARD, 0);
}

uint8_t avrcp_controller_rewind(uint16_t avrcp_cid){
    return request_single_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_REWIND, 0);
}

/* start continuous cmds */

uint8_t avrcp_controller_start_press_and_hold_cmd(uint16_t avrcp_cid, avrcp_operation_id_t operation_id){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, operation_id, 0);
}

uint8_t avrcp_controller_press_and_hold_play(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PLAY, 0);
}
uint8_t avrcp_controller_press_and_hold_stop(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_STOP, 0);
}
uint8_t avrcp_controller_press_and_hold_pause(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_PAUSE, 0);
}
uint8_t avrcp_controller_press_and_hold_forward(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FORWARD, 0);
}
uint8_t avrcp_controller_press_and_hold_backward(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_BACKWARD, 0);
}
uint8_t avrcp_controller_press_and_hold_fast_forward(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_FAST_FORWARD, 0);
}
uint8_t avrcp_controller_press_and_hold_rewind(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_REWIND, 0);
}
uint8_t avrcp_controller_press_and_hold_volume_up(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_UP, 0);
}
uint8_t avrcp_controller_press_and_hold_volume_down(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_VOLUME_DOWN, 0);
}
uint8_t avrcp_controller_press_and_hold_mute(uint16_t avrcp_cid){
    return request_continuous_pass_through_press_control_cmd(avrcp_cid, AVRCP_OPERATION_ID_MUTE, 0);
}

/* stop continuous cmds */
uint8_t avrcp_controller_release_press_and_hold_cmd(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_stop_play: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_W4_STOP) return ERROR_CODE_COMMAND_DISALLOWED;
    return avrcp_controller_request_pass_through_release_control_cmd(connection);
}

uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_controller_register_notification(connection, event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_disable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->notifications_to_deregister |= (1 << event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_unit_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_UNIT_INFO;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    memset(connection->cmd_operands, 0xFF, connection->cmd_operands_length);
    connection->cmd_operands_length = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_subunit_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_SUBUNIT_INFO;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    memset(connection->cmd_operands, 0xFF, connection->cmd_operands_length);
    connection->cmd_operands[0] = 7; // page: 0, extention_code: 7
    connection->cmd_operands_length = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_controller_get_capabilities(uint16_t avrcp_cid, uint8_t capability_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
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

uint8_t avrcp_controller_get_supported_company_ids(uint16_t avrcp_cid){
    return avrcp_controller_get_capabilities(avrcp_cid, AVRCP_CAPABILITY_ID_COMPANY);
}

uint8_t avrcp_controller_get_supported_events(uint16_t avrcp_cid){
    return avrcp_controller_get_capabilities(avrcp_cid, AVRCP_CAPABILITY_ID_EVENT);
}

uint8_t avrcp_controller_get_play_status(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_play_status: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
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

uint8_t avrcp_controller_set_addressed_player(uint16_t avrcp_cid, uint16_t addressed_player_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SET_ADDRESSED_PLAYER; // PDU ID
    connection->cmd_operands[pos++] = 0;
    
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 2);
    pos += 2;

    big_endian_store_16(connection->cmd_operands, pos, addressed_player_id);
    pos += 2;

    connection->cmd_operands_length = pos;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_element_attributes(uint16_t avrcp_cid, uint8_t num_attributes, avrcp_media_attribute_id_t * attributes){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    if (num_attributes >= AVRCP_MEDIA_ATTR_RESERVED) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
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

    connection->cmd_operands[pos++] = num_attributes; // attribute count, if 0 get all attributes
    
    int i;
    for (i = 0; i < num_attributes; i++){
        // every attribute is 4 bytes long
        big_endian_store_32(connection->cmd_operands, pos, attributes[i]);
        pos += 4;
    }

    connection->cmd_operands_length = pos;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_now_playing_info(uint16_t avrcp_cid){
    return avrcp_controller_get_element_attributes(avrcp_cid, 0, NULL);
}

uint8_t avrcp_controller_set_absolute_volume(uint16_t avrcp_cid, uint8_t volume){
     avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    //
    // allow sending of multiple set abs volume commands without waiting for response
    //
    uint8_t status = ERROR_CODE_COMMAND_DISALLOWED;
    switch (connection->state){
        case AVCTP_CONNECTION_OPENED:
            status = ERROR_CODE_SUCCESS;
            break;
        case AVCTP_W2_RECEIVE_RESPONSE:
            // - is pending response also set abs volume
            if (connection->command_opcode  != AVRCP_CMD_OPCODE_VENDOR_DEPENDENT) break;
            if (connection->command_type    != AVRCP_CTYPE_CONTROL)               break;
            if (connection->subunit_type    != AVRCP_SUBUNIT_TYPE_PANEL)          break;
            if (connection->subunit_id      != AVRCP_SUBUNIT_ID)                  break;
            if (connection->cmd_operands[3] != AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME)  break;
            // - is next transaction id valid in window
            if (avrcp_controller_is_transaction_id_valid(connection, avrcp_controller_calc_next_transaction_label(connection->transaction_id_counter)) == false) break;
            status = ERROR_CODE_SUCCESS;
            break;
        default:
            break;
    }
    if (status != ERROR_CODE_SUCCESS) return status;

    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
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

uint8_t avrcp_controller_query_shuffle_and_repeat_modes(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;
    
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_STATUS;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    big_endian_store_24(connection->cmd_operands, 0, BT_SIG_COMPANY_ID);
    connection->cmd_operands[3] = AVRCP_PDU_ID_GET_CURRENT_PLAYER_APPLICATION_SETTING_VALUE; // PDU ID
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

static uint8_t avrcp_controller_set_current_player_application_setting_value(uint16_t avrcp_cid, uint8_t attr_id, uint8_t attr_value){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SET_PLAYER_APPLICATION_SETTING_VALUE; // PDU ID
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

uint8_t avrcp_controller_set_shuffle_mode(uint16_t avrcp_cid, avrcp_shuffle_mode_t mode){
    if ((mode < AVRCP_SHUFFLE_MODE_OFF) || (mode > AVRCP_SHUFFLE_MODE_GROUP)) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    return avrcp_controller_set_current_player_application_setting_value(avrcp_cid, 0x03, mode);
}

uint8_t avrcp_controller_set_repeat_mode(uint16_t avrcp_cid, avrcp_repeat_mode_t mode){
    if ((mode < AVRCP_REPEAT_MODE_OFF) || (mode > AVRCP_REPEAT_MODE_GROUP)) return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    return avrcp_controller_set_current_player_application_setting_value(avrcp_cid, 0x02, mode);
}

uint8_t avrcp_controller_play_item_for_scope(uint16_t avrcp_cid, uint8_t * uid, uint16_t uid_counter, avrcp_browsing_scope_t scope){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("Could not find a connection with cid 0%02x.", avrcp_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state, expected %d, received %d", AVCTP_CONNECTION_OPENED, connection->state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_PLAY_ITEM; // PDU ID
    // reserved
    connection->cmd_operands[pos++] = 0;
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 11);
    pos += 2;
    connection->cmd_operands[pos++]  = scope;
    memset(&connection->cmd_operands[pos], 0, 8);
    if (uid){
        (void)memcpy(&connection->cmd_operands[pos], uid, 8);
    }
    pos += 8;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;
    connection->cmd_operands_length = pos;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_add_item_from_scope_to_now_playing_list(uint16_t avrcp_cid, uint8_t * uid, uint16_t uid_counter, avrcp_browsing_scope_t scope){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("Could not find a connection with cid 0%02x.", avrcp_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state, expected %d, received %d", AVCTP_CONNECTION_OPENED, connection->state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    connection->state = AVCTP_W2_SEND_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    int pos = 0;
    big_endian_store_24(connection->cmd_operands, pos, BT_SIG_COMPANY_ID);
    pos += 3;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_ADD_TO_NOW_PLAYING; // PDU ID
    // reserved
    connection->cmd_operands[pos++] = 0;
    // Parameter Length
    big_endian_store_16(connection->cmd_operands, pos, 11);
    pos += 2;
    connection->cmd_operands[pos++]  = scope;
    memset(&connection->cmd_operands[pos], 0, 8);
    if (uid){
        (void)memcpy(&connection->cmd_operands[pos], uid, 8);
    }
    pos += 8;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;
    connection->cmd_operands_length = pos;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_set_max_num_fragments(uint16_t avrcp_cid, uint8_t max_num_fragments){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_controller_play_item: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->max_num_fragments = max_num_fragments;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_send_custom_command(uint16_t avrcp_cid, avrcp_command_type_t command_type, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t command_opcode, const uint8_t * command_buffer, uint16_t command_len){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        log_error("avrcp_controller_play_item: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->state = AVCTP_W2_SEND_FRAGMENTED_COMMAND;

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = command_opcode;
    connection->command_type = command_type;
    connection->subunit_type = subunit_type;
    connection->subunit_id = subunit_id;
    connection->cmd_operands_fragmented_buffer = command_buffer;
    connection->cmd_operands_fragmented_pos = 0;
    connection->cmd_operands_fragmented_len = command_len;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

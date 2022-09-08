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
    uint8_t delta = ((int8_t) transaction_id - connection->controller_last_confirmed_transaction_id) & 0x0f;
    return delta < 15;
}

static void avrcp_controller_custom_command_data_init(avrcp_connection_t * connection,
                                                      avrcp_command_opcode_t opcode, avrcp_command_type_t command_type,
                                                      avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id,
                                                      avrcp_pdu_id_t pdu_id, uint32_t company_id){

    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode = opcode;
    connection->command_type = command_type;
    connection->subunit_type = subunit_type;
    connection->subunit_id = subunit_id;
    connection->company_id = company_id;
    connection->pdu_id = pdu_id;
    connection->data = NULL;
    connection->data_offset = 0;
    connection->data_len = 0;
}

static void avrcp_controller_vendor_dependent_command_data_init(avrcp_connection_t * connection, avrcp_command_type_t command_type, avrcp_pdu_id_t pdu_id, bool get_next_transaction_label){
    if (get_next_transaction_label){
        connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    }
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id = AVRCP_SUBUNIT_ID;
    connection->company_id = BT_SIG_COMPANY_ID;
   
    connection->command_type = command_type;
    connection->pdu_id = pdu_id;
    connection->data = connection->message_body;
    connection->data_offset = 0;
    connection->data_len = 0;
}

static void avrcp_controller_pass_through_command_data_init(avrcp_connection_t * connection, avrcp_operation_id_t opid){
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    connection->command_opcode =  AVRCP_CMD_OPCODE_PASS_THROUGH;
    connection->command_type = AVRCP_CTYPE_CONTROL;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id =   AVRCP_SUBUNIT_ID;
    
    connection->company_id = 0;
    connection->pdu_id = AVRCP_PDU_ID_UNDEFINED;
    connection->operation_id = opid;

    connection->data = connection->message_body;
    connection->data_offset = 0;
    connection->data_len = 0;
}

static int avrcp_controller_supports_browsing(uint16_t controller_supported_features){
    return controller_supported_features & AVRCP_FEATURE_MASK_BROWSING;
}

static void avrcp_controller_prepare_custom_command_response(avrcp_connection_t * connection, uint16_t response_len, uint8_t * in_place_buffer){
    uint8_t pos = 0;
    in_place_buffer[pos++] = HCI_EVENT_AVRCP_META;
    // skip len
    pos++;
    in_place_buffer[pos++] = AVRCP_SUBEVENT_CUSTOM_COMMAND_RESPONSE;
    little_endian_store_16(in_place_buffer, pos, connection->avrcp_cid);
    pos += 2;
    in_place_buffer[pos++] = (uint8_t)connection->command_type;
    in_place_buffer[pos++] = (uint8_t)connection->pdu_id;
    little_endian_store_16(in_place_buffer, pos, response_len);
    pos += 2;
    in_place_buffer[1] = pos + response_len - 2;
}

static void avrcp_controller_emit_notification_complete(avrcp_connection_t * connection, uint8_t status, uint8_t event_id, bool enabled){
    uint8_t event[8];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_NOTIFICATION_STATE;
    little_endian_store_16(event, pos, connection->avrcp_cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = enabled ? 1 : 0;
    event[pos++] = event_id;
    UNUSED(pos);
    (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_controller_emit_supported_events(avrcp_connection_t * connection){
    uint8_t ctype = (uint8_t) AVRCP_CTYPE_RESPONSE_CHANGED_STABLE;
    uint8_t event_id;

    for (event_id = (uint8_t) AVRCP_NOTIFICATION_EVENT_FIRST_INDEX; event_id < (uint8_t) AVRCP_NOTIFICATION_EVENT_LAST_INDEX; event_id++){
        if ((connection->notifications_supported_by_target & (1 << event_id)) == 0){
            continue;
        }
        uint8_t event[8];
        uint8_t pos = 0;
        event[pos++] = HCI_EVENT_AVRCP_META;
        event[pos++] = sizeof(event) - 2;
        event[pos++] = AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID;
        little_endian_store_16(event, pos, connection->avrcp_cid);
        pos += 2;
        event[pos++] = ctype;
        event[pos++] = 0;
        event[pos++] = event_id;
        UNUSED(pos);
        (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
    }

    uint8_t event[7];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_GET_CAPABILITY_EVENT_ID_DONE;
    little_endian_store_16(event, pos, connection->avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    event[pos++] = 0;
    UNUSED(pos);
    (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
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
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    // reserve one byte for subevent type and data len
	uint16_t data_len_pos = pos;
    pos++;
	uint16_t subevent_type_pos = pos;
    pos++;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = ctype;
    
    switch (attr_id){
        case AVRCP_MEDIA_ATTR_TITLE:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO;
			btstack_assert(value_len <= 255);
            event[pos++] = (uint8_t) value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_ARTIST:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO;
			btstack_assert(value_len <= 255);
			event[pos++] = (uint8_t) value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_ALBUM:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO;
			btstack_assert(value_len <= 255);
			event[pos++] = (uint8_t) value_len;
            (void)memcpy(event + pos, value, value_len);
            break;
        case AVRCP_MEDIA_ATTR_GENRE:
            event[subevent_type_pos] = AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO;
			btstack_assert(value_len <= 255);
			event[pos++] = (uint8_t) value_len;
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
    connection->parser_attribute_header_pos = 0;
    connection->controller_num_received_fragments = 0;
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
            connection->attribute_value_len = btstack_min(attribute_total_value_len, AVRCP_MAX_ATTRIBUTE_SIZE);
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

static void avrcp_send_cmd_with_avctp_fragmentation(avrcp_connection_t * connection){
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    uint16_t max_payload_size;
    connection->avctp_packet_type = avctp_get_packet_type(connection, &max_payload_size);
    connection->avrcp_packet_type = avrcp_get_packet_type(connection);

    // non-fragmented: transport header (1) + PID (2)
    // fragmented:     transport header (1) + num packets (1) + PID (2)

    uint16_t param_len = connection->data_len;
    // AVCTP header
    // transport header : transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    uint16_t pos = 0;
    packet[pos++] = (connection->transaction_id << 4) | (connection->avctp_packet_type << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;

    if (connection->avctp_packet_type == AVCTP_START_PACKET){
        uint16_t max_frame_size = btstack_min(connection->l2cap_mtu, AVRCP_MAX_AV_C_MESSAGE_FRAME_SIZE);
        // first packet: max_payload_size
        // rest packets
        uint16_t num_payload_bytes = param_len - max_payload_size;
        uint16_t frame_size_for_continue_packet = max_frame_size - avctp_get_num_bytes_for_header(AVCTP_CONTINUE_PACKET);
        uint16_t num_avctp_packets = (num_payload_bytes + frame_size_for_continue_packet - 1)/frame_size_for_continue_packet + 1;
		btstack_assert(num_avctp_packets <= 255);
        packet[pos++] = (uint8_t) num_avctp_packets;
    }

    switch (connection->avctp_packet_type){
        case AVCTP_SINGLE_PACKET:
        case AVCTP_START_PACKET:
            // Profile IDentifier (PID)
            packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
            packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

            // command_type
            packet[pos++] = connection->command_type;
            // subunit_type | subunit ID
            packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
            // opcode
            packet[pos++] = (uint8_t)connection->command_opcode;

            switch (connection->command_opcode){
                case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
                    big_endian_store_24(packet, pos, connection->company_id);
                    pos += 3;
                    packet[pos++] = connection->pdu_id;
                    packet[pos++] = connection->avrcp_packet_type;              // reserved(upper 6) | AVRCP packet_type
                    big_endian_store_16(packet, pos, connection->data_len);     // parameter length
                    pos += 2;
                    break;
                case AVRCP_CMD_OPCODE_PASS_THROUGH:
                    packet[pos++] = connection->operation_id;
                    packet[pos++] = (uint8_t)connection->data_len;     // parameter length
                    pos += 2;
                    break;
                case AVRCP_CMD_OPCODE_UNIT_INFO:
                    break;
                case AVRCP_CMD_OPCODE_SUBUNIT_INFO:
                    break;
                default:
                    btstack_assert(false);
                    return;
            }
            break;
        case AVCTP_CONTINUE_PACKET:
        case AVCTP_END_PACKET:
            break;
        default:
            btstack_assert(false);
            return;
    }
    // compare number of bytes to store with the remaining buffer size
    uint16_t bytes_to_copy = btstack_min(connection->data_len - connection->data_offset, max_payload_size - pos);

    (void)memcpy(packet + pos, &connection->data[connection->data_offset], bytes_to_copy);
    pos += bytes_to_copy;
    connection->data_offset += bytes_to_copy;

    l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
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
    btstack_run_loop_set_timer(&connection->controller_press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->controller_press_and_hold_cmd_timer);
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

static void avrcp_press_and_hold_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->controller_press_and_hold_cmd_timer);
    btstack_run_loop_set_timer_handler(&connection->controller_press_and_hold_cmd_timer, avrcp_press_and_hold_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->controller_press_and_hold_cmd_timer, connection);
    btstack_run_loop_set_timer(&connection->controller_press_and_hold_cmd_timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&connection->controller_press_and_hold_cmd_timer);
}

static void avrcp_press_and_hold_timer_stop(avrcp_connection_t * connection){
    connection->controller_press_and_hold_cmd_active = false;
    btstack_run_loop_remove_timer(&connection->controller_press_and_hold_cmd_timer);
} 


static uint8_t avrcp_controller_request_pass_through_release_control_cmd(avrcp_connection_t * connection){
    connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
    if (connection->controller_press_and_hold_cmd_active){
        avrcp_press_and_hold_timer_stop(connection);
    }
    connection->operation_id = (avrcp_operation_id_t)(0x80 | connection->operation_id);
    connection->transaction_id = avrcp_controller_get_next_transaction_label(connection);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_controller_request_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed, bool continuous_cmd){
	UNUSED(playback_speed);

	log_info("Send command %d", opid);
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state %d, expected %d. avrcp cid 0x%02x", connection->state, AVCTP_CONNECTION_OPENED, avrcp_cid);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    connection->state = AVCTP_W2_SEND_PRESS_COMMAND;
    avrcp_controller_pass_through_command_data_init(connection, opid);
    
    connection->controller_press_and_hold_cmd_active = continuous_cmd;
    if (connection->controller_press_and_hold_cmd_active){
        avrcp_press_and_hold_timer_start(connection);
    }

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t request_single_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return avrcp_controller_request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, false);
}

static uint8_t request_continuous_pass_through_press_control_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint16_t playback_speed){
    return avrcp_controller_request_pass_through_press_control_cmd(avrcp_cid, opid, playback_speed, true);
}

static void avrcp_controller_get_capabilities_for_connection(avrcp_connection_t * connection, uint8_t capability_id){
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_STATUS, AVRCP_PDU_ID_GET_CAPABILITIES, true);

    // Parameter Length
    connection->data_len = 1;
    connection->data[0] = capability_id;  // capability ID
    
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
}

static uint8_t avrcp_controller_register_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t event_id){
    if ((connection->remote_capabilities_state == AVRCP_REMOTE_CAPABILITIES_KNOWN) && (connection->notifications_supported_by_target & (1 << event_id)) == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }  
    if ((connection->controller_notifications_to_deregister & (1 << event_id)) != 0){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    if ( (connection->notifications_enabled & (1 << event_id)) != 0){
        return ERROR_CODE_SUCCESS;
    }
    connection->controller_notifications_to_register |= (1 << event_id);

    switch (connection->remote_capabilities_state){
        case AVRCP_REMOTE_CAPABILITIES_NONE:
            connection->remote_capabilities_state = AVRCP_REMOTE_CAPABILITIES_W4_QUERY_RESULT;
            connection->controller_notifications_supported_by_target_suppress_emit_result = true;
            avrcp_controller_get_capabilities_for_connection(connection, AVRCP_CAPABILITY_ID_EVENT);
            break;
        case AVRCP_REMOTE_CAPABILITIES_KNOWN:
            avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
            break;
        default:
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_controller_request_continuation(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id){
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, pdu_id, false);

    // Parameter Length
    connection->data_len = 3;
    big_endian_store_16(connection->data, 0, 1);
    connection->data[2] = AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_controller_request_abort_continuation(avrcp_connection_t * connection){
    return avrcp_controller_request_continuation(connection, AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE);
}

static uint8_t avrcp_controller_request_continue_response(avrcp_connection_t * connection){
    return avrcp_controller_request_continuation(connection, AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE);
}

static void avrcp_controller_handle_notification(avrcp_connection_t *connection, avrcp_command_type_t ctype, uint8_t *payload, uint16_t size) {
    if (size < 1) return;
    uint16_t pos = 0;
    avrcp_notification_event_id_t event_id = (avrcp_notification_event_id_t) payload[pos++];
    if ( (event_id < AVRCP_NOTIFICATION_EVENT_FIRST_INDEX) || (event_id > AVRCP_NOTIFICATION_EVENT_LAST_INDEX)){
        return;
    }

    uint16_t event_mask = (1 << event_id);
    uint16_t reset_event_mask = ~event_mask;

    switch (ctype){
        case AVRCP_CTYPE_RESPONSE_REJECTED:
            connection->controller_notifications_to_deregister &= reset_event_mask;
            connection->controller_notifications_to_register   &= reset_event_mask;
            connection->controller_initial_status_reported     &= reset_event_mask;
            avrcp_controller_emit_notification_complete(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE, event_id, false);
            return;

        case AVRCP_CTYPE_RESPONSE_INTERIM:
            // register as enabled
            connection->notifications_enabled |= event_mask;
            
            // check if initial value is already sent
            if ((connection->controller_initial_status_reported & event_mask) != 0 ){
                return;
            }
            // emit event only once, initially
            avrcp_controller_emit_notification_complete(connection, ERROR_CODE_SUCCESS, event_id, true);
            connection->controller_initial_status_reported |= event_mask;
            // emit initial value after this switch 
            break;
        
        case AVRCP_CTYPE_RESPONSE_CHANGED_STABLE:
            // received change, event is considered de-registered
            // we are re-enabling it automatically, if it is not 
            // explicitly disabled
            connection->notifications_enabled &= reset_event_mask;
            if ((connection->controller_notifications_to_deregister & event_mask) == 0){
                avrcp_controller_register_notification(connection, event_id);
            } else {
                connection->controller_notifications_to_deregister &= reset_event_mask;
                connection->controller_notifications_to_register   &= reset_event_mask;
                connection->controller_initial_status_reported     &= reset_event_mask;
                avrcp_controller_emit_notification_complete(connection, ERROR_CODE_SUCCESS, event_id, false);
            }
            break;

        default:
            return;
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
    avrcp_packet_type_t  vendor_dependent_avrcp_packet_type;

    uint16_t pos = 0;
    connection->controller_last_confirmed_transaction_id = packet[pos] >> 4;
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
            // page, extension code (1)
            pos++;
            uint8_t unit_type = packet[pos] >> 3;
            uint8_t max_subunit_ID = packet[pos] & 0x07;
            log_info("SUBUNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), target_unit_type 0x%02x, max_subunit_ID %d", ctype, subunit_type, subunit_id, opcode, unit_type, max_subunit_ID);
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
            log_info("UNIT INFO response: ctype 0x%02x (0C), subunit_type 0x%02x (1F), subunit_id 0x%02x (07), opcode 0x%02x (30), target_unit_type 0x%02x, unit %d, company_id 0x%06" PRIx32,
                ctype, subunit_type, subunit_id, opcode, unit_type, unit, company_id);
#endif
            break;
        }
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
            if ((size - pos) < 7){
                return;
            }
            // Company ID (3)
            pos += 3;
            pdu_id = packet[pos++];
            vendor_dependent_avrcp_packet_type = (avrcp_packet_type_t)(packet[pos++] & 0x03);
            param_length = big_endian_read_16(packet, pos);
            pos += 2;

            if ((size - pos) < param_length) {
                return;
            }

            // handle asynchronous notifications, without changing state
            if (pdu_id == AVRCP_PDU_ID_REGISTER_NOTIFICATION){
                avrcp_controller_handle_notification(connection, ctype, packet + pos, size - pos);
                break;
            }
            if (connection->state != AVCTP_W2_RECEIVE_RESPONSE) return;
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
                    uint8_t capability_count = 0;
                    if (param_length > 1){
                        capability_count = packet[pos++];
                    }
                    uint16_t i;
                    uint16_t offset = 0;
                    uint8_t event[10];

                    switch (capability_id){

                        case AVRCP_CAPABILITY_ID_COMPANY:
                            for (i = 0; (i < capability_count) && ((size - pos) >= 3); i++){
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
                            for (i = 0; (i < capability_count) && ((size - pos) >= 1); i++){
                                uint8_t event_id = packet[pos++];
                                connection->notifications_supported_by_target |= (1 << event_id);
                            }

                            connection->remote_capabilities_state = AVRCP_REMOTE_CAPABILITIES_KNOWN;

                            // if the get supported events query is triggered by avrcp_controller_enable_notification call,
                            // avrcp_controller_emit_supported_events should be suppressed
                            if (connection->controller_notifications_supported_by_target_suppress_emit_result){
                                connection->controller_notifications_supported_by_target_suppress_emit_result = false;
                                // also, notification might not be supported
                                // if so, emit AVRCP_SUBEVENT_ENABLE_NOTIFICATION_COMPLETE event to app, 
                                // and update controller_notifications_to_register bitmap
                                for (i = (uint8_t)AVRCP_NOTIFICATION_EVENT_FIRST_INDEX; i < (uint8_t) AVRCP_NOTIFICATION_EVENT_LAST_INDEX; i++){
                                    if ((connection->controller_notifications_to_register & (1 << i)) != 0){
                                        if ((connection->notifications_supported_by_target & (1 << i)) == 0){
                                            avrcp_controller_emit_notification_complete(connection, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE, (uint8_t) i, false);
                                            connection->controller_notifications_to_register &= ~(1 << i);
                                        }
                                    }
                                }
                                break;
                            }
                            // supported events are emitted only if the get supported events query 
                            // is triggered by avrcp_controller_get_supported_events call
                            avrcp_controller_emit_supported_events(connection);
                            break;

                        default:
                            // ignore
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
                
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:
                    switch (vendor_dependent_avrcp_packet_type){
                        case AVRCP_START_PACKET:
                        case AVRCP_SINGLE_PACKET:
                            avrcp_parser_reset(connection);
                            connection->list_size = param_length;
                            // num_attributes
                            pos++;

                            avrcp_controller_parse_and_emit_element_attrs(packet+pos, size-pos, connection, ctype);
                            if (vendor_dependent_avrcp_packet_type == AVRCP_START_PACKET){
                                avrcp_controller_request_continue_response(connection);
                                return;
                            }
                            break;
                        case AVRCP_CONTINUE_PACKET:
                        case AVRCP_END_PACKET:
                            connection->controller_num_received_fragments++;
                            
                            if (connection->controller_num_received_fragments < connection->controller_max_num_fragments){
                                avrcp_controller_parse_and_emit_element_attrs(packet+pos, size-pos, connection, ctype);

                                if (vendor_dependent_avrcp_packet_type == AVRCP_CONTINUE_PACKET){
                                    avrcp_controller_request_continue_response(connection);
                                    return;
                                } 
                            } else {
                                avrcp_controller_emit_now_playing_info_event_done(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, 1);
                                avrcp_parser_reset(connection);
                                avrcp_controller_request_abort_continuation(connection);
                                return;
                            }
                            break;
                        default:
                            btstack_assert(false);
                            break;
                    }
                    break;

                default:
                    // custom command response comes here
                    switch (pdu_id){
                        case AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE:
                            avrcp_controller_emit_now_playing_info_event_done(avrcp_controller_context.avrcp_callback, connection->avrcp_cid, ctype, 0);
                            break;
                        default:
                            if (pdu_id != connection->pdu_id) {
                                break;
                            }
                            uint8_t *in_place_buffer = packet + pos - 9;
                            avrcp_controller_prepare_custom_command_response(connection, param_length,
                                                                             in_place_buffer);
                            (*avrcp_controller_context.avrcp_callback)(HCI_EVENT_PACKET, 0, in_place_buffer,
                                                                       param_length + 9);

                            break;
                    }
                    break;
            }
            break;
        case AVRCP_CMD_OPCODE_PASS_THROUGH:{
            if ((size - pos) < 1) return;
            uint8_t operation_id = packet[pos++];
            switch (connection->state){
                case AVCTP_W2_RECEIVE_PRESS_RESPONSE:
                    // trigger release for simple command:
                    if (!connection->controller_press_and_hold_cmd_active){
                        connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
                        break;
                    }
                    // for press and hold, send release if it just has been requested, otherwise, wait for next repeat
                    if (connection->controller_press_and_hold_cmd_release){
                        connection->controller_press_and_hold_cmd_release = false;
                        connection->state = AVCTP_W2_SEND_RELEASE_COMMAND;
                    } else {
                        connection->state = AVCTP_W4_STOP;
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
    if ((connection->state == AVCTP_CONNECTION_OPENED) && connection->controller_notifications_to_register){
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
}

static void avrcp_controller_handle_can_send_now(avrcp_connection_t * connection){
    switch (connection->state){
        case AVCTP_W2_SEND_PRESS_COMMAND:
            avrcp_send_cmd_with_avctp_fragmentation(connection);
            connection->state = AVCTP_W2_RECEIVE_PRESS_RESPONSE;
            break;

        case AVCTP_W2_SEND_RELEASE_COMMAND:
            avrcp_send_cmd_with_avctp_fragmentation(connection);
            connection->state = AVCTP_W2_RECEIVE_RESPONSE;
            break;

        case AVCTP_W2_SEND_COMMAND:
            avrcp_send_cmd_with_avctp_fragmentation(connection);
            if (connection->data_offset < connection->data_len){
                // continue AVCTP fragmentation
                avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                return;
            }
            connection->state = AVCTP_W2_RECEIVE_RESPONSE;
            return;
        default:
            break;
    }
    
    // send register notification if queued
    if (connection->controller_notifications_to_register != 0){
        uint8_t event_id;
        for (event_id = (uint8_t)AVRCP_NOTIFICATION_EVENT_FIRST_INDEX; event_id < (uint8_t)AVRCP_NOTIFICATION_EVENT_LAST_INDEX; event_id++){
            if (connection->controller_notifications_to_register & (1 << event_id)){
                connection->controller_notifications_to_register &= ~ (1 << event_id);
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
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    switch (connection->state){
        // respond when we receive response for (repeated) press command
        case AVCTP_W2_RECEIVE_PRESS_RESPONSE:
            connection->controller_press_and_hold_cmd_release = true;
            break;
        
        // release already sent or on the way, nothing to do
        case AVCTP_W2_RECEIVE_RESPONSE:
        case AVCTP_W2_SEND_RELEASE_COMMAND:
            break;
        
        // about to send next repeated press command or wait for it -> release right away
        case AVCTP_W2_SEND_PRESS_COMMAND:
        case AVCTP_W4_STOP:
            return avrcp_controller_request_pass_through_release_control_cmd(connection);

        // otherwise reject request
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_enable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    return avrcp_controller_register_notification(connection, event_id);
}

uint8_t avrcp_controller_disable_notification(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->remote_capabilities_state != AVRCP_REMOTE_CAPABILITIES_KNOWN){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if ((connection->notifications_supported_by_target & (1 << event_id)) == 0){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    } 

    if ((connection->notifications_enabled & (1 << event_id)) == 0){
        return ERROR_CODE_SUCCESS;
    }

    connection->controller_notifications_to_deregister |= (1 << event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_unit_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_custom_command_data_init(connection, AVRCP_CMD_OPCODE_UNIT_INFO, AVRCP_CTYPE_STATUS,
                                              AVRCP_SUBUNIT_TYPE_UNIT, AVRCP_SUBUNIT_ID_IGNORE, AVRCP_PDU_ID_UNDEFINED,
                                              0);

    connection->data = connection->message_body;
    memset(connection->data, 0xFF, 5);
    connection->data_len = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_subunit_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_custom_command_data_init(connection, AVRCP_CMD_OPCODE_SUBUNIT_INFO, AVRCP_CTYPE_STATUS,
                                              AVRCP_SUBUNIT_TYPE_UNIT, AVRCP_SUBUNIT_ID_IGNORE, AVRCP_PDU_ID_UNDEFINED,
                                              0);

    connection->data = connection->message_body;
    memset(connection->data, 0xFF, 5);
    connection->data[0] = 7; // page: 0, extension_code: 7
    connection->data_len = 5;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_supported_company_ids(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    avrcp_controller_get_capabilities_for_connection(connection, AVRCP_CAPABILITY_ID_COMPANY);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_supported_events(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    switch (connection->remote_capabilities_state){
        case AVRCP_REMOTE_CAPABILITIES_NONE:
            connection->remote_capabilities_state = AVRCP_REMOTE_CAPABILITIES_W4_QUERY_RESULT;
            avrcp_controller_get_capabilities_for_connection(connection, AVRCP_CAPABILITY_ID_EVENT);
            break;
        case AVRCP_REMOTE_CAPABILITIES_KNOWN:
            avrcp_controller_emit_supported_events(connection);
            break;
        default:
            break;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_play_status(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_STATUS, AVRCP_PDU_ID_GET_PLAY_STATUS, true);

    connection->data_len = 0;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_set_addressed_player(uint16_t avrcp_cid, uint16_t addressed_player_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, AVRCP_PDU_ID_SET_ADDRESSED_PLAYER, true);

    connection->data_len = 2;
    big_endian_store_16(connection->data, 0, addressed_player_id);
    
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_element_attributes(uint16_t avrcp_cid, uint8_t num_attributes, avrcp_media_attribute_id_t * attributes){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    if (num_attributes >= AVRCP_MEDIA_ATTR_RESERVED) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_STATUS, AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES, true);

    uint8_t pos = 0;
    // write 8 bytes value
    memset(connection->data, pos, 8); // identifier: PLAYING
    pos += 8;

    uint8_t num_attributes_index = pos;
    pos++;

    // If num_attributes is set to zero, all attribute information shall be returned, 
    // and the AttributeID field is omitted
    connection->data[num_attributes_index] = 0;
    uint8_t i;
    for (i = 0; i < num_attributes; i++){
        // ignore invalid attribute ID and "get all attributes"
        if (AVRCP_MEDIA_ATTR_ALL < attributes[i] && attributes[i] < AVRCP_MEDIA_ATTR_RESERVED){
            // every attribute is 4 bytes long
            big_endian_store_32(connection->data, pos, attributes[i]);
            pos += 4;  
            connection->data[num_attributes_index]++;
        }
    }

    // Parameter Length
    connection->data_len = pos;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_get_now_playing_info(uint16_t avrcp_cid){
    return avrcp_controller_get_element_attributes(avrcp_cid, 0, NULL);
}

uint8_t avrcp_controller_get_now_playing_info_for_media_attribute_id(uint16_t avrcp_cid, avrcp_media_attribute_id_t media_attribute_id){
    if (media_attribute_id == AVRCP_MEDIA_ATTR_ALL){
        return avrcp_controller_get_now_playing_info(avrcp_cid);
    }
    avrcp_media_attribute_id_t media_attrs[1];
    media_attrs[0] = media_attribute_id;
    return avrcp_controller_get_element_attributes(avrcp_cid, 1, media_attrs);
}

uint8_t avrcp_controller_set_absolute_volume(uint16_t avrcp_cid, uint8_t volume){
     avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
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
            if (connection->pdu_id          != AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME)  break;
            // - is next transaction id valid in window
            if (avrcp_controller_is_transaction_id_valid(connection, avrcp_controller_calc_next_transaction_label(connection->transaction_id_counter)) == false) break;
            status = ERROR_CODE_SUCCESS;
            break;
        default:
            break;
    }
    if (status != ERROR_CODE_SUCCESS) return status;

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME, true);

    // Parameter Length
    connection->data_len = 1;
    connection->data[0] = volume;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_query_shuffle_and_repeat_modes(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_STATUS, AVRCP_PDU_ID_GET_CURRENT_PLAYER_APPLICATION_SETTING_VALUE, true);

    connection->data_len = 5;
    connection->data[0] = 4;                     // NumPlayerApplicationSettingAttributeID
    // PlayerApplicationSettingAttributeID1 AVRCP Spec, Appendix F, 133
    connection->data[1] = 0x01;   // equalizer  (1-OFF, 2-ON)
    connection->data[2] = 0x02;   // repeat     (1-off, 2-single track, 3-all tracks, 4-group repeat)
    connection->data[3] = 0x03;   // shuffle    (1-off, 2-all tracks, 3-group shuffle)
    connection->data[4] = 0x04;   // scan       (1-off, 2-all tracks, 3-group scan)

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_controller_set_current_player_application_setting_value(uint16_t avrcp_cid, uint8_t attr_id, uint8_t attr_value){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, AVRCP_PDU_ID_SET_PLAYER_APPLICATION_SETTING_VALUE, true);

    // Parameter Length
    connection->data_len = 3;
    connection->data[0]  = 2;
    connection->data[1]  = attr_id;
    connection->data[2]  = attr_value;

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
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state, expected %d, received %d", AVCTP_CONNECTION_OPENED, connection->state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, AVRCP_PDU_ID_PLAY_ITEM, true);

    // Parameter Length
    connection->data_len = 11;
    connection->data[0]  = scope;
    memset(&connection->data[1], 0, 8);
    if (uid){
        (void)memcpy(&connection->data[1], uid, 8);
    }
    big_endian_store_16(connection->data, 9, uid_counter);

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_add_item_from_scope_to_now_playing_list(uint16_t avrcp_cid, uint8_t * uid, uint16_t uid_counter, avrcp_browsing_scope_t scope){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("Connection in wrong state, expected %d, received %d", AVCTP_CONNECTION_OPENED, connection->state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 
    
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_vendor_dependent_command_data_init(connection, AVRCP_CTYPE_CONTROL, AVRCP_PDU_ID_ADD_TO_NOW_PLAYING, true);

    // Parameter Length
    connection->data_len = 11;
    connection->data[0]  = scope;
    memset(&connection->data[1], 0, 8);
    if (uid){
        (void)memcpy(&connection->data[1], uid, 8);
    }
    big_endian_store_16(connection->data, 9, uid_counter);
    
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_set_max_num_fragments(uint16_t avrcp_cid, uint8_t max_num_fragments){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->controller_max_num_fragments = max_num_fragments;
    return ERROR_CODE_SUCCESS;
}


uint8_t avrcp_controller_send_custom_command(uint16_t avrcp_cid, 
    avrcp_command_type_t command_type, 
    avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, 
    avrcp_pdu_id_t pdu_id, uint32_t company_id, 
    const uint8_t * data, uint16_t data_len){
    
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_custom_command_data_init(connection, AVRCP_CMD_OPCODE_VENDOR_DEPENDENT, command_type, subunit_type,
                                              subunit_id, pdu_id, company_id);

    connection->data = (uint8_t *)data;
    connection->data_len = data_len;

    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_controller_force_send_press_cmd(uint16_t avrcp_cid, avrcp_operation_id_t opid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state < AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection->state = AVCTP_W2_SEND_COMMAND;
    avrcp_controller_pass_through_command_data_init(connection, opid);
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}
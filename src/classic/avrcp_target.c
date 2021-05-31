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

#define BTSTACK_FILE__ "avrcp_target.c"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "classic/avrcp.h"
#include "classic/avrcp_target.h"

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "l2cap.h"

#define AVRCP_ATTR_HEADER_LEN  8

static const uint8_t AVRCP_NOTIFICATION_TRACK_SELECTED[] = {0,0,0,0,0,0,0,0};
static const uint8_t AVRCP_NOTIFICATION_TRACK_NOT_SELECTED[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

avrcp_context_t avrcp_target_context;

static int avrcp_target_supports_browsing(uint16_t target_supported_features){
    return target_supported_features & AVRCP_FEATURE_MASK_BROWSING;
}

void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(0, service, service_record_handle, avrcp_target_supports_browsing(supported_features), supported_features, service_name, service_provider_name);
}

static void
avrcp_target_emit_operation(btstack_packet_handler_t callback, uint16_t avrcp_cid, avrcp_operation_id_t operation_id,
                            bool button_pressed, uint8_t operands_length, uint8_t operand) {
    btstack_assert(callback != NULL);

    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_OPERATION; 
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = operation_id;
    event[pos++] = button_pressed ? 1 : 0;
    event[pos++] = operands_length; 
    event[pos++] = operand; 
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_target_emit_volume_changed(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t absolute_volume){
    btstack_assert(callback != NULL);

    uint8_t event[7];
    int offset = 0;
    event[offset++] = HCI_EVENT_AVRCP_META;
    event[offset++] = sizeof(event) - 2;
    event[offset++] = AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED;
    little_endian_store_16(event, offset, avrcp_cid);
    offset += 2;
    event[offset++] = AVRCP_CTYPE_NOTIFY;
    event[offset++] = absolute_volume;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_target_emit_respond_vendor_dependent_query(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t subevent_id){
    btstack_assert(callback != NULL);

    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_id; 
    little_endian_store_16(event, pos, avrcp_cid);
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// returns number of bytes stored
static uint16_t avrcp_target_pack_single_element_header(uint8_t * packet, uint16_t pos, avrcp_media_attribute_id_t attr_id, uint16_t attr_value_size){
    btstack_assert(attr_id >= 1);
    btstack_assert(attr_id <= AVRCP_MEDIA_ATTR_COUNT);

    big_endian_store_32(packet, pos, attr_id);
    big_endian_store_16(packet, pos+4, RFC2978_CHARSET_MIB_UTF8);
    big_endian_store_16(packet, pos+6, attr_value_size);
    return 8;
}

// returns number of bytes stored
static uint16_t avrcp_target_pack_single_element_attribute_number(uint8_t * packet, uint16_t pos, avrcp_media_attribute_id_t attr_id, uint32_t value){
    uint16_t attr_value_length = sprintf((char *)(packet+pos+8), "%0" PRIu32, value);
    (void) avrcp_target_pack_single_element_header(packet, pos, attr_id, attr_value_length);
    return 8 + attr_value_length;
}

// returns number of bytes stored
static uint16_t avrcp_target_pack_single_element_attribute_string_fragment(uint8_t * packet, uint16_t pos, avrcp_media_attribute_id_t attr_id, uint8_t * attr_value, uint16_t attr_value_to_copy, uint16_t attr_value_size, bool header){
    if (attr_value_size == 0) return 0;
    uint16_t bytes_stored = 0;
    if (header){
        bytes_stored += avrcp_target_pack_single_element_header(packet, pos, attr_id, attr_value_size);
    }
    (void)memcpy(packet + pos + bytes_stored, attr_value, attr_value_to_copy);
    bytes_stored += attr_value_to_copy;
    return bytes_stored;
}

static int avrcp_target_abort_continue_response(uint16_t cid, avrcp_connection_t * connection){
    uint16_t pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();
    
    connection->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type    = AVRCP_CTYPE_RESPONSE_ACCEPTED;
    connection->subunit_type    = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id      = AVRCP_SUBUNIT_ID;

    packet[pos++] = (connection->transaction_id << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    packet[pos++] = connection->command_type;
    // subunit_type | subunit ID
    packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    packet[pos++] = (uint8_t)connection->command_opcode;

    // company id is 3 bytes long
    big_endian_store_24(packet, pos, BT_SIG_COMPANY_ID);
    pos += 3;

    packet[pos++] = AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE;
    
    // reserve byte for packet type
    packet[pos++] = AVRCP_SINGLE_PACKET;
    big_endian_store_16(packet, pos, 0);
    pos += 2;
    return l2cap_send_prepared(cid, pos); 
}

static int avrcp_target_send_now_playing_info(uint16_t cid, avrcp_connection_t * connection){
    uint16_t pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();
    uint16_t  size   = l2cap_get_remote_mtu_for_local_cid(connection->l2cap_signaling_cid);
   
    packet[pos++] = (connection->transaction_id << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    packet[pos++] = connection->command_type;
    // subunit_type | subunit ID
    packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    packet[pos++] = (uint8_t)connection->command_opcode;

    // company id is 3 bytes long
    big_endian_store_24(packet, pos, BT_SIG_COMPANY_ID);
    pos += 3;

    packet[pos++] = AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES;
    
    // reserve byte for packet type
    uint8_t pos_packet_type = pos;
    pos++;
   
    uint16_t playing_info_buffer_len_position = pos;
    pos += 2;
    if (connection->next_attr_id == AVRCP_MEDIA_ATTR_NONE){
        packet[pos_packet_type] = AVRCP_SINGLE_PACKET;
        connection->packet_type = AVRCP_SINGLE_PACKET;
        packet[pos++] = count_set_bits_uint32(connection->now_playing_info_attr_bitmap);
        connection->next_attr_id = AVRCP_MEDIA_ATTR_ALL;
    }
    
    uint8_t fragmented = 0;
    int num_free_bytes = size - pos - 2;
    uint8_t MAX_NUMBER_ATTR_LEN = 10;

    while (!fragmented && (num_free_bytes > 0) && (connection->next_attr_id <= AVRCP_MEDIA_ATTR_SONG_LENGTH_MS)){
        avrcp_media_attribute_id_t attr_id = connection->next_attr_id;
        int attr_index = attr_id - 1;

        if (connection->now_playing_info_attr_bitmap & (1 << attr_id)){
            int num_written_bytes = 0;
            int num_bytes_to_write = 0;
            switch (attr_id){
                case AVRCP_MEDIA_ATTR_ALL:
                case AVRCP_MEDIA_ATTR_NONE:
                    break;
                case AVRCP_MEDIA_ATTR_TRACK:
                    num_bytes_to_write = AVRCP_ATTR_HEADER_LEN + MAX_NUMBER_ATTR_LEN;
                    if (num_free_bytes >= num_bytes_to_write){
                        num_written_bytes = avrcp_target_pack_single_element_attribute_number(packet, pos, attr_id, connection->track_nr);
                        break;
                    }
                    fragmented = 1;
                    connection->attribute_value_offset = 0;
                    break;
                case AVRCP_MEDIA_ATTR_TOTAL_NUM_ITEMS:
                    num_bytes_to_write = AVRCP_ATTR_HEADER_LEN + MAX_NUMBER_ATTR_LEN;
                    if (num_free_bytes >= num_bytes_to_write){
                        num_written_bytes = avrcp_target_pack_single_element_attribute_number(packet, pos, attr_id, connection->total_tracks);
                        break;
                    }
                    fragmented = 1;
                    connection->attribute_value_offset = 0;
                    break;
                case AVRCP_MEDIA_ATTR_SONG_LENGTH_MS:
                    num_bytes_to_write = AVRCP_ATTR_HEADER_LEN + MAX_NUMBER_ATTR_LEN;
                    if (num_free_bytes >= num_bytes_to_write){
                        num_written_bytes = avrcp_target_pack_single_element_attribute_number(packet, pos, attr_id, connection->song_length_ms);
                        break;
                    }
                    fragmented = 1;
                    connection->attribute_value_offset = 0;
                    break;
                default:{
                    bool      header = connection->attribute_value_offset == 0;
                    uint8_t * attr_value =     (uint8_t *) (connection->now_playing_info[attr_index].value + connection->attribute_value_offset);
                    uint16_t  attr_value_len = connection->now_playing_info[attr_index].len - connection->attribute_value_offset;

                    num_bytes_to_write = attr_value_len + (header * AVRCP_ATTR_HEADER_LEN);
                    if (num_bytes_to_write <= num_free_bytes){
                        connection->attribute_value_offset = 0;
                        num_written_bytes = num_bytes_to_write;
                        avrcp_target_pack_single_element_attribute_string_fragment(packet, pos, attr_id, attr_value, attr_value_len, connection->now_playing_info[attr_index].len, header);
                        break;
                    } 
                    fragmented = 1;
                    num_written_bytes = num_free_bytes;
                    attr_value_len = num_free_bytes - (header * AVRCP_ATTR_HEADER_LEN);
                    avrcp_target_pack_single_element_attribute_string_fragment(packet, pos, attr_id, attr_value, attr_value_len, connection->now_playing_info[attr_index].len, header);
                    connection->attribute_value_offset += attr_value_len;
                    break;
                }
            }
            pos += num_written_bytes;
            num_free_bytes -= num_written_bytes; 
        } 
        if (!fragmented){
            // C++ compatible version of connection->next_attr_id++
            connection->next_attr_id = (avrcp_media_attribute_id_t) (((int) connection->next_attr_id) + 1);
        }
    }

    if (fragmented){
        switch (connection->packet_type){
            case AVRCP_SINGLE_PACKET:
                connection->packet_type = AVRCP_START_PACKET;
                break;
            default:
                connection->packet_type = AVRCP_CONTINUE_PACKET;
                break;
        }
    } else {
        if (connection->next_attr_id >= AVRCP_MEDIA_ATTR_SONG_LENGTH_MS){ // DONE
            if (connection->packet_type != AVRCP_SINGLE_PACKET){
                connection->packet_type = AVRCP_END_PACKET;
            }
        }
    }
    packet[pos_packet_type] = connection->packet_type;
    // store attr value length
    big_endian_store_16(packet, playing_info_buffer_len_position, pos - playing_info_buffer_len_position - 2);
    return l2cap_send_prepared(cid, size); 
}



static int avrcp_target_send_response(uint16_t cid, avrcp_connection_t * connection){
    int pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)

    // TODO: check for fragmentation
    connection->packet_type = AVRCP_SINGLE_PACKET;

    packet[pos++] = (connection->transaction_id << 4) | (connection->packet_type << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    // command_type
    packet[pos++] = connection->command_type;
    // subunit_type | subunit ID
    packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    packet[pos++] = (uint8_t)connection->command_opcode;

    (void)memcpy(packet + pos, connection->cmd_operands,
                 connection->cmd_operands_length);
    pos += connection->cmd_operands_length;
    
    return l2cap_send_prepared(cid, pos);
}

static void avrcp_target_response_setup(avrcp_connection_t * connection, avrcp_command_type_t command_type, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id,
                                        avrcp_command_opcode_t opcode){
    connection->command_type = command_type;
    connection->subunit_type = subunit_type;
    connection->subunit_id =   subunit_id;
    connection->command_opcode = opcode;
}

static uint8_t avrcp_target_response_accept(avrcp_connection_t * connection, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t opcode, avrcp_pdu_id_t pdu_id, uint8_t status){
    // AVRCP_CTYPE_RESPONSE_REJECTED
    avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_ACCEPTED, subunit_type, subunit_id, opcode);
    // company id is 3 bytes long
    int pos = connection->cmd_operands_length;
    connection->cmd_operands[pos++] = pdu_id;
    connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = status;
    connection->cmd_operands_length = pos;
    connection->accept_response = 1;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_reject(avrcp_connection_t * connection, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t opcode, avrcp_pdu_id_t pdu_id, avrcp_status_code_t status){
    // AVRCP_CTYPE_RESPONSE_REJECTED
    avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_REJECTED, subunit_type, subunit_id, opcode);
    // company id is 3 bytes long
    int pos = connection->cmd_operands_length;
    connection->cmd_operands[pos++] = pdu_id;
    connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = status;
    connection->cmd_operands_length = pos;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_not_implemented(avrcp_connection_t * connection, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t opcode, avrcp_pdu_id_t pdu_id, uint8_t event_id){
    avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_NOT_IMPLEMENTED, subunit_type, subunit_id, opcode);
    // company id is 3 bytes long
    int pos = connection->cmd_operands_length;
    connection->cmd_operands[pos++] = pdu_id;
    connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = event_id;
    connection->cmd_operands_length = pos;
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_interim(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, uint8_t event_id, const uint8_t * value, uint16_t value_len){

    // company id is 3 bytes long
    int pos = connection->cmd_operands_length;
    connection->cmd_operands[pos++] = pdu_id;
    connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1 + value_len);
    pos += 2;
    connection->cmd_operands[pos++] = event_id;
    if (value && (value_len > 0)){
        (void)memcpy(connection->cmd_operands + pos, value, value_len);
        pos += value_len;
    }
    connection->cmd_operands_length = pos;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_addressed_player_changed_interim(avrcp_connection_t * connection, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t opcode, avrcp_pdu_id_t pdu_id){
    avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);

    // company id is 3 bytes long
    int pos = connection->cmd_operands_length;
    connection->cmd_operands[pos++] = pdu_id;
    connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 5);
    pos += 2;
    connection->cmd_operands[pos++] = AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED;
    big_endian_read_16( &connection->cmd_operands[pos], connection->addressed_player_id);
    pos += 2;
    big_endian_read_16( &connection->cmd_operands[pos], connection->uid_counter);
    pos += 2;
    
    connection->cmd_operands_length = pos;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_pass_through_response(uint16_t avrcp_cid, avrcp_command_type_t cmd_type, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("Could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    avrcp_target_response_setup(connection, cmd_type, AVRCP_SUBUNIT_TYPE_PANEL, AVRCP_SUBUNIT_ID, AVRCP_CMD_OPCODE_PASS_THROUGH);

    int pos = 0; 
    connection->cmd_operands[pos++] = opid;
    connection->cmd_operands[pos++] = operands_length;
    if (operands_length == 1){
        connection->cmd_operands[pos++] = operand;
    }
    connection->cmd_operands_length = pos;    
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_operation_rejected(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand){
    return avrcp_target_pass_through_response(avrcp_cid, AVRCP_CTYPE_RESPONSE_REJECTED, opid, operands_length, operand);
}

uint8_t avrcp_target_operation_accepted(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand){
    return avrcp_target_pass_through_response(avrcp_cid, AVRCP_CTYPE_RESPONSE_ACCEPTED, opid, operands_length, operand);
}

uint8_t avrcp_target_operation_not_implemented(uint16_t avrcp_cid, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand){
    return avrcp_target_pass_through_response(avrcp_cid, AVRCP_CTYPE_RESPONSE_ACCEPTED, opid, operands_length, operand);
}

void avrcp_target_set_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_target_set_unit_info: could not find a connection.");
        return; 
    }
    connection->unit_type = unit_type;
    connection->company_id = company_id;
}

void avrcp_target_set_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, const uint8_t * subunit_info_data, uint16_t subunit_info_data_size){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_target_set_subunit_info: could not find a connection.");
        return; 
    }
    connection->subunit_info_type = subunit_type;
    connection->subunit_info_data = subunit_info_data;
    connection->subunit_info_data_size = subunit_info_data_size;    
}

static uint8_t avrcp_target_unit_info(avrcp_connection_t * connection){
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    uint8_t unit = 0;
    connection->command_type = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    connection->command_opcode = AVRCP_CMD_OPCODE_UNIT_INFO;
    
    connection->cmd_operands_length = 5;
    connection->cmd_operands[0] = 0x07;
    connection->cmd_operands[1] = (connection->unit_type << 4) | unit;
    // company id is 3 bytes long
    big_endian_store_32(connection->cmd_operands, 2, connection->company_id);
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}


static uint8_t avrcp_target_subunit_info(avrcp_connection_t * connection, uint8_t offset){
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    if ((offset - 4) > connection->subunit_info_data_size) return AVRCP_STATUS_INVALID_PARAMETER;
            
    connection->command_opcode = AVRCP_CMD_OPCODE_SUBUNIT_INFO;
    connection->command_type = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;

    uint8_t page = offset / 4;
    uint8_t extension_code = 7;
    connection->cmd_operands_length = 5;
    connection->cmd_operands[0] = (page << 4) | extension_code;

    (void)memcpy(connection->cmd_operands + 1,
                 connection->subunit_info_data + offset, 4);
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static inline uint8_t avrcp_prepare_vendor_dependent_response(uint16_t avrcp_cid, avrcp_connection_t ** out_connection, avrcp_pdu_id_t pdu_id, uint16_t param_length){
    *out_connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!*out_connection){
        log_error("avrcp tartget: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    if ((*out_connection)->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    (*out_connection)->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    (*out_connection)->command_type    = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    (*out_connection)->subunit_type    = AVRCP_SUBUNIT_TYPE_PANEL; 
    (*out_connection)->subunit_id      = AVRCP_SUBUNIT_ID;

    (*out_connection)->cmd_operands[(*out_connection)->cmd_operands_length++] = pdu_id;
    // reserved
    (*out_connection)->cmd_operands[(*out_connection)->cmd_operands_length++] = 0;
    // param length
    big_endian_store_16((*out_connection)->cmd_operands, (*out_connection)->cmd_operands_length, param_length);
    (*out_connection)->cmd_operands_length += 2;
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_capability(uint16_t avrcp_cid, avrcp_capability_id_t capability_id, uint8_t num_capabilities, const uint8_t *capabilities, uint8_t capabilities_size){
    avrcp_connection_t * connection = NULL;
    uint8_t status = avrcp_prepare_vendor_dependent_response(avrcp_cid, &connection, AVRCP_PDU_ID_GET_CAPABILITIES, 2 + capabilities_size);
    if (status != ERROR_CODE_SUCCESS) return status;

    connection->cmd_operands[connection->cmd_operands_length++] = capability_id;
    connection->cmd_operands[connection->cmd_operands_length++] = num_capabilities;
    (void)memcpy(connection->cmd_operands + connection->cmd_operands_length,
                 capabilities, capabilities_size);
    connection->cmd_operands_length += capabilities_size;
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_supported_events(uint16_t avrcp_cid, uint8_t num_event_ids, const uint8_t *event_ids, uint8_t event_ids_size){
    return avrcp_target_capability(avrcp_cid, AVRCP_CAPABILITY_ID_EVENT, num_event_ids, event_ids, event_ids_size);
}

uint8_t avrcp_target_supported_companies(uint16_t avrcp_cid, uint8_t num_company_ids, const uint8_t *company_ids, uint8_t company_ids_size){
    return avrcp_target_capability(avrcp_cid, AVRCP_CAPABILITY_ID_COMPANY, num_company_ids, company_ids, company_ids_size);
}

uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_playback_status_t play_status){
    avrcp_connection_t * connection = NULL;
    uint8_t status = avrcp_prepare_vendor_dependent_response(avrcp_cid, &connection, AVRCP_PDU_ID_GET_PLAY_STATUS, 11);
    if (status != ERROR_CODE_SUCCESS) return status;

    big_endian_store_32(connection->cmd_operands, connection->cmd_operands_length, song_length_ms);
    connection->cmd_operands_length += 4;
    big_endian_store_32(connection->cmd_operands, connection->cmd_operands_length, song_position_ms);
    connection->cmd_operands_length += 4;
    connection->cmd_operands[connection->cmd_operands_length++] = play_status;
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_now_playing_info(avrcp_connection_t * connection){
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->now_playing_info_response = 1;
    connection->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type    = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type    = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id      = AVRCP_SUBUNIT_ID;

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_store_media_attr(avrcp_connection_t * connection, avrcp_media_attribute_id_t attr_id, const char * value){
    int index = attr_id - 1;
    if (!value) return AVRCP_STATUS_INVALID_PARAMETER;
    connection->now_playing_info[index].value = (uint8_t*)value;
    connection->now_playing_info[index].len   = strlen(value);
    return ERROR_CODE_SUCCESS;
}   

uint8_t avrcp_target_set_playback_status(uint16_t avrcp_cid, avrcp_playback_status_t playback_status){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->playback_status == playback_status) return ERROR_CODE_SUCCESS;

    connection->playback_status = playback_status;
    connection->playback_status_changed = 1;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

void avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t * current_track, uint16_t total_tracks){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection. cid 0x%02x\n", avrcp_cid);
        return; 
    }
    if (!current_track){
        connection->track_selected = 0;
        connection->playback_status = AVRCP_PLAYBACK_STATUS_ERROR;
        return;
    } 
    (void)memcpy(connection->track_id, current_track->track_id, 8);
    connection->song_length_ms = current_track->song_length_ms;
    connection->track_nr = current_track->track_nr;
    connection->total_tracks = total_tracks;
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_TITLE, current_track->title);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_ARTIST, current_track->artist);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_ALBUM, current_track->album);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_GENRE, current_track->genre);
    connection->track_selected = 1;
        
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED)) {
        connection->track_changed = 1;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return;
}

uint8_t avrcp_target_track_changed(uint16_t avrcp_cid, uint8_t * track_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_target_track_changed: could not find connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (!track_id) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED)) {
        connection->track_changed = 1;
        (void)memcpy(connection->track_id, track_id, 8);
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_playing_content_changed(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_target_playing_content_changed: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED)) {
        connection->playing_content_changed = 1;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_addressed_player_changed(uint16_t avrcp_cid, uint16_t player_id, uint16_t uid_counter){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED)) {
        connection->uid_counter = uid_counter;
        connection->addressed_player_id = player_id;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_battery_status_changed(uint16_t avrcp_cid, avrcp_battery_status_t battery_status){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->battery_status == battery_status) return ERROR_CODE_SUCCESS;
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED)) {
        connection->battery_status = battery_status;
        connection->battery_status_changed = 1;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_volume_changed(uint16_t avrcp_cid, uint8_t volume_percentage){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->volume_percentage = volume_percentage;
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED )) {
        connection->notify_volume_percentage_changed = 1;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

static void avrcp_target_set_transaction_label_for_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification, uint8_t transaction_label){
    if (notification > AVRCP_NOTIFICATION_EVENT_MAX_VALUE) return;
    connection->notifications_transaction_label[notification] = transaction_label;
}

static uint8_t avrcp_target_get_transaction_label_for_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification){
    if (notification > AVRCP_NOTIFICATION_EVENT_MAX_VALUE) return 0;
    return connection->notifications_transaction_label[notification];
}

static bool avcrp_operation_id_is_valid(avrcp_operation_id_t operation_id){
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_1) return true;

    if (operation_id < AVRCP_OPERATION_ID_0) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_2) return true;

    if (operation_id < AVRCP_OPERATION_ID_CHANNEL_UP) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_3) return true;

    if (operation_id < AVRCP_OPERATION_ID_CHANNEL_UP) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_3) return true;

    if (operation_id < AVRCP_OPERATION_ID_SKIP) return false;
    if (operation_id == AVRCP_OPERATION_ID_SKIP) return true;
    
    if (operation_id < AVRCP_OPERATION_ID_POWER) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_4) return true;
    
    if (operation_id < AVRCP_OPERATION_ID_ANGLE) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_5) return true;
    
    if (operation_id < AVRCP_OPERATION_ID_F1) return false;
    if (operation_id < AVRCP_OPERATION_ID_RESERVED_6) return true;
    
    return false;
}

static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t *packet, uint16_t size){

    if (size < 6u) return;

    uint16_t pid = 0;
    uint8_t transport_header = packet[0];
    connection->transaction_id = transport_header >> 4;

    avrcp_packet_type_t packet_type = (avrcp_packet_type_t) ((transport_header & 0x0F) >> 2);
    switch (packet_type){
        case AVRCP_SINGLE_PACKET:
            pid =  big_endian_read_16(packet, 1);
            break;
        case AVRCP_START_PACKET:
            pid =  big_endian_read_16(packet, 2);
            break;
        default:
            break;
    }
        
    switch (packet_type){
        case AVRCP_SINGLE_PACKET:
        case AVRCP_START_PACKET:
            if (pid != BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL){
                log_info("Invalid pid 0x%02x, expected 0x%02x", connection->invalid_pid, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL);
                connection->reject_transport_header = 1;
                connection->invalid_pid = pid;
                connection->transport_header = (connection->transaction_id << 4) | (AVRCP_SINGLE_PACKET << 2 ) | (AVRCP_RESPONSE_FRAME << 1) | 1;
                connection->state = AVCTP_W2_SEND_RESPONSE;
                avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                return;
            }   
            break;
        default:
            break;
    }

    if (packet_type != AVRCP_SINGLE_PACKET) return;
    
    avrcp_subunit_type_t subunit_type = (avrcp_subunit_type_t) (packet[4] >> 3);
    avrcp_subunit_id_t   subunit_id   = (avrcp_subunit_id_t) (packet[4] & 0x07);
    
    avrcp_command_opcode_t opcode = (avrcp_command_opcode_t) avrcp_cmd_opcode(packet,size);

    int pos = 6;
    uint16_t length;
    avrcp_pdu_id_t   pdu_id;
    connection->cmd_operands_length = 0;
    uint8_t offset;
    uint8_t operand;
    avrcp_operation_id_t operation_id;

    switch (opcode){
        case AVRCP_CMD_OPCODE_UNIT_INFO:
            avrcp_target_unit_info(connection);
            break;
        case AVRCP_CMD_OPCODE_SUBUNIT_INFO:
            if ((size - pos) < 3) return;
            // page: packet[pos] >> 4, 
            offset =  4 * (packet[pos]>>4);
            // extension code (fixed 7) = packet[pos] & 0x0F
            // 4 bytes paga data, all 0xFF
            avrcp_target_subunit_info(connection, offset);
            break;

        case AVRCP_CMD_OPCODE_PASS_THROUGH:
            if (size < 8) return;
            log_info("AVRCP_OPERATION_ID 0x%02x, operands length %d", packet[6], packet[7]);
            operation_id = (avrcp_operation_id_t) (packet[6] & 0x7f);
            operand = 0;
            if ((packet[7] >= 1) && (size >= 9)){
                operand = packet[8];
            }

            if (avcrp_operation_id_is_valid(operation_id)){
                bool button_pressed = (packet[6] & 0x80) == 0;
            
                avrcp_target_operation_accepted(connection->avrcp_cid, (avrcp_operation_id_t) packet[6], packet[7], operand);
                avrcp_target_emit_operation(avrcp_target_context.avrcp_callback, connection->avrcp_cid,
                                                operation_id, button_pressed, packet[7], operand);
            } else {
                avrcp_target_operation_not_implemented(connection->avrcp_cid, (avrcp_operation_id_t) packet[6], packet[7], operand);
            }
            break;
    

        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:

            if (size < 13) return;

            // pos = 6 - company id
            (void)memcpy(connection->cmd_operands, &packet[pos], 3);
            connection->cmd_operands_length = 3;
            pos += 3;
            // pos = 9
            pdu_id = (avrcp_pdu_id_t) packet[pos++];
            // 1 - reserved
            pos++;
            // 2-3 param length,
            length = big_endian_read_16(packet, pos);
            pos += 2;
            // pos = 13
            switch (pdu_id){
                case AVRCP_PDU_ID_SET_ADDRESSED_PLAYER:{
                    if ((pos + 2) > size) return;
                    bool ok = length == 4;
                    if (avrcp_target_context.set_addressed_player_callback != NULL){
                        uint16_t player_id = big_endian_read_16(packet, pos);
                        ok = avrcp_target_context.set_addressed_player_callback(player_id);
                    }
                    if (ok){
                        avrcp_target_response_accept(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_SUCCESS);
                    } else {
                        avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_PLAYER_ID);
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_CAPABILITIES:{
                    avrcp_capability_id_t capability_id = (avrcp_capability_id_t) packet[pos];
                    switch (capability_id){
                        case AVRCP_CAPABILITY_ID_EVENT:
                            avrcp_target_emit_respond_vendor_dependent_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_EVENT_IDS_QUERY);
                            break;
                        case AVRCP_CAPABILITY_ID_COMPANY:
                            avrcp_target_emit_respond_vendor_dependent_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_COMPANY_IDS_QUERY);
                            break;
                        default:
                            avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                            break;
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_PLAY_STATUS:
                    avrcp_target_emit_respond_vendor_dependent_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_PLAY_STATUS_QUERY);
                    break;
                case AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE:
                    if ((pos + 1) > size) return;
                    connection->cmd_operands[0] = packet[pos];
                    connection->abort_continue_response = 1;
                    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    break;
                case AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE:
                    if ((pos + 1) > size) return;
                    if (packet[pos] != AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES){
                        avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                        return;
                    }
                    connection->now_playing_info_response = 1;
                    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    break;
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:{
                    if ((pos + 9) > size) return;
                    uint8_t play_identifier[8];
                    memset(play_identifier, 0, 8);
                    if (memcmp(&packet[pos], play_identifier, 8) != 0) {
                        avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                        return;
                    }
                    pos += 8;
                    uint8_t attribute_count = packet[pos++];
                    connection->next_attr_id = AVRCP_MEDIA_ATTR_NONE;
                    if (!attribute_count){
                        connection->now_playing_info_attr_bitmap = 0xFE;
                    } else {
                        int i;
                        connection->now_playing_info_attr_bitmap = 0;
                        if ((pos + attribute_count * 2) > size) return;
                        for (i=0; i < attribute_count; i++){
                            uint16_t attr_id = big_endian_read_16(packet, pos);
                            pos += 2;
                            connection->now_playing_info_attr_bitmap |= (1 << attr_id);
                        }
                    }
                    log_info("now_playing_info_attr_bitmap 0x%02x", connection->now_playing_info_attr_bitmap);
                    avrcp_target_now_playing_info(connection);
                    break;
                }
                case AVRCP_PDU_ID_REGISTER_NOTIFICATION:{
                    if ((pos + 1) > size) return;
                    avrcp_notification_event_id_t event_id = (avrcp_notification_event_id_t) packet[pos];
                    uint16_t event_mask = (1 << event_id);
                    avrcp_target_set_transaction_label_for_notification(connection, event_id, connection->transaction_id);
                            
                    switch (event_id){
                        case AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED:
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);
                            if (connection->track_selected){
                                avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, AVRCP_NOTIFICATION_TRACK_SELECTED, 8);
                            } else {
                                avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, AVRCP_NOTIFICATION_TRACK_NOT_SELECTED, 8);
                            }
                            break;
                        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED:
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->playback_status, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED: 
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, NULL, 0);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED:
                            connection->notify_volume_percentage_changed = 0;
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->volume_percentage, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED:
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_setup(connection, AVRCP_CTYPE_RESPONSE_INTERIM, subunit_type, subunit_id, opcode);
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->battery_status, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED:
                            avrcp_target_response_not_implemented(connection, subunit_type, subunit_id, opcode, pdu_id, event_id);
                            return;
                        case AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED:
                            connection->notifications_enabled |= event_mask;
                            avrcp_target_response_addressed_player_changed_interim(connection, subunit_type, subunit_id, opcode, pdu_id);
                            return;
                        default:
                            avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                            return;
                    }
                    break;
                }
                case AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME: {
                    if (length != 1){
                        avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                        break;
                    }

                    if ((pos + 1) > size) return;
                    uint8_t absolute_volume = packet[pos];
                    if (absolute_volume < 0x80){
                        connection->volume_percentage = absolute_volume;
                    }
                    avrcp_target_response_accept(connection, subunit_type, subunit_id, opcode, pdu_id, connection->volume_percentage);
                    avrcp_target_emit_volume_changed(avrcp_target_context.avrcp_callback, connection->avrcp_cid, connection->volume_percentage);
                    break;
                }
                default:
                    log_info("AVRCP target: unhandled pdu id 0x%02x", pdu_id);
                    avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                    break;
            }
            break;
        default:
            log_info("AVRCP target: opcode 0x%02x not implemented", avrcp_cmd_opcode(packet,size));
            break;
    }
}

static int avrcp_target_send_notification(uint16_t cid, avrcp_connection_t * connection, avrcp_notification_event_id_t notification_id, uint8_t * value, uint16_t value_len){
    if (!connection){
        log_error("avrcp tartget: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    btstack_assert((value_len == 0) || (value != NULL));

    connection->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type    = AVRCP_CTYPE_RESPONSE_CHANGED_STABLE;
    connection->subunit_type    = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id      = AVRCP_SUBUNIT_ID;
    connection->transaction_id = avrcp_target_get_transaction_label_for_notification(connection, notification_id);
                        
    uint16_t pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    // value <= 8 ==> pdu <= 22 < L2CAP Default MTU
    btstack_assert((14 + value_len) <= l2cap_get_remote_mtu_for_local_cid(connection->l2cap_signaling_cid));

    connection->packet_type = AVRCP_SINGLE_PACKET;
    packet[pos++] = (connection->transaction_id << 4) | (connection->packet_type << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    packet[pos++] = connection->command_type;
    // subunit_type | subunit ID
    packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    packet[pos++] = (uint8_t)connection->command_opcode;

    // company id is 3 bytes long
    big_endian_store_24(packet, pos, BT_SIG_COMPANY_ID);
    pos += 3;

    packet[pos++] = AVRCP_PDU_ID_REGISTER_NOTIFICATION; 
    packet[pos++] = 0;

    big_endian_store_16(packet, pos, 1 + value_len);
    pos += 2;
    packet[pos++] = notification_id;
    if (value_len > 0){
        (void)memcpy(packet + pos, value, value_len);
        pos += value_len;
    }

    return l2cap_send_prepared(cid, pos);
}

static void avrcp_target_reset_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification_id){
    if (!connection){
        log_error("avrcp tartget: could not find a connection.");
        return;
    }
    connection->notifications_enabled &= ~(1 << notification_id);
    connection->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    avrcp_target_set_transaction_label_for_notification(connection, notification_id, 0);
}

static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_connection_t * connection;
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, channel);
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:{
                    connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, channel);
                    
                    if (connection->accept_response){
                        connection->accept_response = 0;
                        avrcp_target_send_response(connection->l2cap_signaling_cid, connection);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }

                    if (connection->abort_continue_response){
                        connection->abort_continue_response = 0;
                        connection->now_playing_info_response = 0;
                        avrcp_target_abort_continue_response(connection->l2cap_signaling_cid, connection);
                        return;
                    }

                    if (connection->now_playing_info_response){
                        connection->now_playing_info_response = 0;
                        avrcp_target_send_now_playing_info(connection->l2cap_signaling_cid, connection);
                        return;
                    }
                    
                    if (connection->track_changed){
                        connection->track_changed = 0;
                        avrcp_target_send_notification(connection->l2cap_signaling_cid, connection, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED, connection->track_id, 8);
                        avrcp_target_reset_notification(connection, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }
                    
                    if (connection->playback_status_changed){
                        connection->playback_status_changed = 0;
                        uint8_t playback_status = (uint8_t) connection->playback_status;
                        avrcp_target_send_notification(connection->l2cap_signaling_cid, connection, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED, &playback_status, 1);
                        avrcp_target_reset_notification(connection, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }
                    
                    if (connection->playing_content_changed){
                        connection->playing_content_changed = 0;
                        avrcp_target_send_notification(connection->l2cap_signaling_cid, connection, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED, NULL, 0);
                        avrcp_target_reset_notification(connection, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }
                    
                    if (connection->battery_status_changed){
                        connection->battery_status_changed = 0;
                        avrcp_target_send_notification(connection->l2cap_signaling_cid, connection, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED, (uint8_t *)&connection->battery_status, 1);
                        avrcp_target_reset_notification(connection, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }
                    
                    if (connection->notify_volume_percentage_changed){
                        connection->notify_volume_percentage_changed = 0;
                        avrcp_target_send_notification(connection->l2cap_signaling_cid, connection, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED, &connection->volume_percentage, 1);
                        avrcp_target_reset_notification(connection, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }

                    if (connection->reject_transport_header){
                        connection->state = AVCTP_CONNECTION_OPENED;
                        connection->reject_transport_header = 0;
                        l2cap_reserve_packet_buffer();
                        uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                        out_buffer[0] = connection->transport_header;
                        big_endian_store_16(out_buffer, 1, connection->invalid_pid);
                        l2cap_send_prepared(connection->l2cap_signaling_cid, 3);
                        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                        return;
                    }

                    switch (connection->state){
                        case AVCTP_W2_SEND_RESPONSE:
                            connection->state = AVCTP_CONNECTION_OPENED;
                            avrcp_target_send_response(connection->l2cap_signaling_cid, connection);
                            avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                            return;
                        default:
                            break;
                    }

                    break;
            }
            default:
                break;
        }
        default:
            break;
    }
}

void avrcp_target_init(void){
    avrcp_target_context.role = AVRCP_TARGET;
    avrcp_target_context.packet_handler = avrcp_target_packet_handler;
    avrcp_register_target_packet_handler(&avrcp_target_packet_handler);
}

void avrcp_target_deinit(void){
    memset(&avrcp_target_context, 0, sizeof(avrcp_context_t));
}

void avrcp_target_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_target_context.avrcp_callback = callback;
}

void avrcp_target_register_set_addressed_player_handler(bool (*callback)(uint16_t player_id)){
    btstack_assert(callback != NULL);
    avrcp_target_context.set_addressed_player_callback = callback;
}



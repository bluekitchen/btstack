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

#define __BTSTACK_FILE__ "avrcp_target.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack.h"
#include "classic/avrcp.h"

static avrcp_context_t avrcp_target_context;

void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    avrcp_create_sdp_record(0, service, service_record_handle, browsing, supported_features, service_name, service_provider_name);
}

static void avrcp_target_emit_respond_query(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t subeventID){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subeventID; 
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_target_emit_respond_subunit_info_query(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t offset){
    if (!callback) return;
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_SUBUNIT_INFO_QUERY; 
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    event[pos++] = offset; 
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_target_emit_respond_vendor_dependent_query(btstack_packet_handler_t callback, uint16_t avrcp_cid, uint8_t subevent_id){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_id; 
    little_endian_store_16(event, pos, avrcp_cid);
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint16_t avrcp_target_pack_single_element_attribute_number(uint8_t * packet, uint16_t size, uint16_t pos, avrcp_media_attribute_id_t attr_id, uint32_t value){
    UNUSED(size);
    if ((attr_id < 1) || (attr_id > AVRCP_MEDIA_ATTR_COUNT)) return 0;
    uint16_t attr_value_length = sprintf((char *)(packet+pos+8), "%0" PRIu32, value);
    big_endian_store_32(packet, pos, attr_id); 
    pos += 4;
    big_endian_store_16(packet, pos, UTF8);
    pos += 2;
    big_endian_store_16(packet, pos, attr_value_length);
    pos += 2;
    return 8 + attr_value_length;
}

static uint16_t avrcp_target_pack_single_element_attribute_string(uint8_t * packet, uint16_t size, uint16_t pos, rfc2978_charset_mib_enumid_t mib_enumid, avrcp_media_attribute_id_t attr_id, uint8_t * attr_value, uint16_t attr_value_length){
    UNUSED(size);
    if (attr_value_length == 0) return 0;
    if ((attr_id < 1) || (attr_id > AVRCP_MEDIA_ATTR_COUNT)) return 0;
    big_endian_store_32(packet, pos, attr_id); 
    pos += 4;
    big_endian_store_16(packet, pos, mib_enumid);
    pos += 2;
    big_endian_store_16(packet, pos, attr_value_length);
    pos += 2;
    memcpy(packet+pos, attr_value, attr_value_length);
    pos += attr_value_length;
    return 8 + attr_value_length;
}

static int avrcp_target_send_now_playing_info(uint16_t cid, avrcp_connection_t * connection){
    uint16_t pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();
    uint16_t  size   = l2cap_get_remote_mtu_for_local_cid(connection->l2cap_signaling_cid);

    packet[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
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
    packet[pos++] = 0;
    
    // num_attrs
    int i;
    
    uint16_t playing_info_buffer_len = 1;
    uint16_t playing_info_buffer_len_position = pos;
    
    uint8_t  media_attr_count = 0;
    uint16_t media_attr_count_position = pos + 2;
    pos += 3;

    for (i = 0; i < AVRCP_MEDIA_ATTR_COUNT; i++){
        int attr_id = i+1;
        int attr_len;
        switch (attr_id){
            case AVRCP_MEDIA_ATTR_TRACK:
                attr_len = avrcp_target_pack_single_element_attribute_number(packet, size, pos, attr_id, connection->track_nr);
                break;
            case AVRCP_MEDIA_ATTR_TOTAL_TRACKS:
                attr_len = avrcp_target_pack_single_element_attribute_number(packet, size, pos, attr_id, connection->total_tracks);
                break;
            case AVRCP_MEDIA_ATTR_SONG_LENGTH:
                attr_len = avrcp_target_pack_single_element_attribute_number(packet, size, pos, attr_id, connection->song_length_ms);
                break;
            default:
                attr_len = avrcp_target_pack_single_element_attribute_string(packet, size, pos, UTF8, attr_id, connection->now_playing_info[i].value, connection->now_playing_info[i].len);
                break;
        }
        if (attr_len > 0) {
            pos += attr_len;
            playing_info_buffer_len += attr_len;
            media_attr_count++;
        }
    }
    big_endian_store_16(packet, playing_info_buffer_len_position, playing_info_buffer_len);
    packet[media_attr_count_position] = media_attr_count;

    // TODO fragmentation
    if (playing_info_buffer_len + pos > l2cap_get_remote_mtu_for_local_cid(connection->l2cap_signaling_cid)) {
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }    

    connection->wait_to_send = 0;
    return l2cap_send_prepared(cid, pos);
}

static int avrcp_target_send_response(uint16_t cid, avrcp_connection_t * connection){
    int pos = 0; 
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    packet[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

    // command_type
    packet[pos++] = connection->command_type;
    // subunit_type | subunit ID
    packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
    // opcode
    packet[pos++] = (uint8_t)connection->command_opcode;
    // operands
    memcpy(packet+pos, connection->cmd_operands, connection->cmd_operands_length);
    pos += connection->cmd_operands_length;
    connection->wait_to_send = 0;
    return l2cap_send_prepared(cid, pos);
}

static uint8_t avrcp_target_response_reject(avrcp_connection_t * connection, avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, avrcp_command_opcode_t opcode, avrcp_pdu_id_t pdu_id, avrcp_status_code_t status){
    // AVRCP_CTYPE_RESPONSE_REJECTED
    connection->command_type = AVRCP_CTYPE_RESPONSE_REJECTED;
    connection->subunit_type = subunit_type; 
    connection->subunit_id =   subunit_id;
    connection->command_opcode = opcode;
    
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

uint8_t avrcp_target_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    uint8_t unit = 0;
    connection->command_type = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type = AVRCP_SUBUNIT_TYPE_UNIT; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;
    connection->command_opcode = AVRCP_CMD_OPCODE_UNIT_INFO;
    
    connection->cmd_operands_length = 5;
    connection->cmd_operands[0] = 0x07;
    connection->cmd_operands[1] = (unit_type << 4) | unit;
    // company id is 3 bytes long
    big_endian_store_32(connection->cmd_operands, 2, company_id);
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, uint8_t offset, uint8_t * subunit_info_data){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    connection->command_opcode = AVRCP_CMD_OPCODE_SUBUNIT_INFO;
    connection->command_type = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type = subunit_type; //vendor unique
    connection->subunit_id =   AVRCP_SUBUNIT_ID_IGNORE;

    uint8_t page = offset / 4;
    uint8_t extension_code = 7;
    connection->cmd_operands_length = 5;
    connection->cmd_operands[0] = (page << 4) | extension_code;
    memcpy(connection->cmd_operands+1, subunit_info_data + offset, 4);
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static inline uint8_t avrcp_prepare_vendor_dependent_response(uint16_t avrcp_cid, avrcp_connection_t ** out_connection, avrcp_pdu_id_t pdu_id, uint16_t param_length){
    *out_connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
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

static uint8_t avrcp_target_capability(uint16_t avrcp_cid, avrcp_capability_id_t capability_id, uint8_t capabilities_num, uint8_t * capabilities, uint8_t size){
    avrcp_connection_t * connection = NULL;
    uint8_t status = avrcp_prepare_vendor_dependent_response(avrcp_cid, &connection, AVRCP_PDU_ID_GET_CAPABILITIES, 2+size);
    if (status != ERROR_CODE_SUCCESS) return status;

    connection->cmd_operands[connection->cmd_operands_length++] = capability_id;
    connection->cmd_operands[connection->cmd_operands_length++] = capabilities_num;
    memcpy(connection->cmd_operands+connection->cmd_operands_length, capabilities, size);
    connection->cmd_operands_length += size;
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_supported_events(uint16_t avrcp_cid, uint8_t capabilities_num, uint8_t * capabilities, uint8_t size){
    return avrcp_target_capability(avrcp_cid, AVRCP_CAPABILITY_ID_EVENT, capabilities_num, capabilities, size);
}

uint8_t avrcp_target_supported_companies(uint16_t avrcp_cid, uint8_t capabilities_num, uint8_t * capabilities, uint8_t size ){
    return avrcp_target_capability(avrcp_cid, AVRCP_CAPABILITY_ID_COMPANY, capabilities_num, capabilities, size);
}

uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_play_status_t play_status){
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

uint8_t avrcp_target_now_playing_info(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp tartget: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    connection->command_opcode  = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->command_type    = AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE;
    connection->subunit_type    = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id      = AVRCP_SUBUNIT_ID;

    connection->now_playing_info_response = 1;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_store_media_attr(uint16_t avrcp_cid, avrcp_media_attribute_id_t attr_id, const char * value){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    int index = attr_id - 1;
    connection->now_playing_info[index].value = (uint8_t*)value;
    connection->now_playing_info[index].len   = strlen(value);
    return ERROR_CODE_SUCCESS;
}   

uint8_t avrcp_target_set_now_playing_title(uint16_t avrcp_cid, const char * title){
    return avrcp_target_store_media_attr(avrcp_cid, AVRCP_MEDIA_ATTR_TITLE, title);
}

uint8_t avrcp_target_set_now_playing_artist(uint16_t avrcp_cid, const char * artist){
    return avrcp_target_store_media_attr(avrcp_cid, AVRCP_MEDIA_ATTR_ARTIST, artist);
}

uint8_t avrcp_target_set_now_playing_album(uint16_t avrcp_cid, const char * album){
    return avrcp_target_store_media_attr(avrcp_cid, AVRCP_MEDIA_ATTR_ALBUM, album);
}

uint8_t avrcp_target_set_now_playing_genre(uint16_t avrcp_cid, const char * genre){
    return avrcp_target_store_media_attr(avrcp_cid, AVRCP_MEDIA_ATTR_GENRE, genre);
}

uint8_t avrcp_target_set_now_playing_song_length_ms(uint16_t avrcp_cid, const uint32_t song_length_ms){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->song_length_ms = song_length_ms;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_set_now_playing_total_tracks(uint16_t avrcp_cid, const int total_tracks){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->total_tracks = total_tracks;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_set_now_playing_track_nr(uint16_t avrcp_cid, const int track_nr){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_unit_info: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->track_nr = track_nr;
    return ERROR_CODE_SUCCESS;
}

static uint8_t * avrcp_get_company_id(uint8_t *packet, uint16_t size){
    UNUSED(size);
    return packet + 6;
}

static uint8_t * avrcp_get_pdu(uint8_t *packet, uint16_t size){
    UNUSED(size);
    return packet + 9;
}

static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t *packet, uint16_t size){
    UNUSED(connection);
    UNUSED(packet);
    UNUSED(size);

    // uint8_t opcode;
    
    uint8_t transport_header = packet[0];
    connection->transaction_label = transport_header >> 4;
    // uint8_t packet_type = (transport_header & 0x0F) >> 2;
    // uint8_t frame_type = (transport_header & 0x03) >> 1;
    // uint8_t ipid = transport_header & 0x01;
    // uint8_t byte_value = packet[2];
    // uint16_t pid = (byte_value << 8) | packet[2];
    
    // avrcp_command_type_t ctype = (avrcp_command_type_t) packet[3];
    // uint8_t byte_value = packet[4];
    avrcp_subunit_type_t subunit_type = (avrcp_subunit_type_t) (packet[4] >> 3);
    avrcp_subunit_id_t   subunit_id   = (avrcp_subunit_id_t) (packet[4] & 0x07);
    // opcode = packet[pos++];
    
    // printf("    Transport header 0x%02x (transaction_label %d, packet_type %d, frame_type %d, ipid %d), pid 0x%4x\n", 
    //     transport_header, transaction_label, packet_type, frame_type, ipid, pid);
    // printf_hexdump(packet+pos, size-pos);
    
    avrcp_command_opcode_t opcode = avrcp_cmd_opcode(packet,size);
    uint8_t * company_id = avrcp_get_company_id(packet, size);
    uint8_t * pdu = avrcp_get_pdu(packet, size);
    // uint16_t param_length = big_endian_read_16(pdu, 2);
    
    int pos = 4;
    uint8_t   pdu_id;
    connection->cmd_operands_length = 0;
    
    switch (opcode){
        case AVRCP_CMD_OPCODE_UNIT_INFO:
            avrcp_target_emit_respond_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_UNIT_INFO_QUERY);
            break;
        case AVRCP_CMD_OPCODE_SUBUNIT_INFO:{
            uint8_t offset =  4 * (packet[pos+2]>>4);
            avrcp_target_emit_respond_subunit_info_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, offset);
            break;
        }
        case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
            pdu_id = pdu[0];
            // 1 - reserved
            // 2-3 param length, 
            memcpy(connection->cmd_operands, company_id, 3);
            connection->cmd_operands_length = 3;
            switch (pdu_id){
                case AVRCP_PDU_ID_GET_CAPABILITIES:{
                    avrcp_capability_id_t capability_id = (avrcp_capability_id_t) pdu[pos];
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
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:{
                    uint8_t play_identifier[8];
                    memset(play_identifier, 0, 8);
                    if (memcmp(pdu+pos, play_identifier, 8) != 0) {
                        avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                        return;
                    }
                    avrcp_target_emit_respond_vendor_dependent_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_NOW_PLAYING_INFO_QUERY);
                    break;
                }
                default:
                    printf("unhandled pdu id 0x%02x\n", pdu_id);
                    avrcp_target_response_reject(connection, subunit_type, subunit_id, opcode, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                    break;
            }
            break;
        default:
            printf("AVRCP source: opcode 0x%02x not implemented\n", avrcp_cmd_opcode(packet,size));
            break;
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_connection_t * connection;
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avrcp_connection_for_l2cap_signaling_cid(channel, &avrcp_target_context);
            if (!connection) break;
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avrcp_connection_for_l2cap_signaling_cid(channel, &avrcp_target_context);
                    if (!connection) break;
                    switch (connection->state){
                        case AVCTP_W2_SEND_RESPONSE:
                            connection->state = AVCTP_CONNECTION_OPENED;
                            if (connection->now_playing_info_response){
                                printf("now_playing_info_response  \n");
                                connection->now_playing_info_response = 0;
                                avrcp_target_send_now_playing_info(connection->l2cap_signaling_cid, connection);
                                break;
                            } 
                            avrcp_target_send_response(connection->l2cap_signaling_cid, connection);
                            break;
                        default:
                            return;
                    }
                    break;
            default:
                avrcp_packet_handler(packet_type, channel, packet, size, &avrcp_target_context);
                break;
        }
        default:
            break;
    }
}

void avrcp_target_init(void){
    avrcp_target_context.role = AVRCP_TARGET;
    avrcp_target_context.connections = NULL;
    avrcp_target_context.packet_handler = avrcp_controller_packet_handler;
    l2cap_register_service(&avrcp_controller_packet_handler, BLUETOOTH_PROTOCOL_AVCTP, 0xffff, LEVEL_0);
}

void avrcp_target_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_target_context.avrcp_callback = callback;
}

uint8_t avrcp_target_connect(bd_addr_t bd_addr, uint16_t * avrcp_cid){
    return avrcp_connect(bd_addr, &avrcp_target_context, avrcp_cid);
}

uint8_t avrcp_target_disconnect(uint16_t avrcp_cid){
    avrcp_connection_t * connection = get_avrcp_connection_for_avrcp_cid(avrcp_cid, &avrcp_target_context);
    if (!connection){
        log_error("avrcp_get_capabilities: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    l2cap_disconnect(connection->l2cap_signaling_cid, 0);
    return ERROR_CODE_SUCCESS;
}


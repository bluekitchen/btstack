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

#define BTSTACK_FILE__ "avrcp_target.c"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "classic/avrcp.h"
#include "classic/avrcp_target.h"

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "l2cap.h"

#include <stdio.h>
#define AVRCP_ATTR_HEADER_LEN  8

static const uint8_t AVRCP_NOTIFICATION_TRACK_SELECTED[] = {0,0,0,0,0,0,0,0};
static const uint8_t AVRCP_NOTIFICATION_TRACK_NOT_SELECTED[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

avrcp_context_t avrcp_target_context;

static uint32_t default_companies[] = {
    0x581900 //BT SIG registered CompanyID
};

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
static uint16_t avrcp_target_pack_single_element_header(uint8_t * buffer, avrcp_media_attribute_id_t attr_id, uint16_t attr_value_size){
    btstack_assert(attr_id > AVRCP_MEDIA_ATTR_ALL);
    btstack_assert(attr_id < AVRCP_MEDIA_ATTR_RESERVED);
    uint16_t pos = 0;
    big_endian_store_32(buffer, pos, attr_id);
    big_endian_store_16(buffer, pos + 4, RFC2978_CHARSET_MIB_UTF8);
    big_endian_store_16(buffer, pos + 6, attr_value_size);
    return 8;
}

static uint16_t avrcp_now_playing_info_attr_id_value_len(avrcp_connection_t * connection, avrcp_media_attribute_id_t attr_id){
    char buffer[AVRCP_MAX_ATTRIBUTE_SIZE];
    uint16_t str_len;
    switch (attr_id) {
        case AVRCP_MEDIA_ATTR_ALL:
        case AVRCP_MEDIA_ATTR_NONE:
            return 0;
        case AVRCP_MEDIA_ATTR_TRACK:
            str_len = snprintf(buffer, sizeof(buffer), "%" PRIu32, connection->target_track_nr);
            break;
        case AVRCP_MEDIA_ATTR_TOTAL_NUM_ITEMS:
            str_len = snprintf(buffer, sizeof(buffer), "%" PRIu32, connection->target_total_tracks);
            break;
        case AVRCP_MEDIA_ATTR_SONG_LENGTH_MS:
            str_len = snprintf(buffer, sizeof(buffer), "%" PRIu32, connection->target_song_length_ms);
            break;
        default:
            str_len = connection->target_now_playing_info[(uint16_t)attr_id - 1].len;
            break;
    }
    return str_len;
}

static uint16_t avrcp_now_playing_info_value_len_with_headers(avrcp_connection_t * connection){
    uint16_t playing_info_len = 0;
    
    uint8_t i;
    for ( i = (uint8_t)AVRCP_MEDIA_ATTR_ALL + 1; i < (uint8_t) AVRCP_MEDIA_ATTR_RESERVED; i++){
        avrcp_media_attribute_id_t attr_id = (avrcp_media_attribute_id_t) i;

        if ((connection->target_now_playing_info_attr_bitmap & (1 << attr_id)) == 0) {
            continue;
        }

        switch (attr_id) {
            case AVRCP_MEDIA_ATTR_ALL:
            case AVRCP_MEDIA_ATTR_NONE:
            case AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART:
                break;
            default:
                playing_info_len += AVRCP_ATTR_HEADER_LEN + avrcp_now_playing_info_attr_id_value_len(connection, attr_id);
                break;
        }
    }
    // for total num bytes that of the attributes + headers
    playing_info_len += 1;
    return playing_info_len;
}

static uint8_t * avrcp_get_attribute_value_from_u32(avrcp_connection_t * connection, uint32_t value, uint16_t * num_bytes_to_copy){
    *num_bytes_to_copy = 0;

    if (connection->attribute_value_len == 0){
		// "4294967296" = 10 chars + \0
        connection->attribute_value_len = snprintf((char *)connection->attribute_value, 11, "%" PRIu32, value);
        connection->attribute_value_offset = 0;
    }
    *num_bytes_to_copy = connection->attribute_value_len - connection->attribute_value_offset;
    return connection->attribute_value + connection->attribute_value_offset;
}

static uint8_t * avrcp_get_next_value_fragment_for_attribute_id(avrcp_connection_t * connection, avrcp_media_attribute_id_t attr_id, uint16_t * num_bytes_to_copy){
    switch (attr_id){
        case AVRCP_MEDIA_ATTR_TRACK:
            return avrcp_get_attribute_value_from_u32(connection, connection->target_track_nr, num_bytes_to_copy);
        case AVRCP_MEDIA_ATTR_TOTAL_NUM_ITEMS:
            return avrcp_get_attribute_value_from_u32(connection, connection->target_total_tracks, num_bytes_to_copy);
        case AVRCP_MEDIA_ATTR_SONG_LENGTH_MS:
            return avrcp_get_attribute_value_from_u32(connection, connection->target_song_length_ms, num_bytes_to_copy);
        default:
            break;
    }
    int attr_index = attr_id - 1;
    if (connection->attribute_value_len == 0){
        connection->attribute_value_len = avrcp_now_playing_info_attr_id_value_len(connection, attr_id);
        connection->attribute_value_offset = 0;
    }
    *num_bytes_to_copy = connection->target_now_playing_info[attr_index].len - connection->attribute_value_offset;
    return (uint8_t *) (connection->target_now_playing_info[attr_index].value + connection->attribute_value_offset);
}

// TODO Review
static uint16_t avrcp_store_avctp_now_playing_info_fragment(avrcp_connection_t * connection, uint16_t packet_size, uint8_t * packet){
    uint16_t num_free_bytes = packet_size;
    
    uint16_t bytes_stored = 0;

    while ((num_free_bytes > 0) && (connection->next_attr_id <= AVRCP_MEDIA_ATTR_SONG_LENGTH_MS)){
        if ((connection->target_now_playing_info_attr_bitmap & (1 << (uint8_t)connection->next_attr_id)) == 0) {
            connection->next_attr_id = (avrcp_media_attribute_id_t) (((int) connection->next_attr_id) + 1);
            continue;
        }

        // prepare attribute value
        uint16_t num_bytes_to_copy;
        uint8_t * attr_value_with_offset = avrcp_get_next_value_fragment_for_attribute_id(connection,
                                                                                          connection->next_attr_id,
                                                                                          &num_bytes_to_copy);

        // store header
        if (connection->attribute_value_offset == 0){
            // pack the whole attribute value header
            if (connection->parser_attribute_header_pos == 0) {
                avrcp_target_pack_single_element_header(connection->parser_attribute_header, connection->next_attr_id,
                                                        connection->attribute_value_len);
            }
        }

        if (connection->parser_attribute_header_pos < AVRCP_ATTRIBUTE_HEADER_LEN){
            uint16_t num_header_bytes_to_store = btstack_min(num_free_bytes, AVRCP_ATTRIBUTE_HEADER_LEN - connection->parser_attribute_header_pos);
            memcpy(packet + bytes_stored, connection->parser_attribute_header + connection->parser_attribute_header_pos, num_header_bytes_to_store);
            connection->parser_attribute_header_pos += num_header_bytes_to_store;
            bytes_stored += num_header_bytes_to_store;
            num_free_bytes -= num_header_bytes_to_store;
            connection->data_offset += num_header_bytes_to_store;

            if (num_free_bytes == 0){
                continue;
            }
        }

        // store value
        uint16_t num_attr_value_bytes_to_store = btstack_min(num_free_bytes, connection->attribute_value_len - connection->attribute_value_offset);
        memcpy(packet + bytes_stored, attr_value_with_offset, num_attr_value_bytes_to_store);
        bytes_stored   += num_attr_value_bytes_to_store;
        num_free_bytes -= num_attr_value_bytes_to_store;
        connection->attribute_value_offset += num_attr_value_bytes_to_store;
        connection->data_offset += num_attr_value_bytes_to_store;

        if (connection->attribute_value_offset == connection->attribute_value_len){
            // C++ compatible version of connection->next_attr_id++
            connection->next_attr_id = (avrcp_media_attribute_id_t) (((int) connection->next_attr_id) + 1);
            connection->attribute_value_offset = 0;
            connection->attribute_value_len = 0;
            connection->parser_attribute_header_pos = 0;
        }
    }
    return bytes_stored;
}

static void avrcp_send_response_with_avctp_fragmentation(avrcp_connection_t * connection){
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)

    uint16_t max_payload_size;
    connection->avctp_packet_type = avctp_get_packet_type(connection, &max_payload_size);
    connection->avrcp_packet_type = avrcp_get_packet_type(connection);

    // AVCTP header
    // transport header : transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    uint16_t pos = 0;
    packet[pos++] = (connection->transaction_id << 4) | (connection->avctp_packet_type << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;

    uint16_t param_len = connection->data_len;

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

    uint16_t bytes_stored = 0;
    uint8_t i;

    switch (connection->avctp_packet_type) {
        case AVCTP_SINGLE_PACKET:
        case AVCTP_START_PACKET:
            // Profile IDentifier (PID)
            packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
            packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;

            // AVRCP message
            // command_type
            packet[pos++] = connection->command_type;
            // subunit_type | subunit ID
            packet[pos++] = (connection->subunit_type << 3) | connection->subunit_id;
            // opcode
            packet[pos++] = (uint8_t) connection->command_opcode;

            switch (connection->command_opcode) {
                case AVRCP_CMD_OPCODE_VENDOR_DEPENDENT:
                    big_endian_store_24(packet, pos, connection->company_id);
                    pos += 3;
                    packet[pos++] = connection->pdu_id;
                    // AVRCP packet type

                    packet[pos++] = (uint8_t)connection->avrcp_packet_type;
                    // parameter length
                    big_endian_store_16(packet, pos, param_len);
                    pos += 2;

                    switch (connection->pdu_id) {
                        // message is small enough to fit the single packet, no need for extra check
                        case AVRCP_PDU_ID_GET_CAPABILITIES:
                            // capability ID
                            packet[pos++] = connection->data[0];
                            // num_capabilities 
                            packet[pos++] = connection->data[1];

                            switch ((avrcp_capability_id_t) connection->data[0]) {
                                case AVRCP_CAPABILITY_ID_EVENT:
                                    for (i = (uint8_t) AVRCP_NOTIFICATION_EVENT_FIRST_INDEX;
                                         i < (uint8_t) AVRCP_NOTIFICATION_EVENT_LAST_INDEX; i++) {
                                        if ((connection->notifications_supported_by_target & (1 << i)) == 0) {
                                            continue;
                                        }
                                        packet[pos++] = i;
                                    }
                                    break;
                                case AVRCP_CAPABILITY_ID_COMPANY:
                                    // use Bluetooth SIG as default company
                                    for (i = 0; i < connection->data[1]; i++) {
                                        little_endian_store_24(packet, pos,
                                                               connection->target_supported_companies[i]);
                                        pos += 3;
                                    }
                                    break;
                                default:
                                    // error response
                                    break;
                            }
                            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
                            return;

                        case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:
                            packet[pos++] = count_set_bits_uint32(connection->target_now_playing_info_attr_bitmap);
                            max_payload_size--;

                            bytes_stored = avrcp_store_avctp_now_playing_info_fragment(connection, max_payload_size, packet + pos);

                            connection->avrcp_frame_bytes_sent += bytes_stored + pos;
                            l2cap_send_prepared(connection->l2cap_signaling_cid, pos + bytes_stored);
                            return;

                        default:
                            // error response and other OPCODEs
                            break;
                    }
                    break;

                case AVRCP_CMD_OPCODE_PASS_THROUGH:
                    packet[pos++] = connection->operation_id;
                    // parameter length
                    packet[pos++] = (uint8_t) connection->data_len;
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
            switch (connection->pdu_id) {
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:
                    bytes_stored = avrcp_store_avctp_now_playing_info_fragment(connection, max_payload_size, packet + pos);

                    connection->avrcp_frame_bytes_sent += bytes_stored + pos;
                    l2cap_send_prepared(connection->l2cap_signaling_cid, pos + bytes_stored);
                    return;

                default:
                    break;
            }
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
    connection->avrcp_frame_bytes_sent += pos;

    l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
}

static void avctp_send_reject_cmd_wrong_pid(avrcp_connection_t * connection){
    l2cap_reserve_packet_buffer();
    uint8_t * packet = l2cap_get_outgoing_buffer();

    // AVCTP header
    // transport header : transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    packet[0] = (connection->transaction_id << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_RESPONSE_FRAME << 1) | 1;
    big_endian_store_16(packet, 1, connection->message_body[0]);
    l2cap_send_prepared(connection->l2cap_signaling_cid, 3);
}

static void avrcp_target_custom_command_data_init(avrcp_connection_t * connection, 
    avrcp_command_opcode_t opcode, avrcp_command_type_t command_type, 
    avrcp_subunit_type_t subunit_type, avrcp_subunit_id_t subunit_id, 
    avrcp_pdu_id_t pdu_id, uint32_t company_id){

    connection->command_opcode = opcode;
    connection->command_type = command_type;
    connection->subunit_type = subunit_type;
    connection->subunit_id = subunit_id;
    connection->company_id = company_id << 16;
    connection->pdu_id = pdu_id;
    connection->data = NULL;
    connection->data_offset = 0;
    connection->data_len = 0;
    connection->avrcp_frame_bytes_sent = 0;
}

static void avrcp_target_vendor_dependent_response_data_init(avrcp_connection_t * connection, avrcp_command_type_t command_type, avrcp_pdu_id_t pdu_id){
    connection->command_opcode = AVRCP_CMD_OPCODE_VENDOR_DEPENDENT;
    connection->subunit_type   = AVRCP_SUBUNIT_TYPE_PANEL;
    connection->subunit_id     = AVRCP_SUBUNIT_ID;
    connection->company_id     = BT_SIG_COMPANY_ID;
   
    connection->command_type = command_type;
    connection->pdu_id = pdu_id;
    connection->data = connection->message_body;
    connection->data_offset = 0;
    connection->data_len = 0;
    connection->avrcp_frame_bytes_sent = 0;
}

static void avrcp_target_pass_through_command_data_init(avrcp_connection_t * connection, avrcp_command_type_t command_type, avrcp_operation_id_t opid){
    connection->command_opcode = AVRCP_CMD_OPCODE_PASS_THROUGH;
    connection->subunit_type   = AVRCP_SUBUNIT_TYPE_PANEL; 
    connection->subunit_id     = AVRCP_SUBUNIT_ID;
    
    connection->command_type = command_type;
    connection->company_id = 0;
    connection->pdu_id = AVRCP_PDU_ID_UNDEFINED;
    connection->operation_id = opid;

    connection->data = connection->message_body;
    connection->data_offset = 0;
    connection->data_len = 0;
    connection->avrcp_frame_bytes_sent = 0;
}


static uint8_t avrcp_target_vendor_dependent_response_accept(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, uint8_t status){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_ACCEPTED, pdu_id);
    connection->data_len = 1;
    connection->data[0] = status;

    connection->target_accept_response = true;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_reject(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, avrcp_status_code_t status){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_REJECTED, pdu_id);
    connection->data_len = 1;
    connection->data[0] = status;

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_not_implemented(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, uint8_t event_id){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_NOT_IMPLEMENTED, pdu_id);
    connection->data_len = 1;
    connection->data[0] = event_id;
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_interim(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, uint8_t event_id, const uint8_t * value, uint16_t value_len){
    btstack_assert(value_len + 1 < AVRCP_MAX_COMMAND_PARAMETER_LENGTH);
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_INTERIM, pdu_id);
    connection->data_len = 1 + value_len;
    connection->data[0] = event_id;

    if (value && (value_len > 0)){
        (void)memcpy(connection->data + 1, value, value_len);
    }
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_addressed_player_changed_interim(avrcp_connection_t * connection, avrcp_pdu_id_t pdu_id, uint8_t event_id){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_INTERIM, pdu_id);

    connection->data_len = 5;
    connection->data[0] = event_id;
    big_endian_store_16(connection->data, 1, connection->target_addressed_player_id);
    big_endian_store_16(connection->data, 3, connection->target_uid_counter);
    
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_pass_through_response(uint16_t avrcp_cid, avrcp_command_type_t ctype, avrcp_operation_id_t opid, uint8_t operands_length, uint8_t operand){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    avrcp_target_pass_through_command_data_init(connection, ctype, opid);

    if (operands_length == 1){
        connection->data_len = 1;
        connection->message_body[0] = operand;
    }
    
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

uint8_t avrcp_target_set_unit_info(uint16_t avrcp_cid, avrcp_subunit_type_t unit_type, uint32_t company_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->target_unit_type = unit_type;
    connection->company_id = company_id;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_set_subunit_info(uint16_t avrcp_cid, avrcp_subunit_type_t subunit_type, const uint8_t * subunit_info_data, uint16_t subunit_info_data_size){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    connection->target_subunit_info_type = subunit_type;
    connection->target_subunit_info_data = subunit_info_data;
    connection->target_subunit_info_data_size = subunit_info_data_size;
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_unit_info(avrcp_connection_t * connection){
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avrcp_target_custom_command_data_init(connection,
                                          AVRCP_CMD_OPCODE_UNIT_INFO, AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE,
                                          AVRCP_SUBUNIT_TYPE_UNIT, AVRCP_SUBUNIT_ID_IGNORE, AVRCP_PDU_ID_UNDEFINED,
                                          connection->company_id);
    
    uint8_t unit = 0;
    connection->data = connection->message_body;
    connection->data_len = 5;
    connection->data[0] = 0x07;
    connection->data[1] = (connection->target_unit_type << 4) | unit;
    // company id is 3 bytes long
    big_endian_store_24(connection->data, 2, connection->company_id);

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_subunit_info(avrcp_connection_t * connection, uint8_t offset){
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    if (offset >= 32) return AVRCP_STATUS_INVALID_PARAMETER;

    avrcp_target_custom_command_data_init(connection, AVRCP_CMD_OPCODE_SUBUNIT_INFO,
                                          AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE,
                                          AVRCP_SUBUNIT_TYPE_UNIT, AVRCP_SUBUNIT_ID_IGNORE, AVRCP_PDU_ID_UNDEFINED,
                                          connection->company_id);

    uint8_t page = offset / 4;
    uint8_t extension_code = 7;
    connection->data = connection->message_body;
    connection->data_len = 5;
    connection->data[0] = (page << 4) | extension_code;

    // mark non-existent entries with 0xff
    memset(&connection->message_body[1], 0xFF, 4);
    if ((connection->data != NULL) && (offset < connection->target_subunit_info_data_size)){
        uint8_t bytes_to_copy = btstack_min(connection->target_subunit_info_data_size - offset, 4);
        memcpy(&connection->data[1], &connection->target_subunit_info_data[offset], bytes_to_copy);
    }

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_supported_events(avrcp_connection_t * connection){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE, AVRCP_PDU_ID_GET_CAPABILITIES);

    uint8_t event_id;
    uint8_t num_events = 0;
    for (event_id = (uint8_t) AVRCP_NOTIFICATION_EVENT_FIRST_INDEX; event_id < (uint8_t) AVRCP_NOTIFICATION_EVENT_LAST_INDEX; event_id++){
        if ((connection->notifications_supported_by_target & (1 << event_id)) == 0){
            continue;
        }
        num_events++;
    }
    
    connection->data[0] = AVRCP_CAPABILITY_ID_EVENT;
    connection->data[1] = num_events;
    connection->data_len = 2 + num_events;
    
    // fill the data later directly to the L2CAP outgoing buffer
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_response_vendor_dependent_supported_companies(avrcp_connection_t * connection){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE, AVRCP_PDU_ID_GET_CAPABILITIES);

    connection->data[0] = AVRCP_CAPABILITY_ID_COMPANY;
    if (connection->target_supported_companies_num == 0){
        connection->target_supported_companies_num = 1;
        connection->target_supported_companies = default_companies;
    }
    
    connection->data[1] = connection->target_supported_companies_num;
    connection->data_len = 2 + connection->data[1] * 3;

    // fill the data later directly to the L2CAP outgoing buffer and
    // use Bluetooth SIG as default company
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_support_event(uint16_t avrcp_cid, avrcp_notification_event_id_t event_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    if ((event_id < (uint8_t)AVRCP_NOTIFICATION_EVENT_FIRST_INDEX) || (event_id > (uint8_t)AVRCP_NOTIFICATION_EVENT_LAST_INDEX)){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    connection->notifications_supported_by_target |= (1 << (uint8_t)event_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_support_companies(uint16_t avrcp_cid, uint8_t num_companies, const uint32_t *companies){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    connection->target_supported_companies_num = num_companies;
    connection->target_supported_companies     = companies;
    return ERROR_CODE_SUCCESS;
}

// TODO Review (use flags)
uint8_t avrcp_target_play_status(uint16_t avrcp_cid, uint32_t song_length_ms, uint32_t song_position_ms, avrcp_playback_status_t play_status){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE, AVRCP_PDU_ID_GET_PLAY_STATUS);
    connection->data_len = 9;
    big_endian_store_32(connection->data, 0, song_length_ms);
    big_endian_store_32(connection->data, 4, song_position_ms);
    connection->data[8] = play_status;

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

static uint8_t avrcp_target_store_media_attr(avrcp_connection_t * connection, avrcp_media_attribute_id_t attr_id, const char * value){
    int index = attr_id - 1;
    if (!value) return AVRCP_STATUS_INVALID_PARAMETER;
	uint16_t value_len = (uint16_t)strlen(value);
	btstack_assert(value_len <= 255);
	connection->target_now_playing_info[index].value = (uint8_t*)value;
    connection->target_now_playing_info[index].len   = value_len;
    return ERROR_CODE_SUCCESS;
}   

uint8_t avrcp_target_set_playback_status(uint16_t avrcp_cid, avrcp_playback_status_t playback_status){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->target_playback_status == playback_status){
        return ERROR_CODE_SUCCESS;
    } 

    connection->target_playback_status = playback_status;
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED)) {
        connection->target_playback_status_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    } 
    return ERROR_CODE_SUCCESS;
}

static void avrcp_target_register_track_changed(avrcp_connection_t * connection, const uint8_t * track_id){
    if (track_id == NULL){
        memset(connection->target_track_id, 0xFF, 8);
        connection->target_track_selected = false;
    } else {
        (void)memcpy(connection->target_track_id, track_id, 8);
        connection->target_track_selected = true;
    }

    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED)) {
        connection->target_track_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
}

uint8_t avrcp_target_track_changed(uint16_t avrcp_cid, uint8_t * track_id){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_target_register_track_changed(connection, track_id);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_set_now_playing_info(uint16_t avrcp_cid, const avrcp_track_t * current_track, uint16_t total_tracks){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!current_track){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    (void)memcpy(connection->target_track_id, current_track->track_id, 8);
    connection->target_song_length_ms = current_track->song_length_ms;
    connection->target_track_nr = current_track->track_nr;
    connection->target_total_tracks = total_tracks;
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_TITLE, current_track->title);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_ARTIST, current_track->artist);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_ALBUM, current_track->album);
    avrcp_target_store_media_attr(connection, AVRCP_MEDIA_ATTR_GENRE, current_track->genre);

    avrcp_target_register_track_changed(connection, current_track->track_id);
    return ERROR_CODE_SUCCESS;
}


uint8_t avrcp_target_playing_content_changed(uint16_t avrcp_cid){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED)) {
        connection->target_playing_content_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_addressed_player_changed(uint16_t avrcp_cid, uint16_t player_id, uint16_t uid_counter){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    if (connection->target_addressed_player_id == player_id){
        return ERROR_CODE_SUCCESS;
    }

    connection->target_uid_counter = uid_counter;
    connection->target_addressed_player_id = player_id;

    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED)) {
        connection->target_addressed_player_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_battery_status_changed(uint16_t avrcp_cid, avrcp_battery_status_t battery_status){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->target_battery_status == battery_status){
        return ERROR_CODE_SUCCESS;
    } 

    connection->target_battery_status = battery_status;
    
    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED)) {
        connection->target_battery_status_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    } 
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_adjust_absolute_volume(uint16_t avrcp_cid, uint8_t absolute_volume){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }

    connection->target_absolute_volume = absolute_volume;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_target_volume_changed(uint16_t avrcp_cid, uint8_t absolute_volume){
    avrcp_connection_t * connection = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (!connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER; 
    }
    if (connection->target_absolute_volume == absolute_volume){
        return ERROR_CODE_SUCCESS;
    }
    
    connection->target_absolute_volume = absolute_volume;

    if (connection->notifications_enabled & (1 << AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED )) {
        connection->target_notify_absolute_volume_changed = true;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
    } 
    return ERROR_CODE_SUCCESS;
}

static void avrcp_target_set_transaction_label_for_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification, uint8_t transaction_label){
    if (notification > AVRCP_NOTIFICATION_EVENT_MAX_VALUE) return;
    connection->target_notifications_transaction_label[notification] = transaction_label;
}

static uint8_t avrcp_target_get_transaction_label_for_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification){
    if (notification > AVRCP_NOTIFICATION_EVENT_MAX_VALUE) return 0;
    return connection->target_notifications_transaction_label[notification];
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

static void avrcp_handle_l2cap_data_packet_for_signaling_connection(avrcp_connection_t * connection, uint8_t * packet, uint16_t size){
    uint8_t avctp_header = packet[0];
    connection->transaction_id = avctp_header >> 4;

    avctp_packet_type_t avctp_packet_type = (avctp_packet_type_t) ((avctp_header & 0x0F) >> 2);
    switch (avctp_packet_type){
        case AVCTP_SINGLE_PACKET:
            break;

#ifdef ENABLE_AVCTP_FRAGMENTATION
        case AVCTP_START_PACKET:
        case AVCTP_CONTINUE_PACKET:
            avctp_reassemble_message(connection, avctp_packet_type, packet, size);
            return;

        case AVCTP_END_PACKET:
            avctp_reassemble_message(connection, avctp_packet_type, packet, size);

            packet = connection->avctp_reassembly_buffer;
            size   = connection->avctp_reassembly_size;
            break;
#endif
        default:
            return;
    }

    if (size < 6u) return;

    uint16_t pid = big_endian_read_16(packet, 1);

    if (pid != BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL){
        log_info("Invalid pid 0x%02x, expected 0x%02x", pid, BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL);
        connection->target_reject_transport_header = true;
        connection->target_invalid_pid = pid;
        connection->state = AVCTP_W2_SEND_RESPONSE;
        avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
        return;
    }

    // avrcp_subunit_type_t subunit_type = (avrcp_subunit_type_t) (packet[4] >> 3);
    // avrcp_subunit_id_t   subunit_id   = (avrcp_subunit_id_t) (packet[4] & 0x07);
    
    avrcp_command_opcode_t opcode = (avrcp_command_opcode_t) avrcp_cmd_opcode(packet,size);

    int pos = 6;
    uint16_t length;
    avrcp_pdu_id_t   pdu_id;
    // connection->data_len = 0;
    uint8_t offset;
    uint8_t operand;
    uint16_t event_mask;
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
            (void)memcpy(connection->message_body, &packet[pos], 3);
            // connection->data_len = 3;
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
                        avrcp_target_vendor_dependent_response_accept(connection, pdu_id, AVRCP_STATUS_SUCCESS);
                    } else {
                        avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_PLAYER_ID);
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_CAPABILITIES:{
                    avrcp_capability_id_t capability_id = (avrcp_capability_id_t) packet[pos];
                    switch (capability_id){
                        case AVRCP_CAPABILITY_ID_EVENT:
                             avrcp_target_response_vendor_dependent_supported_events(connection);
                            break;
                        case AVRCP_CAPABILITY_ID_COMPANY:
                            avrcp_target_response_vendor_dependent_supported_companies(connection);
                            break;
                        default:
                            avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                            break;
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_PLAY_STATUS:
                    avrcp_target_emit_respond_vendor_dependent_query(avrcp_target_context.avrcp_callback, connection->avrcp_cid, AVRCP_SUBEVENT_PLAY_STATUS_QUERY);
                    break;
                case AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE:
                    if ((pos + 1) > size) return;
                    connection->target_abort_continue_response = true;
                    connection->state = AVCTP_W2_SEND_RESPONSE;
                    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    break;
                case AVRCP_PDU_ID_REQUEST_CONTINUING_RESPONSE:
                    if ((pos + 1) > size) return;
                    if (packet[pos] != AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES){
                        avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                        return;
                    }
                    connection->target_continue_response = true;
                    connection->target_now_playing_info_response = true;
                    connection->state = AVCTP_W2_SEND_RESPONSE;
                    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    break;
                case AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES:{
                    if ((pos + 9) > size) return;
                    uint8_t play_identifier[8];
                    memset(play_identifier, 0, 8);
                    if (memcmp(&packet[pos], play_identifier, 8) != 0) {
                        avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                        return;
                    }
                    pos += 8;
                    uint8_t attribute_count = packet[pos++];
                    connection->next_attr_id = AVRCP_MEDIA_ATTR_NONE;
                    if (!attribute_count){
                        connection->next_attr_id = AVRCP_MEDIA_ATTR_TITLE;
                        connection->target_now_playing_info_attr_bitmap = 0xFE;
                    } else {
                        int i;
                        connection->next_attr_id = AVRCP_MEDIA_ATTR_TITLE;
                        connection->target_now_playing_info_attr_bitmap = 0;
                        if ((pos + attribute_count * 4) > size) return;
                        for (i=0; i < attribute_count; i++){
                            uint32_t attr_id = big_endian_read_32(packet, pos);
                            connection->target_now_playing_info_attr_bitmap |= (1 << attr_id);
                            pos += 4;
                        }
                    }
                    log_info("target_now_playing_info_attr_bitmap 0x%02x", connection->target_now_playing_info_attr_bitmap);
                    connection->target_now_playing_info_response = true;
                    connection->state = AVCTP_W2_SEND_RESPONSE;
                    avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
                    break;
                }
                case AVRCP_PDU_ID_REGISTER_NOTIFICATION:{
                    if ((pos + 1) > size) return;
                    avrcp_notification_event_id_t event_id = (avrcp_notification_event_id_t) packet[pos];

                    avrcp_target_set_transaction_label_for_notification(connection, event_id, connection->transaction_id);

                    if (event_id < AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED ||
                        event_id > AVRCP_NOTIFICATION_EVENT_MAX_VALUE){
                        avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                        return;
                    }

                    switch (event_id){
                        case AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END:
                        case AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START:
                        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED:
                        case AVRCP_NOTIFICATION_EVENT_MAX_VALUE:
                            avrcp_target_response_vendor_dependent_not_implemented(connection, pdu_id, event_id);
                            return;
                        default:
                            break;
                    }

                    event_mask = (1 << event_id);
                    connection->notifications_enabled |= event_mask;
                            
                    switch (event_id){
                        case AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED:
                            if (connection->target_track_selected){
                                avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, AVRCP_NOTIFICATION_TRACK_SELECTED, 8);
                            } else {
                                avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, AVRCP_NOTIFICATION_TRACK_NOT_SELECTED, 8);
                            }
                            break;
                        case AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED:
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->target_playback_status, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED: 
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, NULL, 0);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED:
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->target_absolute_volume, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED:
                            avrcp_target_response_vendor_dependent_interim(connection, pdu_id, event_id, (const uint8_t *)&connection->target_battery_status, 1);
                            break;
                        case AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED:
                            avrcp_target_response_addressed_player_changed_interim(connection, pdu_id, event_id);
                            return;
                        default:
                            btstack_assert(false);
                            return;
                    }
                    break;
                }
                case AVRCP_PDU_ID_SET_ABSOLUTE_VOLUME: {
                    if ( (length != 1) || ((pos + 1) > size)){
                        avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                        break;
                    }

                    uint8_t absolute_volume = packet[pos];
                    if (absolute_volume < 0x80){
                        connection->target_absolute_volume = absolute_volume;
                    }
                    avrcp_target_emit_volume_changed(avrcp_target_context.avrcp_callback, connection->avrcp_cid, connection->target_absolute_volume);
                    avrcp_target_vendor_dependent_response_accept(connection, pdu_id, connection->target_absolute_volume);
                    break;
                }
                default:
                    log_info("AVRCP target: unhandled pdu id 0x%02x", pdu_id);
                    avrcp_target_response_vendor_dependent_reject(connection, pdu_id, AVRCP_STATUS_INVALID_COMMAND);
                    break;
            }
            break;
        default:
            log_info("AVRCP target: opcode 0x%02x not implemented", avrcp_cmd_opcode(packet,size));
            break;
    }
}

static void avrcp_target_notification_init(avrcp_connection_t * connection, avrcp_notification_event_id_t notification_id, uint8_t * value, uint16_t value_len){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_CHANGED_STABLE, AVRCP_PDU_ID_REGISTER_NOTIFICATION);
    connection->transaction_id = avrcp_target_get_transaction_label_for_notification(connection, notification_id);

    connection->data_len = 1 + value_len;
    connection->data[0] = notification_id;
    if (value != NULL){
        (void)memcpy(connection->data + 1, value, value_len);
    }
}

static void avrcp_target_notification_addressed_player_changed_init(avrcp_connection_t * connection){
    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_CHANGED_STABLE, AVRCP_PDU_ID_REGISTER_NOTIFICATION);
    connection->transaction_id = avrcp_target_get_transaction_label_for_notification(connection, AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED);

    connection->data_len = 5;
    connection->data[0] = AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED;
    big_endian_store_16(connection->data, 1, connection->target_addressed_player_id);
    big_endian_store_16(connection->data, 3, connection->target_uid_counter);
}


static void avrcp_target_reset_notification(avrcp_connection_t * connection, avrcp_notification_event_id_t notification_id){
    if (notification_id < AVRCP_NOTIFICATION_EVENT_FIRST_INDEX || notification_id > AVRCP_NOTIFICATION_EVENT_LAST_INDEX){
        return;
    }
    connection->notifications_enabled &= ~(1 << notification_id);
    connection->target_notifications_transaction_label[notification_id] = 0;
}

static void avrcp_request_next_avctp_segment(avrcp_connection_t * connection){
    // AVCTP
    switch (connection->avctp_packet_type){
        case AVCTP_END_PACKET:
        case AVCTP_SINGLE_PACKET:
            connection->state = AVCTP_CONNECTION_OPENED;
            break;
        default:
            connection->state = AVCTP_W2_SEND_RESPONSE;
            avrcp_request_can_send_now(connection, connection->l2cap_signaling_cid);
            break;
    }
}

static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_connection_t * connection;
    avrcp_notification_event_id_t notification_id = AVRCP_NOTIFICATION_EVENT_NONE;

    switch (packet_type){
        case L2CAP_DATA_PACKET:
            connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, channel);
            avrcp_handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
            return;

        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) != L2CAP_EVENT_CAN_SEND_NOW){
                return;
            }
    
            connection = avrcp_get_connection_for_l2cap_signaling_cid_for_role(AVRCP_TARGET, channel);
            if (connection == NULL){
                return;
            }

            if (connection->state == AVCTP_W2_SEND_RESPONSE){
                // start AVCTP
                if (connection->target_reject_transport_header){
                    connection->target_reject_transport_header = false;
                    avctp_send_reject_cmd_wrong_pid(connection);
                    connection->state = AVCTP_CONNECTION_OPENED;
                    return;
                }
                // end AVCTP

                // start AVRCP
                if (connection->target_abort_continue_response){
                    connection->target_abort_continue_response = false;
                    avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_ACCEPTED, AVRCP_PDU_ID_REQUEST_ABORT_CONTINUING_RESPONSE);
                    break;
                }

                if (connection->target_now_playing_info_response){
                    connection->target_now_playing_info_response = false;
                    if (connection->target_continue_response){
                        connection->target_continue_response = false;
                        if (connection->data_len == 0){
                            avrcp_target_response_vendor_dependent_reject(connection, connection->pdu_id, AVRCP_STATUS_INVALID_PARAMETER);
                            return;
                        }
                    } else {
                        avrcp_target_vendor_dependent_response_data_init(connection, AVRCP_CTYPE_RESPONSE_IMPLEMENTED_STABLE, AVRCP_PDU_ID_GET_ELEMENT_ATTRIBUTES);
                        connection->data_len = avrcp_now_playing_info_value_len_with_headers(connection);
                    }
                    break;
                }

                // data already prepared
                break;
            }

            // Notifications

            if (connection->target_track_changed){
                connection->target_track_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED;
                avrcp_target_notification_init(connection, notification_id, connection->target_track_id, 8);
                break;
            }

            if (connection->target_playback_status_changed){
                connection->target_playback_status_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED;
                uint8_t playback_status = (uint8_t) connection->target_playback_status;
                avrcp_target_notification_init(connection, notification_id, &playback_status, 1);
                break;
            }
            
            if (connection->target_playing_content_changed){
                connection->target_playing_content_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED;
                avrcp_target_notification_init(connection, notification_id, NULL, 0);
                break;
            }
            
            if (connection->target_battery_status_changed){
                connection->target_battery_status_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED;
                avrcp_target_notification_init(connection, notification_id, (uint8_t *)&connection->target_battery_status, 1);
                break;
            }
            
            if (connection->target_notify_absolute_volume_changed){
                connection->target_notify_absolute_volume_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED;
                avrcp_target_notification_init(connection, notification_id, &connection->target_absolute_volume, 1);
                break;
            }

            if (connection->target_addressed_player_changed){
                connection->target_addressed_player_changed = false;
                notification_id = AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED;
                avrcp_target_notification_addressed_player_changed_init(connection);
                break;
            }

            // nothing to send, exit
            return;

        default:
            return;
    }   

    avrcp_send_response_with_avctp_fragmentation(connection);
    avrcp_target_reset_notification(connection, notification_id);
    avrcp_request_next_avctp_segment(connection);
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



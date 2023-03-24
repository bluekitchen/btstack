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

#define BTSTACK_FILE__ "avdtp_util.c"

#include <stdint.h>
#include <string.h>

#include "classic/avdtp.h"
#include "classic/avdtp_util.h"

#include "btstack_debug.h"
#include "btstack_util.h"
#include "l2cap.h"

/*

 List of AVDTP_SUBEVENTs sorted by packet handler


Sink + Source:
- AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED
- AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED
- AVDTP_SUBEVENT_SIGNALING_SEP_FOUND
- AVDTP_SUBEVENT_SIGNALING_ACCEPT
- AVDTP_SUBEVENT_SIGNALING_REJECT
- AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT
- AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY
- AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE
- AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE

Source:
 - AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT

Sink or Source based on SEP Type:
- AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED
- AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED
- AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW
- AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION
- AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION

*/

static const char * avdtp_si_name[] = {
    "ERROR",
    "AVDTP_SI_DISCOVER",
    "AVDTP_SI_GET_CAPABILITIES",
    "AVDTP_SI_SET_CONFIGURATION",
    "AVDTP_SI_GET_CONFIGURATION",
    "AVDTP_SI_RECONFIGURE", 
    "AVDTP_SI_OPEN", 
    "AVDTP_SI_START", 
    "AVDTP_SI_CLOSE",
    "AVDTP_SI_SUSPEND",
    "AVDTP_SI_ABORT", 
    "AVDTP_SI_SECURITY_CONTROL",
    "AVDTP_SI_GET_ALL_CAPABILITIES", 
    "AVDTP_SI_DELAY_REPORT" 
};
const char * avdtp_si2str(uint16_t index){
    if ((index <= 0) || (index >= sizeof(avdtp_si_name)/sizeof(avdtp_si_name[0]) )) return avdtp_si_name[0];
    return avdtp_si_name[index];
}

void avdtp_reset_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint){
    stream_endpoint->media_con_handle = 0;
    stream_endpoint->l2cap_media_cid = 0;
    stream_endpoint->l2cap_reporting_cid = 0;
    stream_endpoint->l2cap_recovery_cid = 0;
    
    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;

    stream_endpoint->connection = NULL;

    stream_endpoint->sep.in_use = 0;
    memset(&stream_endpoint->remote_sep, 0, sizeof(avdtp_sep_t));

    stream_endpoint->remote_capabilities_bitmap = 0;
    memset(&stream_endpoint->remote_capabilities, 0, sizeof(avdtp_capabilities_t));
    stream_endpoint->remote_configuration_bitmap = 0;
    memset(&stream_endpoint->remote_configuration, 0, sizeof(avdtp_capabilities_t));
    
    // temporary SBC config used by A2DP Source
    memset(stream_endpoint->media_codec_info, 0, 8);

    stream_endpoint->media_disconnect = 0;
    stream_endpoint->media_connect = 0;
    stream_endpoint->start_stream = 0;
    stream_endpoint->close_stream = 0;
    stream_endpoint->request_can_send_now = false;
    stream_endpoint->abort_stream = 0;
    stream_endpoint->suspend_stream = 0;
    stream_endpoint->sequence_number = 0;
}

int get_bit16(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}

uint16_t store_bit16(uint16_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

avdtp_message_type_t avdtp_get_signaling_packet_type(uint8_t * packet){
    return (avdtp_message_type_t) (packet[0] & 0x03);
}

int avdtp_read_signaling_header(avdtp_signaling_packet_t * signaling_header, uint8_t * packet, uint16_t size){
    int pos = 0;
    if (size < 2) return pos;   
    signaling_header->transaction_label = packet[pos] >> 4;
    signaling_header->packet_type = (avdtp_packet_type_t)((packet[pos] >> 2) & 0x03);
    signaling_header->message_type = (avdtp_message_type_t) (packet[pos] & 0x03);
    pos++;
    memset(signaling_header->command, 0, sizeof(signaling_header->command));
    switch (signaling_header->packet_type){
        case AVDTP_SINGLE_PACKET:
            signaling_header->num_packets = 0;
            signaling_header->offset = 0;
            signaling_header->size = 0;
            break;
        case AVDTP_END_PACKET:
            signaling_header->num_packets = 0;
            break;
        case AVDTP_START_PACKET:
            signaling_header->num_packets = packet[pos++];
            signaling_header->size = 0;
            signaling_header->offset = 0;
            break;
        case AVDTP_CONTINUE_PACKET:
            if (signaling_header->num_packets <= 0) {
                log_info("    ERROR: wrong num fragmented packets\n");
                break;
            }
            signaling_header->num_packets--;
            break;
        default:
            btstack_assert(false);
            break;
    }
    signaling_header->signal_identifier = (avdtp_signal_identifier_t)(packet[pos++] & 0x3f);
    return pos;
}

static bool avdtp_is_basic_capability(int service_category){
    return (AVDTP_MEDIA_TRANSPORT <= service_category) && (service_category <= AVDTP_MEDIA_CODEC);
}

int avdtp_pack_service_capabilities(uint8_t *buffer, int size, avdtp_capabilities_t caps, avdtp_service_category_t category) {
    UNUSED(size);

    int i;
    // pos = 0 reserved for length
    int pos = 1;
    switch(category){
        case AVDTP_MEDIA_TRANSPORT:
        case AVDTP_REPORTING:
        case AVDTP_DELAY_REPORTING:
            break;
        case AVDTP_RECOVERY:
            buffer[pos++] = caps.recovery.recovery_type; // 0x01=RFC2733
            buffer[pos++] = caps.recovery.maximum_recovery_window_size;
            buffer[pos++] = caps.recovery.maximum_number_media_packets;
            break;
        case AVDTP_CONTENT_PROTECTION:
            buffer[pos++] = caps.content_protection.cp_type_value_len + 2;
            big_endian_store_16(buffer, pos, caps.content_protection.cp_type);
            pos += 2;
            (void)memcpy(buffer + pos, caps.content_protection.cp_type_value,
                         caps.content_protection.cp_type_value_len);
            pos += caps.content_protection.cp_type_value_len;
            break;
        case AVDTP_HEADER_COMPRESSION:
            buffer[pos++] = (caps.header_compression.back_ch << 7) | (caps.header_compression.media << 6) | (caps.header_compression.recovery << 5);
            break;
        case AVDTP_MULTIPLEXING:
            buffer[pos++] = caps.multiplexing_mode.fragmentation << 7;
            for (i=0; i<caps.multiplexing_mode.transport_identifiers_num; i++){
                buffer[pos++] = caps.multiplexing_mode.transport_session_identifiers[i] << 7;
                buffer[pos++] = caps.multiplexing_mode.tcid[i] << 7;
                // media, reporting. recovery
            }
            break;
        case AVDTP_MEDIA_CODEC:
            buffer[pos++] = ((uint8_t)caps.media_codec.media_type) << 4;
            buffer[pos++] = (uint8_t)caps.media_codec.media_codec_type;
            for (i = 0; i<caps.media_codec.media_codec_information_len; i++){
                buffer[pos++] = caps.media_codec.media_codec_information[i];
            }
            break;
        default:
            break;
    }
    buffer[0] = pos - 1; // length
    return pos;
}

static int avdtp_unpack_service_capabilities_has_errors(avdtp_connection_t * connection, avdtp_signal_identifier_t signal_identifier, avdtp_service_category_t category, uint8_t cap_len){
    connection->error_code = 0;
    
    if ((category == AVDTP_SERVICE_CATEGORY_INVALID_0) || 
        ((category == AVDTP_SERVICE_CATEGORY_INVALID_FF) && (signal_identifier == AVDTP_SI_RECONFIGURE))){
        log_info("    ERROR: BAD SERVICE CATEGORY %d\n", category);
        connection->reject_service_category = category;
        connection->error_code = AVDTP_ERROR_CODE_BAD_SERV_CATEGORY;
        return 1;
    }

    if (signal_identifier == AVDTP_SI_RECONFIGURE){
        if ( (category != AVDTP_CONTENT_PROTECTION) && (category != AVDTP_MEDIA_CODEC)){
            log_info("    ERROR: REJECT CATEGORY, INVALID_CAPABILITIES\n");
            connection->reject_service_category = category;
            connection->error_code = AVDTP_ERROR_CODE_INVALID_CAPABILITIES;
            return 1;
        }
    }

    switch(category){
        case AVDTP_MEDIA_TRANSPORT:   
            if (cap_len != 0){
                log_info("    ERROR: REJECT CATEGORY, BAD_MEDIA_TRANSPORT\n");
                connection->reject_service_category = category;
                connection->error_code = AVDTP_ERROR_CODE_BAD_MEDIA_TRANSPORT_FORMAT;
                return 1;
            }
            break;
        case AVDTP_REPORTING:                
        case AVDTP_DELAY_REPORTING:                
            if (cap_len != 0){
                log_info("    ERROR: REJECT CATEGORY, BAD_LENGTH\n");
                connection->reject_service_category = category;
                connection->error_code = AVDTP_ERROR_CODE_BAD_LENGTH;
                return 1;
            }
            break;
        case AVDTP_RECOVERY:     
            if (cap_len != 3){
                log_info("    ERROR: REJECT CATEGORY, BAD_MEDIA_TRANSPORT\n");
                connection->reject_service_category = category;
                connection->error_code = AVDTP_ERROR_CODE_BAD_RECOVERY_FORMAT;
                return 1;
            }           
            break;
        case AVDTP_CONTENT_PROTECTION:
            if (cap_len < 2){
                log_info("    ERROR: REJECT CATEGORY, BAD_CP_FORMAT\n");
                connection->reject_service_category = category;
                connection->error_code = AVDTP_ERROR_CODE_BAD_CP_FORMAT;
                return 1;
            }
            break;
        case AVDTP_HEADER_COMPRESSION:
            // TODO: find error code for bad header compression
            if (cap_len != 1){
                log_info("    ERROR: REJECT CATEGORY, BAD_HEADER_COMPRESSION\n");
                connection->reject_service_category = category;
                connection->error_code = AVDTP_ERROR_CODE_BAD_RECOVERY_FORMAT;
                return 1;
            }           
            break;
        case AVDTP_MULTIPLEXING:                
            break;
        case AVDTP_MEDIA_CODEC:                
            break;
        default:
            break;
    }
    return 0;
}

uint16_t avdtp_unpack_service_capabilities(avdtp_connection_t * connection, avdtp_signal_identifier_t signal_identifier, avdtp_capabilities_t * caps, uint8_t * packet, uint16_t size){
    
    int i;

    uint16_t registered_service_categories = 0;
    uint16_t to_process = size;

    while (to_process >= 2){

        avdtp_service_category_t category = (avdtp_service_category_t) packet[0];
        uint8_t cap_len = packet[1];
        packet     += 2;
        to_process -= 2;

        if (cap_len > to_process){
            connection->reject_service_category = category;
            connection->error_code = AVDTP_ERROR_CODE_BAD_LENGTH;
            return 0;
        }

        if (avdtp_unpack_service_capabilities_has_errors(connection, signal_identifier, category, cap_len)) return 0;

        int category_valid = 1;

        uint8_t * data = packet;
        uint16_t  pos = 0;

        switch(category){
            case AVDTP_RECOVERY:                
                caps->recovery.recovery_type = data[pos++];
                caps->recovery.maximum_recovery_window_size = data[pos++];
                caps->recovery.maximum_number_media_packets = data[pos++];
                break;
            case AVDTP_CONTENT_PROTECTION:
                caps->content_protection.cp_type = big_endian_read_16(data, 0);
                caps->content_protection.cp_type_value_len = cap_len - 2;
                // connection->reject_service_category = category;
                // connection->error_code = UNSUPPORTED_CONFIGURATION;
                // support for content protection goes here
                break;
            case AVDTP_HEADER_COMPRESSION:
                caps->header_compression.back_ch  = (data[0] >> 7) & 1; 
                caps->header_compression.media    = (data[0] >> 6) & 1;
                caps->header_compression.recovery = (data[0] >> 5) & 1;
                break;
            case AVDTP_MULTIPLEXING:                
                caps->multiplexing_mode.fragmentation = (data[pos++] >> 7) & 1;
                // read [tsid, tcid] for media, reporting. recovery respectively
                caps->multiplexing_mode.transport_identifiers_num = 3;
                for (i=0; i<caps->multiplexing_mode.transport_identifiers_num; i++){
                    caps->multiplexing_mode.transport_session_identifiers[i] = (data[pos++] >> 7) & 1;
                    caps->multiplexing_mode.tcid[i] = (data[pos++] >> 7) & 1;
                }
                break;
            case AVDTP_MEDIA_CODEC:   
                caps->media_codec.media_type = (avdtp_media_type_t)(data[pos++] >> 4);
                caps->media_codec.media_codec_type = (avdtp_media_codec_type_t)(data[pos++]);
                caps->media_codec.media_codec_information_len = cap_len - 2;
                caps->media_codec.media_codec_information = &data[pos++];
                break;
            case AVDTP_MEDIA_TRANSPORT:   
            case AVDTP_REPORTING:                
            case AVDTP_DELAY_REPORTING:             
                break;
            default:
                category_valid = 0;
                break;
        }

        if (category_valid) {
            registered_service_categories = store_bit16(registered_service_categories, category, 1);
        }

        packet     += cap_len;
        to_process -= cap_len;
    }

    return registered_service_categories;
}

void avdtp_prepare_capabilities(avdtp_signaling_packet_t * signaling_packet, uint8_t transaction_label, uint16_t service_categories, avdtp_capabilities_t capabilities, uint8_t identifier){
    if (signaling_packet->offset) return;
    bool basic_capabilities_only = false;
    signaling_packet->message_type = AVDTP_RESPONSE_ACCEPT_MSG;
    int i;
    
    signaling_packet->size = 0;
    memset(signaling_packet->command, 0 , sizeof(signaling_packet->command));

    switch (identifier) {
        case AVDTP_SI_GET_CAPABILITIES:
            basic_capabilities_only = true;
            break;
        case AVDTP_SI_GET_ALL_CAPABILITIES:
            break;
        case AVDTP_SI_SET_CONFIGURATION:
            signaling_packet->command[signaling_packet->size++] = signaling_packet->acp_seid << 2;
            signaling_packet->command[signaling_packet->size++] = signaling_packet->int_seid << 2;
            signaling_packet->message_type = AVDTP_CMD_MSG;
            break;
        case AVDTP_SI_RECONFIGURE:
            signaling_packet->command[signaling_packet->size++] = signaling_packet->acp_seid << 2;
            signaling_packet->message_type = AVDTP_CMD_MSG;
            break;
        default: 
            log_error("avdtp_prepare_capabilities wrong identifier %d", identifier);
            break;
    } 
    
    for (i = AVDTP_MEDIA_TRANSPORT; i <= AVDTP_DELAY_REPORTING; i++){
        int registered_category = get_bit16(service_categories, i);
        if (!registered_category && (identifier == AVDTP_SI_SET_CONFIGURATION)){
            // TODO: introduce bitmap of mandatory categories
            if (i == AVDTP_MEDIA_TRANSPORT){
                registered_category = true;
            }
        }
        // AVDTP_SI_GET_CAPABILITIES reports only basic capabilities (i.e., it skips non-basic categories)
        if (basic_capabilities_only && !avdtp_is_basic_capability(i)){
            registered_category = false;
        }

        if (registered_category){
            // service category
            signaling_packet->command[signaling_packet->size++] = i;
            signaling_packet->size += avdtp_pack_service_capabilities(signaling_packet->command + signaling_packet->size,
                    sizeof(signaling_packet->command) - signaling_packet->size, capabilities, (avdtp_service_category_t) i);
        }
    }
    signaling_packet->signal_identifier = (avdtp_signal_identifier_t)identifier;
    signaling_packet->transaction_label = transaction_label;
}

int avdtp_signaling_create_fragment(uint16_t cid, avdtp_signaling_packet_t * signaling_packet, uint8_t * out_buffer) {
    int mtu = l2cap_get_remote_mtu_for_local_cid(cid);
    int data_len = 0;

    uint16_t offset = signaling_packet->offset;
    uint16_t pos = 1;
    
    if (offset == 0){
        if (signaling_packet->size <= (mtu - 2)){
            signaling_packet->packet_type = AVDTP_SINGLE_PACKET;
            out_buffer[pos++] = signaling_packet->signal_identifier;
            data_len = signaling_packet->size;
        } else {
            signaling_packet->packet_type = AVDTP_START_PACKET;
            out_buffer[pos++] = (mtu + signaling_packet->size)/ (mtu-1);
            out_buffer[pos++] = signaling_packet->signal_identifier;
            data_len = mtu - 3;
            signaling_packet->offset = data_len;
        }
    } else {
        int remaining_bytes = signaling_packet->size - offset;
        if (remaining_bytes <= (mtu - 1)){
            signaling_packet->packet_type = AVDTP_END_PACKET;
            data_len = remaining_bytes;
            signaling_packet->offset = 0;
        } else{
            signaling_packet->packet_type = AVDTP_CONTINUE_PACKET;
            data_len = mtu - 1;
            signaling_packet->offset += data_len;
        }
    }
    out_buffer[0] = avdtp_header(signaling_packet->transaction_label, signaling_packet->packet_type, signaling_packet->message_type);
    (void)memcpy(out_buffer + pos, signaling_packet->command + offset,
                 data_len);
    pos += data_len; 
    return pos;
}


void avdtp_signaling_emit_connection_established(uint16_t avdtp_cid, bd_addr_t addr, hci_con_handle_t con_handle, uint8_t status) {
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = status;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_connection_released(uint16_t avdtp_cid) {
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_sep(uint16_t avdtp_cid, avdtp_sep_t sep) {
    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_SEP_FOUND;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = sep.seid;
    event[pos++] = sep.in_use;
    event[pos++] = sep.media_type;
    event[pos++] = sep.type;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_sep_done(uint16_t avdtp_cid) {
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_accept(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier, bool is_initiator) {
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_ACCEPT;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = is_initiator ? 1 : 0;
    event[pos++] = identifier;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_accept_for_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint, uint8_t local_seid,  avdtp_signal_identifier_t identifier, bool is_initiator){
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_ACCEPT;
    little_endian_store_16(event, pos, stream_endpoint->connection->avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = is_initiator ? 1 : 0;
    event[pos++] = identifier;

    btstack_packet_handler_t packet_handler = avdtp_packet_handler_for_stream_endpoint(stream_endpoint);
    (*packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

void avdtp_signaling_emit_reject(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier, bool is_initiator) {
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_REJECT;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = is_initiator ? 1 : 0;
    event[pos++] = identifier;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_general_reject(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier, bool is_initiator) {
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = is_initiator ? 1 : 0;
    event[pos++] = identifier;
    avdtp_emit_sink_and_source(event, pos);
}

static inline void
avdtp_signaling_emit_capability(uint8_t capability_subevent_id, uint16_t avdtp_cid, uint8_t remote_seid) {
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = capability_subevent_id;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_sbc_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec) {
    const uint8_t * media_codec_information = media_codec.media_codec_information;
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;
    event[pos++] = media_codec_information[0] >> 4;
    event[pos++] = media_codec_information[0] & 0x0F;
    event[pos++] = media_codec_information[1] >> 4;
    event[pos++] = (media_codec_information[1] & 0x0F) >> 2;
    event[pos++] = media_codec_information[1] & 0x03;
    event[pos++] = media_codec_information[2];
    event[pos++] = media_codec_information[3];
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_mpeg_audio_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec) {
    const uint8_t * media_codec_information = media_codec.media_codec_information;
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;

    uint8_t layer_bitmap              =   media_codec_information[0] >> 5;
    uint8_t crc                       =  (media_codec_information[0] >> 4) & 0x01;
    uint8_t channel_mode_bitmap       =   media_codec_information[0] & 0x07;
    uint8_t mpf                       =  (media_codec_information[1] >> 6) & 0x01;
    uint8_t sampling_frequency_bitmap =   media_codec_information[1] & 0x3F;
    uint8_t vbr                       =  (media_codec_information[2] >> 7) & 0x01;
    uint16_t bit_rate_index_bitmap    = ((media_codec_information[3] & 0x3f) << 8) | media_codec.media_codec_information[4];

    event[pos++] = layer_bitmap;
    event[pos++] = crc;
    event[pos++] = channel_mode_bitmap;
    event[pos++] = mpf;
    event[pos++] = sampling_frequency_bitmap;
    event[pos++] = vbr;
    little_endian_store_16(event, pos, bit_rate_index_bitmap);           // bit rate index
    pos += 2;
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_mpeg_aac_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec) {
    const uint8_t * media_codec_information = media_codec.media_codec_information;
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;

    uint8_t  object_type_bitmap        =   media_codec_information[0];
    uint16_t sampling_frequency_bitmap =  (media_codec_information[1] << 4) | (media_codec_information[2] >> 4);
    uint8_t  channels_bitmap           =  (media_codec_information[2] >> 2) & 0x03;
    uint32_t bit_rate_bitmap           = ((media_codec_information[3] & 0x7f) << 16) | (media_codec_information[4] << 8) | media_codec_information[5];
    uint8_t  vbr                       =   media_codec_information[3] >> 7;

    event[pos++] =  object_type_bitmap;
    little_endian_store_16(event, pos, sampling_frequency_bitmap);
    pos += 2;
    event[pos++] = channels_bitmap;
    little_endian_store_24(event, pos, bit_rate_bitmap);
    pos += 3;
    event[pos++] = vbr;
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_atrac_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec) {
    const uint8_t * media_codec_information = media_codec.media_codec_information;
    uint8_t event[16];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    pos++; // set later
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;

    uint8_t  version                   =  media_codec_information[0] >> 5;
    uint8_t  channel_mode_bitmap       = (media_codec_information[0] >> 2) & 0x07;
    uint8_t sampling_frequency_bitmap = (media_codec_information[1] >> 4) & 0x03;
    uint8_t  vbr                       = (media_codec_information[1] >> 3) & 0x01;
    uint16_t bit_rate_index_bitmap     = ((media_codec_information[1]) & 0x07) << 16 | (media_codec_information[2] << 8) | media_codec_information[3];
    uint16_t maximum_sul               = (media_codec_information[4] << 8) | media_codec_information[5];

    event[pos++] = version;
    event[pos++] = channel_mode_bitmap;
    event[pos++] = sampling_frequency_bitmap;
    event[pos++] = vbr;
    little_endian_store_24(event, pos, bit_rate_index_bitmap);
    pos += 3;
    little_endian_store_16(event, pos, maximum_sul);
    pos += 2;
    event[1] = pos - 2;
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_other_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec) {
    uint8_t event[AVDTP_MAX_MEDIA_CODEC_INFORMATION_LENGTH + 11];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    pos++; // set later
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;
    little_endian_store_16(event, pos, media_codec.media_codec_type);
    pos += 2;
    little_endian_store_16(event, pos, media_codec.media_codec_information_len);
    pos += 2;
    uint32_t media_codec_info_len = btstack_min(media_codec.media_codec_information_len, AVDTP_MAX_MEDIA_CODEC_INFORMATION_LENGTH);
    (void)memcpy(event + pos, media_codec.media_codec_information, media_codec_info_len);
    pos += media_codec_info_len;
    event[1] = pos - 2;
    avdtp_emit_sink_and_source(event, pos);
}

static void
avdtp_signaling_emit_media_transport_capability(uint16_t avdtp_cid, uint8_t remote_seid) {
	avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY, avdtp_cid,
									remote_seid);
}

static void avdtp_signaling_emit_reporting_capability(uint16_t avdtp_cid, uint8_t remote_seid) {
	avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY, avdtp_cid, remote_seid);
}

static void
avdtp_signaling_emit_delay_reporting_capability(uint16_t avdtp_cid, uint8_t remote_seid) {
	avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY, avdtp_cid,
									remote_seid);
}

static void avdtp_signaling_emit_recovery_capability(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_recovery_capabilities_t *recovery) {
    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = recovery->recovery_type;
    event[pos++] = recovery->maximum_recovery_window_size;
    event[pos++] = recovery->maximum_number_media_packets;
    avdtp_emit_sink_and_source(event, pos);
}

#define MAX_CONTENT_PROTECTION_VALUE_LEN 32
static void
avdtp_signaling_emit_content_protection_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_content_protection_t *content_protection) {
    uint8_t event[10 + MAX_CONTENT_PROTECTION_VALUE_LEN];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    pos++; // set later
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;

    little_endian_store_16(event, pos, content_protection->cp_type);
    pos += 2;

    // drop cp protection value if longer than expected
    if (content_protection->cp_type_value_len <= MAX_CONTENT_PROTECTION_VALUE_LEN){
        little_endian_store_16(event, pos, content_protection->cp_type_value_len);
        pos += 2;
        (void)memcpy(event + pos, content_protection->cp_type_value, content_protection->cp_type_value_len);
        pos += content_protection->cp_type_value_len;
    } else {
        little_endian_store_16(event, pos, 0);
        pos += 2;
    }
    event[1] = pos - 2;
    avdtp_emit_sink_and_source(event, pos);
}


static void
avdtp_signaling_emit_header_compression_capability(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_header_compression_capabilities_t *header_compression) {
    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    event[pos++] = header_compression->back_ch;
    event[pos++] = header_compression->media;
    event[pos++] = header_compression->recovery;
    avdtp_emit_sink_and_source(event, pos);
}

static void
avdtp_signaling_emit_content_multiplexing_capability(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_multiplexing_mode_capabilities_t *multiplexing_mode) {
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    
    event[pos++] = multiplexing_mode->fragmentation;
    event[pos++] = multiplexing_mode->transport_identifiers_num;

    int i;
    for (i = 0; i < 3; i++){
        event[pos++] = multiplexing_mode->transport_session_identifiers[i];
    }
    for (i = 0; i < 3; i++){
        event[pos++] = multiplexing_mode->tcid[i];
    }
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_capability_done(uint16_t avdtp_cid, uint8_t remote_seid) {
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = remote_seid;
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_media_codec_capability(uint16_t avdtp_cid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec){
    switch (media_codec.media_codec_type){
        case AVDTP_CODEC_SBC:
            avdtp_signaling_emit_media_codec_sbc_capability(avdtp_cid, remote_seid, media_codec);
            break;
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            avdtp_signaling_emit_media_codec_mpeg_audio_capability(avdtp_cid, remote_seid, media_codec);
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            avdtp_signaling_emit_media_codec_mpeg_aac_capability(avdtp_cid, remote_seid, media_codec);
            break;
        case AVDTP_CODEC_ATRAC_FAMILY:
            avdtp_signaling_emit_media_codec_atrac_capability(avdtp_cid, remote_seid, media_codec);
            break;
        default:
            avdtp_signaling_emit_media_codec_other_capability(avdtp_cid, remote_seid, media_codec);
            break;
    }
}

// emit events for all capabilities incl. final done event
void avdtp_signaling_emit_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_capabilities_t *capabilities,
									   uint16_t registered_service_categories) {
    if (get_bit16(registered_service_categories, AVDTP_MEDIA_CODEC)){
        avdtp_signaling_emit_media_codec_capability(avdtp_cid, remote_seid, capabilities->media_codec);
    }

    if (get_bit16(registered_service_categories, AVDTP_MEDIA_TRANSPORT)){
		avdtp_signaling_emit_media_transport_capability(avdtp_cid, remote_seid);
    }
    if (get_bit16(registered_service_categories, AVDTP_REPORTING)){
		avdtp_signaling_emit_reporting_capability(avdtp_cid, remote_seid);
    }
    if (get_bit16(registered_service_categories, AVDTP_RECOVERY)){
		avdtp_signaling_emit_recovery_capability(avdtp_cid, remote_seid, &capabilities->recovery);
    }
    if (get_bit16(registered_service_categories, AVDTP_CONTENT_PROTECTION)){
		avdtp_signaling_emit_content_protection_capability(avdtp_cid, remote_seid,
														   &capabilities->content_protection);
    }
    if (get_bit16(registered_service_categories, AVDTP_HEADER_COMPRESSION)){
		avdtp_signaling_emit_header_compression_capability(avdtp_cid, remote_seid,
														   &capabilities->header_compression);
    }
    if (get_bit16(registered_service_categories, AVDTP_MULTIPLEXING)){
		avdtp_signaling_emit_content_multiplexing_capability(avdtp_cid, remote_seid,
															 &capabilities->multiplexing_mode);
    }
    if (get_bit16(registered_service_categories, AVDTP_DELAY_REPORTING)){
		avdtp_signaling_emit_delay_reporting_capability(avdtp_cid, remote_seid);
    }
	avdtp_signaling_emit_capability_done(avdtp_cid, remote_seid);
}

static uint16_t
avdtp_signaling_setup_media_codec_sbc_config_event(uint8_t *event, uint16_t size,
                                                   const avdtp_stream_endpoint_t *stream_endpoint,
                                                   uint16_t avdtp_cid, uint8_t reconfigure,
                                                   const uint8_t *media_codec_information) {

    btstack_assert(size >= AVDTP_MEDIA_CONFIG_SBC_EVENT_LEN);

    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    uint8_t remote_seid = avdtp_remote_seid(stream_endpoint);

    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = AVDTP_MEDIA_CONFIG_SBC_EVENT_LEN - 2;

    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] = AVDTP_CODEC_SBC;

    uint8_t sampling_frequency_bitmap = media_codec_information[0] >> 4;
    uint8_t channel_mode_bitmap = media_codec_information[0] & 0x0F;
    uint8_t block_length_bitmap = media_codec_information[1] >> 4;
    uint8_t subbands_bitmap = (media_codec_information[1] & 0x0F) >> 2;

    uint8_t num_channels = 0;
    avdtp_channel_mode_t channel_mode;

    if (channel_mode_bitmap & AVDTP_SBC_JOINT_STEREO){
        channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
        num_channels = 2;
    } else if (channel_mode_bitmap & AVDTP_SBC_STEREO){
        channel_mode = AVDTP_CHANNEL_MODE_STEREO;
        num_channels = 2;
    } else if (channel_mode_bitmap & AVDTP_SBC_DUAL_CHANNEL){
        channel_mode = AVDTP_CHANNEL_MODE_DUAL_CHANNEL;
        num_channels = 2;
    } else {
        channel_mode = AVDTP_CHANNEL_MODE_MONO;
        num_channels = 1;
    }

    uint16_t sampling_frequency = 0;
    if (sampling_frequency_bitmap & AVDTP_SBC_48000) {
        sampling_frequency = 48000;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_44100) {
        sampling_frequency = 44100;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_32000) {
        sampling_frequency = 32000;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_16000) {
        sampling_frequency = 16000;
    }

    uint8_t subbands = 0;
    if (subbands_bitmap & AVDTP_SBC_SUBBANDS_8){
        subbands = 8;
    } else if (subbands_bitmap & AVDTP_SBC_SUBBANDS_4){
        subbands = 4;
    }

    uint8_t block_length = 0;
    if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_16){
        block_length = 16;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_12){
        block_length = 12;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_8){
        block_length = 8;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_4){
        block_length = 4;
    }

    little_endian_store_16(event, pos, sampling_frequency);
    pos += 2;

    event[pos++] = (uint8_t) channel_mode;
    event[pos++] = num_channels;
    event[pos++] = block_length;
    event[pos++] = subbands;
    event[pos++] = media_codec_information[1] & 0x03;
    event[pos++] = media_codec_information[2];
    event[pos++] = media_codec_information[3];

    btstack_assert(pos == AVDTP_MEDIA_CONFIG_SBC_EVENT_LEN);

    return pos;
}

static uint16_t
avdtp_signaling_setup_media_codec_mpeg_audio_config_event(uint8_t *event, uint16_t size,
                                                          const avdtp_stream_endpoint_t *stream_endpoint,
                                                          uint16_t avdtp_cid, uint8_t reconfigure,
                                                          const uint8_t *media_codec_information) {

    btstack_assert(size >= AVDTP_MEDIA_CONFIG_MPEG_AUDIO_EVENT_LEN);

    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    uint8_t remote_seid = avdtp_remote_seid(stream_endpoint);

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = AVDTP_MEDIA_CONFIG_MPEG_AUDIO_EVENT_LEN - 2;

    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] = AVDTP_CODEC_MPEG_1_2_AUDIO;

    uint8_t layer_bitmap              =   media_codec_information[0] >> 5;
    uint8_t crc                       =  (media_codec_information[0] >> 4) & 0x01;
    uint8_t channel_mode_bitmap       =  (media_codec_information[0] & 0x07);
    uint8_t mpf                       =  (media_codec_information[1] >> 6) & 0x01;
    uint8_t sampling_frequency_bitmap =  (media_codec_information[1] & 0x3F);
    uint8_t vbr                       =  (media_codec_information[2] >> 7) & 0x01;
    uint16_t bit_rate_index_bitmap    = ((media_codec_information[2] & 0x3f) << 8) | media_codec_information[3];

    uint8_t layer = 0;
    if (layer_bitmap & 0x04){
        layer = AVDTP_MPEG_LAYER_1;
    } else if (layer_bitmap & 0x02){
        layer = AVDTP_MPEG_LAYER_2;
    } else if (layer_bitmap & 0x01){
        layer = AVDTP_MPEG_LAYER_3;
    }

    uint8_t num_channels = 0;
    avdtp_channel_mode_t channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    if (channel_mode_bitmap & 0x08){
        num_channels = 1;
        channel_mode = AVDTP_CHANNEL_MODE_MONO;
    } else if (channel_mode_bitmap & 0x04){
        num_channels = 2;
        channel_mode = AVDTP_CHANNEL_MODE_DUAL_CHANNEL;
    } else if (channel_mode_bitmap & 0x02){
        num_channels = 2;
        channel_mode = AVDTP_CHANNEL_MODE_STEREO;
    } else if (channel_mode_bitmap & 0x02){
        num_channels = 2;
        channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    }

    uint16_t sampling_frequency = 0;
    if (sampling_frequency_bitmap & 0x01) {
        sampling_frequency = 48000;
    } else if (sampling_frequency_bitmap & 0x02) {
        sampling_frequency = 44100;
    } else if (sampling_frequency_bitmap & 0x04) {
        sampling_frequency = 32000;
    } else if (sampling_frequency_bitmap & 0x08) {
        sampling_frequency = 24000;
    } else if (sampling_frequency_bitmap & 0x10) {
        sampling_frequency = 22050;
    } else if (sampling_frequency_bitmap & 0x20) {
        sampling_frequency = 16000;
    }

    uint8_t bitrate_index = 0;
    uint8_t i;
    for (i=0;i<14;i++){
        if (bit_rate_index_bitmap & (1U << i)) {
            bitrate_index = i;
        }
    }

    event[pos++] = (uint8_t) layer;
    event[pos++] = crc;
    event[pos++] = (uint8_t) channel_mode;
    event[pos++] = num_channels;
    event[pos++] = mpf;
    little_endian_store_16(event, pos, sampling_frequency);
    pos += 2;
    event[pos++] = vbr;
    event[pos++] = bitrate_index;

    btstack_assert(pos == AVDTP_MEDIA_CONFIG_MPEG_AUDIO_EVENT_LEN);

    return pos;
}

static uint16_t
avdtp_signaling_setup_media_codec_mpec_aac_config_event(uint8_t *event, uint16_t size,
                                                        const avdtp_stream_endpoint_t *stream_endpoint,
                                                        uint16_t avdtp_cid, uint8_t reconfigure,
                                                        const uint8_t *media_codec_information) {

    btstack_assert(size >= AVDTP_MEDIA_CONFIG_MPEG_AUDIO_EVENT_LEN);

    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    uint8_t remote_seid = avdtp_remote_seid(stream_endpoint);

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = AVDTP_MEDIA_CONFIG_MPEG_AAC_EVENT_LEN - 2;

    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] =AVDTP_CODEC_MPEG_2_4_AAC;

    uint8_t  object_type_bitmap        =   media_codec_information[0];
    uint16_t sampling_frequency_bitmap =  (media_codec_information[1] << 4) | (media_codec_information[2] >> 4);
    uint8_t  channels_bitmap           =  (media_codec_information[2] >> 2) & 0x03;
    uint8_t  vbr                       =   media_codec_information[3] >> 7;
    uint32_t bit_rate                  = ((media_codec_information[3] & 0x7f) << 16) | (media_codec_information[4] << 8) | media_codec_information[5];

    uint8_t object_type = 0;
    if (object_type_bitmap & 0x80){
        object_type = AVDTP_AAC_MPEG2_LC;
    } else if (object_type_bitmap & 0x40){
        object_type = AVDTP_AAC_MPEG4_LC;
    } else if (object_type_bitmap & 0x020){
        object_type = AVDTP_AAC_MPEG4_LTP;
    } else if (object_type_bitmap & 0x010){
        object_type = AVDTP_AAC_MPEG4_SCALABLE;
    }

    uint32_t sampling_frequency = 0;
    uint8_t i;
    const uint32_t aac_sampling_frequency_table[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000
    };
    for (i=0;i<12;i++){
        if (sampling_frequency_bitmap & (1U << i)) {
            sampling_frequency = aac_sampling_frequency_table[i];
        }
    }

    uint8_t num_channels = 0;
    if (channels_bitmap & 0x02){
        num_channels = 1;
    } else if (channels_bitmap & 0x01){
        num_channels = 2;
    }

    event[pos++] = object_type;
    little_endian_store_24(event, pos, sampling_frequency);
    pos += 3;
    event[pos++] = num_channels;
    little_endian_store_24(event, pos, bit_rate);
    pos += 3;
    event[pos++] = vbr;

    btstack_assert(AVDTP_MEDIA_CONFIG_MPEG_AAC_EVENT_LEN == pos);

    return pos;
}

static uint16_t avdtp_signaling_setup_media_codec_atrac_config_event(uint8_t *event, uint16_t size,
                                                                     const avdtp_stream_endpoint_t *stream_endpoint,
                                                                     uint16_t avdtp_cid, uint8_t reconfigure,
                                                                     const uint8_t *media_codec_information) {
    btstack_assert(size >= AVDTP_MEDIA_CONFIG_ATRAC_EVENT_LEN);

    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    uint8_t remote_seid = avdtp_remote_seid(stream_endpoint);

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = AVDTP_MEDIA_CONFIG_ATRAC_EVENT_LEN - 2;

    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] = AVDTP_CODEC_ATRAC_FAMILY;

    avdtp_atrac_version_t  version     = (avdtp_atrac_version_t) (media_codec_information[0] >> 5);
    uint8_t  channel_mode_bitmap       = (media_codec_information[0] >> 2) & 0x07;
    uint16_t sampling_frequency_bitmap = (media_codec_information[1] >> 4) & 0x03;
    uint8_t  vbr                       = (media_codec_information[1] >> 3) & 0x01;
    uint16_t bit_rate_index_bitmap     = ((media_codec_information[1]) & 0x07) << 16 | (media_codec_information[2] << 8) | media_codec_information[3];
    uint16_t maximum_sul               = (media_codec_information[4] << 8) | media_codec_information[5];

    uint8_t num_channels = 0;
    avdtp_channel_mode_t channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    if (channel_mode_bitmap & 0x04){
        num_channels = 1;
        channel_mode = AVDTP_CHANNEL_MODE_MONO;
    } else if (channel_mode_bitmap & 0x02){
        num_channels = 2;
        channel_mode = AVDTP_CHANNEL_MODE_DUAL_CHANNEL;
    } else if (channel_mode_bitmap & 0x01){
        num_channels = 2;
        channel_mode = AVDTP_CHANNEL_MODE_JOINT_STEREO;
    }

    uint16_t sampling_frequency = 0;
    if (sampling_frequency_bitmap & 0x02){
        sampling_frequency = 44100;
    } else if (sampling_frequency_bitmap & 0x01){
        sampling_frequency = 48000;
    }

    // bit 0 = index 0x18, bit 19 = index 0
    uint8_t bit_rate_index = 0;
    uint8_t i;
    for (i=0;i <= 19;i++){
        if (bit_rate_index_bitmap & (1U << i)) {
            bit_rate_index = 18 - i;
        }
    }

    event[pos++] = (uint8_t) version;
    event[pos++] = (uint8_t) channel_mode;
    event[pos++] = num_channels;
    little_endian_store_16(event, pos, sampling_frequency);
    pos += 2;
    event[pos++] = vbr;
    event[pos++] = bit_rate_index;
    little_endian_store_16(event, pos, maximum_sul);
    pos += 2;

    btstack_assert(pos == AVDTP_MEDIA_CONFIG_ATRAC_EVENT_LEN);
    return pos;
}

static uint16_t avdtp_signaling_setup_media_codec_other_config_event(uint8_t *event, uint16_t size,
                                                                     const avdtp_stream_endpoint_t *stream_endpoint,
                                                                     uint16_t avdtp_cid, uint8_t reconfigure,
                                                                     const adtvp_media_codec_capabilities_t *media_codec) {
    btstack_assert(size >= AVDTP_MEDIA_CONFIG_OTHER_EVENT_LEN);

    uint8_t local_seid = avdtp_local_seid(stream_endpoint);
    uint8_t remote_seid = avdtp_remote_seid(stream_endpoint);

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    pos++;  // set later
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] = media_codec->media_type;
    little_endian_store_16(event, pos, media_codec->media_codec_type);
    pos += 2;
    little_endian_store_16(event, pos, media_codec->media_codec_information_len);
    pos += 2;

    btstack_assert(pos == 13);

    uint16_t media_codec_len = btstack_min(AVDTP_MAX_MEDIA_CODEC_INFORMATION_LENGTH, media_codec->media_codec_information_len);
    (void)memcpy(event + pos, media_codec->media_codec_information, media_codec_len);
    pos += media_codec_len;
    event[1] = pos - 2;
    return pos;
}

void avdtp_signaling_emit_delay(uint16_t avdtp_cid, uint8_t local_seid, uint16_t delay) {
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    little_endian_store_16(event, pos, delay);
    pos += 2;
    avdtp_emit_source(event, pos);
}

uint16_t avdtp_setup_media_codec_config_event(uint8_t *event, uint16_t size, const avdtp_stream_endpoint_t *stream_endpoint,
                                              uint16_t avdtp_cid, uint8_t reconfigure,
                                              const adtvp_media_codec_capabilities_t * media_codec) {
    switch (media_codec->media_codec_type){
        case AVDTP_CODEC_SBC:
            return avdtp_signaling_setup_media_codec_sbc_config_event(event, size, stream_endpoint, avdtp_cid, reconfigure,
                                                                     media_codec->media_codec_information);
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            return avdtp_signaling_setup_media_codec_mpeg_audio_config_event(event, size, stream_endpoint, avdtp_cid, reconfigure,
                                                                             media_codec->media_codec_information);
        case AVDTP_CODEC_MPEG_2_4_AAC:
            return avdtp_signaling_setup_media_codec_mpec_aac_config_event(event, size, stream_endpoint, avdtp_cid, reconfigure,
                                                                           media_codec->media_codec_information);
        case AVDTP_CODEC_ATRAC_FAMILY:
            return avdtp_signaling_setup_media_codec_atrac_config_event(event, size, stream_endpoint, avdtp_cid, reconfigure,
                                                                        media_codec->media_codec_information);
        default:
            return avdtp_signaling_setup_media_codec_other_config_event(event, size, stream_endpoint, avdtp_cid, reconfigure,
                                                                        media_codec);
    }
}

void avdtp_signaling_emit_configuration(avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid, uint8_t reconfigure,
                                        avdtp_capabilities_t *configuration, uint16_t configured_service_categories) {
    
    if (get_bit16(configured_service_categories, AVDTP_MEDIA_CODEC)){
        uint16_t pos = 0;
        // assume MEDIA_CONFIG_OTHER_EVENT_LEN is larger than all other events
        uint8_t event[AVDTP_MEDIA_CONFIG_OTHER_EVENT_LEN];
        pos = avdtp_setup_media_codec_config_event(event, sizeof(event), stream_endpoint, avdtp_cid, reconfigure,
                                                   &configuration->media_codec);
        btstack_packet_handler_t packet_handler = avdtp_packet_handler_for_stream_endpoint(stream_endpoint);
        (*packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    }
}

void avdtp_streaming_emit_connection_established(avdtp_stream_endpoint_t *stream_endpoint, uint8_t status) {
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, stream_endpoint->connection->avdtp_cid);
    pos += 2;
    reverse_bd_addr(stream_endpoint->connection->remote_addr, &event[pos]);
    pos += 6;
    event[pos++] = avdtp_local_seid(stream_endpoint);
    event[pos++] = avdtp_remote_seid(stream_endpoint);
    event[pos++] = status;

    btstack_packet_handler_t packet_handler = avdtp_packet_handler_for_stream_endpoint(stream_endpoint);
    (*packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

void avdtp_streaming_emit_connection_released(avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid, uint8_t local_seid) {
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;

    btstack_packet_handler_t packet_handler = avdtp_packet_handler_for_stream_endpoint(stream_endpoint);
    (*packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

void avdtp_streaming_emit_can_send_media_packet_now(avdtp_stream_endpoint_t *stream_endpoint, uint16_t sequence_number) {
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW;
    little_endian_store_16(event, pos, stream_endpoint->connection->avdtp_cid);
    pos += 2;
    event[pos++] = avdtp_local_seid(stream_endpoint);
    little_endian_store_16(event, pos, sequence_number);
    pos += 2;
    event[1] = pos - 2;

    btstack_packet_handler_t packet_handler = avdtp_packet_handler_for_stream_endpoint(stream_endpoint);
    (*packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

uint8_t avdtp_request_can_send_now_acceptor(avdtp_connection_t *connection) {
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->wait_to_send_acceptor = true;
    l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_request_can_send_now_initiator(avdtp_connection_t *connection) {
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->wait_to_send_initiator = true;
    l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_local_seid(const avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return 0;
    return stream_endpoint->sep.seid;

}

uint8_t avdtp_remote_seid(const avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return AVDTP_INVALID_SEP_SEID;
    return stream_endpoint->remote_sep.seid;
}

// helper to set/get configuration
void avdtp_config_sbc_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz){
    avdtp_sbc_sampling_frequency_t sampling_frequency;
    switch (sampling_frequency_hz){
        case 16000:
            sampling_frequency = AVDTP_SBC_16000;
            break;
        case 32000:
            sampling_frequency = AVDTP_SBC_32000;
            break;
        case 48000:
            sampling_frequency = AVDTP_SBC_48000;
            break;
        default:
            sampling_frequency = AVDTP_SBC_44100;
            break;
    }
    config[0] = (((uint8_t) sampling_frequency) << 4) | (config[0] & 0x0f);
}

void avdtp_config_sbc_store(uint8_t * config, const avdtp_configuration_sbc_t * configuration){
    avdtp_sbc_channel_mode_t sbc_channel_mode;
    switch (configuration->channel_mode){
        case AVDTP_CHANNEL_MODE_MONO:
            sbc_channel_mode = AVDTP_SBC_MONO;
            break;
        case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
            sbc_channel_mode = AVDTP_SBC_DUAL_CHANNEL;
            break;
        case AVDTP_CHANNEL_MODE_STEREO:
            sbc_channel_mode = AVDTP_SBC_STEREO;
            break;
        default:
            sbc_channel_mode = AVDTP_SBC_JOINT_STEREO;
            break;
    }
    config[0] = (uint8_t) sbc_channel_mode;
    config[1] = (configuration->block_length << 4) | (configuration->subbands << 2) | configuration->allocation_method;
    config[2] = configuration-> min_bitpool_value;
    config[3] = configuration->max_bitpool_value;
    avdtp_config_sbc_set_sampling_frequency(config, configuration->sampling_frequency);
}

void avdtp_config_mpeg_audio_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz) {
    uint8_t sampling_frequency_index = 0;
    switch (sampling_frequency_hz){
        case 16000:
            sampling_frequency_index = 5;
            break;
        case 22040:
            sampling_frequency_index = 4;
            break;
        case 24000:
            sampling_frequency_index = 3;
            break;
        case 32000:
            sampling_frequency_index = 2;
            break;
        case 44100:
            sampling_frequency_index = 1;
            break;
        case 48000:
            sampling_frequency_index = 0;
            break;
        default:
            btstack_assert(false);
            break;
    }
    config[1] = (config[1] & 0xC0) | (1 << sampling_frequency_index);
}

void avdtp_config_mpeg_audio_store(uint8_t * config, const avdtp_configuration_mpeg_audio_t * configuration){

    config[0] = (1 << (7 - (configuration->layer - AVDTP_MPEG_LAYER_1))) | ((configuration->crc & 0x01) << 4) | (1 << (configuration->channel_mode - AVDTP_CHANNEL_MODE_MONO));
    config[1] = ((configuration->media_payload_format & 0x01) << 6) ;
    uint16_t bit_rate_mask = 1 << configuration->bit_rate_index;
    config[2] = ((configuration->vbr & 0x01) << 7) | ((bit_rate_mask >> 8) & 0x3f);
    config[3] = bit_rate_mask & 0xff;
    avdtp_config_mpeg_audio_set_sampling_frequency(config, configuration->sampling_frequency);
}


void avdtp_config_mpeg_aac_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz) {
    uint16_t sampling_frequency_bitmap = 0;
    uint8_t i;
    const uint32_t aac_sampling_frequency_table[] = {
            96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000
    };
    for (i=0;i<12;i++){
        if (sampling_frequency_hz == aac_sampling_frequency_table[i]){
            sampling_frequency_bitmap = 1 << i;
            break;
        }
    }
    config[1] = sampling_frequency_bitmap >> 4;
    config[2] = ((sampling_frequency_bitmap & 0x0f) << 4) | (config[2] & 0x0f);
}

void avdtp_config_mpeg_aac_store(uint8_t * config, const avdtp_configuration_mpeg_aac_t * configuration) {
    config[0] = 1 << (7 -(configuration->object_type - AVDTP_AAC_MPEG2_LC));
    uint8_t channels_bitmap = 0;
    switch (configuration->channels){
        case 1:
            channels_bitmap = 0x02;
            break;
        case 2:
            channels_bitmap = 0x01;
            break;
        default:
            break;
    }
    config[2] = channels_bitmap << 2;
    config[3] = ((configuration->vbr & 0x01) << 7) | ((configuration->bit_rate >> 16) & 0x7f);
    config[4] = (configuration->bit_rate >> 8) & 0xff;
    config[5] =  configuration->bit_rate & 0xff;
    avdtp_config_mpeg_aac_set_sampling_frequency(config, configuration->sampling_frequency);
}

void avdtp_config_atrac_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz) {
    uint8_t fs_bitmap = 0;
    switch (sampling_frequency_hz){
        case 44100:
            fs_bitmap = 2;
            break;
        case 48000:
            fs_bitmap = 1;
            break;
        default:
            break;
    }
    config[1] = (fs_bitmap << 4) | (config[1] & 0x0F);
}

void avdtp_config_atrac_store(uint8_t * config, const avdtp_configuration_atrac_t * configuration){
    uint8_t channel_mode_bitmap = 0;
    switch (configuration->channel_mode){
        case AVDTP_CHANNEL_MODE_MONO:
            channel_mode_bitmap = 4;
            break;
        case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
            channel_mode_bitmap = 2;
            break;
        case AVDTP_CHANNEL_MODE_JOINT_STEREO:
            channel_mode_bitmap = 1;
            break;
        default:
            break;
    }
    config[0] = ((configuration->version - AVDTP_ATRAC_VERSION_1 + 1) << 5) | (channel_mode_bitmap << 2);
    uint32_t bit_rate_bitmap = 1 << (0x18 - configuration->bit_rate_index);
    config[1] = ((configuration->vbr & 0x01) << 3) | ((bit_rate_bitmap >> 16) & 0x07);
    config[2] = (bit_rate_bitmap >> 8) & 0xff;
    config[3] = bit_rate_bitmap & 0xff;
    config[4] = configuration->maximum_sul >> 8;
    config[5] = configuration->maximum_sul & 0xff;
    config[6] = 0;
    avdtp_config_atrac_set_sampling_frequency(config, configuration->sampling_frequency);
}

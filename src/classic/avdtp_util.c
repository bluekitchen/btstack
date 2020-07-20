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

#define BTSTACK_FILE__ "avdtp_util.c"

#include <stdint.h>
#include <string.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"

#define MAX_MEDIA_CODEC_INFORMATION_LENGTH 100

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

    stream_endpoint->connection = NULL;
    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;

    stream_endpoint->sep.in_use = 0;
    memset(&stream_endpoint->remote_sep, 0, sizeof(avdtp_sep_t));
    // memset(&stream_endpoint->remote_capabilities, 0, sizeof(avdtp_capabilities_t));
    // memset(&stream_endpoint->remote_configuration, 0, sizeof(avdtp_capabilities_t));
    
    stream_endpoint->remote_capabilities_bitmap = 0;
    stream_endpoint->remote_configuration_bitmap = 0;

    stream_endpoint->media_disconnect = 0;
    stream_endpoint->media_connect = 0;
    stream_endpoint->start_stream = 0;
    stream_endpoint->stop_stream = 0;
    stream_endpoint->send_stream = 0;
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
        connection->error_code = BAD_SERV_CATEGORY;
        return 1;
    }

    if (signal_identifier == AVDTP_SI_RECONFIGURE){
        if ( (category != AVDTP_CONTENT_PROTECTION) && (category != AVDTP_MEDIA_CODEC)){
            log_info("    ERROR: REJECT CATEGORY, INVALID_CAPABILITIES\n");
            connection->reject_service_category = category;
            connection->error_code = INVALID_CAPABILITIES;
            return 1;
        }
    }

    switch(category){
        case AVDTP_MEDIA_TRANSPORT:   
            if (cap_len != 0){
                log_info("    ERROR: REJECT CATEGORY, BAD_MEDIA_TRANSPORT\n");
                connection->reject_service_category = category;
                connection->error_code = BAD_MEDIA_TRANSPORT_FORMAT;
                return 1;
            }
            break;
        case AVDTP_REPORTING:                
        case AVDTP_DELAY_REPORTING:                
            if (cap_len != 0){
                log_info("    ERROR: REJECT CATEGORY, BAD_LENGTH\n");
                connection->reject_service_category = category;
                connection->error_code = BAD_LENGTH;
                return 1;
            }
            break;
        case AVDTP_RECOVERY:     
            if (cap_len != 3){
                log_info("    ERROR: REJECT CATEGORY, BAD_MEDIA_TRANSPORT\n");
                connection->reject_service_category = category;
                connection->error_code = BAD_RECOVERY_FORMAT;
                return 1;
            }           
            break;
        case AVDTP_CONTENT_PROTECTION:
            if (cap_len < 2){
                log_info("    ERROR: REJECT CATEGORY, BAD_CP_FORMAT\n");
                connection->reject_service_category = category;
                connection->error_code = BAD_CP_FORMAT;
                return 1;
            }
            break;
        case AVDTP_HEADER_COMPRESSION:
            // TODO: find error code for bad header compression
            if (cap_len != 1){
                log_info("    ERROR: REJECT CATEGORY, BAD_HEADER_COMPRESSION\n");
                connection->reject_service_category = category;
                connection->error_code = BAD_RECOVERY_FORMAT;
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
            connection->error_code = BAD_LENGTH;
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


void avdtp_signaling_emit_connection_established(uint16_t avdtp_cid, bd_addr_t addr, uint8_t status) {
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
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

static void avdtp_signaling_emit_media_codec_sbc_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                            adtvp_media_codec_capabilities_t media_codec) {
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;
    event[pos++] = media_codec.media_codec_information[0] >> 4;
    event[pos++] = media_codec.media_codec_information[0] & 0x0F;
    event[pos++] = media_codec.media_codec_information[1] >> 4;
    event[pos++] = (media_codec.media_codec_information[1] & 0x0F) >> 2;
    event[pos++] = media_codec.media_codec_information[1] & 0x03;
    event[pos++] = media_codec.media_codec_information[2];
    event[pos++] = media_codec.media_codec_information[3];
    avdtp_emit_sink_and_source(event, pos);
}

static inline void
avdtp_signaling_emit_capability(uint8_t capability_subevent_id, uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid) {
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = capability_subevent_id;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    avdtp_emit_sink_and_source(event, pos);
}

static void
avdtp_signaling_emit_media_transport_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid) {
    avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY, avdtp_cid, local_seid,
                                    remote_seid);
}

static void avdtp_signaling_emit_reporting_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid) {
    avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY, avdtp_cid, local_seid, remote_seid);
}

static void
avdtp_signaling_emit_delay_reporting_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid) {
    avdtp_signaling_emit_capability(AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY, avdtp_cid, local_seid,
                                    remote_seid);
}

static void avdtp_signaling_emit_recovery_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                     avdtp_recovery_capabilities_t *recovery) {
    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = recovery->recovery_type;
    event[pos++] = recovery->maximum_recovery_window_size;
    event[pos++] = recovery->maximum_number_media_packets;
    avdtp_emit_sink_and_source(event, pos);
}

static void
avdtp_signaling_emit_content_protection_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                   adtvp_content_protection_t *content_protection) {
    uint8_t event[22];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    
    little_endian_store_16(event, pos, content_protection->cp_type);
    pos += 2;
    little_endian_store_16(event, pos, content_protection->cp_type_value_len);
    pos += 2;
    
    //TODO: reserve place for value
    if (content_protection->cp_type_value_len < 10){
        (void)memcpy(event + pos, content_protection->cp_type_value,
                     content_protection->cp_type_value_len);
    }
    avdtp_emit_sink_and_source(event, pos);
}


static void
avdtp_signaling_emit_header_compression_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                   avdtp_header_compression_capabilities_t *header_compression) {
    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = header_compression->back_ch;
    event[pos++] = header_compression->media;
    event[pos++] = header_compression->recovery;
    avdtp_emit_sink_and_source(event, pos);
}

static void
avdtp_signaling_emit_content_multiplexing_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                     avdtp_multiplexing_mode_capabilities_t *multiplexing_mode) {
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
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

static void
avdtp_signaling_emit_media_codec_other_capability(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid,
                                                  adtvp_media_codec_capabilities_t media_codec) {
    uint8_t event[MAX_MEDIA_CODEC_INFORMATION_LENGTH + 12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = media_codec.media_type;
    little_endian_store_16(event, pos, media_codec.media_codec_type);
    pos += 2;
    little_endian_store_16(event, pos, media_codec.media_codec_information_len);
    pos += 2;
    (void)memcpy(event + pos, media_codec.media_codec_information,
                 btstack_min(media_codec.media_codec_information_len, MAX_MEDIA_CODEC_INFORMATION_LENGTH));
    avdtp_emit_sink_and_source(event, pos);
}

static void avdtp_signaling_emit_capability_done(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid) {
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    avdtp_emit_sink_and_source(event, pos);
}

void avdtp_signaling_emit_media_codec_other(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, adtvp_media_codec_capabilities_t media_codec, uint8_t reconfigure){
    btstack_assert(callback != NULL);
    uint8_t event[MAX_MEDIA_CODEC_INFORMATION_LENGTH + 13];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVDTP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION;
    little_endian_store_16(event, pos, avdtp_cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = reconfigure;
    event[pos++] = media_codec.media_type;
    little_endian_store_16(event, pos, media_codec.media_codec_type);
    pos += 2;
    little_endian_store_16(event, pos, media_codec.media_codec_information_len);
    pos += 2;

    int media_codec_len = btstack_min(MAX_MEDIA_CODEC_INFORMATION_LENGTH, media_codec.media_codec_information_len);
    (void)memcpy(event + pos, media_codec.media_codec_information,
                 media_codec_len);
    
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void avdtp_emit_capabilities(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, avdtp_capabilities_t *capabilities,
                        uint16_t registered_service_categories) {
    if (get_bit16(registered_service_categories, AVDTP_MEDIA_CODEC)){
        switch (capabilities->media_codec.media_codec_type){
            case AVDTP_CODEC_SBC:
                avdtp_signaling_emit_media_codec_sbc_capability(avdtp_cid, local_seid, remote_seid, capabilities->media_codec);
                break;
            default:
                avdtp_signaling_emit_media_codec_other_capability(avdtp_cid, local_seid, remote_seid, capabilities->media_codec);
                break;
        }
    }

    if (get_bit16(registered_service_categories, AVDTP_MEDIA_TRANSPORT)){
        avdtp_signaling_emit_media_transport_capability(avdtp_cid, local_seid, remote_seid);
    }
    if (get_bit16(registered_service_categories, AVDTP_REPORTING)){
        avdtp_signaling_emit_reporting_capability(avdtp_cid, local_seid, remote_seid);
    }
    if (get_bit16(registered_service_categories, AVDTP_RECOVERY)){
        avdtp_signaling_emit_recovery_capability(avdtp_cid, local_seid, remote_seid, &capabilities->recovery);
    }
    if (get_bit16(registered_service_categories, AVDTP_CONTENT_PROTECTION)){
        avdtp_signaling_emit_content_protection_capability(avdtp_cid, local_seid, remote_seid,
                                                           &capabilities->content_protection);
    }
    if (get_bit16(registered_service_categories, AVDTP_HEADER_COMPRESSION)){
        avdtp_signaling_emit_header_compression_capability(avdtp_cid, local_seid, remote_seid,
                                                           &capabilities->header_compression);
    }
    if (get_bit16(registered_service_categories, AVDTP_MULTIPLEXING)){
        avdtp_signaling_emit_content_multiplexing_capability(avdtp_cid, local_seid, remote_seid,
                                                             &capabilities->multiplexing_mode);
    }
    if (get_bit16(registered_service_categories, AVDTP_DELAY_REPORTING)){
        avdtp_signaling_emit_delay_reporting_capability(avdtp_cid, local_seid, remote_seid);
    }
    avdtp_signaling_emit_capability_done(avdtp_cid, local_seid, remote_seid);
}

uint8_t avdtp_request_can_send_now_acceptor(avdtp_connection_t * connection, uint16_t l2cap_cid){
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->wait_to_send_acceptor = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_request_can_send_now_initiator(avdtp_connection_t * connection, uint16_t l2cap_cid){
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->wait_to_send_initiator = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_local_seid(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return 0;
    return stream_endpoint->sep.seid;

}

uint8_t avdtp_remote_seid(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return AVDTP_INVALID_SEP_SEID;
    return stream_endpoint->remote_sep.seid;
}

void a2dp_streaming_emit_connection_established(btstack_packet_handler_t callback, uint16_t cid, bd_addr_t addr, uint8_t local_seid, uint8_t remote_seid, uint8_t status){
    btstack_assert(callback != NULL);
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAM_ESTABLISHED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

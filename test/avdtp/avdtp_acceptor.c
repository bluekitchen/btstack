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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_acceptor.h"

static int avdtp_pack_service_capabilities(uint8_t * buffer, int size, avdtp_capabilities_t caps, avdtp_service_category_t category, uint8_t pack_all_capabilities){
    int i;
    // pos = 0 reserved for length
    int pos = 1;
    switch(category){
        case AVDTP_MEDIA_TRANSPORT:
        case AVDTP_REPORTING:
            break;
        case AVDTP_DELAY_REPORTING:
            if (!pack_all_capabilities) break;
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
            memcpy(buffer+pos, caps.content_protection.cp_type_value, caps.content_protection.cp_type_value_len);
            printf("AVDTP_CONTENT_PROTECTION 0%04x\n", caps.content_protection.cp_type);
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
    }
    buffer[0] = pos - 1; // length
    return pos;
}

static uint16_t avdtp_unpack_service_capabilities(avdtp_connection_t * connection, avdtp_capabilities_t * caps, uint8_t * packet, uint16_t size){
    uint16_t registered_service_categories = 0;
    int pos = 0;
    avdtp_service_category_t category = (avdtp_service_category_t)packet[pos++];
    
    if (category < AVDTP_MEDIA_TRANSPORT || category > AVDTP_DELAY_REPORTING){
        printf(" avdtp_unpack_service_capabilities wrong category %d\n", category);
        return 0;
    }
    int i;
    uint8_t cap_len = packet[pos++];
    printf("    ACP: avdtp_unpack_service_capabilities: category %d, capability len %d, record size %d\n", category, cap_len, size);
    int processed_cap_len = 0;
    while (pos < size){
        processed_cap_len = pos;
        switch(category){
            case AVDTP_MEDIA_TRANSPORT:                
                break;
            case AVDTP_REPORTING:                
                break;
            case AVDTP_DELAY_REPORTING:                
                break;
            case AVDTP_RECOVERY:                
                caps->recovery.recovery_type = packet[pos++];
                caps->recovery.maximum_recovery_window_size = packet[pos++];
                caps->recovery.maximum_number_media_packets = packet[pos++];
                break;
            case AVDTP_CONTENT_PROTECTION:
                if (cap_len < 2){
                    connection->reject_service_category = category;
                    connection->error_code = BAD_CP_FORMAT;
                    return 0;
                }
                
                caps->content_protection.cp_type = big_endian_read_16(packet, pos);
                pos+=2;
                
                caps->content_protection.cp_type_value_len = cap_len - 2;
                pos += caps->content_protection.cp_type_value_len;
                
                // connection->reject_service_category = category;
                // connection->error_code = UNSUPPORTED_CONFIGURATION;
                // support for content protection goes here
                return 0;
                
            case AVDTP_HEADER_COMPRESSION:
                caps->header_compression.back_ch  = packet[pos] >> 7; 
                caps->header_compression.media    = packet[pos] >> 6;
                caps->header_compression.recovery = packet[pos] >> 5;
                pos++;
                break;
            case AVDTP_MULTIPLEXING:                
                caps->multiplexing_mode.fragmentation = packet[pos++] >> 7;
                // read [tsid, tcid] for media, reporting. recovery respectively
                caps->multiplexing_mode.transport_identifiers_num = 3;
                for (i=0; i<caps->multiplexing_mode.transport_identifiers_num; i++){
                    caps->multiplexing_mode.transport_session_identifiers[i] = packet[pos++] >> 7;
                    caps->multiplexing_mode.tcid[i] = packet[pos++] >> 7;
                }
                break;
            case AVDTP_MEDIA_CODEC:                
                caps->media_codec.media_type = packet[pos++] >> 4;
                caps->media_codec.media_codec_type = packet[pos++];
                caps->media_codec.media_codec_information_len = cap_len - 2;
                // printf_hexdump(packet+pos, caps->media_codec.media_codec_information_len);
                pos += caps->media_codec.media_codec_information_len;
                break;
            default:
                break;
        }

        registered_service_categories = store_bit16(registered_service_categories, category, 1);
        processed_cap_len = pos - processed_cap_len;
        if (cap_len <= processed_cap_len && pos < size-2){
            category = (avdtp_service_category_t)packet[pos++];
            cap_len = packet[pos++];
            printf("    ACP: avdtp_unpack_service_capabilities: category %d, capability len %d, remaining record size %d\n", category, cap_len, size-pos);
        }
    }
    // printf(" avdtp_unpack_service_capabilities done\n");
    return registered_service_categories;
}

static inline void avdtp_acceptor_prepare_capabilities_response(avdtp_signaling_packet_t * signaling_packet, uint8_t transaction_label, avdtp_sep_t sep, uint8_t identifier){
    if (signaling_packet->offset) return;
    uint8_t pack_all_capabilities = 1;
    if (identifier == AVDTP_SI_GET_CAPABILITIES){
        pack_all_capabilities = 0;
    } 
    
    signaling_packet->size = 0;
    int i = 0;
    for (i = 1; i < 9; i++){
        if (get_bit16(sep.registered_service_categories, i)){
            // service category
            signaling_packet->command[signaling_packet->size++] = i;
            signaling_packet->size += avdtp_pack_service_capabilities(signaling_packet->command+signaling_packet->size, sizeof(signaling_packet->command)-signaling_packet->size, sep.capabilities, (avdtp_service_category_t)i, pack_all_capabilities);
        }
    }
    signaling_packet->command[signaling_packet->size++] = 0x04;
    signaling_packet->command[signaling_packet->size++] = 0x02;
    signaling_packet->command[signaling_packet->size++] = 0x02;
    signaling_packet->command[signaling_packet->size++] = 0x00;
    
    // printf(" avdtp_acceptor_send_capabilities_response: \n");
    // printf_hexdump(signaling_packet->command, signaling_packet->size);
    signaling_packet->signal_identifier = identifier;
    signaling_packet->transaction_label = transaction_label;
    signaling_packet->message_type = AVDTP_RESPONSE_ACCEPT_MSG;
}

static int avdtp_acceptor_send_accept_response(uint16_t cid,  uint8_t transaction_label, avdtp_signal_identifier_t identifier){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_acceptor_process_chunk(avdtp_signaling_packet_t * signaling_packet, uint8_t * packet, uint16_t size){
    memcpy(signaling_packet->command + signaling_packet->size, packet, size);
    signaling_packet->size += size;
    return signaling_packet->packet_type == AVDTP_SINGLE_PACKET || signaling_packet->packet_type == AVDTP_END_PACKET;
}

int avdtp_acceptor_stream_config_subsm(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, uint8_t * packet, uint16_t size){
    if (!stream_endpoint) return 0;
    
    if (!avdtp_acceptor_process_chunk(&connection->signaling_packet, packet, size)) return 0;

    uint16_t packet_size = connection->signaling_packet.size;
    connection->signaling_packet.size = 0;

    int request_to_send = 1;
    switch (stream_endpoint->acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    printf("    ACP: AVDTP_SI_GET_ALL_CAPABILITIES\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_ALL_CAPABILITIES;
                    break;
                case AVDTP_SI_GET_CAPABILITIES:
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES;
                    break;
                case AVDTP_SI_SET_CONFIGURATION:{
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION \n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION;
                    connection->reject_service_category = 0;
                    stream_endpoint->remote_sep_index = 0xFF;

                    avdtp_sep_t sep;
                    sep.seid = connection->signaling_packet.command[3] >> 2;
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, connection->signaling_packet.command+4, packet_size-4);
                    sep.in_use = 1;

                    if (connection->error_code){
                        // fire capabilities parsing errors 
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }

                    printf("    ACP .. seid %d\n", sep.seid);
                    // find or add sep
                    int i;
                    for (i=0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                        }
                    }
                    if (stream_endpoint->remote_sep_index != 0xFF){
                        if (stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].in_use){
                            // reject if already configured
                            connection->error_code = SEP_IN_USE;
                            // find first registered category and fire the error
                            connection->reject_service_category = 0;
                            for (i = 1; i < 9; i++){
                                if (get_bit16(sep.registered_service_categories, i-1)){
                                    connection->reject_service_category = i;
                                    break;
                                }
                            }    
                            
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        }
                    } else {
                        // add new
                        printf("    ACP: seid %d not found in %p\n", sep.seid, stream_endpoint);
                        stream_endpoint->remote_sep_index = stream_endpoint->remote_seps_num;
                        stream_endpoint->remote_seps_num++;
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    ACP: add seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    } 
                    break;
                }
                case AVDTP_SI_RECONFIGURE:{
                    // if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_OPENED){
                    //     printf("    ACP: AVDTP_SI_RECONFIGURE, bad state %d \n", stream_endpoint->state);
                    //     stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                    //     connection->error_code = BAD_STATE;
                    //     connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                    //     break;
                    // }
            
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE;
                    connection->reject_service_category = 0;
                    stream_endpoint->remote_sep_index = 0xFF;

                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE %p in state %d (AVDTP_STREAM_ENDPOINT_OPENED %d)\n", stream_endpoint, stream_endpoint->state, AVDTP_STREAM_ENDPOINT_OPENED);
                    avdtp_sep_t sep;
                    sep.seid = packet[3] >> 2;
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, packet+4, packet_size-4);
                    
                    // find sep or raise error
                    int i;
                    for (i = 0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                        }
                    }

                    if (stream_endpoint->remote_sep_index == 0xFF){
                        printf("    ACP: AVDTP_SI_RECONFIGURE, bad state seid %d not found\n", sep.seid);
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = BAD_ACP_SEID;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }

                    // only codec, and 
                    uint16_t remaining_service_categories = sep.registered_service_categories;
                    if (get_bit16(sep.registered_service_categories, AVDTP_MEDIA_CODEC-1)){
                        remaining_service_categories = store_bit16(sep.registered_service_categories, AVDTP_MEDIA_CODEC-1, 0);
                    }
                    if (get_bit16(sep.registered_service_categories, AVDTP_CONTENT_PROTECTION-1)){
                        remaining_service_categories = store_bit16(sep.registered_service_categories, AVDTP_CONTENT_PROTECTION-1, 0);
                    }
                    if (!remaining_service_categories) break;
                     
                    // find first category that shouldn't be reconfigured
                    for (i = 1; i < 9; i++){
                        if (get_bit16(sep.registered_service_categories, i-1)){
                            connection->reject_service_category = i;
                        }
                    }    
                    break;
                }
                case AVDTP_SI_OPEN:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURED) return 0;
                    printf("    ACP: AVDTP_STREAM_ENDPOINT_W2_ANSWER_OPEN_STREAM\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    break;
                case AVDTP_SI_GET_CONFIGURATION:
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION;
                    break;
                case AVDTP_SI_START:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED) return 0;
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM;
                    break;
                case AVDTP_SI_CLOSE:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM\n");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM;
                            break;
                        default:
                            printf("    ACP: AVDTP_SI_CLOSE, bad state %d \n", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_ABORT:
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM;
                    break;
                default:
                    printf("    ACP: NOT IMPLEMENTED, Reject signal_identifier %02x\n", connection->signaling_packet.signal_identifier);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD;
                    connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                    break;
            }
            break;
        default:
            return 0;
    }

    if (!request_to_send){
        printf("    ACP: NOT IMPLEMENTED\n");
    }
    return request_to_send;
}

static int avdtp_acceptor_send_response_reject_service_category(uint16_t cid,  avdtp_signal_identifier_t identifier, uint8_t category, uint8_t error_code, uint8_t transaction_label){
    uint8_t command[4];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = category;
    command[3] = error_code;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_acceptor_send_response_reject(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

int avdtp_acceptor_send_response_reject_with_error_code(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t error_code, uint8_t transaction_label){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = error_code;
    return l2cap_send(cid, command, sizeof(command));
}


static int avdtp_acceptor_signaling_response_create_fragment(uint16_t cid, avdtp_signaling_packet_t * signaling_packet, uint8_t * out_buffer) {
    int mtu = l2cap_get_remote_mtu_for_local_cid(cid);
    // hack for test
    // int mtu = 6;
    int data_len = 0;

    uint16_t offset = signaling_packet->offset;
    uint16_t pos = 1;
    // printf(" avdtp_acceptor_signaling_response_create_fragment offset %d, packet type %d\n",  signaling_packet->offset, signaling_packet->packet_type);
    
    if (offset == 0){
        if (signaling_packet->size <= mtu - 2){
            // printf(" AVDTP_SINGLE_PACKET\n");
            signaling_packet->packet_type = AVDTP_SINGLE_PACKET;
            out_buffer[pos++] = signaling_packet->signal_identifier;
            data_len = signaling_packet->size;
        } else {
            signaling_packet->packet_type = AVDTP_START_PACKET;
            out_buffer[pos++] = (mtu + signaling_packet->size)/ (mtu-1);
            out_buffer[pos++] = signaling_packet->signal_identifier;
            data_len = mtu - 3;
            signaling_packet->offset = data_len;
            // printf(" AVDTP_START_PACKET len %d, offset %d\n", signaling_packet->size, signaling_packet->offset);
        }
    } else {
        int remaining_bytes = signaling_packet->size - offset;
        if (remaining_bytes <= mtu - 1){
            //signaling_packet->fragmentation = 1;
            signaling_packet->packet_type = AVDTP_END_PACKET;
            data_len = remaining_bytes;
            signaling_packet->offset = 0;
            // printf(" AVDTP_END_PACKET len %d, offset %d\n", signaling_packet->size, signaling_packet->offset);
        } else{
            signaling_packet->packet_type = AVDTP_CONTINUE_PACKET;
            data_len = mtu - 1;
            signaling_packet->offset += data_len;
            // printf(" AVDTP_CONTINUE_PACKET len %d, offset %d\n", signaling_packet->size, signaling_packet->offset);
        }
    }
    out_buffer[0] = avdtp_header(signaling_packet->transaction_label, signaling_packet->packet_type, signaling_packet->message_type);
    memcpy(out_buffer+pos, signaling_packet->command + offset, data_len);
    pos += data_len; 
    return pos;
}

static inline int avdtp_acceptor_send_fragmented_packet(uint16_t cid, avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, avdtp_acceptor_stream_endpoint_state_t acceptor_config_state){
    l2cap_reserve_packet_buffer();
    uint8_t * out_buffer = l2cap_get_outgoing_buffer();
    uint16_t pos = avdtp_acceptor_signaling_response_create_fragment(cid, &connection->signaling_packet, out_buffer);
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        stream_endpoint->acceptor_config_state = acceptor_config_state;
        printf("    ACP: fragmented\n");
    }
    return l2cap_send_prepared(cid, pos);
}

int avdtp_acceptor_stream_config_subsm_run(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return 0;
    uint8_t reject_service_category = connection->reject_service_category;
    avdtp_signal_identifier_t reject_signal_identifier = connection->reject_signal_identifier;
    uint8_t error_code = connection->error_code;
    uint16_t cid = stream_endpoint->connection ? stream_endpoint->connection->l2cap_signaling_cid : connection->l2cap_signaling_cid;
    uint8_t trid = stream_endpoint->connection ? stream_endpoint->connection->acceptor_transaction_label : connection->acceptor_transaction_label;

    avdtp_acceptor_stream_endpoint_state_t acceptor_config_state = stream_endpoint->acceptor_config_state;
    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
    int sent = 1;

    switch (acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES:
            avdtp_acceptor_prepare_capabilities_response(&connection->signaling_packet, trid, stream_endpoint->sep, AVDTP_SI_GET_CAPABILITIES);
            avdtp_acceptor_send_fragmented_packet(cid, connection, stream_endpoint, acceptor_config_state);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_GET_ALL_CAPABILITIES:
            avdtp_acceptor_prepare_capabilities_response(&connection->signaling_packet, trid, stream_endpoint->sep, AVDTP_SI_GET_ALL_CAPABILITIES);
            avdtp_acceptor_send_fragmented_packet(cid, connection, stream_endpoint, acceptor_config_state);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION:
            printf("    ACP: DONE\n");
            printf("    -> AVDTP_STREAM_ENDPOINT_CONFIGURED\n");
            stream_endpoint->connection = connection;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SET_CONFIGURATION);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE:
            printf("    ACP: DONE \n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_RECONFIGURE);
            break;

        case AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION:
            avdtp_acceptor_prepare_capabilities_response(&connection->signaling_packet, trid, stream_endpoint->sep, AVDTP_SI_GET_CONFIGURATION);
            avdtp_acceptor_send_fragmented_packet(cid, connection, stream_endpoint, acceptor_config_state);
            break;
        case AVDTP_ACCEPTOR_W4_L2CAP_FOR_MEDIA_CONNECTED:
            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W4_L2CAP_FOR_MEDIA_CONNECTED;
            return 0;
        case AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM:
            printf("    ACP: DONE\n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_OPEN);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM:
            printf("    ACP: DONE \n");
            printf("    -> AVDTP_STREAM_ENDPOINT_STREAMING \n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_START);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM:
            printf("    ACP: DONE\n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_CLOSE);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM:
            printf("    ACP: DONE\n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_ABORT);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD:
            printf("    ACP: REJECT\n");
            connection->reject_signal_identifier = 0;
            avdtp_acceptor_send_response_reject(cid, reject_signal_identifier, trid);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
            printf("    ACP: REJECT CATEGORY\n");
            connection->reject_service_category = 0;
            avdtp_acceptor_send_response_reject_service_category(cid, reject_signal_identifier, reject_service_category, error_code, trid);
            break;
    
        case AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
            printf("    ACP: REJECT\n");
            connection->reject_signal_identifier = 0;
            connection->error_code = 0;
            avdtp_acceptor_send_response_reject_with_error_code(cid, reject_signal_identifier, error_code, trid);
            break;
        default:  
            printf("    ACP: NOT IMPLEMENTED\n");
            return 0;
    } 
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        stream_endpoint->acceptor_config_state = acceptor_config_state;
        connection->wait_to_send_acceptor = 1;
    }
    return sent;
}

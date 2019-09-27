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

#define BTSTACK_FILE__ "avdtp_initiator.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "l2cap.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"
#include "classic/avdtp_initiator.h"

static int avdtp_initiator_send_signaling_cmd(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_initiator_send_signaling_cmd_with_seid(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label, uint8_t sep_id){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = sep_id << 2;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_initiator_send_signaling_cmd_delay_report(uint16_t cid, uint8_t transaction_label, uint8_t sep_id, uint16_t delay_ms){
    uint8_t command[5];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = AVDTP_SI_DELAYREPORT;
    command[2] = sep_id << 2;
    big_endian_store_16(command, 3, delay_ms);
    return l2cap_send(cid, command, sizeof(command));
}

void avdtp_initiator_stream_config_subsm(avdtp_connection_t * connection, uint8_t *packet, uint16_t size, int offset, avdtp_context_t * context){
    // int status = 0;
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    
    avdtp_sep_t sep;
    if (connection->initiator_connection_state == AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER) {
        connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
    } else {
        stream_endpoint = avdtp_stream_endpoint_associated_with_acp_seid(connection->remote_seid, context);
        if (!stream_endpoint){
            stream_endpoint = avdtp_stream_endpoint_with_seid(connection->local_seid, context);
        }
        if (!stream_endpoint) return;
        sep.seid = connection->remote_seid;
        
        if (stream_endpoint->initiator_config_state != AVDTP_INITIATOR_W4_ANSWER) return;
        stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
    }
    
    switch (connection->signaling_packet.message_type){
        case AVDTP_RESPONSE_ACCEPT_MSG:
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_DISCOVER:{
                    if (connection->signaling_packet.transaction_label != connection->initiator_transaction_label){
                        log_info("    unexpected transaction label, got %d, expected %d", connection->signaling_packet.transaction_label, connection->initiator_transaction_label);
                        // status = BAD_HEADER_FORMAT;
                        break;
                    }
                    
                    if (size == 3){
                        log_info("    ERROR code %02x", packet[offset]);
                        break;
                    }
                    
                    int i;
                    for (i = offset; i < size; i += 2){
                        sep.seid = packet[i] >> 2;
                        offset++;
                        if (sep.seid < 0x01 || sep.seid > 0x3E){
                            log_info("    invalid sep id");
                            // status = BAD_ACP_SEID;
                            break;
                        }
                        sep.in_use = (packet[i] >> 1) & 0x01;
                        sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                        sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                        avdtp_signaling_emit_sep(context->avdtp_callback, connection->avdtp_cid, sep);
                    }
                    avdtp_signaling_emit_sep_done(context->avdtp_callback, connection->avdtp_cid);
                    break;
                }
                
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, packet+offset, size-offset);
                    avdtp_emit_capabilities(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, connection->remote_seid, &sep.capabilities, sep.registered_service_categories);
                    break;
                case AVDTP_SI_DELAYREPORT:
                    avdtp_signaling_emit_delay(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, little_endian_read_16(packet, offset));
                    break;
                case AVDTP_SI_GET_CONFIGURATION:
                    // sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, packet+offset, size-offset);
                    // if (get_bit16(sep.configured_service_categories, AVDTP_MEDIA_CODEC)){
                    //     switch (sep.configuration.media_codec.media_codec_type){
                    //         case AVDTP_CODEC_SBC: 
                    //             avdtp_signaling_emit_media_codec_sbc_configuration(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, connection->remote_seid, 
                    //                 sep.configuration.media_codec.media_type, sep.configuration.media_codec.media_codec_information);
                    //             break;
                    //         default:
                    //             avdtp_signaling_emit_media_codec_other_configuration(context->avdtp_callback, connection->avdtp_cid, connection->local_seid,  connection->remote_seid, sep.configuration.media_codec);
                    //             break;
                    //     }
                    // }
                    break;
                
                case AVDTP_SI_RECONFIGURE:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_RECONFIGURE: stream endpoint is null");
                        break;
                    }
                    // copy sbc media codec info
                    stream_endpoint->remote_sep.configured_service_categories |= stream_endpoint->remote_configuration_bitmap;
                    stream_endpoint->remote_sep.configuration = stream_endpoint->remote_configuration;
                    memcpy(stream_endpoint->media_codec_sbc_info, stream_endpoint->remote_configuration.media_codec.media_codec_information, 4);
                    stream_endpoint->remote_sep.configuration.media_codec.media_codec_information = stream_endpoint->media_codec_sbc_info; 
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    break;

                case AVDTP_SI_SET_CONFIGURATION:{
                    avdtp_configuration_timer_stop(connection);
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_SET_CONFIGURATION: stream endpoint is null");
                        break;
                    }
                    sep.configured_service_categories = stream_endpoint->remote_configuration_bitmap;
                    sep.configuration = stream_endpoint->remote_configuration;
                    sep.in_use = 1;
                    
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
                    stream_endpoint->remote_sep = sep;
                    stream_endpoint->connection = connection;
                
                    log_info("INT: configured remote seid %d, to %p", stream_endpoint->remote_sep.seid, stream_endpoint);
                   
                    switch (stream_endpoint->media_codec_type){
                        case AVDTP_CODEC_SBC: 
                            avdtp_signaling_emit_media_codec_sbc_configuration(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, connection->remote_seid, 
                                stream_endpoint->media_type, stream_endpoint->media_codec_sbc_info);
                            break;
                        default:
                            // TODO: we don\t have codec info to emit config
                            avdtp_signaling_emit_media_codec_other_configuration(context->avdtp_callback, connection->avdtp_cid, connection->local_seid,  connection->remote_seid, sep.configuration.media_codec);
                            break;
                    }
                    break;
                }
                
                case AVDTP_SI_OPEN:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_OPEN: stream endpoint is null");
                        break;
                    }
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM) {
                        log_error("AVDTP_SI_OPEN in wrong stream endpoint state");
                        return;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    connection->local_seid = stream_endpoint->sep.seid;
                    l2cap_create_channel(context->packet_handler, connection->remote_addr, BLUETOOTH_PSM_AVDTP, 0xffff, NULL);
                    return;
                case AVDTP_SI_START:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_START: stream endpoint is null");
                        break;
                    }
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED) {
                        log_error("AVDTP_SI_START in wrong stream endpoint state");
                        return;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
                    break;
                case AVDTP_SI_SUSPEND:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_SUSPEND: stream endpoint is null");
                        break;
                    }
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_STREAMING) {
                        log_error("AVDTP_SI_SUSPEND in wrong stream endpoint state");
                        return;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    break;
                case AVDTP_SI_CLOSE:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_CLOSE: stream endpoint is null");
                        break;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                    break;
                case AVDTP_SI_ABORT:
                    if (!stream_endpoint){
                        log_error("AVDTP_SI_ABORT: stream endpoint is null");
                        break;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                    break;
                default:
                    log_info("    AVDTP_RESPONSE_ACCEPT_MSG, signal %d not implemented", connection->signaling_packet.signal_identifier);
                    break;
            }
            avdtp_signaling_emit_accept(context->avdtp_callback, connection->avdtp_cid, 0, connection->signaling_packet.signal_identifier);
            connection->initiator_transaction_label++;
            break;
        case AVDTP_RESPONSE_REJECT_MSG:
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_SET_CONFIGURATION:
                    connection->is_initiator = 0;
                    log_info("Received reject for set configuration, role changed from initiator to acceptor. Start timer.");
                    avdtp_configuration_timer_start(connection);
                    break;
                default:
                    break;
            }
            log_info("    AVDTP_RESPONSE_REJECT_MSG signal %d", connection->signaling_packet.signal_identifier);
            avdtp_signaling_emit_reject(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, connection->signaling_packet.signal_identifier);
            return;
        case AVDTP_GENERAL_REJECT_MSG:
            log_info("    AVDTP_GENERAL_REJECT_MSG signal %d", connection->signaling_packet.signal_identifier);
            avdtp_signaling_emit_general_reject(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, connection->signaling_packet.signal_identifier);
            return;
        default:
            break;
    }
}

void avdtp_initiator_stream_config_subsm_run(avdtp_connection_t * connection, avdtp_context_t * context){
    int sent = 1;
    switch (connection->initiator_connection_state){
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS:
            log_info("INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd(connection->l2cap_signaling_cid, AVDTP_SI_DISCOVER, connection->initiator_transaction_label);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES:  
            log_info("INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CAPABILITIES, connection->initiator_transaction_label, connection->remote_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES:
            log_info("INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_ALL_CAPABILITIES, connection->initiator_transaction_label, connection->remote_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION:
            log_info("INT: AVDTP_INITIATOR_W4_GET_CONFIGURATION");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CONFIGURATION, connection->initiator_transaction_label, connection->remote_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_DELAY_REPORT:
            log_info("INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_DELAY_REPORT");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_delay_report(connection->l2cap_signaling_cid, connection->initiator_transaction_label, 
                connection->remote_seid, connection->delay_ms);
            break;
        default:
            sent = 0;
            break;
    }
    
    if (sent) return;
    sent = 1;
    
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    
    stream_endpoint = avdtp_stream_endpoint_associated_with_acp_seid(connection->remote_seid, context);
    if (!stream_endpoint){
        stream_endpoint = avdtp_stream_endpoint_with_seid(connection->local_seid, context);
    }
    if (!stream_endpoint) return;
    
    avdtp_initiator_stream_endpoint_state_t stream_endpoint_state = stream_endpoint->initiator_config_state;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
    
    if (stream_endpoint->start_stream){
        stream_endpoint->start_stream = 0;
        if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_OPENED){
            connection->local_seid = stream_endpoint->sep.seid;
            connection->remote_seid = stream_endpoint->remote_sep.seid;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_START, connection->initiator_transaction_label++, connection->remote_seid);
            return;            
        } 
        return;
    }
    
    if (stream_endpoint->stop_stream){
        stream_endpoint->stop_stream = 0;
        if (stream_endpoint->state >= AVDTP_STREAM_ENDPOINT_OPENED){
            connection->local_seid = stream_endpoint->sep.seid;
            connection->remote_seid = stream_endpoint->remote_sep.seid;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_CLOSE, connection->initiator_transaction_label++, connection->remote_seid);
            return;            
        }
    }
    
    if (stream_endpoint->abort_stream){
        stream_endpoint->abort_stream = 0;
        switch (stream_endpoint->state){
            case AVDTP_STREAM_ENDPOINT_CONFIGURED:
            case AVDTP_STREAM_ENDPOINT_CLOSING:
            case AVDTP_STREAM_ENDPOINT_OPENED:
            case AVDTP_STREAM_ENDPOINT_STREAMING:
                connection->local_seid = stream_endpoint->sep.seid;
                connection->remote_seid = stream_endpoint->remote_sep.seid;
                stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_ABORT, connection->initiator_transaction_label++, connection->remote_seid);
                return;
            default:
                break;
        }
    }
    
    if (stream_endpoint->suspend_stream){
        stream_endpoint->suspend_stream = 0;
        if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING){
            connection->local_seid = stream_endpoint->sep.seid;
            connection->remote_seid = stream_endpoint->remote_sep.seid;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_SUSPEND, connection->initiator_transaction_label, connection->remote_seid);
            return;
        }
    }
    
    if (stream_endpoint->send_stream){
        stream_endpoint->send_stream = 0;
        if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING){
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
            avdtp_streaming_emit_can_send_media_packet_now(context->avdtp_callback, stream_endpoint->l2cap_media_cid, stream_endpoint->sep.seid, stream_endpoint->sequence_number);
            return;
        }
    }

    switch (stream_endpoint_state){
        case AVDTP_INITIATOR_W2_SET_CONFIGURATION:
        case AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID:{
            if (stream_endpoint_state == AVDTP_INITIATOR_W2_SET_CONFIGURATION && !connection->is_initiator){
                log_info("initiator SM stop sending SET_CONFIGURATION cmd: current role is acceptor");
                connection->is_configuration_initiated_locally = 0;
                break;
            }
            log_info("initiator SM prepare SET_CONFIGURATION cmd");
            connection->is_configuration_initiated_locally = 1;
            log_info("INT: AVDTP_INITIATOR_W2_(RE)CONFIGURATION bitmap, int seid %d, acp seid %d", connection->local_seid, connection->remote_seid);
            // log_info_hexdump(  connection->remote_capabilities.media_codec.media_codec_information,  connection->remote_capabilities.media_codec.media_codec_information_len);
            connection->signaling_packet.acp_seid = connection->remote_seid;
            connection->signaling_packet.int_seid = connection->local_seid;
            
            connection->signaling_packet.signal_identifier = AVDTP_SI_SET_CONFIGURATION;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE;
            if (stream_endpoint_state == AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID){
                connection->signaling_packet.signal_identifier = AVDTP_SI_RECONFIGURE;
            }
            
            avdtp_prepare_capabilities(&connection->signaling_packet, connection->initiator_transaction_label, stream_endpoint->remote_configuration_bitmap, stream_endpoint->remote_configuration, connection->signaling_packet.signal_identifier);
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->initiator_config_state = AVDTP_INITIATOR_FRAGMENTATED_COMMAND;
                log_info("INT: fragmented");
            }
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
            break;
        }
        case AVDTP_INITIATOR_FRAGMENTATED_COMMAND:{
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->initiator_config_state = AVDTP_INITIATOR_FRAGMENTATED_COMMAND;
                log_info("INT: fragmented");
            }
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
            break;
        }
        case AVDTP_INITIATOR_W2_OPEN_STREAM:
            switch (stream_endpoint->state){
                case AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM:
                    log_info("INT: AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM");
                    avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_OPEN, connection->initiator_transaction_label, connection->remote_seid);
                    break;
                default:
                    sent = 0;
                    break;
            }
            break;
        default:
            sent = 0;
            break;
    }

    // check fragmentation
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    }
}

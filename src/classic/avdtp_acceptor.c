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
#define BTSTACK_FILE__ "avdtp_acceptor.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"
#include "classic/avdtp_acceptor.h"


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

static int avdtp_acceptor_validate_msg_length(avdtp_signal_identifier_t signal_identifier, uint16_t msg_size){
    int minimal_msg_lenght = 2;
    switch (signal_identifier){
        case AVDTP_SI_GET_CAPABILITIES:
        case AVDTP_SI_GET_ALL_CAPABILITIES:
        case AVDTP_SI_SET_CONFIGURATION:
        case AVDTP_SI_GET_CONFIGURATION:
        case AVDTP_SI_START:
        case AVDTP_SI_CLOSE:
        case AVDTP_SI_ABORT:
        case AVDTP_SI_RECONFIGURE:
        case AVDTP_SI_OPEN:
            minimal_msg_lenght = 3;
            break;
        default:
            break;
        }
    return msg_size >= minimal_msg_lenght;
}

void avdtp_acceptor_stream_config_subsm(avdtp_connection_t * connection, uint8_t * packet, uint16_t size, int offset, avdtp_context_t * context){
    avdtp_stream_endpoint_t * stream_endpoint;
    connection->acceptor_transaction_label = connection->signaling_packet.transaction_label;
    if (!avdtp_acceptor_validate_msg_length(connection->signaling_packet.signal_identifier, size)) {
        connection->error_code = BAD_LENGTH;
        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
        avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
        return;
    }
    
    // handle error cases
    switch (connection->signaling_packet.signal_identifier){
        case AVDTP_SI_DISCOVER:
            if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
            log_info("ACP: AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS");
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS;
            avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
            return;
        case AVDTP_SI_GET_CAPABILITIES:
        case AVDTP_SI_GET_ALL_CAPABILITIES:
        case AVDTP_SI_SET_CONFIGURATION:
        case AVDTP_SI_GET_CONFIGURATION:
        case AVDTP_SI_START:
        case AVDTP_SI_CLOSE:
        case AVDTP_SI_ABORT:
        case AVDTP_SI_OPEN:
        case AVDTP_SI_RECONFIGURE:
        case AVDTP_SI_DELAYREPORT:
            connection->local_seid  = packet[offset++] >> 2;
            stream_endpoint = avdtp_stream_endpoint_with_seid(connection->local_seid, context);
            if (!stream_endpoint){
                log_info("ACP: cmd %d - RESPONSE REJECT", connection->signaling_packet.signal_identifier);
                connection->error_code = BAD_ACP_SEID;
                if (connection->signaling_packet.signal_identifier == AVDTP_SI_OPEN){
                    connection->error_code = BAD_STATE;
                }
                
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                if (connection->signaling_packet.signal_identifier == AVDTP_SI_RECONFIGURE){
                    connection->reject_service_category = connection->local_seid;
                    connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                }
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
                return;
            }
            break;
        
        case AVDTP_SI_SUSPEND:{
            int i;
            log_info("ACP: AVDTP_SI_SUSPEND seids: ");
            connection->num_suspended_seids = 0;

            for (i = offset; i < size; i++){
                connection->suspended_seids[connection->num_suspended_seids] = packet[i] >> 2;
                offset++;
                log_info("%d, ", connection->suspended_seids[connection->num_suspended_seids]);
                connection->num_suspended_seids++;
            }
            
            if (connection->num_suspended_seids == 0) {
                log_info("ACP: CATEGORY RESPONSE REJECT BAD_ACP_SEID");
                connection->error_code = BAD_ACP_SEID;
                connection->reject_service_category = connection->local_seid;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
                return;
            }
            // deal with first susspended seid 
            connection->local_seid = connection->suspended_seids[0];
            stream_endpoint = avdtp_stream_endpoint_with_seid(connection->local_seid, context);
            if (!stream_endpoint){
                log_info("ACP: stream_endpoint not found, CATEGORY RESPONSE REJECT BAD_ACP_SEID");
                connection->error_code = BAD_ACP_SEID;
                connection->reject_service_category = connection->local_seid;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                connection->num_suspended_seids = 0;
                avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
                return;
            }
            break;
        }
        default:
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE;
            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
            log_info("AVDTP_CMD_MSG signal %d not implemented, general reject", connection->signaling_packet.signal_identifier);
            avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
            return;
    }

    if (!stream_endpoint) {
        return;
    }

    if (!avdtp_acceptor_process_chunk(&connection->signaling_packet, packet, size)) return;
    
    uint16_t packet_size = connection->signaling_packet.size;
    connection->signaling_packet.size = 0;
    
    int request_to_send = 1;
    switch (stream_endpoint->acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_DELAYREPORT:
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_DELAY_REPORT");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_DELAY_REPORT;
                    avdtp_signaling_emit_delay(context->avdtp_callback, connection->avdtp_cid, connection->local_seid, big_endian_read_16(packet, offset));
                    break;
                
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    log_info("ACP: AVDTP_SI_GET_ALL_CAPABILITIES");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_ALL_CAPABILITIES;
                    break;
                case AVDTP_SI_GET_CAPABILITIES:
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES;
                    break;
                case AVDTP_SI_SET_CONFIGURATION:{
                    // log_info("acceptor SM received SET_CONFIGURATION cmd: role is_initiator %d", connection->is_initiator);
                    if (connection->is_initiator){
                        if (connection->is_configuration_initiated_locally){
                            log_info("ACP: Set configuration already initiated locally, reject cmd ");
                            // fire configuration parsing errors 
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD;
                            connection->is_initiator = 1;
                            break;
                        }
                        connection->is_initiator = 0;   
                        log_info("acceptor SM received SET_CONFIGURATION cmd: change role to acceptor, is_initiator %d", connection->is_initiator);
                    }
                    
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION connection %p", connection);
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE;
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION;
                    connection->reject_service_category = 0;
                    stream_endpoint->connection = connection;
                    avdtp_sep_t sep;
                    sep.seid = connection->signaling_packet.command[offset++] >> 2;
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, connection->signaling_packet.command+offset, packet_size-offset);
                    sep.in_use = 1;

                    if (connection->error_code){
                        log_info("fire configuration parsing errors ");
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }
                    // find or add sep

                    log_info("ACP .. seid %d, remote sep seid %d", sep.seid, stream_endpoint->remote_sep.seid);
                    
                    if (is_avdtp_remote_seid_registered(stream_endpoint)){
                        if (stream_endpoint->remote_sep.in_use){
                            log_info("reject as it is already in use");
                            connection->error_code = SEP_IN_USE;
                            // find first registered category and fire the error
                            connection->reject_service_category = 0;
                            int i;
                            for (i = 1; i < 9; i++){
                                if (get_bit16(sep.configured_service_categories, i)){
                                    connection->reject_service_category = i;
                                    break;
                                }
                            }    
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        } else {
                            stream_endpoint->remote_sep = sep;
                            log_info("ACP: update seid %d, to %p", stream_endpoint->remote_sep.seid, stream_endpoint);
                        }
                    } else {
                        // add new
                        log_info("ACP: seid %d not found in %p", sep.seid, stream_endpoint);
                        stream_endpoint->remote_sep = sep;
                        log_info("ACP: add seid %d, to %p", stream_endpoint->remote_sep.seid, stream_endpoint);
                    } 
                    
                    avdtp_emit_configuration(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), &sep.configuration, sep.configured_service_categories);
                    avdtp_signaling_emit_accept(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), connection->signaling_packet.signal_identifier);
                    break;
                }
                case AVDTP_SI_RECONFIGURE:{
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE;
                    connection->reject_service_category = 0;

                    avdtp_sep_t sep;
                    sep.seid = connection->local_seid;
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE seid %d", sep.seid);
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, connection->signaling_packet.command+offset, packet_size-offset);
                    if (connection->error_code){
                        // fire configuration parsing errors 
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }

                    // find sep or raise error
                    if (!is_avdtp_remote_seid_registered(stream_endpoint)){
                        log_info("ACP: REJECT AVDTP_SI_RECONFIGURE, BAD_ACP_SEID");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = BAD_ACP_SEID;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
                    stream_endpoint->remote_sep = sep;
                    log_info("ACP: update seid %d, to %p", stream_endpoint->remote_sep.seid, stream_endpoint);

                    avdtp_emit_configuration(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), &sep.configuration, sep.configured_service_categories);
                    avdtp_signaling_emit_accept(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), connection->signaling_packet.signal_identifier);
                    break;
                }

                case AVDTP_SI_GET_CONFIGURATION:
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION;
                    break;
                case AVDTP_SI_OPEN:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURED){
                        log_info("ACP: REJECT AVDTP_SI_OPEN, BAD_STATE");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->error_code = BAD_STATE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
                    log_info("ACP: AVDTP_STREAM_ENDPOINT_W2_ANSWER_OPEN_STREAM");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    connection->local_seid = stream_endpoint->sep.seid;
                    break;
                case AVDTP_SI_START:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED){
                        log_info("ACP: REJECT AVDTP_SI_START, BAD_STATE, state %d", stream_endpoint->state);
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = BAD_STATE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
                    log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM;
                    break;
                case AVDTP_SI_CLOSE:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM;
                            break;
                        default:
                            log_info("ACP: AVDTP_SI_CLOSE, bad state %d ", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_ABORT:
                     switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_CONFIGURED:
                        case AVDTP_STREAM_ENDPOINT_CLOSING:
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM;
                            break;
                        default:
                            log_info("ACP: AVDTP_SI_ABORT, bad state %d ", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_SUSPEND:
                    log_info(" entering AVDTP_SI_SUSPEND");
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                            connection->num_suspended_seids--;
                            if (connection->num_suspended_seids <= 0){
                                log_info("ACP: AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM");
                                stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM;
                            }
                            break;
                        default:
                            log_info("ACP: AVDTP_SI_SUSPEND, bad state ");
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }

                    //stream_endpoint->state = AVDTP_STREAM_ENDPOINT_SUSPENDING;
                    //stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_SUSPEND_STREAM;
                    break;
                default:
                    log_info("ACP: NOT IMPLEMENTED, Reject signal_identifier %02x", connection->signaling_packet.signal_identifier);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD;
                    connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                    break;
            }
            break;
        default:
            return;
    }

    if (!request_to_send){
        log_info("ACP: NOT IMPLEMENTED");
    }
    avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
}

static int avdtp_acceptor_send_seps_response(uint16_t cid, uint8_t transaction_label, avdtp_stream_endpoint_t * endpoints){
    uint8_t command[2+2*AVDTP_MAX_NUM_SEPS];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_SI_DISCOVER;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        command[pos++] = (stream_endpoint->sep.seid << 2) | (stream_endpoint->sep.in_use<<1);
        command[pos++] = (stream_endpoint->sep.media_type << 4) | (stream_endpoint->sep.type << 3);
    }
    return l2cap_send(cid, command, pos);
}

static int avdtp_acceptor_send_response_reject_service_category(uint16_t cid,  avdtp_signal_identifier_t identifier, uint8_t category, uint8_t error_code, uint8_t transaction_label){
    uint8_t command[4];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = category;
    command[3] = error_code;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_acceptor_send_response_general_reject(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_GENERAL_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_acceptor_send_response_reject(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_acceptor_send_response_reject_with_error_code(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t error_code, uint8_t transaction_label){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = error_code;
    return l2cap_send(cid, command, sizeof(command));
}

void avdtp_acceptor_stream_config_subsm_run(avdtp_connection_t * connection, avdtp_context_t * context){
    int sent = 1;
    btstack_linked_list_t * stream_endpoints = &context->stream_endpoints;

    switch (connection->acceptor_connection_state){
        case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS:
            connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
            avdtp_acceptor_send_seps_response(connection->l2cap_signaling_cid, connection->acceptor_transaction_label, (avdtp_stream_endpoint_t *) stream_endpoints);
            break;
        case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
            avdtp_acceptor_send_response_reject_with_error_code(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->error_code, connection->acceptor_transaction_label);
            break;
        case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
            avdtp_acceptor_send_response_reject_service_category(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->reject_service_category, connection->error_code, connection->acceptor_transaction_label);
            break;
        case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE:
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
            avdtp_acceptor_send_response_general_reject(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->acceptor_transaction_label);
            break;
        default:
            sent = 0;
            break;
    }
    if (sent){
        log_info("ACP: DONE");
        return;      
    } 
    
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(connection->local_seid, context);
    if (!stream_endpoint) return;

    uint8_t reject_service_category = connection->reject_service_category;
    avdtp_signal_identifier_t reject_signal_identifier = connection->reject_signal_identifier;
    uint8_t error_code = connection->error_code;
    uint16_t cid = stream_endpoint->connection ? stream_endpoint->connection->l2cap_signaling_cid : connection->l2cap_signaling_cid;
    uint8_t trid = stream_endpoint->connection ? stream_endpoint->connection->acceptor_transaction_label : connection->acceptor_transaction_label;

    avdtp_acceptor_stream_endpoint_state_t acceptor_config_state = stream_endpoint->acceptor_config_state;
    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
    uint8_t * out_buffer;
    uint16_t pos;

    switch (acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_GET_CAPABILITIES:
            avdtp_prepare_capabilities(&connection->signaling_packet, trid, stream_endpoint->sep.registered_service_categories, stream_endpoint->sep.capabilities, AVDTP_SI_GET_CAPABILITIES);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("ACP: fragmented");
            } else {
                log_info("ACP:DONE");
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_DELAY_REPORT:
            log_info("ACP: DONE ");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_DELAYREPORT);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_GET_ALL_CAPABILITIES:
            avdtp_prepare_capabilities(&connection->signaling_packet, trid, stream_endpoint->sep.registered_service_categories, stream_endpoint->sep.capabilities, AVDTP_SI_GET_ALL_CAPABILITIES);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("ACP: fragmented");
            } else {
                log_info("ACP:DONE");
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION:
            log_info("ACP: DONE");
            log_info("    -> AVDTP_STREAM_ENDPOINT_CONFIGURED");
            stream_endpoint->connection = connection;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
            // TODO: consider reconfiguration
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SET_CONFIGURATION);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE:
            log_info("ACP: DONE ");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_RECONFIGURE);
            break;

        case AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION:{
            avdtp_sep_t sep = stream_endpoint->remote_sep;
            avdtp_prepare_capabilities(&connection->signaling_packet, trid, sep.configured_service_categories, sep.configuration, AVDTP_SI_GET_CONFIGURATION);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("ACP: fragmented");
            } else {
                log_info("ACP:DONE");
            }
            l2cap_send_prepared(cid, pos);
            break;
        }
        case AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM:
            log_info("ACP: DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_OPEN);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM:
            log_info("ACP: DONE ");
            log_info("    -> AVDTP_STREAM_ENDPOINT_STREAMING ");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_START);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_CLOSE_STREAM:
            log_info("ACP: DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_CLOSE);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM:
            log_info("ACP: DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_ABORT);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM:
            log_info("ACP: DONE");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SUSPEND);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD:
            log_info("ACP: DONE REJECT");
            connection->reject_signal_identifier = AVDTP_SI_NONE;
            avdtp_acceptor_send_response_reject(cid, reject_signal_identifier, trid);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
            log_info("ACP: DONE REJECT CATEGORY");
            connection->reject_service_category = 0;
            avdtp_acceptor_send_response_reject_service_category(cid, reject_signal_identifier, reject_service_category, error_code, trid);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
            log_info("ACP: DONE REJECT");
            connection->reject_signal_identifier = AVDTP_SI_NONE;
            connection->error_code = 0;
            avdtp_acceptor_send_response_reject_with_error_code(cid, reject_signal_identifier, error_code, trid);
            break;
        default:  
            log_info("ACP: NOT IMPLEMENTED");
            sent = 0;
            break;
    } 
    avdtp_signaling_emit_accept(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), connection->signaling_packet.signal_identifier);
    // check fragmentation
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
    }
}

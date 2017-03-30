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

#define __BTSTACK_FILE__ "avdtp_acceptor.c"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_acceptor.h"


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
    
    switch (connection->signaling_packet.signal_identifier){
        case AVDTP_SI_DISCOVER:
            if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
            printf("    ACP: AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS\n");
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
            connection->query_seid  = packet[offset++] >> 2;
            stream_endpoint = avdtp_stream_endpoint_with_seid(connection->query_seid, context);
            if (!stream_endpoint){
                printf("    ACP: cmd %d - RESPONSE REJECT\n", connection->signaling_packet.signal_identifier);
                connection->error_code = BAD_ACP_SEID;
                if (connection->signaling_packet.signal_identifier == AVDTP_SI_OPEN){
                    connection->error_code = BAD_STATE;
                }
                
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                if (connection->signaling_packet.signal_identifier == AVDTP_SI_RECONFIGURE){
                    connection->reject_service_category = connection->query_seid;
                    connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                }
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
                return;
            }
            break;
        
        case AVDTP_SI_SUSPEND:{
            int i;
            printf("    ACP: AVDTP_SI_SUSPEND seids: ");
            connection->num_suspended_seids = 0;

            for (i = offset; i < size; i++){
                connection->suspended_seids[connection->num_suspended_seids] = packet[i] >> 2;
                offset++;
                printf("%d, \n", connection->suspended_seids[connection->num_suspended_seids]);
                connection->num_suspended_seids++;
            }
            
            if (connection->num_suspended_seids == 0) {
                printf("    ACP: CATEGORY RESPONSE REJECT BAD_ACP_SEID\n");
                connection->error_code = BAD_ACP_SEID;
                connection->reject_service_category = connection->query_seid;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
                return;
            }
            // deal with first susspended seid 
            connection->query_seid = connection->suspended_seids[0];
            stream_endpoint = avdtp_stream_endpoint_with_seid(connection->query_seid, context);
            if (!stream_endpoint){
                printf("    ACP: stream_endpoint not found, CATEGORY RESPONSE REJECT BAD_ACP_SEID\n");
                connection->error_code = BAD_ACP_SEID;
                connection->reject_service_category = connection->query_seid;
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
            printf("AVDTP_CMD_MSG signal %d not implemented, general reject\n", connection->signaling_packet.signal_identifier);
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
                    
                    avdtp_sep_t sep;
                    sep.seid = connection->signaling_packet.command[offset++] >> 2;
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, connection->signaling_packet.command+offset, packet_size-offset);
                    sep.in_use = 1;

                    if (connection->error_code){
                        printf("fire configuration parsing errors \n");
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }
                    // find or add sep
                    int i;
                    stream_endpoint->remote_sep_index = 0xFF;
                    for (i=0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                        }
                    }
                    printf("    ACP .. seid %d, index %d\n", sep.seid, stream_endpoint->remote_sep_index);
                    
                    if (stream_endpoint->remote_sep_index != 0xFF){
                        if (stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].in_use){
                            // reject if already configured
                            connection->error_code = SEP_IN_USE;
                            // find first registered category and fire the error
                            connection->reject_service_category = 0;
                            for (i = 1; i < 9; i++){
                                if (get_bit16(sep.configured_service_categories, i)){
                                    connection->reject_service_category = i;
                                    break;
                                }
                            }    
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        } else {
                            stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                            printf("    ACP: update seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                        }
                    } else {
                        // add new
                        printf("    ACP: seid %d not found in %p\n", sep.seid, stream_endpoint);
                        stream_endpoint->remote_sep_index = stream_endpoint->remote_seps_num;
                        stream_endpoint->remote_seps_num++;
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    ACP: add seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    } 
                    
                    if (get_bit16(sep.configured_service_categories, AVDTP_MEDIA_CODEC)){
                        switch (sep.configuration.media_codec.media_codec_type){
                            case AVDTP_CODEC_SBC: 
                                avdtp_signaling_emit_media_codec_sbc_configuration(context->avdtp_callback, connection->con_handle, sep.configuration.media_codec);
                                break;
                            default:
                                avdtp_signaling_emit_media_codec_other_configuration(context->avdtp_callback, connection->con_handle, sep.configuration.media_codec);
                                break;
                        }
                    }
                    avdtp_signaling_emit_accept(context->avdtp_callback, connection->con_handle, connection->signaling_packet.signal_identifier, 0);
                    break;
                }
                case AVDTP_SI_RECONFIGURE:{
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE;
                    connection->reject_service_category = 0;
                    
                    avdtp_sep_t sep;
                    sep.seid = connection->query_seid;
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE seid %d\n", sep.seid);
                    // printf_hexdump(connection->signaling_packet.command, packet_size);

                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, connection->signaling_packet.command+offset, packet_size-offset);
                    
                    if (connection->error_code){
                        // fire configuration parsing errors 
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }

                    // find sep or raise error
                    int i;
                    stream_endpoint->remote_sep_index = 0xFF;
                    for (i = 0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                        }
                    }

                    if (stream_endpoint->remote_sep_index == 0xFF){
                        printf("    ACP: REJECT AVDTP_SI_RECONFIGURE, BAD_ACP_SEID\n");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = BAD_ACP_SEID;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
                    stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                    printf("    ACP: update seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);

                    if (get_bit16(sep.configured_service_categories, AVDTP_MEDIA_CODEC)){
                        switch (sep.capabilities.media_codec.media_codec_type){
                            case AVDTP_CODEC_SBC: 
                                avdtp_signaling_emit_media_codec_sbc_reconfiguration(context->avdtp_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                            default:
                                avdtp_signaling_emit_media_codec_other_reconfiguration(context->avdtp_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                        }
                    }
                    avdtp_signaling_emit_accept(context->avdtp_callback, connection->con_handle, connection->signaling_packet.signal_identifier, 0);
                    break;
                }

                case AVDTP_SI_GET_CONFIGURATION:
                    printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION;
                    break;
                case AVDTP_SI_OPEN:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURED){
                        printf("    ACP: REJECT AVDTP_SI_OPEN, BAD_STATE\n");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->error_code = BAD_STATE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
                    printf("    ACP: AVDTP_STREAM_ENDPOINT_W2_ANSWER_OPEN_STREAM\n");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    connection->query_seid = stream_endpoint->sep.seid;
                    break;
                case AVDTP_SI_START:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED){
                        printf("    ACP: REJECT AVDTP_SI_START, BAD_STATE\n");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = BAD_STATE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        break;
                    }
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
                     switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_CONFIGURED:
                        case AVDTP_STREAM_ENDPOINT_CLOSING:
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM\n");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_ABORT_STREAM;
                            break;
                        default:
                            printf("    ACP: AVDTP_SI_ABORT, bad state %d \n", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_SUSPEND:
                    printf(" entering AVDTP_SI_SUSPEND\n");
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                            connection->num_suspended_seids--;
                            if (connection->num_suspended_seids <= 0){
                                printf("    ACP: AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM\n");
                                stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM;
                            }
                            break;
                        default:
                            printf("    ACP: AVDTP_SI_SUSPEND, bad state \n");
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            connection->error_code = BAD_STATE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            break;
                    }

                    //stream_endpoint->state = AVDTP_STREAM_ENDPOINT_SUSPENDING;
                    //stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_SUSPEND_STREAM;
                    break;
                default:
                    printf("    ACP: NOT IMPLEMENTED, Reject signal_identifier %02x\n", connection->signaling_packet.signal_identifier);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD;
                    connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                    break;
            }
            break;
        default:
            return;
    }

    if (!request_to_send){
        printf("    ACP: NOT IMPLEMENTED\n");
    }
    avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
}

static int avdtp_acceptor_send_seps_response(uint16_t cid, uint8_t transaction_label, avdtp_stream_endpoint_t * endpoints){
    uint8_t command[2+2*MAX_NUM_SEPS];
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
        default:
            sent = 0;
            break;
    }
    if (sent){
        printf("    ACP: DONE\n");
        return;      
    } 
    
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(connection->query_seid, context);
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

    int status = 0;
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
                printf("    ACP: fragmented\n");
            } else {
                printf("    ACP:DONE\n");
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_GET_ALL_CAPABILITIES:
            avdtp_prepare_capabilities(&connection->signaling_packet, trid, stream_endpoint->sep.registered_service_categories, stream_endpoint->sep.capabilities, AVDTP_SI_GET_ALL_CAPABILITIES);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                printf("    ACP: fragmented\n");
            } else {
                printf("    ACP:DONE\n");
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_SET_CONFIGURATION:
            printf("    ACP: DONE\n");
            printf("    -> AVDTP_STREAM_ENDPOINT_CONFIGURED\n");
            stream_endpoint->connection = connection;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
            // TODO: use actual config
            // TODO: consider reconfiguration
            btstack_sbc_encoder_init(&stream_endpoint->sbc_encoder_state, SBC_MODE_STANDARD, 16, 8, 2, 44100, 53);

            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SET_CONFIGURATION);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_RECONFIGURE:
            printf("    ACP: DONE \n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_RECONFIGURE);
            break;

        case AVDTP_ACCEPTOR_W2_ANSWER_GET_CONFIGURATION:{
            avdtp_sep_t sep = stream_endpoint->remote_seps[stream_endpoint->remote_sep_index];
            avdtp_prepare_capabilities(&connection->signaling_packet, trid, sep.configured_service_categories, sep.configuration, AVDTP_SI_GET_CONFIGURATION);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                printf("    ACP: fragmented\n");
            } else {
                printf("    ACP:DONE\n");
            }
            l2cap_send_prepared(cid, pos);
            break;
        }
        case AVDTP_ACCEPTOR_W2_ANSWER_OPEN_STREAM:
            printf("    ACP: DONE\n");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_OPEN);
            break;
        case AVDTP_ACCEPTOR_W2_ANSWER_START_STREAM:
            printf("    ACP: DONE \n");
            printf("    -> AVDTP_STREAM_ENDPOINT_STREAMING \n");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
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
        case AVDTP_ACCEPTOR_W2_ANSWER_SUSPEND_STREAM:
            printf("    ACP: DONE\n");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SUSPEND);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD:
            status = 1;
            printf("    ACP: DONE REJECT\n");
            connection->reject_signal_identifier = 0;
            avdtp_acceptor_send_response_reject(cid, reject_signal_identifier, trid);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
            status = 1;
            printf("    ACP: DONE REJECT CATEGORY\n");
            connection->reject_service_category = 0;
            avdtp_acceptor_send_response_reject_service_category(cid, reject_signal_identifier, reject_service_category, error_code, trid);
            break;
    
        case AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
            status = 1;
            printf("    ACP: DONE REJECT\n");
            connection->reject_signal_identifier = 0;
            connection->error_code = 0;
            avdtp_acceptor_send_response_reject_with_error_code(cid, reject_signal_identifier, error_code, trid);
            break;
        default:  
            status = 0;
            printf("    ACP: NOT IMPLEMENTED\n");
            sent = 0;
    } 
    avdtp_signaling_emit_accept(context->avdtp_callback, connection->con_handle, connection->signaling_packet.signal_identifier, status);

    // check fragmentation
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        avdtp_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
    }
}

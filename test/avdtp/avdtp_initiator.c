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
#include "avdtp_initiator.h"

// static int avdtp_initiator_send_get_all_capabilities_cmd(uint16_t cid, uint8_t transaction_label, uint8_t sep_id){
//     uint8_t command[3];
//     command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
//     command[1] = AVDTP_SI_GET_ALL_CAPABILITIES;
//     command[2] = sep_id << 2;
//     return l2cap_send(cid, command, sizeof(command));
// }

// static int avdtp_initiator_send_get_capabilities_cmd(uint16_t cid, uint8_t transaction_label, uint8_t sep_id){
//     uint8_t command[3];
//     command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
//     command[1] = AVDTP_SI_GET_CAPABILITIES;
//     command[2] = sep_id << 2;
//     return l2cap_send(cid, command, sizeof(command));
// }

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

void avdtp_initiator_stream_config_subsm(avdtp_connection_t * connection, uint8_t *packet, uint16_t size, int offset){
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
    int status = 0;

    switch (connection->signaling_packet.message_type){
        case AVDTP_RESPONSE_ACCEPT_MSG:
            printf("    INT: AVDTP_RESPONSE_ACCEPT_MSG: ");
            
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_DISCOVER:{
                    printf("AVDTP_SI_DISCOVER\n");
                    if (connection->signaling_packet.transaction_label != connection->initiator_transaction_label){
                        printf("    unexpected transaction label, got %d, expected %d\n", connection->signaling_packet.transaction_label, connection->initiator_transaction_label);
                        status = BAD_HEADER_FORMAT;
                        break;
                    }
                    
                    if (size == 3){
                        printf("    ERROR code %02x\n", packet[offset]);
                        status = packet[offset];
                        break;
                    }
                    
                    avdtp_sep_t sep;
                    int i;
                    for (i = offset; i < size; i += 2){
                        sep.seid = packet[i] >> 2;
                        offset++;
                        if (sep.seid < 0x01 || sep.seid > 0x3E){
                            printf("    invalid sep id\n");
                            status = BAD_ACP_SEID;
                            break;
                        }
                        sep.in_use = (packet[i] >> 1) & 0x01;
                        sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                        sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                        avdtp_signaling_emit_sep(avdtp_sink_callback, connection->con_handle, sep);
                    }
                    connection->initiator_transaction_label++;
                    break;
                }
                
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:{
                    printf("AVDTP_SI_GET(_ALL)_CAPABILITIES\n");
                    avdtp_sep_t sep;
                    sep.seid = connection->query_seid;
                    
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, packet+offset, size-offset);
                    // printf_hexdump(packet, size);

                    if (get_bit16(sep.registered_service_categories, AVDTP_MEDIA_CODEC)){
                        switch (sep.capabilities.media_codec.media_codec_type){
                            case AVDTP_CODEC_SBC: 
                                avdtp_signaling_emit_media_codec_sbc_capability(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                            default:
                                avdtp_signaling_emit_media_codec_other_capability(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                        }
                    }
                    break;
                }

                case AVDTP_SI_GET_CONFIGURATION:{
                    printf("AVDTP_SI_GET_CONFIGURATION\n");
                    avdtp_sep_t sep;
                    sep.seid = connection->query_seid;
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, packet+offset, size-offset);
                    sep.in_use = 1;
                    
                    printf("    INT .. seid %d, configured services 0%02x\n", sep.seid, sep.configured_service_categories);

                    if (get_bit16(sep.configured_service_categories, AVDTP_MEDIA_CODEC)){
                        printf(" AVDTP_SI_GET_CONFIGURATION 1 %d\n", sep.capabilities.media_codec.media_codec_type);
                        switch (sep.capabilities.media_codec.media_codec_type){
                            case AVDTP_CODEC_SBC: 
                                avdtp_signaling_emit_media_codec_sbc_configuration(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                            default:
                                avdtp_signaling_emit_media_codec_other_configuration(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                        }
                        break;
                    }
                    break;
                }
                
                case AVDTP_SI_SET_CONFIGURATION:{
                    printf("AVDTP_SI_SET_CONFIGURATION\n");
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->int_seid);
                    
                    avdtp_sep_t sep;
                    sep.seid = connection->query_seid;
                    sep.configured_service_categories = connection->remote_capabilities_bitmap;
                    sep.configuration = connection->remote_capabilities;
                    sep.in_use = 1;

                    // find or add sep
                    int i;
                    stream_endpoint->remote_sep_index = 0xFF;
                    for (i=0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                            break;
                        }
                    }
                    
                    if (stream_endpoint->remote_sep_index != 0xFF){
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    INT: update seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    } else {
                        // add new
                        printf("    INT: seid %d not found in %p\n", sep.seid, stream_endpoint);
                        stream_endpoint->remote_sep_index = stream_endpoint->remote_seps_num;
                        stream_endpoint->remote_seps_num++;
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    INT: add seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
                    break;
                }

                case AVDTP_SI_RECONFIGURE:
                    printf("AVDTP_SI_RECONFIGURE\n");
                    avdtp_sep_t sep;
                    sep.seid = connection->query_seid;
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, &sep.configuration, connection->signaling_packet.command+4, connection->signaling_packet.size-4);
                    sep.in_use = 1;

                    // find or sep
                    int i;
                    stream_endpoint->remote_sep_index = 0xFF;
                    for (i=0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                            break;
                        }
                    }
                    
                    if (stream_endpoint->remote_sep_index != 0xFF){
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
                        printf("    INT: update seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    } 
                    break;
                
                case AVDTP_SI_OPEN:
                    printf("AVDTP_SI_OPEN\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    break;
                case AVDTP_SI_START:
                    printf("AVDTP_SI_START\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
                    break;
                case AVDTP_SI_SUSPEND:
                    printf("AVDTP_SI_SUSPEND\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    break;
                case AVDTP_SI_CLOSE:
                    printf("AVDTP_SI_CLOSE\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                    break;
                case AVDTP_SI_ABORT:
                    printf("AVDTP_SI_ABORT\n");
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                    break;
                default:
                    status = 1;
                    printf("    AVDTP_RESPONSE_ACCEPT_MSG, signal %d not implemented\n", connection->signaling_packet.signal_identifier);
                    break;
                }
            break;
        case AVDTP_RESPONSE_REJECT_MSG:
            printf("    AVDTP_RESPONSE_REJECT_MSG signal %d\n", connection->signaling_packet.signal_identifier);
            avdtp_signaling_emit_reject(avdtp_sink_callback, connection->con_handle, connection->signaling_packet.signal_identifier);
            return;
        case AVDTP_GENERAL_REJECT_MSG:
            printf("    AVDTP_GENERAL_REJECT_MSG signal %d\n", connection->signaling_packet.signal_identifier);
            avdtp_signaling_emit_general_reject(avdtp_sink_callback, connection->con_handle, connection->signaling_packet.signal_identifier);
            return;
        default:
            break;
    }
    avdtp_signaling_emit_accept(avdtp_sink_callback, connection->con_handle, connection->signaling_packet.signal_identifier, status);
}

void avdtp_initiator_stream_config_subsm_run(avdtp_connection_t * connection){
    int sent = 1;

    switch (connection->initiator_connection_state){
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SEPS_DISCOVERED\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SEPS_DISCOVERED;
            avdtp_initiator_send_signaling_cmd(connection->l2cap_signaling_cid, AVDTP_SI_DISCOVER, connection->initiator_transaction_label);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_CAPABILITIES;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CAPABILITIES, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_ALL_CAPABILITIES\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ALL_CAPABILITIES;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_ALL_CAPABILITIES, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_CONFIGURATION\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_CONFIGURATION;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CONFIGURATION, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SET_CONFIGURATION:{
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SET_CONFIGURATION bitmap %02x\n",  connection->remote_capabilities_bitmap);
            // printf_hexdump(  connection->remote_capabilities.media_codec.media_codec_information,  connection->remote_capabilities.media_codec.media_codec_information_len);
            connection->signaling_packet.acp_seid = connection->query_seid;
            connection->signaling_packet.int_seid = connection->int_seid;
            connection->signaling_packet.signal_identifier = AVDTP_SI_SET_CONFIGURATION;

            avdtp_prepare_capabilities(&connection->signaling_packet, connection->initiator_transaction_label, connection->remote_capabilities_bitmap, connection->remote_capabilities, AVDTP_SI_SET_CONFIGURATION);
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SET_CONFIGURATION;
                printf("    INT: fragmented\n");
            }
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
            break;
        }
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID;                

            connection->signaling_packet.acp_seid = connection->query_seid;
            connection->signaling_packet.int_seid = 0;
            connection->signaling_packet.signal_identifier = AVDTP_SI_RECONFIGURE;

            avdtp_prepare_capabilities(&connection->signaling_packet, connection->initiator_transaction_label, connection->remote_capabilities_bitmap, connection->remote_capabilities, AVDTP_SI_RECONFIGURE);
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
            if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID;
                printf("    INT: fragmented\n");
            }
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
            break;

        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_MEDIA_CONNECTED:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_MEDIA_CONNECTED\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_MEDIA_CONNECTED;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_OPEN, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SUSPEND_STREAM_WITH_SEID:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SUSPEND_STREAM_WITH_SEID\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SUSPEND_STREAM_WITH_SEID;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_SUSPEND, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STARTED:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STARTED\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STARTED;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_START, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STOPED:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STOPED\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STOPED;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_CLOSE, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_ABORTED:
            printf("    INT: AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_ABORTED\n");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_ABORTED;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_ABORT, connection->initiator_transaction_label, connection->query_seid);
            break;
        default:
            sent = 0;
            break;
    }

    // check fragmentation
    if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
        avdtp_sink_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    }
}

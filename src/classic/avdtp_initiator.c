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

static bool avdtp_initiator_have_bytes(uint16_t pos, uint16_t end, uint16_t bytes_needed){
    return (pos <= end) && (bytes_needed <= (uint16_t)(end - pos));
}

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


static void avdtp_initiator_parser_reset(avdtp_connection_t * connection){
    memset(connection->initiator_signaling_packet.command, 0, sizeof(connection->initiator_signaling_packet.command));
    connection->parser_value_offset = 0;
    connection->parser_value_size = 0;
    connection->capability_parser_state = AVDTP_PARSER_GET_SERVICE_CATEGORY;
    connection->parser_service_category_id = AVDTP_SERVICE_CATEGORY_INVALID_FF;
}

static void avdtp_initiator_parser_handle_service_category_complete(avdtp_connection_t *connection) {
    avdtp_capabilities_t capabilities;
    avdtp_unpack_service_capabilities(connection, connection->initiator_signaling_packet.signal_identifier, &capabilities, connection->initiator_signaling_packet.command, connection->parser_value_size + 2);
    if (connection->error_code == ERROR_CODE_SUCCESS){
        avdtp_signaling_emit_capabilities_of_service_category(connection->avdtp_cid, connection->initiator_remote_seid, &capabilities, connection->parser_service_category_id);
    }
}

static void avdtp_initiator_parser_process_byte(uint8_t byte, avdtp_connection_t * connection){
    switch(connection->capability_parser_state){
        case AVDTP_PARSER_GET_SERVICE_CATEGORY:
            connection->parser_service_category_id = byte;
            connection->initiator_signaling_packet.command[0] = byte;
            connection->capability_parser_state = AVDTP_PARSER_GET_CAPABILITIES_VALUE_LEN;
            return;

        case AVDTP_PARSER_GET_CAPABILITIES_VALUE_LEN:
            connection->parser_value_offset = 0;
            connection->parser_value_size = byte;
            connection->initiator_signaling_packet.command[1] = byte;

            // if the capability value does not fit into buffer, ignore the value
            if (connection->parser_value_size > (AVDTP_MAX_DATA_BUFFER_SIZE - 2)){
                connection->capability_parser_state = AVDTP_PARSER_IGNORE_REST_OF_CAPABILITY_VALUE;
                log_info("Capability 0x%02X value length %u exceeds the buffer size %u",
                         connection->initiator_signaling_packet.command[0],
                         connection->parser_value_size,
                         AVDTP_MAX_DATA_BUFFER_SIZE - 2);
                break;
            }

            if (connection->parser_value_size == 0){
                avdtp_initiator_parser_handle_service_category_complete(connection);
                connection->capability_parser_state = AVDTP_PARSER_GET_SERVICE_CATEGORY;
                connection->parser_service_category_id = AVDTP_SERVICE_CATEGORY_INVALID_FF;
            } else {
                connection->capability_parser_state = AVDTP_PARSER_GET_CAPABILITIES_VALUE;
            }
            break;

        case AVDTP_PARSER_GET_CAPABILITIES_VALUE:
            connection->initiator_signaling_packet.command[connection->parser_value_offset + 2] = byte;
            connection->parser_value_offset++;

            if (connection->parser_value_offset == connection->parser_value_size) {
                // done, emit value
                avdtp_initiator_parser_handle_service_category_complete(connection);
                connection->capability_parser_state = AVDTP_PARSER_GET_SERVICE_CATEGORY;
                connection->parser_service_category_id = AVDTP_SERVICE_CATEGORY_INVALID_FF;
            }
            break;

        case AVDTP_PARSER_IGNORE_REST_OF_CAPABILITY_VALUE:
            connection->parser_value_offset++;

            if (connection->parser_value_offset == connection->parser_value_size) {
                // done, don't emit the capability
                connection->capability_parser_state = AVDTP_PARSER_GET_SERVICE_CATEGORY;
                connection->parser_service_category_id = AVDTP_SERVICE_CATEGORY_INVALID_FF;
            }
            break;
        default:
            break;
    }
}

static void avdtp_initiator_parser_process_packet(avdtp_connection_t * connection, uint8_t * packet, uint16_t packet_size){
    for (int i=0; i < packet_size; i++){
        avdtp_initiator_parser_process_byte(packet[i], connection);
    }
}

static bool avdtp_initiator_num_packets_valid(const avdtp_connection_t *connection, const avdtp_signaling_packet_t *signaling_header) {
    switch (signaling_header->packet_type) {
        case AVDTP_START_PACKET:
        case AVDTP_CONTINUE_PACKET:
            return (connection->initiator_signaling_packet.num_packets > 1);
        case AVDTP_END_PACKET:
            return connection->initiator_signaling_packet.num_packets == 1;
        default:
            return true;
    }
}

static void avdtp_initiator_handle_general_connection_responses(avdtp_connection_t *connection, uint8_t *packet, uint16_t size, int offset) {
    avdtp_sep_t sep;

    switch (connection->initiator_signaling_packet.message_type){
        case AVDTP_RESPONSE_ACCEPT_MSG:
            switch (connection->initiator_signaling_packet.signal_identifier){
                case AVDTP_SI_DISCOVER:{
                    if (connection->initiator_signaling_packet.transaction_label == connection->initiator_transaction_label) {
                        int i;
                        for (i = offset; i < size; i += 2){
                            if (!avdtp_initiator_have_bytes((uint16_t)i, size, 2u)) break;
                            sep.seid = packet[i] >> 2;
                            if ((sep.seid < 0x01) || (sep.seid > 0x3E)){
                                log_info("invalid sep id");
                                // status = BAD_ACP_SEID;
                                break;
                            }
                            sep.in_use = (packet[i] >> 1) & 0x01;
                            sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                            sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                            avdtp_signaling_emit_sep(connection->avdtp_cid, sep);
                        }
                    } else {
                        log_info("unexpected transaction label, got %d, expected %d", connection->initiator_signaling_packet.transaction_label, connection->initiator_transaction_label);
                        // status = BAD_HEADER_FORMAT;
                    }

                    avdtp_signaling_emit_sep_done(connection->avdtp_cid);
                    break;
                }
                
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    connection->initiator_signaling_packet.size = size - offset;
                    connection->initiator_signaling_packet.offset = 0;

                    switch (connection->initiator_signaling_packet.packet_type){
                        case AVDTP_START_PACKET:
                            avdtp_signaling_emit_accept(connection->avdtp_cid, 0, connection->initiator_signaling_packet.signal_identifier, true);
                            avdtp_initiator_parser_reset(connection);
                            avdtp_initiator_parser_process_packet(connection, packet + offset, size - offset);
                            break;

                        case AVDTP_CONTINUE_PACKET:
                            avdtp_initiator_parser_process_packet(connection, packet + offset, size - offset);
                            break;

                        case AVDTP_END_PACKET:
                            avdtp_initiator_parser_process_packet(connection, packet + offset, size - offset);
                            avdtp_initiator_parser_reset(connection);
                            avdtp_signaling_emit_capability_done(connection->avdtp_cid, connection->initiator_remote_seid);
                            break;

                        default: // single packet
                            avdtp_signaling_emit_accept(connection->avdtp_cid, 0, connection->initiator_signaling_packet.signal_identifier, true);
                            avdtp_initiator_parser_reset(connection);
                            avdtp_initiator_parser_process_packet(connection, packet + offset, size - offset);
                            avdtp_signaling_emit_capability_done(connection->avdtp_cid, connection->initiator_remote_seid);
                            break;
                    }
                    break;
                
                case AVDTP_SI_DELAYREPORT:
                case AVDTP_SI_GET_CONFIGURATION:
                    break;
                default:
                    log_info("AVDTP_RESPONSE_ACCEPT_MSG, signal %d not implemented", connection->initiator_signaling_packet.signal_identifier);
                    break;
            }
            avdtp_signaling_emit_accept(connection->avdtp_cid, 0, connection->initiator_signaling_packet.signal_identifier, true);
            break;

        case AVDTP_RESPONSE_REJECT_MSG:
            log_info("AVDTP_RESPONSE_REJECT_MSG signal %s", avdtp_si2str(connection->initiator_signaling_packet.signal_identifier));
            avdtp_signaling_emit_reject(connection->avdtp_cid, connection->initiator_local_seid,
                                        connection->initiator_signaling_packet.signal_identifier, true);
            return;

        case AVDTP_GENERAL_REJECT_MSG:
            log_info("AVDTP_GENERAL_REJECT_MSG signal %s", avdtp_si2str(connection->initiator_signaling_packet.signal_identifier));
            avdtp_signaling_emit_general_reject(connection->avdtp_cid, connection->initiator_local_seid,
                                                connection->initiator_signaling_packet.signal_identifier, true);
            return;
        default:
            break;
    }
}

static void avdtp_initiator_handle_stream_endpoint_responses(avdtp_connection_t *connection, avdtp_stream_endpoint_t * stream_endpoint){
    avdtp_stream_endpoint_t * stream_endpoint_for_event = NULL;

    avdtp_sep_t sep;
    sep.seid = connection->initiator_remote_seid;

    switch (connection->initiator_signaling_packet.message_type){
        case AVDTP_RESPONSE_ACCEPT_MSG:
            switch (connection->initiator_signaling_packet.signal_identifier){
                case AVDTP_SI_RECONFIGURE:
                    stream_endpoint_for_event = stream_endpoint;
                    stream_endpoint->remote_sep.configured_service_categories |= stream_endpoint->remote_configuration_bitmap;
                    stream_endpoint->remote_sep.configuration = stream_endpoint->remote_configuration;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;

                    // copy media codec configuration if reconfigured and emit config
                    if ((stream_endpoint->remote_configuration_bitmap & (1 << AVDTP_MEDIA_CODEC)) != 0){
                        btstack_assert(stream_endpoint->remote_configuration.media_codec.media_codec_information_len == stream_endpoint->media_codec_configuration_len);
                        (void)memcpy(stream_endpoint->media_codec_configuration_info, stream_endpoint->remote_configuration.media_codec.media_codec_information, stream_endpoint->media_codec_configuration_len);
                        stream_endpoint->sep.configuration.media_codec = stream_endpoint->remote_configuration.media_codec;
                        avdtp_signaling_emit_configuration(stream_endpoint, connection->avdtp_cid, 1, &stream_endpoint->sep.configuration, (1 << AVDTP_MEDIA_CODEC));
                    }
                    break;

                case AVDTP_SI_SET_CONFIGURATION:
                    stream_endpoint_for_event = stream_endpoint;

                    sep.configured_service_categories = stream_endpoint->remote_configuration_bitmap;
                    sep.configuration = stream_endpoint->remote_configuration;
                    sep.in_use = 1;
                    
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
                    stream_endpoint->sep.in_use = 1;
                    stream_endpoint->remote_sep = sep;
                    stream_endpoint->connection = connection;
                    connection->configuration_state = AVDTP_CONFIGURATION_STATE_LOCAL_CONFIGURED;

                    log_info("configured remote seid %d", stream_endpoint->remote_sep.seid);

                    // copy media codec configuration if configured and emit config
                    if ((stream_endpoint->remote_configuration_bitmap & (1 << AVDTP_MEDIA_CODEC)) != 0) {
                        btstack_assert(stream_endpoint->remote_configuration.media_codec.media_codec_information_len == stream_endpoint->media_codec_configuration_len);
                        // copy media codec configuration if length correct
                        (void) memcpy(stream_endpoint->media_codec_configuration_info,
                                      stream_endpoint->remote_configuration.media_codec.media_codec_information,
                                      stream_endpoint->media_codec_configuration_len);
                        stream_endpoint->sep.configuration.media_codec = stream_endpoint->remote_configuration.media_codec;
                        avdtp_signaling_emit_configuration(stream_endpoint, connection->avdtp_cid, 0,  &sep.configuration, (1 << AVDTP_MEDIA_CODEC));
					}
                    break;

                case AVDTP_SI_OPEN:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_ACCEPT_OPEN_STREAM) {
                        log_error("AVDTP_SI_OPEN in wrong stream endpoint state %d", stream_endpoint->state);
                        return;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    connection->initiator_local_seid = stream_endpoint->sep.seid;
                    l2cap_create_channel(avdtp_packet_handler, connection->remote_addr, BLUETOOTH_PSM_AVDTP, AVDTP_L2CAP_MTU, NULL);
                    return;
                case AVDTP_SI_START:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED) {
                        log_error("AVDTP_SI_START in wrong stream endpoint state %d", stream_endpoint->state);
                        return;
                    }
                    stream_endpoint_for_event = stream_endpoint;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
                    break;
                case AVDTP_SI_SUSPEND:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_STREAMING) {
                        log_error("AVDTP_SI_SUSPEND in wrong stream endpoint state %d", stream_endpoint->state);
                        return;
                    }
                    stream_endpoint_for_event = stream_endpoint;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    break;
                case AVDTP_SI_CLOSE:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            stream_endpoint_for_event = stream_endpoint;
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                            l2cap_disconnect(stream_endpoint->l2cap_media_cid);
                            break;
                        default:
                            break;
                    }
                    break;
                case AVDTP_SI_ABORT:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_CONFIGURED:
                        case AVDTP_STREAM_ENDPOINT_CLOSING:
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            stream_endpoint_for_event = stream_endpoint;
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                            l2cap_disconnect(stream_endpoint->l2cap_media_cid);
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    log_info("AVDTP_RESPONSE_ACCEPT_MSG, signal %d not implemented", connection->initiator_signaling_packet.signal_identifier);
                    return;
            }
            if (stream_endpoint_for_event != NULL){
                avdtp_signaling_emit_accept_for_stream_endpoint(stream_endpoint_for_event, connection->initiator_local_seid,
                                                                connection->initiator_signaling_packet.signal_identifier, true);
            }
            break;

        case AVDTP_RESPONSE_REJECT_MSG:
            switch (connection->initiator_signaling_packet.signal_identifier){
                case AVDTP_SI_SET_CONFIGURATION:
                    connection->configuration_state = AVDTP_CONFIGURATION_STATE_IDLE;
                    stream_endpoint->sep.in_use = 0;
                    log_info("Received reject for set configuration, role changed from initiator to acceptor. TODO: implement retry.");
                    break;
                case AVDTP_SI_RECONFIGURE:
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    log_info("Received reject for reconfigure, state = opened");
                    break;
                default:
                    break;
            }
            log_info("AVDTP_RESPONSE_REJECT_MSG signal %s", avdtp_si2str(connection->initiator_signaling_packet.signal_identifier));
            avdtp_signaling_emit_reject(connection->avdtp_cid, connection->initiator_local_seid,
                                        connection->initiator_signaling_packet.signal_identifier, true);
            return;

        case AVDTP_GENERAL_REJECT_MSG:
            log_info("AVDTP_GENERAL_REJECT_MSG signal %s", avdtp_si2str(connection->initiator_signaling_packet.signal_identifier));
            avdtp_signaling_emit_general_reject(connection->avdtp_cid, connection->initiator_local_seid,
                                                connection->initiator_signaling_packet.signal_identifier, true);
            return;
        default:
            break;
    }
}

void avdtp_initiator_stream_config_subsm(avdtp_connection_t *connection, uint8_t *packet, uint16_t size, int offset) {
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    avdtp_signaling_packet_t * signaling_header = &connection->initiator_signaling_packet;

    switch (connection->initiator_connection_state){
        // This state is in W4_ANSWER for the general queries that do not involve stream endpoint configuration or control,
        // e.g. SEP discovery, getting capabilities and configuration, delay report.
        // Once the answer is received, the state is set back to IDLE.

        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER:
            switch (connection->initiator_signaling_packet.signal_identifier) {
                // handle fragmented answer for GET_CAPABILITIES/GET_ALL_CAPABILITIES, ignore for all other
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    if (!avdtp_initiator_num_packets_valid(connection, signaling_header)) {
                        connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                        avdtp_signaling_emit_capability_done(connection->avdtp_cid, connection->initiator_remote_seid);
                        return;
                    }
                    if ((signaling_header->packet_type == AVDTP_END_PACKET) ||
                        (signaling_header->packet_type == AVDTP_SINGLE_PACKET)) {
                        connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                        break;
                    }
                    // assembly implemented, keep AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER state
                    break;
                default:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    if ((signaling_header->packet_type != AVDTP_SINGLE_PACKET)) {
                        log_info("Fragmentation for signal identifier %d not implemented", connection->initiator_signaling_packet.signal_identifier);
                        return;
                    }
                    break;
            }
            avdtp_initiator_handle_general_connection_responses(connection, packet, size, offset);
            break;

        case AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE:
            // Handle answers to stream endpoint configuration and control commands
            stream_endpoint = avdtp_get_stream_endpoint_for_seid(connection->initiator_local_seid);
            if (stream_endpoint == NULL) {
                log_debug("no stream endpoint for local seid %u, signal identifier %d", connection->initiator_local_seid, connection->initiator_signaling_packet.signal_identifier);
                return;
            }
            log_debug("using stream endpoint %p for local seid %u, signal identifier %d", stream_endpoint, connection->initiator_local_seid, connection->initiator_signaling_packet.signal_identifier);

            if (stream_endpoint->initiator_config_state != AVDTP_INITIATOR_W4_ANSWER) {
                log_error("initiator_config_state is in wrong state %d, expected %d, signal identifier %d", stream_endpoint->initiator_config_state, AVDTP_INITIATOR_W4_ANSWER, connection->initiator_signaling_packet.signal_identifier);
                return;
            }

            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
            avdtp_initiator_handle_stream_endpoint_responses(connection, stream_endpoint);
            break;

        default:
            return;
    }
}

static bool avdtp_initiator_stream_config_subsm_run_signaling(avdtp_connection_t * connection){
    switch (connection->initiator_connection_state){
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS:
            log_info("W2_DISCOVER_SEPS");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd(connection->l2cap_signaling_cid, AVDTP_SI_DISCOVER, connection->initiator_transaction_label);
            return true;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES:
            log_info("W2_GET_CAPABILITIES");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CAPABILITIES, connection->initiator_transaction_label, connection->initiator_remote_seid);
            return true;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES:
            log_info("W2_GET_ALL_CAPABILITIES");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_ALL_CAPABILITIES, connection->initiator_transaction_label, connection->initiator_remote_seid);
            return true;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION:
            log_info("W4_GET_CONFIGURATION");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CONFIGURATION, connection->initiator_transaction_label, connection->initiator_remote_seid);
            return true;
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_DELAY_REPORT:
            log_info("W4_DELAY_REPORT");
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER;
            avdtp_initiator_send_signaling_cmd_delay_report(connection->l2cap_signaling_cid, connection->initiator_transaction_label,
                                                            connection->initiator_remote_seid, connection->delay_ms);
            return true;
        default:
            break;
    }
    return false;
}

static bool avdtp_initiator_stream_config_subsm_run_endpoint(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint){
	log_debug("SE %p, initiator_config_state: 0x%02x", stream_endpoint, stream_endpoint->initiator_config_state);
    switch (stream_endpoint->initiator_config_state){
        case AVDTP_INITIATOR_W2_SET_CONFIGURATION:
        case AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID:{
            if ((stream_endpoint->initiator_config_state == AVDTP_INITIATOR_W2_SET_CONFIGURATION) && (connection->configuration_state != AVDTP_CONFIGURATION_STATE_LOCAL_INITIATED)){
                log_info("initiator SM stop sending SET_CONFIGURATION cmd:");
                break;
            }
            log_info("W2_(RE)CONFIGURATION bitmap, local seid 0x%02x, remote seid 0x%02x", connection->initiator_local_seid, connection->initiator_remote_seid);
            // log_info_hexdump(  connection->remote_capabilities.media_codec.media_codec_information,  connection->remote_capabilities.media_codec.media_codec_information_len);
            connection->initiator_signaling_packet.acp_seid = connection->initiator_remote_seid;
            connection->initiator_signaling_packet.int_seid = connection->initiator_local_seid;

            connection->initiator_signaling_packet.signal_identifier = AVDTP_SI_SET_CONFIGURATION;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE;
            if (stream_endpoint->initiator_config_state == AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID){
                connection->initiator_signaling_packet.signal_identifier = AVDTP_SI_RECONFIGURE;
            }

            avdtp_prepare_capabilities(&connection->initiator_signaling_packet, connection->initiator_transaction_label, stream_endpoint->remote_configuration_bitmap, stream_endpoint->remote_configuration, connection->initiator_signaling_packet.signal_identifier);
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->initiator_signaling_packet, out_buffer);
            if ((connection->initiator_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->initiator_signaling_packet.packet_type != AVDTP_END_PACKET)){
                stream_endpoint->initiator_config_state = AVDTP_INITIATOR_FRAGMENTATED_COMMAND;
                log_info("fragmented");
            } else {
				stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
            }
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
            if (stream_endpoint->initiator_config_state == AVDTP_INITIATOR_FRAGMENTATED_COMMAND){
				avdtp_request_can_send_now_initiator(connection);
			}
            return true;
        }
        case AVDTP_INITIATOR_FRAGMENTATED_COMMAND:{
            l2cap_reserve_packet_buffer();
            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
            uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->initiator_signaling_packet, out_buffer);
            if ((connection->initiator_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->initiator_signaling_packet.packet_type != AVDTP_END_PACKET)){
                stream_endpoint->initiator_config_state = AVDTP_INITIATOR_FRAGMENTATED_COMMAND;
                log_info("fragmented");
            } else {
				stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
			}
            l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
			if (stream_endpoint->initiator_config_state == AVDTP_INITIATOR_FRAGMENTATED_COMMAND){
				avdtp_request_can_send_now_initiator(connection);
			}
            return true;
        }
        case AVDTP_INITIATOR_W2_OPEN_STREAM:
            switch (stream_endpoint->state){
                case AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM:
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_ACCEPT_OPEN_STREAM;
                    log_info("send AVDTP_SI_OPEN signaling to remote, transaction_label %d, remote seid 0x%02x", connection->initiator_transaction_label, connection->initiator_remote_seid);
                    avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_OPEN, connection->initiator_transaction_label, connection->initiator_remote_seid);
					stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
                    return true;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return false;
}

void avdtp_initiator_stream_config_subsm_handle_can_send_now_signaling(avdtp_connection_t *connection) {
    bool sent;

    sent = avdtp_initiator_stream_config_subsm_run_signaling(connection);
    if (sent) return;

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(connection->initiator_local_seid);

	if (stream_endpoint == NULL) return;
	sent = avdtp_initiator_stream_config_subsm_run_endpoint(connection, stream_endpoint);
	if (sent) return;

    log_debug("stream: start %u, close %u, abort %u, suspend %u, request %u", stream_endpoint->start_stream, stream_endpoint->close_stream,
			  stream_endpoint->abort_stream, stream_endpoint->suspend_stream, stream_endpoint->request_can_send_now);

    if (stream_endpoint->start_stream){
        stream_endpoint->start_stream = 0;
        if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_OPENED){
			stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
            connection->initiator_local_seid = stream_endpoint->sep.seid;
            connection->initiator_remote_seid = stream_endpoint->remote_sep.seid;
            connection->initiator_transaction_label = avdtp_get_next_transaction_label();
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_START, connection->initiator_transaction_label, connection->initiator_remote_seid);
            return;            
        } 
    }
    
    if (stream_endpoint->close_stream){
        stream_endpoint->close_stream = 0;
        switch (stream_endpoint->state){
            case AVDTP_STREAM_ENDPOINT_OPENED:
            case AVDTP_STREAM_ENDPOINT_STREAMING:
				stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
                connection->initiator_local_seid = stream_endpoint->sep.seid;
                connection->initiator_remote_seid = stream_endpoint->remote_sep.seid;
                connection->initiator_transaction_label = avdtp_get_next_transaction_label();
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_CLOSE, connection->initiator_transaction_label, connection->initiator_remote_seid);
                return;   
            default:
                break;         
        }
    }
    
    if (stream_endpoint->abort_stream){
        stream_endpoint->abort_stream = 0;
		stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
        connection->initiator_transaction_label = avdtp_get_next_transaction_label();
        avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_ABORT, connection->initiator_transaction_label, connection->initiator_remote_seid);
        return;
    }
    
    if (stream_endpoint->suspend_stream){
        stream_endpoint->suspend_stream = 0;
        if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING){
			stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ANSWER;
            connection->initiator_local_seid = stream_endpoint->sep.seid;
            connection->initiator_remote_seid = stream_endpoint->remote_sep.seid;
            connection->initiator_transaction_label = avdtp_get_next_transaction_label();
            avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_SUSPEND, connection->initiator_transaction_label, connection->initiator_remote_seid);
            return;
        }
    }
    
}

void avdtp_initiator_stream_config_subsm_handle_can_send_now_stream_endpoint(avdtp_stream_endpoint_t *  stream_endpoint) {
	if (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING){
		stream_endpoint->state =  AVDTP_STREAM_ENDPOINT_STREAMING;
		avdtp_streaming_emit_can_send_media_packet_now(stream_endpoint, stream_endpoint->sequence_number);
	}
}

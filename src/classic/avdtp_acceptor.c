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
#define BTSTACK_FILE__ "avdtp_acceptor.c"

#include <stdint.h>
#include <string.h>

#include "classic/avdtp.h"
#include "classic/avdtp_util.h"
#include "classic/avdtp_acceptor.h"

#include "btstack_debug.h"
#include "btstack_util.h"
#include "l2cap.h"


static int avdtp_acceptor_send_accept_response(uint16_t cid,  uint8_t transaction_label, avdtp_signal_identifier_t identifier){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

// returns true if command complete
static bool avdtp_acceptor_process_chunk(avdtp_signaling_packet_t * signaling_packet, uint8_t * packet, uint16_t size){
    if ((signaling_packet->size + size) >= sizeof(signaling_packet->command)) {
        log_info("Dropping incoming data, doesn't fit into command buffer");
        signaling_packet->size = 0;
        return false;
    }

    (void)memcpy(signaling_packet->command + signaling_packet->size, packet, size);
    signaling_packet->size += size;
    return (signaling_packet->packet_type == AVDTP_SINGLE_PACKET) || (signaling_packet->packet_type == AVDTP_END_PACKET);
}

static int avdtp_acceptor_validate_msg_length(avdtp_signal_identifier_t signal_identifier, uint16_t header_size, uint16_t msg_size){
    // payload length is num bytes after header (incl. signaling identifier), e.g. 1 for Get Capabilities
    int minimal_payload_length;
    switch (signal_identifier){
        case AVDTP_SI_GET_CAPABILITIES:
        case AVDTP_SI_GET_ALL_CAPABILITIES:
        case AVDTP_SI_GET_CONFIGURATION:
        case AVDTP_SI_RECONFIGURE:
        case AVDTP_SI_OPEN:
        case AVDTP_SI_START:
        case AVDTP_SI_CLOSE:
        case AVDTP_SI_ABORT:
        case AVDTP_SI_SECURITY_CONTROL:
            minimal_payload_length = 1;
            break;
        case AVDTP_SI_SET_CONFIGURATION:
            minimal_payload_length = 2;
            break;
        case AVDTP_SI_DELAYREPORT:
            minimal_payload_length = 3;
            break;
        case AVDTP_SI_DISCOVER:
        default:
            minimal_payload_length = 0;
            break;
    }
    return msg_size >= (header_size + minimal_payload_length);
}

static void
avdtp_acceptor_handle_configuration_command(avdtp_connection_t *connection, int offset, uint16_t packet_size, avdtp_stream_endpoint_t *stream_endpoint) {
    log_info("W2_ANSWER_SET_CONFIGURATION cid 0x%02x", connection->avdtp_cid);
    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE;
    stream_endpoint->connection = connection;

    // process capabilities, first rejected service category is stored in connection
    connection->reject_service_category = 0;
    avdtp_sep_t sep;
    sep.seid = connection->acceptor_signaling_packet.command[offset++] >> 2;
    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, connection->acceptor_signaling_packet.signal_identifier, &sep.configuration, connection->acceptor_signaling_packet.command+offset, packet_size-offset);
    sep.in_use = 1;

    // test if sep already in use
    if (stream_endpoint->sep.in_use != 0){
        log_info("stream endpoint already in use");
        connection->error_code = AVDTP_ERROR_CODE_SEP_IN_USE;
        connection->reject_service_category = 0;
    }

    // let application validate media configuration as well
    if (connection->error_code == 0){
        if ((sep.configured_service_categories & (1 << AVDTP_MEDIA_CODEC)) != 0){
            const adtvp_media_codec_capabilities_t * media = &sep.configuration.media_codec;
            uint8_t error_code = avdtp_validate_media_configuration(stream_endpoint, connection->avdtp_cid, 0, media);
            if (error_code != 0){
                log_info("media codec rejected by validator, error 0x%02x", error_code);
                connection->reject_service_category = AVDTP_MEDIA_CODEC;
                connection->error_code              = error_code;
            }
        }
    }

    if (connection->error_code){
        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
        return;
    }

    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_SET_CONFIGURATION;
    // find or add sep

    log_info("local seid %d, remote seid %d", connection->acceptor_local_seid, sep.seid);
    
    if (is_avdtp_remote_seid_registered(stream_endpoint)){
        if (stream_endpoint->remote_sep.in_use){
            log_info("remote seid already in use");
            connection->error_code = AVDTP_ERROR_CODE_SEP_IN_USE;
            // find first registered category and fire the error
            connection->reject_service_category = 0;
            int i;
            for (i = 1; i < 9; i++){
                if (get_bit16(sep.configured_service_categories, i)){
                    connection->reject_service_category = i;
                    break;
                }
            }
            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
            return;
        } else {
            stream_endpoint->remote_sep = sep;
            log_info("update remote seid %d", stream_endpoint->remote_sep.seid);
        }
    } else {
        // add new
        stream_endpoint->remote_sep = sep;
        log_info("add remote seid %d", stream_endpoint->remote_sep.seid);
    }
    
    // mark as in_use
    stream_endpoint->sep.in_use = 1;

	// if media codec configuration set, copy configuration and emit event
	if ((sep.configured_service_categories & (1 << AVDTP_MEDIA_CODEC)) != 0){
		if  (stream_endpoint->media_codec_configuration_len == sep.configuration.media_codec.media_codec_information_len){
            (void) memcpy(stream_endpoint->media_codec_configuration_info, sep.configuration.media_codec.media_codec_information, stream_endpoint->media_codec_configuration_len);
            // update media codec info to point to user configuration
            stream_endpoint->remote_sep.configuration.media_codec.media_codec_information = stream_endpoint->media_codec_configuration_info;
            // emit event
            avdtp_signaling_emit_configuration(stream_endpoint, connection->avdtp_cid, 0, &sep.configuration, sep.configured_service_categories);
		}
	}

    avdtp_signaling_emit_accept(connection->avdtp_cid, avdtp_local_seid(stream_endpoint),
                                connection->acceptor_signaling_packet.signal_identifier, false);
}

void avdtp_acceptor_stream_config_subsm(avdtp_connection_t *connection, uint8_t *packet, uint16_t size, int offset) {
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    connection->acceptor_transaction_label = connection->acceptor_signaling_packet.transaction_label;
    if (!avdtp_acceptor_validate_msg_length(connection->acceptor_signaling_packet.signal_identifier, offset, size)) {
        connection->error_code = AVDTP_ERROR_CODE_BAD_LENGTH;
        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
		avdtp_request_can_send_now_acceptor(connection);
        return;
    }

    int i;

    // handle error cases
    switch (connection->acceptor_signaling_packet.signal_identifier){
        case AVDTP_SI_DISCOVER:
            if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
            log_info("W2_ANSWER_DISCOVER_SEPS");
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS;
			avdtp_request_can_send_now_acceptor(connection);
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
            connection->acceptor_local_seid  = packet[offset++] >> 2;
            stream_endpoint = avdtp_get_stream_endpoint_for_seid(connection->acceptor_local_seid);
            if (!stream_endpoint){
                log_info("cmd %d - REJECT", connection->acceptor_signaling_packet.signal_identifier);
                connection->error_code = AVDTP_ERROR_CODE_BAD_ACP_SEID;
                if (connection->acceptor_signaling_packet.signal_identifier == AVDTP_SI_OPEN){
                    connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                }
                
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                if (connection->acceptor_signaling_packet.signal_identifier == AVDTP_SI_RECONFIGURE){
                    connection->reject_service_category = connection->acceptor_local_seid;
                    connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                }
                connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
				avdtp_request_can_send_now_acceptor(connection);
                return;
            }
            break;
        
        case AVDTP_SI_SUSPEND:
            connection->num_suspended_seids = 0;
            log_info("AVDTP_SI_SUSPEND");
            for (i = offset; (i < size) && (connection->num_suspended_seids < AVDTP_MAX_NUM_SEPS); i++){
                connection->suspended_seids[connection->num_suspended_seids] = packet[i] >> 2;
                offset++;
                log_info("%d, ", connection->suspended_seids[connection->num_suspended_seids]);
                connection->num_suspended_seids++;
            }
            // reject if no SEIDs
            if (connection->num_suspended_seids == 0) {
                log_info("no suspended SEIDs, BAD_ACP_SEID");
                connection->error_code = AVDTP_ERROR_CODE_BAD_ACP_SEID;
                connection->reject_service_category = connection->acceptor_local_seid;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
				avdtp_request_can_send_now_acceptor(connection);
                return;
            }
            // deal with first suspended SEID
            connection->acceptor_local_seid = connection->suspended_seids[0];
            stream_endpoint = avdtp_get_stream_endpoint_for_seid(connection->acceptor_local_seid);
            if (!stream_endpoint){
                log_info("stream_endpoint not found, BAD_ACP_SEID");
                connection->error_code = AVDTP_ERROR_CODE_BAD_ACP_SEID;
                connection->reject_service_category = connection->acceptor_local_seid;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                connection->num_suspended_seids = 0;
				avdtp_request_can_send_now_acceptor(connection);
                return;
            }
            break;

        default:
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE;
            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
            log_info("AVDTP_CMD_MSG signal %d not implemented, general reject", connection->acceptor_signaling_packet.signal_identifier);
			avdtp_request_can_send_now_acceptor(connection);
            return;
    }

    btstack_assert(stream_endpoint != NULL);

    bool command_complete = avdtp_acceptor_process_chunk(&connection->acceptor_signaling_packet, packet, size);
    if (!command_complete) return;
    
    uint16_t packet_size = connection->acceptor_signaling_packet.size;
    connection->acceptor_signaling_packet.size = 0;
    
    int request_to_send = 1;
    switch (stream_endpoint->acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            switch (connection->acceptor_signaling_packet.signal_identifier){
                case AVDTP_SI_DELAYREPORT:
                    log_info("W2_ANSWER_DELAY_REPORT, local seid %d", connection->acceptor_local_seid);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_DELAY_REPORT;
                    avdtp_signaling_emit_delay(connection->avdtp_cid, connection->acceptor_local_seid,
                                               big_endian_read_16(packet, offset));
                    break;
                
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                    log_info("AVDTP_SI_GET_ALL_CAPABILITIES, local seid %d", connection->acceptor_local_seid);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_GET_ALL_CAPABILITIES;
                    break;
                case AVDTP_SI_GET_CAPABILITIES:
                    log_info("W2_ANSWER_GET_CAPABILITIES, local seid %d", connection->acceptor_local_seid);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_GET_CAPABILITIES;
                    break;
                case AVDTP_SI_SET_CONFIGURATION:{
                    log_info("Received SET_CONFIGURATION cmd: config state %d", connection->configuration_state);
                    switch (connection->configuration_state){
                        case AVDTP_CONFIGURATION_STATE_IDLE:
                            avdtp_acceptor_handle_configuration_command(connection, offset, packet_size,
                                                                        stream_endpoint);
                            connection->configuration_state = AVDTP_CONFIGURATION_STATE_REMOTE_INITIATED;
                            break;
                        case AVDTP_CONFIGURATION_STATE_LOCAL_INITIATED:
                        case AVDTP_CONFIGURATION_STATE_REMOTE_INITIATED:
                            log_info("Reject SET_CONFIGURATION BAD_STATE %d", connection->configuration_state);
                            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                            connection->reject_service_category = 0;
                            connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            break;
                        case AVDTP_CONFIGURATION_STATE_LOCAL_CONFIGURED:
                        case AVDTP_CONFIGURATION_STATE_REMOTE_CONFIGURED:
                            log_info("Reject SET_CONFIGURATION SEP_IN_USE");
                            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                            connection->reject_service_category = 0;
                            connection->error_code = AVDTP_ERROR_CODE_SEP_IN_USE;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case AVDTP_SI_RECONFIGURE:{
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_RECONFIGURE;
                    connection->reject_service_category = 0;

                    avdtp_sep_t sep;
                    log_info("W2_ANSWER_RECONFIGURE, local seid %d, remote seid %d", connection->acceptor_local_seid, stream_endpoint->remote_sep.seid);
                    sep.configured_service_categories = avdtp_unpack_service_capabilities(connection, connection->acceptor_signaling_packet.signal_identifier, &sep.configuration, connection->acceptor_signaling_packet.command+offset, packet_size-offset);
                    if (connection->error_code){
                        // fire configuration parsing errors 
                        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        break;
                    }

                    // find sep or raise error
                    if (!is_avdtp_remote_seid_registered(stream_endpoint)){
                        log_info("REJECT AVDTP_SI_RECONFIGURE, BAD_ACP_SEID");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = AVDTP_ERROR_CODE_BAD_ACP_SEID;
                        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                        break;
                    }
                    stream_endpoint->remote_sep.configured_service_categories = sep.configured_service_categories;
                    stream_endpoint->remote_sep.configuration = sep.configuration;
                    
                    log_info("update active remote seid %d", stream_endpoint->remote_sep.seid);

					// if media codec configuration updated, copy configuration and emit event
					if ((sep.configured_service_categories & (1 << AVDTP_MEDIA_CODEC)) != 0){
						if (stream_endpoint->media_codec_configuration_len == sep.configuration.media_codec.media_codec_information_len){
							(void) memcpy(stream_endpoint->media_codec_configuration_info, sep.configuration.media_codec.media_codec_information, stream_endpoint->media_codec_configuration_len);
                            stream_endpoint->sep.configuration.media_codec = stream_endpoint->remote_configuration.media_codec;
                            avdtp_signaling_emit_configuration(stream_endpoint, connection->avdtp_cid, 1, &sep.configuration, sep.configured_service_categories);
						}
					}
                    break;
                }

                case AVDTP_SI_GET_CONFIGURATION:
                    log_info("W2_ANSWER_GET_CONFIGURATION");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_GET_CONFIGURATION;
                    break;
                case AVDTP_SI_OPEN:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURED){
                        log_info("REJECT AVDTP_SI_OPEN, BAD_STATE");
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                        break;
                    }
                    log_info("AVDTP_STREAM_ENDPOINT_W2_ANSWER_OPEN_STREAM");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_OPEN_STREAM;
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
                    connection->acceptor_local_seid = stream_endpoint->sep.seid;
                    break;
                case AVDTP_SI_START:
                    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_OPENED){
                        log_info("REJECT AVDTP_SI_START, BAD_STATE, state %d", stream_endpoint->state);
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                        connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                        break;
                    }
#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
                    log_info("W2_ACCEPT_START_STREAM");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W4_USER_CONFIRM_START_STREAM;
#else 
                    log_info("W2_ANSWER_START_STREAM");
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_START_STREAM;
#endif
                    break;
                case AVDTP_SI_CLOSE:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            log_info("W2_ANSWER_CLOSE_STREAM");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_CLOSE_STREAM;
                            break;
                        default:
                            log_info("AVDTP_SI_CLOSE, bad state %d ", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_ABORT:
                     switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_CONFIGURED:
                        case AVDTP_STREAM_ENDPOINT_CLOSING:
                        case AVDTP_STREAM_ENDPOINT_OPENED:
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            log_info("W2_ANSWER_ABORT_STREAM");
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_ABORT_STREAM;
                            break;
                        default:
                            log_info("AVDTP_SI_ABORT, bad state %d ", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                            connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                case AVDTP_SI_SUSPEND:
                    switch (stream_endpoint->state){
                        case AVDTP_STREAM_ENDPOINT_STREAMING:
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                            connection->num_suspended_seids--;
                            if (connection->num_suspended_seids <= 0){
                                log_info("W2_ANSWER_SUSPEND_STREAM");
                                stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_ACCEPT_SUSPEND_STREAM;
                            }
                            break;
                        default:
                            log_info("AVDTP_SI_SUSPEND, bad state %d", stream_endpoint->state);
                            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            connection->error_code = AVDTP_ERROR_CODE_BAD_STATE;
                            connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                            break;
                    }
                    break;
                default:
                    log_info("NOT IMPLEMENTED, Reject signal_identifier %02x", connection->acceptor_signaling_packet.signal_identifier);
                    stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD;
                    connection->reject_signal_identifier = connection->acceptor_signaling_packet.signal_identifier;
                    break;
            }
            break;
        default:
            return;
    }

    if (!request_to_send){
        log_info("NOT IMPLEMENTED");
    }
	avdtp_request_can_send_now_acceptor(connection);
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

void avdtp_acceptor_stream_config_subsm_run(avdtp_connection_t *connection) {
    int sent = 1;
    btstack_linked_list_t * stream_endpoints = avdtp_get_stream_endpoints();

    switch (connection->acceptor_connection_state){
        case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS:
            connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
            avdtp_acceptor_send_seps_response(connection->l2cap_signaling_cid, connection->acceptor_transaction_label, (avdtp_stream_endpoint_t *) stream_endpoints);
            avdtp_signaling_emit_accept(connection->avdtp_cid, 0, connection->acceptor_signaling_packet.signal_identifier, false);
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
        log_info("DONE");
        return;      
    } 
    
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(connection->acceptor_local_seid);
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
    avdtp_sep_t * remote_sep;

    bool emit_accept = false;
    bool emit_reject = false;
    
    switch (acceptor_config_state){
        case AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE:
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_GET_CAPABILITIES:
            avdtp_prepare_capabilities(&connection->acceptor_signaling_packet, trid, stream_endpoint->sep.registered_service_categories, stream_endpoint->sep.capabilities, AVDTP_SI_GET_CAPABILITIES);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->acceptor_signaling_packet, out_buffer);
            if ((connection->acceptor_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->acceptor_signaling_packet.packet_type != AVDTP_END_PACKET)){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("fragmented");
            } else {
                log_info("ACP:DONE");
                emit_accept = true;
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_DELAY_REPORT:
            log_info("DONE ");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_DELAYREPORT);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_GET_ALL_CAPABILITIES:
            avdtp_prepare_capabilities(&connection->acceptor_signaling_packet, trid, stream_endpoint->sep.registered_service_categories, stream_endpoint->sep.capabilities, AVDTP_SI_GET_ALL_CAPABILITIES);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->acceptor_signaling_packet, out_buffer);
            if ((connection->acceptor_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->acceptor_signaling_packet.packet_type != AVDTP_END_PACKET)){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("fragmented");
            } else {
                log_info("ACP:DONE");
                emit_accept = true;
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_SET_CONFIGURATION:
            log_info("DONE");
            log_info("    -> AVDTP_STREAM_ENDPOINT_CONFIGURED");
            stream_endpoint->connection = connection;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
            connection->configuration_state = AVDTP_CONFIGURATION_STATE_REMOTE_CONFIGURED;
            // TODO: consider reconfiguration
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SET_CONFIGURATION);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_RECONFIGURE:
            log_info("DONE ");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_RECONFIGURE);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_GET_CONFIGURATION:
            remote_sep = &stream_endpoint->remote_sep;
            avdtp_prepare_capabilities(&connection->acceptor_signaling_packet, trid, remote_sep->configured_service_categories, remote_sep->configuration, AVDTP_SI_GET_CONFIGURATION);
            l2cap_reserve_packet_buffer();
            out_buffer = l2cap_get_outgoing_buffer();
            pos = avdtp_signaling_create_fragment(cid, &connection->acceptor_signaling_packet, out_buffer);
            if ((connection->acceptor_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->acceptor_signaling_packet.packet_type != AVDTP_END_PACKET)){
                stream_endpoint->acceptor_config_state = acceptor_config_state;
                log_info("fragmented");
            } else {
                log_info("ACP:DONE");
                emit_accept = true;
            }
            l2cap_send_prepared(cid, pos);
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_OPEN_STREAM:
            log_info("DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_OPEN);
            emit_accept = true;
            break;

#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
        case AVDTP_ACCEPTOR_W4_USER_CONFIRM_START_STREAM:
            // keep state until user calls API to confirm or reject starting the stream
            stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_W4_USER_CONFIRM_START_STREAM;
            avdtp_signaling_emit_accept(connection->avdtp_cid, avdtp_local_seid(stream_endpoint), AVDTP_SI_ACCEPT_START, false);
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_START_STREAM:
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
            connection->acceptor_signaling_packet.signal_identifier = AVDTP_SI_START;
            emit_reject = true;
            avdtp_acceptor_send_response_reject(cid, AVDTP_SI_START, trid);
            break;
#endif
        case AVDTP_ACCEPTOR_W2_ACCEPT_START_STREAM:
            log_info("DONE ");
            log_info("    -> AVDTP_STREAM_ENDPOINT_STREAMING ");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_START);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_CLOSE_STREAM:
            log_info("DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_CLOSE);
            connection->configuration_state = AVDTP_CONFIGURATION_STATE_IDLE;
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_ABORT_STREAM:
            log_info("DONE");
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_ABORT);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_ACCEPT_SUSPEND_STREAM:
            log_info("DONE");
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
            avdtp_acceptor_send_accept_response(cid, trid, AVDTP_SI_SUSPEND);
            emit_accept = true;
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD:
            log_info("DONE REJECT");
            connection->reject_signal_identifier = AVDTP_SI_NONE;
            avdtp_acceptor_send_response_reject(cid, reject_signal_identifier, trid);
            emit_reject = true;
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
            log_info("DONE REJECT CATEGORY");
            connection->reject_service_category = 0;
            avdtp_acceptor_send_response_reject_service_category(cid, reject_signal_identifier, reject_service_category, error_code, trid);
            emit_reject = true;
            break;
        case AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
            log_info("DONE REJECT");
            connection->reject_signal_identifier = AVDTP_SI_NONE;
            connection->error_code = 0;
            avdtp_acceptor_send_response_reject_with_error_code(cid, reject_signal_identifier, error_code, trid);
            emit_reject = true;
            break;
        default:  
            log_info("NOT IMPLEMENTED");
            sent = 0;
            break;
    }

    if (emit_accept == true){
        avdtp_signaling_emit_accept(connection->avdtp_cid, avdtp_local_seid(stream_endpoint),
                                    connection->acceptor_signaling_packet.signal_identifier, false);
    } else if (emit_reject == true){
        avdtp_signaling_emit_reject(connection->avdtp_cid, avdtp_local_seid(stream_endpoint),
                                    connection->acceptor_signaling_packet.signal_identifier, false);
    }
    // check fragmentation
    if ((connection->acceptor_signaling_packet.packet_type != AVDTP_SINGLE_PACKET) && (connection->acceptor_signaling_packet.packet_type != AVDTP_END_PACKET)){
		avdtp_request_can_send_now_acceptor(connection);
    }
}

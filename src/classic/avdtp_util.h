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

/*
 * avdtp_util.h
 * 
 * Audio/Video Distribution Transport Protocol - Utils
 *
 */

#ifndef AVDTP_UITL_H
#define AVDTP_UITL_H

#include <stdint.h>
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

avdtp_connection_t * avdtp_connection_for_bd_addr(bd_addr_t addr, avdtp_context_t * context);
avdtp_connection_t * avdtp_connection_for_avdtp_cid(uint16_t l2cap_cid, avdtp_context_t * context);
avdtp_connection_t * avdtp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid, avdtp_context_t * context);
avdtp_stream_endpoint_t * avdtp_stream_endpoint_for_l2cap_cid(uint16_t l2cap_cid, avdtp_context_t * context);
avdtp_stream_endpoint_t * avdtp_stream_endpoint_with_seid(uint8_t seid, avdtp_context_t * context);
avdtp_stream_endpoint_t * avdtp_stream_endpoint_associated_with_acp_seid(uint16_t acp_seid, avdtp_context_t * context);
avdtp_stream_endpoint_t * avdtp_stream_endpoint_for_seid(uint16_t seid, avdtp_context_t * context);
avdtp_stream_endpoint_t * avdtp_stream_endpoint_for_signaling_cid(uint16_t l2cap_cid, avdtp_context_t * context);

static inline uint8_t avdtp_header(uint8_t tr_label, avdtp_packet_type_t packet_type, avdtp_message_type_t msg_type){
    return (tr_label<<4) | ((uint8_t)packet_type<<2) | (uint8_t)msg_type;
}

int     avdtp_read_signaling_header(avdtp_signaling_packet_t * signaling_header, uint8_t * packet, uint16_t size);

uint16_t store_bit16(uint16_t bitmap, int position, uint8_t value);
int     get_bit16(uint16_t bitmap, int position);

int avdtp_pack_service_capabilities(uint8_t * buffer, int size, avdtp_capabilities_t caps, avdtp_service_category_t category, uint8_t pack_all_capabilities);
uint16_t avdtp_unpack_service_capabilities(avdtp_connection_t * connection, avdtp_capabilities_t * caps, uint8_t * packet, uint16_t size);

void avdtp_prepare_capabilities(avdtp_signaling_packet_t * signaling_packet, uint8_t transaction_label, uint16_t registered_service_categories, avdtp_capabilities_t configuration, uint8_t identifier);
int avdtp_signaling_create_fragment(uint16_t cid, avdtp_signaling_packet_t * signaling_packet, uint8_t * out_buffer);

void avdtp_signaling_emit_connection_established(btstack_packet_handler_t callback, uint16_t avdtp_cid, bd_addr_t addr, uint8_t status);
void avdtp_streaming_emit_connection_established(btstack_packet_handler_t callback, uint16_t avdtp_cid, bd_addr_t addr, uint8_t int_seid, uint8_t acp_seid, uint8_t status);

void avdtp_signaling_emit_connection_released(btstack_packet_handler_t callback, uint16_t avdtp_cid);
void avdtp_streaming_emit_connection_released(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t local_seid);

void avdtp_signaling_emit_sep(btstack_packet_handler_t callback, uint16_t avdtp_cid, avdtp_sep_t sep);
void avdtp_signaling_emit_sep_done(btstack_packet_handler_t callback, uint16_t avdtp_cid);
void avdtp_signaling_emit_delay(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t local_seid, uint16_t delay);

void avdtp_signaling_emit_accept(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t seid, avdtp_signal_identifier_t identifier);
void avdtp_signaling_emit_general_reject(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, avdtp_signal_identifier_t identifier);
void avdtp_signaling_emit_reject(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, avdtp_signal_identifier_t identifier);
void avdtp_streaming_emit_can_send_media_packet_now(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, uint16_t sequence_number);

void avdtp_emit_capabilities(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, avdtp_capabilities_t * configuration, uint16_t configured_service_categories);
void avdtp_emit_configuration(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, avdtp_capabilities_t * configuration, uint16_t configured_service_categories);

void avdtp_signaling_emit_media_codec_sbc_configuration(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, avdtp_media_type_t media_type, const uint8_t * media_codec_information);
void avdtp_signaling_emit_media_codec_other_configuration(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, adtvp_media_codec_capabilities_t media_codec);
void avdtp_signaling_emit_media_codec_sbc_reconfiguration(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, avdtp_media_type_t media_type, const uint8_t * media_codec_information);
void avdtp_signaling_emit_media_codec_other_reconfiguration(btstack_packet_handler_t callback, uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, adtvp_media_codec_capabilities_t media_codec);

uint8_t avdtp_request_can_send_now_acceptor(avdtp_connection_t * connection, uint16_t l2cap_cid);
uint8_t avdtp_request_can_send_now_initiator(avdtp_connection_t * connection, uint16_t l2cap_cid);
uint8_t avdtp_request_can_send_now_self(avdtp_connection_t * connection, uint16_t l2cap_cid);

void avdtp_reset_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

// uint16_t avdtp_cid(avdtp_stream_endpoint_t * stream_endpoint);
uint8_t  avdtp_local_seid(avdtp_stream_endpoint_t * stream_endpoint);
uint8_t  avdtp_remote_seid(avdtp_stream_endpoint_t * stream_endpoint);
const char * avdtp_si2str(uint16_t index);

void a2dp_streaming_emit_connection_established(btstack_packet_handler_t callback, uint16_t cid, bd_addr_t addr, uint8_t local_seid, uint8_t remote_seid, uint8_t status);

#if defined __cplusplus
}
#endif

#endif // AVDTP_UITL_H

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

/**
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

#define AVDTP_MAX_MEDIA_CODEC_INFORMATION_LENGTH 30

    // assume AVDTP_MEDIA_CONFIG_OTHER_EVENT_LEN greater or equal to all others
#define AVDTP_MEDIA_CONFIG_SBC_EVENT_LEN 18
#define AVDTP_MEDIA_CONFIG_MPEG_AUDIO_EVENT_LEN 18
#define AVDTP_MEDIA_CONFIG_MPEG_AAC_EVENT_LEN 18
#define AVDTP_MEDIA_CONFIG_ATRAC_EVENT_LEN 18
#define AVDTP_MEDIA_CONFIG_OTHER_EVENT_LEN (13 + AVDTP_MAX_MEDIA_CODEC_INFORMATION_LENGTH)

static inline uint8_t avdtp_header(uint8_t tr_label, avdtp_packet_type_t packet_type, avdtp_message_type_t msg_type){
    return (tr_label<<4) | ((uint8_t)packet_type<<2) | (uint8_t)msg_type;
}

avdtp_message_type_t avdtp_get_signaling_packet_type(uint8_t * packet);

int     avdtp_read_signaling_header(avdtp_signaling_packet_t * signaling_header, uint8_t * packet, uint16_t size);

uint16_t store_bit16(uint16_t bitmap, int position, uint8_t value);
int     get_bit16(uint16_t bitmap, int position);

int avdtp_pack_service_capabilities(uint8_t * buffer, int size, avdtp_capabilities_t caps, avdtp_service_category_t category);
uint16_t avdtp_unpack_service_capabilities(avdtp_connection_t * connection, avdtp_signal_identifier_t signal_identifier, avdtp_capabilities_t * caps, uint8_t * packet, uint16_t size);

void avdtp_prepare_capabilities(avdtp_signaling_packet_t * signaling_packet, uint8_t transaction_label, uint16_t service_categories, avdtp_capabilities_t capabilities, uint8_t identifier);
int avdtp_signaling_create_fragment(uint16_t cid, avdtp_signaling_packet_t * signaling_packet, uint8_t * out_buffer);

void avdtp_signaling_emit_connection_established(uint16_t avdtp_cid, bd_addr_t addr, hci_con_handle_t con_handle, uint8_t status);

void avdtp_signaling_emit_connection_released(uint16_t avdtp_cid);

void avdtp_signaling_emit_sep(uint16_t avdtp_cid, avdtp_sep_t sep);

void avdtp_signaling_emit_sep_done(uint16_t avdtp_cid);

void avdtp_signaling_emit_accept(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier,
                                 bool is_initiator);
void avdtp_signaling_emit_accept_for_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint, uint8_t local_seid,
                                                     avdtp_signal_identifier_t identifier, bool is_initiator);

void avdtp_signaling_emit_general_reject(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier,
                                         bool is_initiator);
void avdtp_signaling_emit_reject(uint16_t avdtp_cid, uint8_t local_seid, avdtp_signal_identifier_t identifier,
                                 bool is_initiator);

void avdtp_signaling_emit_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_capabilities_t *capabilities,
									   uint16_t registered_service_categories);

void avdtp_signaling_emit_delay(uint16_t avdtp_cid, uint8_t local_seid, uint16_t delay);

void
avdtp_signaling_emit_configuration(avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid, uint8_t reconfigure,
                                   avdtp_capabilities_t *configuration, uint16_t configured_service_categories);

uint16_t avdtp_setup_media_codec_config_event(uint8_t *event, uint16_t size, const avdtp_stream_endpoint_t *stream_endpoint,
                                              uint16_t avdtp_cid, uint8_t reconfigure,
                                              const adtvp_media_codec_capabilities_t * media_codec);

void avdtp_streaming_emit_connection_established(avdtp_stream_endpoint_t *stream_endpoint, uint8_t status);

void avdtp_streaming_emit_connection_released(avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid, uint8_t local_seid);

void avdtp_streaming_emit_can_send_media_packet_now(avdtp_stream_endpoint_t *stream_endpoint, uint16_t sequence_number);

uint8_t avdtp_request_can_send_now_acceptor(avdtp_connection_t *connection);

uint8_t avdtp_request_can_send_now_initiator(avdtp_connection_t *connection);

void avdtp_reset_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

// uint16_t avdtp_cid(avdtp_stream_endpoint_t * stream_endpoint);
uint8_t  avdtp_local_seid(const avdtp_stream_endpoint_t * stream_endpoint);
uint8_t  avdtp_remote_seid(const avdtp_stream_endpoint_t * stream_endpoint);
const char * avdtp_si2str(uint16_t index);

// helper to set/get configuration
void avdtp_config_sbc_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz);
void avdtp_config_sbc_store(uint8_t * config, const avdtp_configuration_sbc_t * configuration);
void avdtp_config_mpeg_audio_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz);
void avdtp_config_mpeg_audio_store(uint8_t * config,  const avdtp_configuration_mpeg_audio_t * configuration);
void avdtp_config_mpeg_aac_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz);
void avdtp_config_mpeg_aac_store(uint8_t * config, const avdtp_configuration_mpeg_aac_t * configuration);
void avdtp_config_atrac_set_sampling_frequency(uint8_t * config, uint16_t sampling_frequency_hz);
void avdtp_config_atrac_store(uint8_t * config, const avdtp_configuration_atrac_t * configuration);

#if defined __cplusplus
}
#endif

#endif // AVDTP_UITL_H

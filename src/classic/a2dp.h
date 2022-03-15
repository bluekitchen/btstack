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
 * Audio/Video Distribution Transport Protocol (A2DP)
 *
 * Code shared by A2DP Sink and A2DP Sourece, mainly automatic stream endpoitn configuration
 */

#ifndef A2DP_H
#define A2DP_H

#include <stdint.h>
#include "btstack_defines.h"
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

// common
void a2dp_init(void);

void a2dp_deinit(void);

void a2dp_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t service_class_uuid, uint16_t supported_features, const char * service_name, const char * service_provider_name);

// source
void a2dp_register_source_packet_handler(btstack_packet_handler_t callback);

void a2dp_emit_source(uint8_t * packet, uint16_t size);

void a2dp_replace_subevent_id_and_emit_source(uint8_t * packet, uint16_t size, uint8_t subevent_id);

void a2dp_emit_source_stream_event(uint16_t cid, uint8_t local_seid, uint8_t subevent_id);

void a2dp_emit_source_streaming_connection_failed(avdtp_connection_t *connection, uint8_t status);

void a2dp_emit_source_stream_reconfigured(uint16_t cid, uint8_t local_seid, uint8_t status);

void a2dp_source_ready_for_sep_discovery(avdtp_connection_t * connection);

uint8_t a2dp_source_config_init(avdtp_connection_t *connection, uint8_t local_seid, uint8_t remote_seid,
                                avdtp_media_codec_type_t codec_type);

void a2dp_source_config_process_avdtp_event_handler(uint8_t *packet, uint16_t size);

#if defined __cplusplus
}
#endif

#endif // A2DP_H

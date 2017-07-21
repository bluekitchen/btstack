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
 * a2dp_sink.h
 * 
 * Advanced Audio Distribution Transport Profile (A2DP) Sink
 *
 * A2DP Sink is a device that accepts streamed media data.  
 */

#ifndef __A2DP_SINK_H
#define __A2DP_SINK_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
	
/**
 * @brief A2DP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see a2dp_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void a2dp_sink_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Set up A2DP Sink device.
 */
void a2dp_sink_init(void);

uint8_t a2dp_sink_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, 
	uint8_t * codec_capabilities, uint16_t codec_capabilities_len,
	uint8_t * codec_configuration, uint16_t codec_configuration_len);

/**
 * @brief Register callback for the A2DP Sink client. 
 * @param callback
 */
void a2dp_sink_register_packet_handler(btstack_packet_handler_t callback);


/**
 * @brief Register media handler for the A2DP Sink client. 
 * @param callback
 */
void a2dp_sink_register_media_handler(void (*callback)(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size));

/**
 * @brief Open stream
 * @param bd_addr
 * @param local_seid
 * @param avdtp_cid
 */
uint8_t a2dp_sink_establish_stream(bd_addr_t bd_addr, uint8_t local_seid, uint16_t * avdtp_cid);


/**
 * @brief Start stream
 * @param local_seid
 */
void a2dp_sink_start_stream(uint8_t local_seid);

/**
 * @brief Suspend stream
 * @param local_seid
 */
void a2dp_sink_pause(uint8_t local_seid);

/**
 * @brief Abort stream
 * @param local_seid
 */
void a2dp_sink_stop_stream(uint8_t local_seid);

void a2dp_sink_disconnect(uint16_t a2dp_cid);
/* API_END */


#if defined __cplusplus
}
#endif

#endif // __A2DP_SINK_H
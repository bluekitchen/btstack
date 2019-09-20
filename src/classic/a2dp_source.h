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
 * a2dp_source.h
 * 
 * Advanced Audio Distribution Transport Profile (A2DP) Source
 *
 * A2DP Source is a device that streames media data.
 */

#ifndef A2DP_SOURCE_H
#define A2DP_SOURCE_H

#include <stdint.h>
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Create A2DP Source service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVDTP_SOURCE_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void a2dp_source_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Initialize up A2DP Source device.
 */
void a2dp_source_init(void);

/**
 * @brief Create a stream endpoint of type SOURCE, and register media codec by specifying its capabilities and the default configuration.
 * @param media_type    			See avdtp_media_type_t values in avdtp.h (audio, video or multimedia).
 * @param media_codec_type 			See avdtp_media_codec_type_t values in avdtp.h 
 * @param codec_capabilities        Media codec capabilities as defined in A2DP spec, section 4 - Audio Codec Interoperability Requirements.
 * @param codec_capabilities_len	Media codec capabilities length.
 * @param codec_configuration 		Default media codec configuration.
 * @param codec_configuration_len	Media codec configuration length. 
 *
 * @return local_stream_endpoint 				
 */
avdtp_stream_endpoint_t * a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, 
	uint8_t * codec_capabilities, uint16_t codec_capabilities_len,
	uint8_t * codec_configuration, uint16_t codec_configuration_len);

/**
 * @brief Register callback for the A2DP Source client. It will receive following subevents of HCI_EVENT_A2DP_META HCI event type: 
 * - A2DP_SUBEVENT_INCOMING_CONNECTION_ESTABLISHED:		        Received when signaling connection with a remote is established .
 * - A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:				Received when signaling connection with a remote is released .
 * - A2DP_SUBEVENT_STREAM_ESTABLISHED:							Received when stream to a remote device is established.
 * - A2DP_SUBEVENT_STREAM_STARTED:								Received when stream is started.
 * - A2DP_SUBEVENT_STREAM_SUSPENDED:							Received when stream is paused.
 * - A2DP_SUBEVENT_STREAM_STOPED:							    received when stream is aborted or stopped.
 * - A2DP_SUBEVENT_STREAM_RELEASED:								Received when stream is released.
 * - A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:			Indicates that the next media packet can be sent.
 *
 * @param callback
 */
void a2dp_source_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Open stream.
 * @param remote
 * @param local_seid	 	ID assigned to a local stream endpoint
 * @param out_a2dp_cid 		Assigned A2DP channel identifyer used for furhter A2DP commands. 
 */
uint8_t a2dp_source_establish_stream(bd_addr_t remote, uint8_t local_seid, uint16_t * out_a2dp_cid);

    /**
     * @brief Reconfigure stream.
     * @param local_seid	 	  ID assigned to a local stream endpoint
     * @param sampling_frequency  New sampling frequency to use. Cannot be called while stream is active
     */
uint8_t a2dp_source_reconfigure_stream_sampling_frequency(uint16_t a2dp_cid, uint32_t sampling_frequency);

/**
 * @brief Start stream.
 * @param a2dp_cid 			A2DP channel identifyer.
 * @param local_seid	 	ID of a local stream endpoint.
 */
uint8_t a2dp_source_start_stream(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Pause stream.
 * @param a2dp_cid 			A2DP channel identifyer.
 * @param local_seid  		ID of a local stream endpoint.
 */
uint8_t a2dp_source_pause_stream(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Release stream and disconnect from remote. 
 * @param a2dp_cid 			A2DP channel identifyer.
 */
uint8_t a2dp_source_disconnect(uint16_t a2dp_cid);

/**
 * @brief Request to send a media packet. Packet can be then sent on reception of A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW event.
 * @param a2dp_cid 			A2DP channel identifyer.
 * @param local_seid  		ID of a local stream endpoint.
 */
void 	a2dp_source_stream_endpoint_request_can_send_now(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Return maximal media payload size, does not include media header.
 * @param a2dp_cid 			A2DP channel identifyer.
 * @param local_seid  		ID of a local stream endpoint.
 * @return max_media_payload_size_without_media_header
 */
int 	a2dp_max_media_payload_size(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Send media payload.
 * @param a2dp_cid 			A2DP channel identifyer.
 * @param local_seid  		ID of a local stream endpoint.
 * @param storage
 * @param num_bytes_to_copy
 * @param num_frames
 * @param marker
 * @return max_media_payload_size_without_media_header
 */
int  	a2dp_source_stream_send_media_payload(uint16_t a2dp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // A2DP_SOURCE_H

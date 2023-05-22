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
 * @title A2DP Sink
 *
 * Audio/Video Distribution Transport Protocol A2DP Sink is a device that accepts streamed media data.  
 *
 */

#ifndef A2DP_SINK_H
#define A2DP_SINK_H

#include <stdint.h>
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
	
/**
 * @brief Create A2DP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void a2dp_sink_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Initialize up A2DP Sink device.
 */
void a2dp_sink_init(void);

/**
 * @brief Create a stream endpoint of type SINK, and register media codec by specifying its capabilities and the default configuration.
 * @param media_type    			see avdtp_media_type_t values in avdtp.h (audio, video or multimedia)
 * @param media_codec_type 			see avdtp_media_codec_type_t values in avdtp.h 
 * @param codec_capabilities        media codec capabilities as defined in A2DP spec, section 4 - Audio Codec Interoperability Requirements.
 * @param codec_capabilities_len	media codec capabilities length
 * @param codec_configuration 		default media codec configuration
 * @param codec_configuration_len	media codec configuration length 
 *
 * @return local_stream_endpoint 	
 */
avdtp_stream_endpoint_t *  a2dp_sink_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type,
                                                            const uint8_t *codec_capabilities, uint16_t codec_capabilities_len,
                                                            uint8_t * codec_configuration, uint16_t codec_configuration_len);

/**
 *  @brief Unregister stream endpoint and free it's memory
 *  @param stream_endpoint created by a2dp_sink_create_stream_endpoint
 */
void a2dp_sink_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

/**
 * @brief Register callback for the A2DP Sink client. It will receive following subevents of HCI_EVENT_A2DP_META HCI event type: 
 * - A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:		indicates from remote chosen SBC media codec configuration 
 * - A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:	indicates from remote chosen other then SBC media codec configuration 
 * - A2DP_SUBEVENT_STREAM_ESTABLISHED:							received when stream to a remote device is established
 * - A2DP_SUBEVENT_STREAM_STARTED:								received when stream is started
 * - A2DP_SUBEVENT_STREAM_SUSPENDED:							received when stream is paused
 * - A2DP_SUBEVENT_STREAM_STOPED:							    received when stream is aborted or stopped
 * - A2DP_SUBEVENT_STREAM_RELEASED:								received when stream is released
 * - A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED: 				received when signaling channel is disconnected
 *
 * @param callback
 */
void a2dp_sink_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Register media handler for the A2DP Sink client. 
 * @param callback
 * @param packet
 * @param size
 */
void a2dp_sink_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size));

/**
 * @brief Establish stream.
 * @param remote
 * @param out_a2dp_cid 		Assigned A2DP channel identifier used for furhter A2DP commands.
 */
uint8_t a2dp_sink_establish_stream(bd_addr_t remote, uint16_t * out_a2dp_cid);

#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
/**
 * @brief Accept starting the stream on A2DP_SUBEVENT_START_STREAM_REQUESTED event.
 * @param a2dp_cid      A2DP channel identifier.   
 * @param local_seid        ID of a local stream endpoint.
 */
uint8_t a2dp_sink_start_stream_accept(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Reject starting the stream on A2DP_SUBEVENT_START_STREAM_REQUESTED event.
 * @param a2dp_cid      A2DP channel identifier.
 * @param local_seid        ID of a local stream endpoint.
 */
uint8_t a2dp_sink_start_stream_reject(uint16_t a2dp_cid, uint8_t local_seid);
#endif

/**
 * @brief Release stream and disconnect from remote.
 * @param a2dp_cid 			A2DP channel identifier.
 */
void a2dp_sink_disconnect(uint16_t a2dp_cid);

/**
 * @brief Select and configure SBC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param configuration     SBC Configuration
 * @return status
 */
uint8_t a2dp_sink_set_config_sbc(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_sbc_t * configuration);

/**
 * @brief Select and configure MPEG AUDIO endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param configuration     MPEG AUDIO Configuration
 * @return status
 */
uint8_t a2dp_sink_set_config_mpeg_audio(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_mpeg_audio_t * configuration);

/**
 * @brief Select and configure MPEG AAC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param configuration     MPEG AAC Configuration
 * @return status
 */
uint8_t a2dp_sink_set_config_mpeg_aac(uint16_t a2dp_cid,  uint8_t local_seid,  uint8_t remote_seid, const avdtp_configuration_mpeg_aac_t * configuration);

/**
 * @brief Select and configure ATRAC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param configuration     ATRAC Configuration
 * @return status
 */
uint8_t a2dp_sink_set_config_atrac(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_atrac_t * configuration);

/**
 * @brief Select and configure Non-A2DP endpoint. Bytes 0-3 of codec information contain Vendor ID, bytes 4-5 contain Vendor Specific Codec ID (little endian)
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param media_codec_information
 * @param media_codec_information_len
 * @return status
 */
uint8_t a2dp_sink_set_config_other(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, const uint8_t * media_codec_information, uint8_t media_codec_information_len);

/**
 * @brief Register media configuration validator. Can reject insuitable configuration or report stream endpoint as currently busy
 * @note validator has to return AVDTP error codes like: AVDTP_ERROR_CODE_SEP_IN_USE or AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION
 *       the callback receives the media configuration in the same format as the existing A2dP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION
 *       and similar
 * @param callback
 */
void a2dp_sink_register_media_config_validator(uint8_t (*callback)(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size));

/**
 * @brief De-Init A2DP Sink device.
 */
void a2dp_sink_deinit(void);

/* API_END */


#if defined __cplusplus
}
#endif

#endif // A2DP_SINK_H

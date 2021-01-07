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
 *  @brief Unregister stream endpoint and free it's memory
 *  @param stream_endpoint created by a2dp_source_create_stream_endpoint
 */
void a2dp_source_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

/**
 * @brief Register callback for the A2DP Source client. It will receive following subevents of HCI_EVENT_A2DP_META HCI event type: 
 * - A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:			Indicates that the next media packet can be sent.
 *
 * - A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED             Received when signaling connection with a remote is established.
 * - A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED                Received when signaling connection with a remote is released
 * - A2DP_SUBEVENT_STREAM_ESTABLISHED                           Received when stream to a remote device is established.
 * - A2DP_SUBEVENT_STREAM_STARTED                               Received when stream is started.
 * - A2DP_SUBEVENT_STREAM_SUSPENDED                             Received when stream is paused.
 * - A2DP_SUBEVENT_STREAM_STOPED                                received when stream is aborted or stopped.
 * - A2DP_SUBEVENT_STREAM_RELEASED                              Received when stream is released.
 * - A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY         Currently the only capability that is passed to client
 * - A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE                  Signals that all capabilities are reported
 * - A2DP_SUBEVENT_SIGNALING_DELAY_REPORT                       Delay report
 * - A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION      SBC configuration
 * - A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW          Signals that a media packet can be sent
 * - A2DP_SUBEVENT_COMMAND_REJECTED                             Command reject
 * @param callback
 */
void a2dp_source_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Open stream.
 * @param remote_addr
 * @param avdtp_cid 		Assigned A2DP channel identifier used for further A2DP commands.
 */
uint8_t a2dp_source_establish_stream(bd_addr_t remote_addr, uint16_t *avdtp_cid);

    /**
     * @brief Reconfigure stream.
     * @param local_seid	 	  ID assigned to a local stream endpoint
     * @param sampling_frequency  New sampling frequency to use. Cannot be called while stream is active
     */
uint8_t a2dp_source_reconfigure_stream_sampling_frequency(uint16_t a2dp_cid, uint32_t sampling_frequency);

/**
 * @brief Start stream.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid	 	ID of a local stream endpoint.
 */
uint8_t a2dp_source_start_stream(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Pause stream.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 */
uint8_t a2dp_source_pause_stream(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Release stream and disconnect from remote. 
 * @param a2dp_cid 			A2DP channel identifier.
 */
uint8_t a2dp_source_disconnect(uint16_t a2dp_cid);

/**
 * @brief Request to send a media packet. Packet can be then sent on reception of A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW event.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 */
void 	a2dp_source_stream_endpoint_request_can_send_now(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Return maximal media payload size, does not include media header.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @return max_media_payload_size_without_media_header
 */
int 	a2dp_max_media_payload_size(uint16_t a2dp_cid, uint8_t local_seid);

/**
 * @brief Send media payload.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param storage
 * @param num_bytes_to_copy
 * @param num_frames
 * @param marker
 * @return max_media_payload_size_without_media_header
 */
int  	a2dp_source_stream_send_media_payload(uint16_t a2dp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker);

/**
 * @brief Send media payload.
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param marker
 * @param payload
 * @param payload_size
 * @param marker
 * @return status
 */
uint8_t a2dp_source_stream_send_media_payload_rtp(uint16_t a2dp_cid, uint8_t local_seid, uint8_t marker, uint8_t * payload, uint16_t payload_size);

/**
 * @brief Send media packet
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param packet
 * @param size
 * @return status
 */
uint8_t	a2dp_source_stream_send_media_packet(uint16_t a2dp_cid, uint8_t local_seid, const uint8_t * packet, uint16_t size);

/**
 * @brief Select and configure SBC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param sampling_frequency
 * @param channel_mode
 * @param block_length
 * @param subbands
 * @param allocation_method
 * @param min_bitpool_value
 * @param max_bitpool_value
 * @return status
 */
uint8_t a2dp_source_set_config_sbc(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t sampling_frequency, avdtp_channel_mode_t channel_mode,
                                   uint8_t block_length, uint8_t subbands, avdtp_sbc_allocation_method_t  allocation_method, uint8_t min_bitpool_value, uint8_t max_bitpool_value);

/**
 * @brief Select and configure MPEG AUDIO endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param layer
 * @param crc
 * @param channel_mode
 * @param media_payload_format
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 * @return status
 */
uint8_t a2dp_source_set_config_mpeg_audio(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, avdtp_mpeg_layer_t layer, uint8_t crc,
                                          avdtp_channel_mode_t channel_mode, uint8_t media_payload_format,
                                          uint16_t sampling_frequency, uint8_t vbr, uint8_t bit_rate_index);

/**
 * @brief Select and configure MPEG AAC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param object_type
 * @param sampling_frequency
 * @param channels
 * @param bit_rate
 * @param vbr
 * @return status
 */
uint8_t a2dp_source_set_config_mpeg_aac(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, avdtp_aac_object_type_t object_type,
                                        uint32_t sampling_frequency, uint8_t channels, uint32_t bit_rate, uint8_t vbr);

/**
 * @brief Select and configure ATRAC endpoint
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param media_type
 * @param version
 * @param channel_mode
 * @param sampling_frequency
 * @param vbr
 * @param bit_rate_index
 * @param maximum_sul
 * @return status
 */
uint8_t a2dp_source_set_config_atrac(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, avdtp_atrac_version_t version,
                                     avdtp_channel_mode_t channel_mode, uint16_t sampling_frequency,
                                     uint8_t vbr, uint8_t bit_rate_index, uint16_t maximum_sul);

/**
 * @brief Select and configure Non-A2DP endpoint. Bytes 0-3 of codec information contain Vendor ID, bytes 4-5 contain Vendor Specific Codec ID (little endian)
 * @param a2dp_cid 			A2DP channel identifier.
 * @param local_seid  		ID of a local stream endpoint.
 * @param remote_seid  		ID of a remote stream endpoint.
 * @param media_codec_information
 * @param media_codec_information_len
 * @return status
 */
uint8_t a2dp_source_set_config_other(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, const uint8_t * media_codec_information, uint8_t media_codec_information_len);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // A2DP_SOURCE_H

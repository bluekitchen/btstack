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
 * avdtp_source.h
 * 
 * Audio/Video Distribution Transport Protocol Source
 *
 * AVDTP Source is a device that streames media data.
 */

#ifndef AVDTP_SOURCE_H
#define AVDTP_SOURCE_H

#include <stdint.h>
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Register media transport category with local stream endpoint identified by seid
 * @param seid
 */
void avdtp_source_register_media_transport_category(uint8_t seid);

/**
 * @brief Register reporting category with local stream endpoint identified by seid
 * @param seid
 */
void avdtp_source_register_reporting_category(uint8_t seid);

/**
 * @brief Register delay reporting category with local stream endpoint identified by seid
 * @param seid
 */
void avdtp_source_register_delay_reporting_category(uint8_t seid);

/**
 * @brief Register recovery category with local stream endpoint identified by seid
 * @param seid
 * @param maximum_recovery_window_size
 * @param maximum_number_media_packets
 */
void avdtp_source_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets);

/**
 * @brief Register content protection category with local stream endpoint identified by seid
 * @param seid
 * @param cp_type
 * @param cp_type_value
 * @param cp_type_value_len
 */
void avdtp_source_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len);

/**
 * @brief Register header compression category with local stream endpoint identified by seid
 * @param seid
 * @param back_ch
 * @param media
 * @param recovery
 */
void avdtp_source_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery);

/**
 * @brief Register media codec category with local stream endpoint identified by seid
 * @param seid
 * @param media_type
 * @param media_codec_type
 * @param media_codec_info
 * @param media_codec_info_len
 */
void avdtp_source_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, uint8_t * media_codec_info, uint16_t media_codec_info_len);

/**
 * @brief Register multiplexing category with local stream endpoint identified by seid
 * @param seid
 * @param fragmentation
 */
void avdtp_source_register_multiplexing_category(uint8_t seid, uint8_t fragmentation);

/**
 * @brief Initialize up AVDTP Source device.
 */
void avdtp_source_init(void);

/**
 * @brief Register callback for the AVDTP Source client. See btstack_defines.h for AVDTP_SUBEVENT_* events
 *
 * @param callback
 */
void avdtp_source_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address. (and perform configuration?)
 * @param bd_addr
 * @param avdtp_cid Assigned avdtp cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise BTSTACK_MEMORY_ALLOC_FAILED, SDP_QUERY_BUSY
 */
uint8_t avdtp_source_connect(bd_addr_t bd_addr, uint16_t * avdtp_cid);

/**
 * @brief Disconnect from device with connection handle. 
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST
 */
uint8_t avdtp_source_disconnect(uint16_t avdtp_cid);

/**
 * @brief Discover stream endpoints
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE
 */
uint8_t avdtp_source_discover_stream_endpoints(uint16_t avdtp_cid);

/**
 * @brief Get capabilities
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE
 */
uint8_t avdtp_source_get_capabilities(uint16_t avdtp_cid, uint8_t acp_seid);

/**
 * @brief Get all capabilities
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE
 */
uint8_t avdtp_source_get_all_capabilities(uint16_t avdtp_cid, uint8_t acp_seid);

/**
 * @brief Set configuration
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE, AVDTP_STREAM_ENDPOINT_DOES_NOT_EXIST, AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE
 */
uint8_t avdtp_source_set_configuration(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Reconfigure stream
 * @param avdtp_cid
 * @param seid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE, AVDTP_STREAM_ENDPOINT_DOES_NOT_EXIST, AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE
 */
uint8_t avdtp_source_reconfigure(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Get configuration
 * @param avdtp_cid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE
 */
uint8_t avdtp_source_get_configuration(uint16_t avdtp_cid, uint8_t acp_seid);


/**
 * @brief Open stream
 * @param avdtp_cid
 * @param seid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_CONNECTION_IN_WRONG_STATE, AVDTP_SEID_DOES_NOT_EXIST, AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST
 */
uint8_t avdtp_source_open_stream(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid);

/**
 * @brief Start stream
 * @param local_seid
* @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_SEID_DOES_NOT_EXIST, AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST
 */
uint8_t avdtp_source_start_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Abort stream
 * @param local_seid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_SEID_DOES_NOT_EXIST, AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST
 */
uint8_t avdtp_source_abort_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Start stream
 * @param local_seid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_SEID_DOES_NOT_EXIST, AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST, 
 */
uint8_t avdtp_source_stop_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Suspend stream
 * @param local_seid
 * @return status ERROR_CODE_SUCCESS if succesful, otherwise AVDTP_CONNECTION_DOES_NOT_EXIST, AVDTP_SEID_DOES_NOT_EXIST, AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST 
 */
uint8_t avdtp_source_suspend(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Create stream endpoint
 * @param sep_type          AVDTP_SOURCE or AVDTP_SINK, see avdtp.h
 * @param media_type        AVDTP_AUDIO, AVDTP_VIDEO or AVDTP_MULTIMEDIA, see avdtp.h
 * @return pointer to newly created stream endpoint, or NULL if allocation failed
 */
avdtp_stream_endpoint_t * avdtp_source_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type);

/**
 * @brief Send media payload.
 * @param avdtp_cid         AVDTP channel identifyer.
 * @param local_seid        ID of a local stream endpoint.
 * @param storage
 * @param num_bytes_to_copy
 * @param num_frames
 * @param marker
 * @return max_media_payload_size_without_media_header
 */
int avdtp_source_stream_send_media_payload(uint16_t avdtp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker);

/**
 * @brief Request to send a media packet. Packet can be then sent on reception of AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW event.
 * @param avdtp_cid         AVDTP channel identifyer.
 * @param local_seid        ID of a local stream endpoint.
 */
void avdtp_source_stream_endpoint_request_can_send_now(uint16_t avddp_cid, uint8_t local_seid);

/**
 * @brief Return maximal media payload size, does not include media header.
 * @param avdtp_cid         AVDTP channel identifyer.
 * @param local_seid        ID of a local stream endpoint.
 */
int avdtp_max_media_payload_size(uint16_t avdtp_cid, uint8_t local_seid);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // AVDTP_SOURCE_H

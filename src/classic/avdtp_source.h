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

#ifndef __AVDTP_SOURCE_H
#define __AVDTP_SOURCE_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief AVDTP Source service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVDTP_SOURCE_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void a2dp_source_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

void avdtp_source_register_media_transport_category(uint8_t seid);
void avdtp_source_register_reporting_category(uint8_t seid);
void avdtp_source_register_delay_reporting_category(uint8_t seid);
void avdtp_source_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets);
void avdtp_source_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len);
void avdtp_source_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery);
void avdtp_source_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t * media_codec_info, uint16_t media_codec_info_len);
void avdtp_source_register_multiplexing_category(uint8_t seid, uint8_t fragmentation);


void avdtp_source_init(void);
void avdtp_source_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address. (and perform configuration?)
 * @param bd_addr
 */
void avdtp_source_connect(bd_addr_t bd_addr);

void avdtp_source_register_media_handler(void (*callback)(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size));
/**
 * @brief Disconnect from device with connection handle. 
 * @param con_handle
 */
void avdtp_source_disconnect(uint16_t con_handle);

/**
 * @brief Discover stream endpoints
 * @param con_handle
 */
void avdtp_source_discover_stream_endpoints(uint16_t con_handle);

/**
 * @brief Get capabilities
 * @param con_handle
 */
void avdtp_source_get_capabilities(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Get all capabilities
 * @param con_handle
 */
void avdtp_source_get_all_capabilities(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Set configuration
 * @param con_handle
 */
void avdtp_source_set_configuration(uint16_t con_handle, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Reconfigure stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_reconfigure(uint16_t con_handle, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Get configuration
 * @param con_handle
 */
void avdtp_source_get_configuration(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Suspend stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_suspend(uint16_t con_handle, uint8_t acp_seid);


/**
 * @brief Open stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_open_stream(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Start stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_start_stream(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Start stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_abort_stream(uint16_t con_handle, uint8_t acp_seid);

/**
 * @brief Start stream
 * @param con_handle
 * @param seid
 */
void avdtp_source_stop_stream(uint16_t con_handle, uint8_t acp_seid);

avdtp_stream_endpoint_t * avdtp_source_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type);

void avdtp_source_stream_data_start(uint16_t con_handle);
void avdtp_source_stream_data_stop(uint16_t con_handle);
/* API_END */

#if defined __cplusplus
}
#endif

#endif // __AVDTP_SOURCE_H
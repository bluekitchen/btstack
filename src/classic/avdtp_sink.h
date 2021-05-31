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

/**
 * @title AVDTP Sink
 *
 * Audio/Video Distribution Transport Protocol (AVDTP) Sink is a device that accepts streamed media data.  
 *
 */

#ifndef AVDTP_SINK_H
#define AVDTP_SINK_H

#include <stdint.h>
#include "hci.h"
#include "classic/avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Set up AVDTP Sink device.
 */
void avdtp_sink_init(void);

// returns avdtp_stream_endpoint_t *
avdtp_stream_endpoint_t * avdtp_sink_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type);

/**
 *  @brief Unregister stream endpoint and free it's memory
 *  @param stream_endpoint created by avdtp_sink_create_stream_endpoint
 */
void avdtp_sink_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

void avdtp_sink_register_media_transport_category(uint8_t seid);
void avdtp_sink_register_reporting_category(uint8_t seid);
void avdtp_sink_register_delay_reporting_category(uint8_t seid);
void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets);
void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery);
void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation);

void avdtp_sink_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t *media_codec_info, uint16_t media_codec_info_len);
void avdtp_sink_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len);

/**
 * @brief Register callback for the AVDTP Sink client. 
 * @param callback
 */
void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address. (and perform configuration?)
 * @param bd_addr
 * @param avdtp_cid Assigned avdtp cid
 */
uint8_t avdtp_sink_connect(bd_addr_t bd_addr, uint16_t * avdtp_cid);

void avdtp_sink_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size));
/**
 * @brief Disconnect from device with connection handle. 
 * @param avdtp_cid
 */
uint8_t avdtp_sink_disconnect(uint16_t avdtp_cid);

/**
 * @brief Discover stream endpoints
 * @param avdtp_cid
 */
uint8_t avdtp_sink_discover_stream_endpoints(uint16_t avdtp_cid);

/**
 * @brief Get capabilities
 * @param avdtp_cid
 */
uint8_t avdtp_sink_get_capabilities(uint16_t avdtp_cid, uint8_t acp_seid);

/**
 * @brief Get all capabilities
 * @param avdtp_cid
 */
uint8_t avdtp_sink_get_all_capabilities(uint16_t avdtp_cid, uint8_t acp_seid);

/**
 * @brief Set configuration
 * @param avdtp_cid
 */
uint8_t avdtp_sink_set_configuration(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Reconfigure stream
 * @param avdtp_cid
 * @param seid
 */
uint8_t avdtp_sink_reconfigure(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);

/**
 * @brief Get configuration
 * @param avdtp_cid
 */
uint8_t avdtp_sink_get_configuration(uint16_t avdtp_cid, uint8_t acp_seid);


/**
 * @brief Open stream
 * @param avdtp_cid
 * @param seid
 */
uint8_t avdtp_sink_open_stream(uint16_t avdtp_cid, uint8_t int_seid, uint8_t acp_seid);


/**
 * @brief Start stream
 * @param local_seid
 */
uint8_t avdtp_sink_start_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Abort stream
 * @param local_seid
 */
uint8_t avdtp_sink_abort_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Start stream
 * @param local_seid
 */
uint8_t avdtp_sink_stop_stream(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Suspend stream
 * @param local_seid
 */
uint8_t avdtp_sink_suspend(uint16_t avdtp_cid, uint8_t local_seid);

/**
 * @brief Report delay
 * @param local_seid
 * @param delay_100us
 */
uint8_t avdtp_sink_delay_report(uint16_t avdtp_cid, uint8_t local_seid, uint16_t delay_100us);

/**
 * @brief De-Init AVDTP Sink.
 */
void avdtp_sink_deinit(void);

// AVDTP_SI_DELAYREPORT

/* API_END */


#if defined __cplusplus
}
#endif

#endif // AVDTP_SINK_H

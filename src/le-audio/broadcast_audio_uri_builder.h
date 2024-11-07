/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 *  @brief TODO
 */

#ifndef BROADCAST_AUDIO_URI_BUILDER_H
#define BROADCAST_AUDIO_URI_BUILDER_H

#include <stdint.h>
#include "btstack_bool.h"
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t * buffer;
    uint16_t  size;
    uint16_t len;
} broadcast_audio_uri_builder_t;

/**
 * Initialize Broadcast Audio URI builder
 * @param builder
 * @param buffer to setup BASE
 * @param size of buffer
 */
void broadcast_audio_uri_builder_init(broadcast_audio_uri_builder_t * builder, uint8_t * buffer, uint16_t size);

/**
 * @brief Query remaining space in buffer
 * @param builder
 * @return size
 */
uint16_t broadcast_audio_uri_builder_get_remaining_space(const broadcast_audio_uri_builder_t * builder);

/**
 * Get Size of Broadcast Audio URI
 * @param builder
 * @return size
 */
uint16_t broadcast_audio_uri_builder_get_size(const broadcast_audio_uri_builder_t * builder);

/**
 * Append bytes
 * @param builder
 * @param data
 * @param len
 * @return true if there was sufficient space in the underlying buffer
 */
bool broadcast_audio_uri_builder_append_bytes(broadcast_audio_uri_builder_t * builder, const uint8_t * data, uint16_t len);

/**
 * Append String
 * @param builder
 * @param text as utf-8
 * @return true if there was sufficient space in the underlying buffer
 */
bool broadcast_audio_uri_builder_append_string(broadcast_audio_uri_builder_t * builder, const char * text);

/**
 * Append Broadcast Name (BN) base64 encoded
 * @param builder
 * @param broadcast_name
 * @return
 */
bool broadcast_audio_uri_builder_append_broadcast_name(broadcast_audio_uri_builder_t * builder, const char * broadcast_name);

/**
 * Append Advertiser Address Type
 * @param builder
 * @param advertiser_address_type
 * @return
 */
bool broadcast_audio_uri_builder_append_advertiser_address_type(broadcast_audio_uri_builder_t * builder, bd_addr_type_t advertiser_address_type);

/**
 * Append Advertiser Address
 * @param builder
 * @param advertiser_address_type
 * @return
 */
bool broadcast_audio_uri_builder_append_advertiser_address(broadcast_audio_uri_builder_t * builder, bd_addr_t advertiser_address);

/**
 * Append Broadcast ID
 * @param builder
 * @param Broadcast ID (24 bit)
 * @return
 */
bool broadcast_audio_uri_builder_append_broadcast_id(broadcast_audio_uri_builder_t * builder, uint32_t broadcast_id);

/**
 * Append Broadcast ID
 * @param builder
 * @param Broadcast Code (128 bit)
 * @return
 */
bool broadcast_audio_uri_builder_append_broadcast_code(broadcast_audio_uri_builder_t * builder, const uint8_t * broadcast_code);

/**
 * Append Standard Quality
 * @param builder
 * @param advertiser_address_type
 * @return
 */
bool broadcast_audio_uri_builder_append_standard_quality(broadcast_audio_uri_builder_t * builder, bool standard_quality);

/**
 * Append High Quality
 * @param builder
 * @param advertiser_address_type
 * @return
 */
bool broadcast_audio_uri_builder_append_high_quality(broadcast_audio_uri_builder_t * builder, bool high_quality);

/**
 * Append Vendor Specific data
 * @param builder
 * @param vendor_id
 * @param data
 * @param data_len
 * @return
 */
bool broadcast_audio_uri_builder_append_vendor_specific(broadcast_audio_uri_builder_t * builder, uint16_t vendor_id, const uint8_t * data, uint16_t data_len);

/**
 * Append Advertising SID
 * @param builder
 * @param advertising_sid
 * @return
 */
bool broadcast_audio_uri_builder_append_advertising_sid(broadcast_audio_uri_builder_t * builder, uint8_t advertising_sid);

/**
 * Append PA Interval
 * @param builder
 * @param pa_interval
 * @return
 */
bool broadcast_audio_uri_builder_append_pa_interval(broadcast_audio_uri_builder_t * builder, uint16_t pa_interval);

/**
 * Append Num Subgroups
 * @param builder
 * @param num_subgroups
 * @return
 */
bool broadcast_audio_uri_builder_append_num_subgroups(broadcast_audio_uri_builder_t * builder, uint8_t num_subgroups);

/**
 * Append BIS_Sync
 * @param builder
 * @param Broadcast ID (24 bit)
 * @return
 */
bool broadcast_audio_uri_builder_append_bis_sync(broadcast_audio_uri_builder_t * builder, uint32_t bis_sync);

/**
 * Append SG Number of BISes
 * @param builder
 * @param sg_number_of_bises
 * @return
 */
bool broadcast_audio_uri_builder_append_sg_number_of_bises(broadcast_audio_uri_builder_t * builder, uint32_t sg_number_of_bises);

/**
 * Append SG_Metadata
 * @param builder
 * @param metadata
 * @param metadata_len
 * @return
 */
bool broadcast_audio_uri_builder_append_sg_metadata(broadcast_audio_uri_builder_t * builder, const uint8_t * metadata, uint16_t metadata_len);

/**
 * Append Public Broadcast Announcement Metadata
 * @param builder
 * @param metadata
 * @param metadata_len
 * @return
 */
bool broadcast_audio_uri_builder_append_public_broadcast_announcement_metadata(broadcast_audio_uri_builder_t * builder, const uint8_t * metadata, uint16_t metadata_len);

#if defined __cplusplus
}
#endif
#endif // BROADCAST_AUDIO_URI_BUILDER_H

/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Published Audio Capabilities Service Server (PACS)
 * 
 */

#ifndef PUBLISHED_AUDIO_CAPABILITIES_SERVICE_UTIL_H
#define PUBLISHED_AUDIO_CAPABILITIES_SERVICE_UTIL_H

#include <stdint.h>
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

// Audio capabilities
typedef struct {
    // Codec Specific Capabilities
    // defines which of le_audio_codec_capability_type_t codec capabilities values are set
    uint8_t  codec_capability_mask;

    uint16_t sampling_frequency_mask;          
    uint8_t  supported_frame_durations_mask;
    uint8_t  supported_audio_channel_counts_mask;
    uint16_t octets_per_frame_min_num;          // 0-1 octet
    uint16_t octets_per_frame_max_num;          // 2-3 octet
    uint8_t  frame_blocks_per_sdu_max_num;
} pacs_codec_capability_t;

typedef struct {
    le_audio_codec_id_t codec_id;

    pacs_codec_capability_t codec_capability;

    uint8_t metadata_length;
    le_audio_metadata_t metadata;
} pacs_record_t;

typedef struct {
    pacs_record_t * records;
    uint8_t  records_num;
    uint32_t audio_locations_mask;
    uint16_t available_audio_contexts_mask;
    uint16_t supported_audio_contexts_mask;
} pacs_streamendpoint_t;


/* API_END */

#if defined __cplusplus
}
#endif

#endif


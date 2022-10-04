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
 */

/**
 * @title Broadcast Audio Source Endpoint AD Parser
 */

#ifndef LE_AUDIO_BASE_PARSER_H
#define LE_AUDIO_BASE_PARSER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t * buffer;
    uint16_t size;
    uint8_t  subgroup_index;
    uint8_t  subgroup_count;
    uint16_t subgroup_offset;
    uint8_t  subgroup_codec_specific_configuration_len;
    uint8_t  subgroup_metadata_len;
    uint8_t  bis_index;
    uint8_t  bis_count;
    uint16_t bis_offset;
} le_audio_base_parser_t;

/**
 * Initialize BASE
 * @param parser
 * @param buffer to setup BASE
 * @param size of buffer
 * @param presentation_delay_us
 * @return valid BASE struct found
 */
bool le_audio_base_parser_init(le_audio_base_parser_t * parser, const uint8_t * buffer, uint16_t size);

/**
 * Get number of subgroups in BASE
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_get_num_subgroups(le_audio_base_parser_t * parser);

/**
 * Get Pressentation Delay in us
 * @param parser
 * @return
 */
uint32_t le_audio_base_parser_get_presentation_delay(le_audio_base_parser_t * parser);

/**
 * Get Num BIS for current subgroup
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_subgroup_get_num_bis(le_audio_base_parser_t * parser);

/**
 * Get Codec ID for current subgroup
 * @param parser
 * @return true if successful
 */
const uint8_t * le_audio_base_parser_subgroup_get_codec_id(le_audio_base_parser_t * parser);

/**
 * Get Codec Specific Configuration Length for current subgroup
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_subgroup_get_codec_specific_configuration_length(le_audio_base_parser_t * parser);

/**
 * Get Codec Specific Configuration for current subgroup
 * @param parser
 * @return
 */
const uint8_t * le_audio_base_parser_subgroup_get_codec_specific_configuration(le_audio_base_parser_t * parser);

/**
 * Get Metadata Length for current subgroup
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_subgroup_get_metadata_length(le_audio_base_parser_t * parser);

/**
 * Get Metadata for current subgroup
 * @param parser
 * @return
 */
const uint8_t * le_audio_base_subgroup_parser_get_metadata(le_audio_base_parser_t * parser);

/**
 * Process next subgroup
 * @param parser
 */
void le_audio_base_parser_subgroup_next(le_audio_base_parser_t * parser);

/**
 * Get BIS Index for current BIS
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_bis_get_index(le_audio_base_parser_t * parser);

/**
 * Get Codec Specific Configuration Length for current BIS
 * @param parser
 * @return
 */
uint8_t le_audio_base_parser_bis_get_codec_specific_configuration_length(le_audio_base_parser_t * parser);

/**
 * Get Codec Specific Configuration for current BIS
 * @param parser
 * @return
 */
const uint8_t * le_audio_base_bis_parser_get_codec_specific_configuration(le_audio_base_parser_t * parser);

/**
 * Process next BIS
 * @param parser
 */
void le_audio_base_parser_bis_next(le_audio_base_parser_t * parser);

#if defined __cplusplus
}
#endif

#endif // BASE_BUILDER_H

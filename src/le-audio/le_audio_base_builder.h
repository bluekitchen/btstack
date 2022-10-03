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
 * @title Broadcast Audio Source Endpoint AD Builder
 */

#ifndef LE_AUDIO_BASE_BUILDER_H
#define LE_AUDIO_BASE_BUILDER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t * buffer;
    uint16_t  size;
    uint16_t len;
    uint16_t subgroup_offset;
    uint16_t bis_offset;
} le_audio_base_builder_t;

/**
 * Initialize BASE
 * @param builder
 * @param buffer to setup BASE
 * @param size of buffer
 * @param presentation_delay_us
 */
void le_audio_base_builder_init(le_audio_base_builder_t * builder, uint8_t * buffer, uint16_t size, uint32_t presentation_delay_us);

/**
 * Add subgroup to current BASE
 * @param builder
 * @param codec_id of 5 bytes
 * @param codec_specific_configuration_length
 * @param codec_specific_configuration
 * @param metadata_length
 * @param metadata
 */
void le_audio_base_builder_add_subgroup(le_audio_base_builder_t * builder,
                                        const uint8_t * codec_id,
                                        uint8_t codec_specific_configuration_length, const uint8_t * codec_specific_configuration,
                                        uint8_t metadata_length, const uint8_t * metadata);

/**
 * Add BIS to current BASE
 * @param builder
 * @param bis_index
 * @param codec_specific_configuration_length
 * @param codec_specific_configuration
 */
void le_audio_base_builder_add_bis(le_audio_base_builder_t * builder,
                                   uint8_t bis_index,
                                   uint8_t codec_specific_configuration_length,
                                   const uint8_t * codec_specific_configuration);

/**
 * Get Size of BASE for Periodic Advertising
 * @param builder
 * @return
 */
uint16_t le_audio_base_builder_get_ad_data_size(const le_audio_base_builder_t * builder);

#if defined __cplusplus
}
#endif

#endif // LE_AUDIO_BASE_BUILDER_H

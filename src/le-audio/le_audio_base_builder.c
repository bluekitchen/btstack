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

#define BTSTACK_FILE__ "le_audio_base_builder.c"

/**
 * @title Broadcast Audio Source Endpoint AD Builder
 */

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "le-audio/le_audio_base_builder.h"

void le_audio_base_builder_init(le_audio_base_builder_t * builder, uint8_t  * buffer, uint16_t size, uint32_t presentation_delay_us){
    btstack_assert(size >= 8);
    // default init
    memset(builder, 0, sizeof(le_audio_base_builder_t));
    builder->buffer = buffer;
    builder->size = size;
    builder->len = 0;
    builder->buffer[builder->len++] = 7;
    builder->buffer[builder->len++] = BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID;
    little_endian_store_16(builder->buffer, 2, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE);
    builder->len += 2;
    little_endian_store_24(builder->buffer, 4, presentation_delay_us);
    builder->len += 3;
    builder->buffer[builder->len++] = 0;
    builder->subgroup_offset = builder->len;
}

void le_audio_base_builder_add_subgroup(le_audio_base_builder_t * builder,
                                        const uint8_t * codec_id,
                                        uint8_t codec_specific_configuration_length, const uint8_t * codec_specific_configuration,
                                        uint8_t metadata_length, const uint8_t * metadata){
    // check len
    uint16_t additional_len = 1 + 5 + 1 + codec_specific_configuration_length + 1 + metadata_length;
    btstack_assert((builder->len + additional_len) <= builder->size);

    builder->buffer[builder->len++] = 0;
    memcpy(&builder->buffer[builder->len], codec_id, 5);
    builder->len += 5;
    builder->buffer[builder->len++] = codec_specific_configuration_length;
    memcpy(&builder->buffer[builder->len], codec_specific_configuration, codec_specific_configuration_length);
    builder->len += codec_specific_configuration_length;
    builder->buffer[builder->len++] = metadata_length;
    memcpy(&builder->buffer[builder->len], metadata, metadata_length);
    builder->len += metadata_length;
    builder->bis_offset = builder->len;

    // update num subgroups
    builder->buffer[7]++;

    // update total len
    builder->buffer[0] = builder->len - 1;
}

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
                                   const uint8_t * codec_specific_configuration){
    // check len
    uint16_t additional_len = 1 + 1 + codec_specific_configuration_length;
    btstack_assert((builder->len + additional_len) <= builder->size);

    // append data
    builder->buffer[builder->len++] = bis_index;
    builder->buffer[builder->len++] = codec_specific_configuration_length;
    memcpy(&builder->buffer[builder->len], codec_specific_configuration, codec_specific_configuration_length);
    builder->len += codec_specific_configuration_length;

    // update num bis
    builder->buffer[builder->subgroup_offset]++;

    // update total len
    builder->buffer[0] = builder->len - 1;
}

uint16_t le_audio_base_builder_get_ad_data_size(const le_audio_base_builder_t * builder){
    return builder->len;
}

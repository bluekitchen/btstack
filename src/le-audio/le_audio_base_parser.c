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

#define BTSTACK_FILE__ "le_audio_base_parser.c"

/**
 * @title Broadcast Audio Source Endpoint AD Parser
 */

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "le-audio/le_audio_base_parser.h"

// precondition: subgroup_offset set
static void le_audio_base_parser_fetch_subgroup_info(le_audio_base_parser_t * parser){
    const uint8_t * buffer = parser->buffer;
    uint16_t offset = parser->subgroup_offset;
    parser->bis_count = buffer[offset++];
    // Codec ID
    offset += 5;
    parser->subgroup_codec_specific_configuration_len = buffer[offset++];
    // Codec Specific Configuration
    offset += parser->subgroup_codec_specific_configuration_len;
    parser->subgroup_metadata_len = buffer[offset++];
    // Metadata
    offset  +=  parser->subgroup_metadata_len;
    parser->bis_index = 0;
    parser->bis_offset =offset;
}

bool le_audio_base_parser_init(le_audio_base_parser_t * parser, const uint8_t * buffer, uint16_t size){
    memset(parser, 0, sizeof(le_audio_base_parser_t));
    // check buffer
    if (size < 8) {
        return false;
    }
    uint16_t offset = 1;
    if (buffer[offset++] != BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID) {
        return false;
    }
    if (little_endian_read_16(buffer, offset) != ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE){
        return false;
    }
    offset += 5;

    parser->buffer = buffer;
    parser->size = size;

    parser->subgroup_count = buffer[offset++];
    parser->subgroup_index = 0;

    // fully validate structure
    uint8_t i;
    uint8_t k;
    for (i=0;i<parser->subgroup_count;i++){
        if ((offset+8) > size) {
            return false;
        }
        uint8_t num_bis = buffer[offset++];
        offset += 5;
        uint8_t config_len = buffer[offset++];
        offset += config_len;
        if ((offset+1) > size) {
            return false;
        }
        int8_t meta_len = buffer[offset++];
        offset += meta_len;
        if (offset > size) {
            return false;
        }
        for (k=0;k<num_bis;k++){
            if ((offset+2) > size) {
                return false;
            }
            offset++;
            config_len = buffer[offset++];
            offset += config_len;
            if (offset > size) {
                return false;
            }
        }
    }

    if (parser->subgroup_count > 0){
        parser->subgroup_offset = 8;
        le_audio_base_parser_fetch_subgroup_info(parser);
    }

    return true;
}

uint32_t le_audio_base_parser_get_presentation_delay(le_audio_base_parser_t * parser){
    btstack_assert(parser->buffer != NULL);
    return little_endian_read_24(parser->buffer, 4);
}

uint8_t le_audio_base_parser_subgroup_get_num_bis(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return parser->bis_count;
}

uint8_t le_audio_base_parser_get_num_subgroups(le_audio_base_parser_t * parser){
    btstack_assert(parser->buffer != NULL);
    return parser->subgroup_count;
}

const uint8_t * le_audio_base_parser_subgroup_get_codec_id(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return &parser->buffer[parser->subgroup_offset];
}

uint8_t le_audio_base_parser_subgroup_get_codec_specific_configuration_length(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return parser->subgroup_codec_specific_configuration_len;
}

const uint8_t * le_audio_base_parser_subgroup_get_codec_specific_configuration(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return &parser->buffer[parser->subgroup_offset + 7];
}

uint8_t le_audio_base_parser_subgroup_get_metadata_length(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return parser->buffer[parser->subgroup_offset + 7 + parser->subgroup_codec_specific_configuration_len];
}

const uint8_t * le_audio_base_subgroup_parser_get_metadata(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    return &parser->buffer[parser->subgroup_offset + 7 + parser->subgroup_codec_specific_configuration_len + 1];
}

uint8_t le_audio_base_parser_bis_get_index(le_audio_base_parser_t * parser){
    btstack_assert(parser->bis_offset > 0);
    return parser->buffer[parser->bis_offset];
}

uint8_t le_audio_base_parser_bis_get_codec_specific_configuration_length(le_audio_base_parser_t * parser){
    btstack_assert(parser->bis_offset > 0);
    return parser->buffer[parser->bis_offset + 1];
}

const uint8_t * le_audio_base_bis_parser_get_codec_specific_configuration(le_audio_base_parser_t * parser){
    btstack_assert(parser->bis_offset > 0);
    return &parser->buffer[parser->bis_offset + 2];
}

void le_audio_base_parser_bis_next(le_audio_base_parser_t * parser){
    btstack_assert(parser->bis_offset > 0);
    parser->bis_index++;
    if (parser->bis_index < parser->bis_count){
        parser->bis_offset += 1 + 1 + parser->buffer[parser->bis_offset+1];
    } else {
        parser->bis_offset = 0;
    }
}

void le_audio_base_parser_subgroup_next(le_audio_base_parser_t * parser){
    btstack_assert(parser->subgroup_offset > 0);
    while (parser->bis_index < parser->bis_count){
        le_audio_base_parser_bis_next(parser);
    }
    parser->subgroup_index++;
    if (parser->subgroup_index < parser->subgroup_count){
        parser->subgroup_offset = parser->bis_offset;
        le_audio_base_parser_fetch_subgroup_info(parser);
    } else {
        parser->bis_offset = 0;
    }
}

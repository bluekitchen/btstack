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

#define BTSTACK_FILE__ "le_audio_util.c"

#include "btstack_util.h"
#include "btstack_debug.h"
#include "le-audio/le_audio_util.h"


static const le_audio_codec_configuration_t codec_specific_config_settings[] = {
    {"8_1",  LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ,  LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   26},
    {"8_2",  LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ,  LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US,  30},
    {"16_1", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   30},
    {"16_2", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US,  40},
    {"24_1", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   45},
    {"24_2", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US,  60},
    {"32_1", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   60},
    {"32_2", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US,  80},
    {"441_1",LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   97},
    {"441_2",LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US, 130},
    {"48_1", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   75},
    {"48_2", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US, 100},
    {"48_3", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,   90},
    {"48_4", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US, 120},
    {"48_5", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US,  117},
    {"48_6", LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ, LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US, 155},
};

static const le_audio_qos_configuration_t qos_config_settings[] = {
    {"8_1_1",    7500, 0,  26, 2, 8},
    {"8_2_1",   10000, 0,  30, 2, 10},
    {"16_1_1",   7500, 0,  30, 2, 8},
    {"16_2_1",  10000, 0,  40, 2, 10},
    {"24_1_1",   7500, 0,  45, 2, 8},
    {"24_2_1",  10000, 0,  60, 2, 10},
    {"32_1_1",   7500, 0,  60, 2, 8},
    {"32_2_1",  10000, 0,  80, 2, 10},
    {"441_1_1",  8163, 1,  97, 5, 24},
    {"441_2_1", 10884, 1, 130, 5, 31},
    {"48_1_1",   7500, 0,  75, 5, 15},
    {"48_2_1",  10000, 0, 100, 5, 20},
    {"48_3_1",   7500, 0,  90, 5, 15},
    {"48_4_1",  10000, 0, 120, 5, 20},
    {"48_5_1",   7500, 0, 117, 5, 15},
    {"48_6_1",  10000, 0, 115, 5, 20},

    {"8_1_2",    7500, 0,  26, 13, 75},
    {"8_2_2",   10000, 0,  30, 13, 95},
    {"16_1_2",   7500, 0,  30, 13, 75},
    {"16_2_2",  10000, 0,  40, 13, 95},
    {"24_1_2",   7500, 0,  45, 13, 75},
    {"24_2_2",  10000, 0,  60, 13, 95},
    {"32_1_2",   7500, 0,  60, 13, 75},
    {"32_2_2",  10000, 0,  80, 13, 95},
    {"441_1_2",  8163, 1,  97, 13, 80},
    {"441_2_2", 10884, 1, 130, 13, 85},
    {"48_1_2",   7500, 0,  75, 13, 75},
    {"48_2_2",  10000, 0, 100, 13, 95},
    {"48_3_2",   7500, 0,  90, 13, 75},
    {"48_4_2",  10000, 0, 120, 13, 100},
    {"48_5_2",   7500, 0, 117, 13, 75},
    {"48_6_2",  10000, 0, 115, 13, 100}
};

static uint8_t codec_offset(le_audio_codec_sampling_frequency_index_t sampling_frequency_index, 
    le_audio_codec_frame_duration_index_t frame_duration_index, 
    le_audio_quality_t audio_quality){

    btstack_assert(audio_quality <= LE_AUDIO_QUALITY_HIGH);
    
    if (sampling_frequency_index == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ){
        return 10 + (audio_quality - LE_AUDIO_QUALITY_LOW) * 2 + (frame_duration_index - LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US);
    }
    return (sampling_frequency_index - LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ) * 2 + (frame_duration_index - LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US);
}

const le_audio_codec_configuration_t * le_audio_util_get_codec_setting(
    le_audio_codec_sampling_frequency_index_t sampling_frequency_index, 
    le_audio_codec_frame_duration_index_t frame_duration_index, 
    le_audio_quality_t audio_quality){
    
    return &codec_specific_config_settings[codec_offset(sampling_frequency_index, frame_duration_index, audio_quality)];
}

const le_audio_qos_configuration_t * le_audio_util_get_qos_setting(
    le_audio_codec_sampling_frequency_index_t sampling_frequency_index, 
    le_audio_codec_frame_duration_index_t frame_duration_index, 
    le_audio_quality_t audio_quality, uint8_t num_channels){

    btstack_assert((num_channels >= 1) && (num_channels <= 2));

    return &qos_config_settings[(num_channels - 1) * 16 + codec_offset(sampling_frequency_index, frame_duration_index, audio_quality)];
}


// help with buffer == NULL
uint16_t le_audio_util_virtual_memcpy_helper(
    const uint8_t * field_data, uint16_t field_len, uint16_t field_offset,
    uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){

    // only calc total size
    if (buffer == NULL) {
        return field_len;
    }
    return btstack_virtual_memcpy(field_data, field_len, field_offset, buffer, buffer_size, buffer_offset);
}

uint16_t le_audio_util_metadata_virtual_memcpy(const le_audio_metadata_t * metadata, uint8_t metadata_length, uint16_t * records_offset, uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){
    uint16_t metadata_type;
    uint8_t  field_data[7];
    uint16_t stored_bytes = 0;

    uint16_t metadata_length_pos = *records_offset;
    field_data[0] = 0;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, *records_offset, buffer, buffer_size,
                                                        buffer_offset);
    *records_offset += 1;

    if (metadata_length == 0){
        return stored_bytes;
    }

    for (metadata_type = (uint16_t)LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS; metadata_type < (uint16_t) LE_AUDIO_METADATA_TYPE_RFU; metadata_type++){
        if ((metadata->metadata_mask & (1 << metadata_type) ) != 0 ){
            // reserve field_data[0] for num butes to store
            field_data[0] = 1;
            field_data[1] = metadata_type;

            switch ((le_audio_metadata_type_t)metadata_type){
                case LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS:
                    field_data[0] += 2;
                    little_endian_store_16(field_data, 2, metadata->preferred_audio_contexts_mask);
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, field_data[0] + 1, *records_offset,
                                                                        buffer, buffer_size, buffer_offset);
                    *records_offset += field_data[0] + 1;
                    break;
                case LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS:
                    field_data[0] += 2;
                    little_endian_store_16(field_data, 2, metadata->streaming_audio_contexts_mask);
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, field_data[0] + 1, *records_offset,
                                                                        buffer, buffer_size, buffer_offset);
                    *records_offset += field_data[0] + 1;
                    break;
                case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO:
                    field_data[0] += metadata->program_info_length;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 2, *records_offset, buffer,
                                                                        buffer_size, buffer_offset);
                    *records_offset += 2;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(metadata->program_info,
                                                                        metadata->program_info_length, *records_offset,
                                                                        buffer, buffer_size, buffer_offset);
                    *records_offset += metadata->program_info_length;
                    break;
                case LE_AUDIO_METADATA_TYPE_LANGUAGE:
                    field_data[0] += 3;
                    little_endian_store_24(field_data, 2, metadata->language_code);
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, field_data[0] + 1, *records_offset,
                                                                        buffer, buffer_size, buffer_offset);
                    *records_offset += field_data[0] + 1;
                    break;
                case LE_AUDIO_METADATA_TYPE_CCID_LIST:
                    field_data[0] += metadata->ccids_num;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 2, *records_offset, buffer,
                                                                        buffer_size, buffer_offset);
                    *records_offset += 2;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(metadata->ccids, metadata->ccids_num,
                                                                        *records_offset, buffer, buffer_size,
                                                                        buffer_offset);
                    *records_offset += metadata->ccids_num;
                    break;
                case LE_AUDIO_METADATA_TYPE_PARENTAL_RATING:
                    field_data[0] += 1;
                    field_data[2] = (uint8_t) metadata->parental_rating;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, field_data[0] + 1, *records_offset,
                                                                        buffer, buffer_size, buffer_offset);
                    *records_offset += field_data[0] + 1;
                    break;
                case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO_URI:
                    field_data[0] += metadata->program_info_uri_length;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 2, *records_offset, buffer,
                                                                        buffer_size, buffer_offset);
                    *records_offset += 2;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(metadata->program_info_uri,
                                                                        metadata->program_info_uri_length,
                                                                        *records_offset, buffer, buffer_size,
                                                                        buffer_offset);
                    *records_offset += metadata->program_info_uri_length;
                    break;
                case LE_AUDIO_METADATA_TYPE_MAPPED_EXTENDED_METADATA_BIT_POSITION:
                    field_data[0] += 2 + metadata->extended_metadata_length;
                    field_data[1] = LE_AUDIO_METADATA_TYPE_EXTENDED_METADATA;
                    little_endian_store_16(field_data, 2, metadata->extended_metadata_type);
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 4, *records_offset, buffer,
                                                                        buffer_size, buffer_offset);
                    *records_offset += 4;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(metadata->extended_metadata,
                                                                        metadata->extended_metadata_length,
                                                                        *records_offset, buffer, buffer_size,
                                                                        buffer_offset);
                    *records_offset += metadata->extended_metadata_length;
                    break;
                case LE_AUDIO_METADATA_TYPE_MAPPED_VENDOR_SPECIFIC_METADATA_BIT_POSITION:
                    field_data[0] += 2 + metadata->vendor_specific_metadata_length;
                    field_data[1] = LE_AUDIO_METADATA_TYPE_VENDOR_SPECIFIC_METADATA;
                    little_endian_store_16(field_data, 2, metadata->vendor_specific_company_id);
                    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 4, *records_offset, buffer,
                                                                        buffer_size, buffer_offset);
                    *records_offset += 4;
                    stored_bytes += le_audio_util_virtual_memcpy_helper(metadata->vendor_specific_metadata,
                                                                        metadata->vendor_specific_metadata_length,
                                                                        *records_offset, buffer, buffer_size,
                                                                        buffer_offset);
                    *records_offset += metadata->vendor_specific_metadata_length;
                    break;
                default:
                    btstack_assert(false);
                    break;
            } 
        }
    }

    field_data[0] =  *records_offset - metadata_length_pos - 1;
    le_audio_util_virtual_memcpy_helper(field_data, 1, metadata_length_pos, buffer, buffer_size, buffer_offset);
    return stored_bytes;
}


uint16_t le_audio_util_metadata_parse(const uint8_t *buffer, uint8_t buffer_size, le_audio_metadata_t * metadata){
    // parse config to get sampling frequency and frame duration
    uint8_t offset = 0;
    uint8_t metadata_config_lenght = buffer[offset++];
    if (buffer_size < metadata_config_lenght){
        return 0;
    }

    metadata->metadata_mask = 0;

    while ((offset + 1) < metadata_config_lenght){
        uint8_t ltv_len = buffer[offset++];

        le_audio_metadata_type_t ltv_type = (le_audio_metadata_type_t)buffer[offset];
        le_audio_parental_rating_t parental_rating; 

        switch (ltv_type){
            case LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS:
                metadata->preferred_audio_contexts_mask = little_endian_read_16(buffer, offset+1);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS:
                metadata->streaming_audio_contexts_mask = little_endian_read_16(buffer, offset+1);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO:
                metadata->program_info_length = btstack_min(ltv_len, LE_AUDIO_PROGRAM_INFO_MAX_LENGTH);
                memcpy(metadata->program_info, &buffer[offset+1], metadata->program_info_length);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_LANGUAGE:
                metadata->language_code = little_endian_read_24(buffer, offset+1);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_CCID_LIST:
                metadata->ccids_num = btstack_min(ltv_len, LE_CCIDS_MAX_NUM);
                memcpy(metadata->ccids, &buffer[offset+1], metadata->ccids_num);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_PARENTAL_RATING:
                parental_rating = (le_audio_parental_rating_t)buffer[offset+1];
                metadata->parental_rating = parental_rating;
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO_URI:
                metadata->program_info_uri_length = btstack_min(ltv_len, LE_AUDIO_PROGRAM_INFO_URI_MAX_LENGTH);
                memcpy(metadata->program_info_uri, &buffer[offset+1], metadata->program_info_uri_length);
                metadata->metadata_mask |= (1 << ltv_type);
                break;
            case LE_AUDIO_METADATA_TYPE_EXTENDED_METADATA:
                if (ltv_len < 2){
                    break;
                }
                metadata->extended_metadata_length = btstack_min(ltv_len - 2, LE_AUDIO_EXTENDED_METADATA_MAX_LENGHT);
                metadata->extended_metadata_type = little_endian_read_16(buffer, offset+1);
                memcpy(metadata->extended_metadata, &buffer[offset+3], metadata->extended_metadata_length);
                metadata->metadata_mask |= (1 << LE_AUDIO_METADATA_TYPE_MAPPED_EXTENDED_METADATA_BIT_POSITION);
                break;
            case LE_AUDIO_METADATA_TYPE_VENDOR_SPECIFIC_METADATA:
                if (ltv_len < 2){
                    break;
                }
                metadata->vendor_specific_metadata_length = btstack_min(ltv_len - 2, LE_AUDIO_VENDOR_SPECIFIC_METADATA_MAX_LENGTH);
                metadata->vendor_specific_company_id = little_endian_read_16(buffer, offset+1);
                memcpy(metadata->vendor_specific_metadata, &buffer[offset+3], metadata->vendor_specific_metadata_length);
                metadata->metadata_mask |= (1 << LE_AUDIO_METADATA_TYPE_MAPPED_VENDOR_SPECIFIC_METADATA_BIT_POSITION);
                break;
            default:
                metadata->metadata_mask |= (1 << LE_AUDIO_METADATA_TYPE_RFU);
                break;
        }
        offset += ltv_len;
    }
    return offset;
}

uint16_t le_audio_util_metadata_serialize(const le_audio_metadata_t *metadata, uint8_t * event, uint16_t event_size){
    uint8_t pos = 0;
    
    event[pos++] = (uint8_t)metadata->metadata_mask;
    little_endian_store_16(event, pos, metadata->preferred_audio_contexts_mask);
    pos += 2;
    little_endian_store_16(event, pos, metadata->streaming_audio_contexts_mask);
    pos += 2;

    event[pos++] = metadata->program_info_length;
    memcpy(&event[pos], &metadata->program_info[0], metadata->program_info_length);
    pos += metadata->program_info_length;

    little_endian_store_24(event, pos, metadata->language_code);
    pos += 3;

    event[pos++] = metadata->ccids_num;
    memcpy(&event[pos], &metadata->ccids[0], metadata->ccids_num);
    pos += metadata->ccids_num;
    
    event[pos++] = (uint8_t)metadata->parental_rating;
    
    event[pos++] = metadata->program_info_uri_length;
    memcpy(&event[pos], &metadata->program_info_uri[0], metadata->program_info_uri_length);
    pos += metadata->program_info_uri_length;

    little_endian_store_16(event, pos, metadata->extended_metadata_type);
    pos += 2;

    event[pos++] = metadata->extended_metadata_length;
    memcpy(&event[pos], &metadata->extended_metadata[0], metadata->extended_metadata_length);
    pos += metadata->extended_metadata_length;

    little_endian_store_16(event, pos, metadata->vendor_specific_company_id);
    pos += 2;

    event[pos++] = metadata->vendor_specific_metadata_length;
    memcpy(&event[pos], &metadata->vendor_specific_metadata[0], metadata->vendor_specific_metadata_length);
    pos += metadata->vendor_specific_metadata_length;

    return pos;
}

static uint16_t le_audio_util_get_value_size_for_metadata_type(const le_audio_metadata_t *metadata, le_audio_metadata_type_t metadata_type){
    switch (metadata_type){
        case LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS:
        case LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS:
            return 2;

        case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO:
            return metadata->program_info_length;

        case LE_AUDIO_METADATA_TYPE_LANGUAGE:
            return 3;

        case LE_AUDIO_METADATA_TYPE_CCID_LIST:
            return metadata->ccids_num;

        case LE_AUDIO_METADATA_TYPE_PARENTAL_RATING:
            return 1;

        case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO_URI:
            return metadata->program_info_uri_length;

        case LE_AUDIO_METADATA_TYPE_MAPPED_EXTENDED_METADATA_BIT_POSITION:
            return 2 + metadata->extended_metadata_length;

        case LE_AUDIO_METADATA_TYPE_MAPPED_VENDOR_SPECIFIC_METADATA_BIT_POSITION:
            return 2 + metadata->vendor_specific_metadata_length;
        default:
            break;
    }
    return 0;
}

uint16_t le_audio_util_metadata_serialize_using_mask(const le_audio_metadata_t *metadata, uint8_t * tlv_buffer, uint16_t tlv_buffer_size){
    uint16_t metadata_type;
    uint16_t pos = 0;

    uint16_t remaining_bytes = tlv_buffer_size;

    for (metadata_type = (uint16_t)LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS; metadata_type < (uint16_t) LE_AUDIO_METADATA_TYPE_RFU; metadata_type++){
        if ((metadata->metadata_mask & (1 << metadata_type)) == 0){
            continue;
        }

        uint8_t payload_length = le_audio_util_get_value_size_for_metadata_type(metadata, (le_audio_metadata_type_t)metadata_type);
        // ensure that there is enough space in TLV to store length (1), type(1) and payload
        if (remaining_bytes < (2 + payload_length)){
            return pos;
        }

        tlv_buffer[pos++] = 1 + payload_length; // add one extra byte to count size of type (1 byte)
        tlv_buffer[pos++] = metadata_type;

        switch ((le_audio_metadata_type_t)metadata_type){
            case LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS:
                little_endian_store_16(tlv_buffer, pos, metadata->preferred_audio_contexts_mask);
                break;
            case LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS:
                little_endian_store_16(tlv_buffer, pos, metadata->streaming_audio_contexts_mask);
                break;
            case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO:
                memcpy(&tlv_buffer[pos], metadata->program_info, metadata->program_info_length);
                break;
            case LE_AUDIO_METADATA_TYPE_LANGUAGE:
                little_endian_store_24(tlv_buffer, pos, metadata->language_code);
                break;
            case LE_AUDIO_METADATA_TYPE_CCID_LIST:
                memcpy(&tlv_buffer[pos], metadata->ccids, metadata->ccids_num);
                break;
            case LE_AUDIO_METADATA_TYPE_PARENTAL_RATING:
                break;
            case LE_AUDIO_METADATA_TYPE_PROGRAM_INFO_URI:
                memcpy(&tlv_buffer[pos], metadata->program_info_uri, metadata->program_info_uri_length);
                break;
            case LE_AUDIO_METADATA_TYPE_MAPPED_EXTENDED_METADATA_BIT_POSITION:
                little_endian_store_16(tlv_buffer, pos, metadata->extended_metadata_type);
                memcpy(&tlv_buffer[pos + 2], metadata->extended_metadata, metadata->extended_metadata_length);
                break;
            case LE_AUDIO_METADATA_TYPE_MAPPED_VENDOR_SPECIFIC_METADATA_BIT_POSITION:
                little_endian_store_16(tlv_buffer, pos, metadata->vendor_specific_company_id);
                memcpy(&tlv_buffer[pos + 2], metadata->vendor_specific_metadata, metadata->vendor_specific_metadata_length);
                break;
            default:
                break;
        }
        pos += payload_length;
        remaining_bytes -= (payload_length + 2);
    }
    return pos;
}

btstack_lc3_frame_duration_t le_audio_util_get_btstack_lc3_frame_duration(le_audio_codec_frame_duration_index_t frame_duration_index){
    switch (frame_duration_index){
        case LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US:
            return BTSTACK_LC3_FRAME_DURATION_7500US;
        case LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US:
            return BTSTACK_LC3_FRAME_DURATION_10000US;
        default:
            btstack_assert(false);
            break;
    }
    return 0;
}

uint16_t le_audio_get_frame_duration_us(le_audio_codec_frame_duration_index_t frame_duration_index){
    switch (frame_duration_index){
        case LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US:
            return 7500;
        case LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US:
            return 10000;
        default:
            return 0;
    }
}

le_audio_codec_frame_duration_index_t le_audio_get_frame_duration_index(uint16_t frame_duration_us){
    switch (frame_duration_us){
        case 7500:
            return LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US;
        case 10000:
            return LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US;
        default:
            return LE_AUDIO_CODEC_FRAME_DURATION_INDEX_RFU;
    }
}

uint32_t le_audio_get_sampling_frequency_hz(le_audio_codec_sampling_frequency_index_t sampling_frequency_index){
    switch (sampling_frequency_index){
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ:
                return 8000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_11025_HZ:
                return 11025;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ:
                return 16000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_22050_HZ:
                return 22050;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ:
                return 24000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ:
                return 32000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ:
                return 44100;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ:
                return 48000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_88200_HZ:
                return 88200;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_96000_HZ:
                return 96000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_176400_HZ:
                return 176400;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_192000_HZ:
                return 192000;
        case LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_384000_HZ:
                return 384000;
        default:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_INVALID;
    }
}

le_audio_codec_sampling_frequency_index_t le_audio_get_sampling_frequency_index(uint32_t sampling_frequency_hz){
    switch (sampling_frequency_hz){
        case 0:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_INVALID;
        case 8000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ;
        case 11025:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_11025_HZ;
        case 16000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ;
        case 22050:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_22050_HZ;
        case 24000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ;
        case 32000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ;
        case 44100:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ;
        case 48000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ;
        case 88200:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_88200_HZ;
        case 96000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_96000_HZ;
        case 176400:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_176400_HZ;
        case 192000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_192000_HZ;
        case 384000:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_384000_HZ;
        default:
            return LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_RFU;
    }
}



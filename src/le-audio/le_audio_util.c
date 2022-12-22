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


uint16_t le_audio_util_metadata_parse(uint8_t * buffer, uint8_t buffer_size, le_audio_metadata_t * metadata){
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

uint16_t le_audio_util_metadata_serialize(le_audio_metadata_t * metadata, uint8_t * event, uint16_t event_size){
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

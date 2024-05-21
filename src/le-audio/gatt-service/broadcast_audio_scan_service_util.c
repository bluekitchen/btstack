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

#define BTSTACK_FILE__ "broadcast_audio_scan_service_server.c"

#include <stdio.h>

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/broadcast_audio_scan_service_util.h"
#include "le-audio/le_audio.h"
#include "le-audio/le_audio_util.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

// offset gives position into fully serialized bass record
uint16_t bass_util_source_data_header_virtual_memcpy(const bass_source_data_t * data, uint16_t *source_offset, uint16_t buffer_offset, uint8_t * buffer, uint16_t buffer_size){
    uint16_t stored_bytes = 0;
    uint8_t  field_data[16];

    field_data[0] = (uint8_t)data->address_type;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, *source_offset, buffer, buffer_size,
                                                        buffer_offset);
    (*source_offset)++;
    
    reverse_bd_addr(data->address, &field_data[0]);
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 6, *source_offset, buffer, buffer_size,
                                                        buffer_offset);
    (*source_offset) += 6;

    field_data[0] = data->adv_sid;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, *source_offset, buffer, buffer_size,
                                                        buffer_offset);
    (*source_offset)++;

    little_endian_store_24(field_data, 0, data->broadcast_id);
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 3, *source_offset, buffer, buffer_size,
                                                        buffer_offset);
    (*source_offset) += 3;

    return stored_bytes;
}

// offset gives position into fully serialized bass record
uint16_t
bass_util_source_data_subgroups_virtual_memcpy(const bass_source_data_t *data, bool use_state_fields, uint16_t *source_offset,
                                               uint16_t buffer_offset, uint8_t *buffer, uint16_t buffer_size) {
    uint16_t stored_bytes = 0;
    uint8_t  field_data[16];
    
    field_data[0] = (uint8_t)data->subgroups_num;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, *source_offset, buffer, buffer_size,
                                                        buffer_offset);
    (*source_offset)++;

    uint8_t i;
    for (i = 0; i < data->subgroups_num; i++){
        bass_subgroup_t subgroup = data->subgroups[i];

        if (use_state_fields) {
            little_endian_store_32(field_data, 0, subgroup.bis_sync_state);
        } else {
            little_endian_store_32(field_data, 0, subgroup.bis_sync);
        }
        stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 4, *source_offset, buffer, buffer_size,
                                                            buffer_offset);
        (*source_offset) += 4;
        stored_bytes += le_audio_util_metadata_virtual_memcpy(&subgroup.metadata, subgroup.metadata_length,
                                                              source_offset, buffer, buffer_size, buffer_offset);
    }
    return stored_bytes;
}

bool bass_util_pa_sync_state_and_subgroups_in_valid_range(const uint8_t *buffer, uint16_t buffer_size){
    uint8_t pos = 0;
    // pa_sync_state
    uint8_t pa_sync_state = buffer[pos++];
    if (pa_sync_state >= (uint8_t)LE_AUDIO_PA_SYNC_STATE_RFU){
        log_info("Unexpected pa_sync_state 0x%02X", pa_sync_state);
        return false;
    }

    // pa_interval
    pos += 2;
    uint8_t num_subgroups = buffer[pos++];
    if (num_subgroups > BASS_SUBGROUPS_MAX_NUM){
        log_info("Number of subgroups %u exceedes maximum %u", num_subgroups, BASS_SUBGROUPS_MAX_NUM);
        return false;
    }

    // If a BIS_Sync parameter value is not 0xFFFFFFFF for a subgroup, 
    // and if a BIS_index value written by a client is set to a value of 0b1 in more than one subgroup, 
    // the server shall ignore the operation.
    uint8_t i;
    uint32_t mask_total = 0;
    for (i = 0; i < num_subgroups; i++){
        // bis_sync
        uint32_t bis_sync = little_endian_read_32(buffer, pos);
        pos += 4;
        
        if (bis_sync != 0xFFFFFFFF){
            uint32_t mask_add = mask_total + ~bis_sync;
            uint32_t mask_or  = mask_total | ~bis_sync;
            if (mask_add != mask_or){
                // not disjunct
                return false;
            }
            mask_total = mask_add; 
        }
        
        // check if we can read metadata_length
        if (pos >= buffer_size){
            log_info("Metadata length not specified, subgroup %u", i);
            return false;
        }
        
        uint8_t metadata_length = buffer[pos++];

        if ((buffer_size - pos) < metadata_length){
            log_info("Metadata length %u exceedes remaining buffer %u", metadata_length, buffer_size - pos);
            return false;
        }
        // metadata
        pos += metadata_length;
    }

    return (pos == buffer_size);
}

bool bass_util_source_buffer_in_valid_range(const uint8_t *buffer, uint16_t buffer_size){
    if (buffer_size < 15){ 
        log_info("Add Source opcode, buffer too small");
        return false;
    }

    uint8_t pos = 0;
    // addr type
    uint8_t adv_type = buffer[pos++];
    if (adv_type > (uint8_t)BD_ADDR_TYPE_LE_RANDOM){
        log_info("Unexpected adv_type 0x%02X", adv_type);
        return false;
    }

    // address
    pos += 6;

    // advertising_sid Range: 0x00-0x0F
    uint8_t advertising_sid = buffer[pos++];
    if (advertising_sid > 0x0F){
        log_info("Advertising sid out of range 0x%02X", advertising_sid);
        return false;
    }

    // broadcast_id
    pos += 3;
    return bass_util_pa_sync_state_and_subgroups_in_valid_range(buffer+pos, buffer_size-pos);
}

void
bass_util_pa_info_and_subgroups_parse(const uint8_t *buffer, uint16_t buffer_size, bass_source_data_t *source_data,
                                      bool is_broadcast_receive_state) {
    UNUSED(buffer_size);
    uint8_t pos = 0;
    // for Broadcast Receive state, we have BIG_Encryption + Bad_Code, while for Add/Modify we have PA_Interval
    if (is_broadcast_receive_state){
        source_data->pa_sync_state = (le_audio_pa_sync_state_t)buffer[pos++];
        source_data->big_encryption = (le_audio_big_encryption_t) buffer[pos++];
        if (source_data->big_encryption == LE_AUDIO_BIG_ENCRYPTION_BAD_CODE){
            memcpy(source_data->bad_code, &buffer[pos], 16);
            pos += 16;
        } else {
            memset(source_data->bad_code, 0, 16);
        }
    } else {
        source_data->pa_sync       = (le_audio_pa_sync_t)buffer[pos++];
        source_data->pa_interval = little_endian_read_16(buffer, pos);
        pos += 2;
    }

    source_data->subgroups_num = buffer[pos++];
    uint8_t i;
    for (i = 0; i < source_data->subgroups_num; i++){
        // bis_sync / state
        if (is_broadcast_receive_state){
            source_data->subgroups[i].bis_sync_state = little_endian_read_32(buffer, pos);
        } else {
            source_data->subgroups[i].bis_sync = little_endian_read_32(buffer, pos);
        }
        pos += 4;
       
        uint8_t metadata_bytes_read = le_audio_util_metadata_parse(&buffer[pos], buffer_size - pos,
                                                                   &source_data->subgroups[i].metadata);
        source_data->subgroups[i].metadata_length = metadata_bytes_read - 1;
        pos += metadata_bytes_read;
    }
}

void bass_util_source_data_parse(const uint8_t *buffer, uint16_t buffer_size, bass_source_data_t *source_data,
                                 bool is_broadcast_receive_state) {
    UNUSED(buffer_size);
    uint8_t pos = 0;
    
    source_data->address_type = (bd_addr_type_t)buffer[pos++];
    
    reverse_bd_addr(&buffer[pos], source_data->address);
    pos += 6;

    source_data->adv_sid = buffer[pos++];

    source_data->broadcast_id = little_endian_read_24(buffer, pos);
    pos += 3;

    bass_util_pa_info_and_subgroups_parse(buffer + pos, buffer_size - pos, source_data, is_broadcast_receive_state);
}
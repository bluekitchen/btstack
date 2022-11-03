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


#ifndef BROADCAST_AUDIO_SCAN_SERVICE_UTIL_H
#define BROADCAST_AUDIO_SCAN_SERVICE_UTIL_H

#include <stdint.h>

#include "btstack_bool.h"
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

#define BASS_ERROR_CODE_OPCODE_NOT_SUPPORTED            0x80
#define BASS_ERROR_CODE_INVALID_SOURCE_ID               0x81
#define BASS_SUBGROUPS_MAX_NUM                          10

#define BASS_MAX_NOTIFY_BUFFER_SIZE                             200
#define BASS_INVALID_SOURCE_INDEX                               0xFF

/* API_START */

typedef enum {
    BASS_OPCODE_REMOTE_SCAN_STOPPED = 0x00,
    BASS_OPCODE_REMOTE_SCAN_STARTED,
    BASS_OPCODE_ADD_SOURCE,
    BASS_OPCODE_MODIFY_SOURCE,
    BASS_OPCODE_SET_BROADCAST_CODE,
    BASS_OPCODE_REMOVE_SOURCE, 
    BASS_OPCODE_RFU
} bass_opcode_t;

typedef enum {
    BASS_ROLE_CLIENT = 0,
    BASS_ROLE_SERVER   
} bass_role_t;

typedef struct {
    // BIS_Sync parameter
    // 4-octet bitfield. Shall not exist if Num_Subgroups = 0
    // Bit 0-30 = BIS_index[1-31] 0x00000000:
    // - 0b0 = Do not synchronize to BIS_index[x] 0xxxxxxxxx:
    // - 0b1 = Synchronize to BIS_index[x]
    // 0xFFFFFFFF: No preference
    // written by client into control point
    uint32_t bis_sync;

    // 4-octet bitfield. Shall not exist if num_subgroups = 0
    // Bit 0-30 = BIS_index[1-31] 
    // 0x00000000: 0 = Not synchronized to BIS_index[x] 
    // 0xxxxxxxxx: 1 = Synchronized to BIS_index[x] 
    // 0xFFFFFFFF: Failed to sync to BIG
    // state send to client by server
    uint32_t bis_sync_state;

    uint8_t  metadata_length;
    le_audio_metadata_t metadata;
} bass_subgroup_t;

typedef struct {
    // assigned by client via control point
    bd_addr_type_t address_type; 
    bd_addr_t address;
    uint8_t   adv_sid;
    uint32_t  broadcast_id;
    le_audio_pa_sync_t       pa_sync;       // written by client into control point
    le_audio_pa_sync_state_t pa_sync_state; // state send to client by server
    uint16_t  pa_interval;
    
    uint8_t  subgroups_num;
    // Shall not exist if num_subgroups = 0
    bass_subgroup_t subgroups[BASS_SUBGROUPS_MAX_NUM];
} bass_source_data_t;

// offset gives position into fully serialized BASS record
uint16_t bass_util_source_data_header_virtual_memcpy(const bass_source_data_t * data, uint16_t *source_offset, uint16_t buffer_offset, uint8_t * buffer, uint16_t buffer_size);

uint16_t bass_util_source_data_subgroups_virtual_memcpy(const bass_source_data_t *data, bool use_state_fields, uint16_t *source_offset,
                                                        uint16_t buffer_offset, uint8_t *buffer, uint16_t buffer_size);

bool bass_util_pa_sync_state_and_subgroups_in_valid_range(uint8_t *buffer, uint16_t buffer_size);

bool bass_util_source_buffer_in_valid_range(uint8_t *buffer, uint16_t buffer_size);

void bass_util_source_data_parse(uint8_t *buffer, uint16_t buffer_size, bass_source_data_t *source_data,
                                 bool is_broadcast_receive_state);

void bass_util_pa_info_and_subgroups_parse(uint8_t *buffer, uint16_t buffer_size, bass_source_data_t *source_data,
                                      bool is_broadcast_receive_state);

/* API_END */

#if defined __cplusplus
}
#endif

#endif


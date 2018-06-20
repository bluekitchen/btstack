/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define __BTSTACK_FILE__ "hfp_msbc.c"
 
// *****************************************************************************
//
// HFP mSBC encoder wrapper
//
// *****************************************************************************

#include "btstack_config.h"

#include <string.h>

#include "btstack_debug.h"
#include "btstack_sbc.h"
#include "hfp_msbc.h"

#define MSBC_FRAME_SIZE 57
#define MSBC_HEADER_H2_SIZE 2
#define MSBC_PADDING_SIZE 1
#define MSBC_EXTRA_SIZE (MSBC_HEADER_H2_SIZE + MSBC_PADDING_SIZE)



//static btstack_sbc_encoder_state_t state;




void hfp_msbc_init(btstack_sbc_encoder_state_t* state){
    btstack_sbc_encoder_init(state, SBC_MODE_mSBC, 16, 8, 0, 16000, 26, 0);
    state->msbc_buffer_offset = 0;
    state->msbc_sequence_number = 0;
}

int hfp_msbc_can_encode_audio_frame_now(btstack_sbc_encoder_state_t* state){
    return sizeof(state->msbc_buffer) - state->msbc_buffer_offset >= MSBC_FRAME_SIZE + MSBC_EXTRA_SIZE; 
}

void hfp_msbc_encode_audio_frame(btstack_sbc_encoder_state_t* state, int16_t * pcm_samples){
    if (!hfp_msbc_can_encode_audio_frame_now(state)) return;

    // Synchronization Header H2
    state->msbc_buffer[state->msbc_buffer_offset++] = msbc_header_h2_byte_0;
    state->msbc_buffer[state->msbc_buffer_offset++] = msbc_header_h2_byte_1_table[state->msbc_sequence_number];
    state->msbc_sequence_number = (state->msbc_sequence_number + 1) & 3;

    // SBC Frame
    btstack_sbc_encoder_process_data(state, pcm_samples);
    memcpy(state->msbc_buffer + state->msbc_buffer_offset, btstack_sbc_encoder_sbc_buffer(state), MSBC_FRAME_SIZE);
    state->msbc_buffer_offset += MSBC_FRAME_SIZE;

    // Final padding to use 60 bytes for 120 audio samples
    state->msbc_buffer[state->msbc_buffer_offset++] = 0;
}

void hfp_msbc_read_from_stream(btstack_sbc_encoder_state_t* state, uint8_t * buf, int size){
    int bytes_to_copy = size;
    if (size > state->msbc_buffer_offset){
        bytes_to_copy = state->msbc_buffer_offset;
        printf("sbc frame storage is smaller then the output buffer");
        return;
    }

    memcpy(buf, state->msbc_buffer, bytes_to_copy);
    memmove(state->msbc_buffer, state->msbc_buffer + bytes_to_copy, sizeof(state->msbc_buffer) - bytes_to_copy);
    state->msbc_buffer_offset -= bytes_to_copy;
}

int hfp_msbc_num_bytes_in_stream(btstack_sbc_encoder_state_t* state){
    return state->msbc_buffer_offset;
}

int hfp_msbc_num_audio_samples_per_frame(btstack_sbc_encoder_state_t* state){
    return btstack_sbc_encoder_num_audio_frames(state);
}



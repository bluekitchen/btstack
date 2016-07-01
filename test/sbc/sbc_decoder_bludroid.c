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
 
// *****************************************************************************
//
// SBC decoder tests
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "sbc_decoder.h"
#include "oi_codec_sbc.h"
#include "oi_assert.h"

static uint8_t frame_buffer[5000];
static int16_t pcm_data[1000];
static uint32_t pcm_bytes = sizeof(pcm_data);

static OI_UINT32 decoder_data[10000];

typedef struct {
    OI_UINT32 bytes_read;
    OI_UINT32 bytes_in_frame;
    const OI_BYTE *frame_data;
    OI_CODEC_SBC_DECODER_CONTEXT decoder_context;
} bludroid_decoder_state_t;

static bludroid_decoder_state_t bd_state;

int sbc_decoder_num_samples_per_frame(sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_blocks * decoder_state->decoder_context.common.frameInfo.nrof_subbands;
}

int sbc_decoder_num_channels(sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_channels;
}

int sbc_decoder_sample_rate(sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.frequency;
}


void OI_AssertFail(char* file, int line, char* reason){
    printf("AssertFail file %s, line %d, reason %s\n", file, line, reason);
}

void sbc_decoder_init(sbc_decoder_state_t * state, void (*callback)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context), void * context){
    OI_STATUS status = OI_CODEC_mSBC_DecoderReset(&(bd_state.decoder_context), decoder_data, sizeof(decoder_data));
    if (status != 0){
        printf("Reset decoder error %d\n", status);
    }
    bd_state.bytes_in_frame = 0;
    bd_state.bytes_read = 0;
    bd_state.frame_data = NULL;
    
    state->handle_pcm_data = callback;
    state->context = context;
    state->decoder_state = &bd_state;
    //OI_STATUS status = OI_CODEC_SBC_DecoderReset(&context, decoder_data, sizeof(decoder_data), 1, 1, FALSE);
}

static void append_received_sbc_data(bludroid_decoder_state_t * state, uint8_t * buffer, int size){
    int numFreeBytes = sizeof(frame_buffer) - state->bytes_in_frame;

    if (size > numFreeBytes){
        printf("sbc data: more bytes read %u than free bytes in buffer %u", size, numFreeBytes);
        exit(10);
    }

    memcpy(frame_buffer + state->bytes_in_frame, buffer, size);
    state->bytes_in_frame += size;
    state->bytes_read = state->bytes_in_frame;
    state->frame_data = frame_buffer;
}

void sbc_decoder_process_data(sbc_decoder_state_t * state, uint8_t * buffer, int size){
    int bytes_read = size;
    while (bytes_read > 0){
        bludroid_decoder_state_t * bd_decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
        int space_in_frame_buffer = sizeof(frame_buffer) - bd_decoder_state->bytes_in_frame;
        int bytes_to_append = space_in_frame_buffer > bytes_read ? bytes_read : space_in_frame_buffer;
        append_received_sbc_data(bd_decoder_state, buffer, bytes_to_append);
        
        while (1){
            OI_STATUS status = OI_CODEC_SBC_DecodeFrame(&(bd_decoder_state->decoder_context), &(bd_decoder_state->frame_data), &(bd_decoder_state->bytes_in_frame), pcm_data, &pcm_bytes);
        
            if (status != 0){
                if (status != OI_CODEC_SBC_NOT_ENOUGH_HEADER_DATA && status != OI_CODEC_SBC_NOT_ENOUGH_BODY_DATA){
                    OI_CODEC_SBC_DumpConfig(&(bd_decoder_state->decoder_context.common.frameInfo));
                    printf("Frame decode error %d\n", status);
                    break;
                }
                // printf("Not enough data, read next %u bytes, move %d bytes\n", state->bytes_read-state->bytes_in_frame, state->bytes_in_frame);
                memmove(frame_buffer, frame_buffer + bd_decoder_state->bytes_read - bd_decoder_state->bytes_in_frame, bd_decoder_state->bytes_in_frame);
                break;
            }
            state->handle_pcm_data(pcm_data, sbc_decoder_num_samples_per_frame(state), sbc_decoder_num_channels(state), sbc_decoder_sample_rate(state), state->context);
        }
        
        memmove(buffer, buffer + bytes_to_append, size - bytes_to_append);
        bytes_read -= bytes_to_append;
    }
}

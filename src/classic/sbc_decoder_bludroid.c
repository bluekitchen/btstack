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
#include "sbc_plc.h"

#include "oi_codec_sbc.h"
#include "oi_assert.h"

#include "btstack.h"

#define SBC_MAX_CHANNELS 2
#define DECODER_DATA_SIZE (SBC_MAX_CHANNELS*SBC_MAX_BLOCKS*SBC_MAX_BANDS * 2 + SBC_CODEC_MIN_FILTER_BUFFERS*SBC_MAX_BANDS*SBC_MAX_CHANNELS * 2)

typedef struct {
    OI_UINT32 bytes_in_frame_buffer;
    OI_CODEC_SBC_DECODER_CONTEXT decoder_context;
    
    uint8_t frame_buffer[SBC_MAX_FRAME_LEN];
    int16_t pcm_plc_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    int16_t pcm_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    uint32_t pcm_bytes;
    OI_UINT32 decoder_data[(DECODER_DATA_SIZE+3)/4]; 
} bludroid_decoder_state_t;

static sbc_decoder_state_t * sbc_state_singelton = NULL;
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

void sbc_decoder_init(sbc_decoder_state_t * state, sbc_mode_t mode, void (*callback)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context), void * context){
    if (sbc_state_singelton && sbc_state_singelton != state ){
        log_error("SBC decoder: different sbc decoder state is allready registered");
    } 
    OI_STATUS status;
    switch (mode){
        case SBC_MODE_STANDARD:
            status = OI_CODEC_SBC_DecoderReset(&(bd_state.decoder_context), bd_state.decoder_data, sizeof(bd_state.decoder_data), 2, 1, FALSE);
            break;
        case SBC_MODE_mSBC:
            status = OI_CODEC_mSBC_DecoderReset(&(bd_state.decoder_context), bd_state.decoder_data, sizeof(bd_state.decoder_data));
            break;
    }

    if (status != 0){
        log_error("SBC decoder: error during reset %d\n", status);
    }
    
    sbc_state_singelton = state;
    bd_state.bytes_in_frame_buffer = 0;
    bd_state.pcm_bytes = sizeof(bd_state.pcm_data);

    state->handle_pcm_data = callback;
    state->mode = mode;
    state->context = context;
    state->decoder_state = &bd_state;
    sbc_plc_init(&state->plc_state);
}

static void append_received_sbc_data(bludroid_decoder_state_t * state, uint8_t * buffer, int size){
    int numFreeBytes = sizeof(state->frame_buffer) - state->bytes_in_frame_buffer;

    if (size > numFreeBytes){
        log_error("SBC data: more bytes read %u than free bytes in buffer %u", size, numFreeBytes);
    }

    memcpy(state->frame_buffer + state->bytes_in_frame_buffer, buffer, size);
    state->bytes_in_frame_buffer += size;
}


void sbc_decoder_process_data(sbc_decoder_state_t * state, uint8_t * buffer, int size){
    bludroid_decoder_state_t * bd_decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
    int bytes_to_process = size;
    int msbc_frame_size = 57;

    // printf("<<-- enter -->>\n");

    // printf("Process data: in buffer %u, new %u\n", bd_decoder_state->bytes_in_frame_buffer, size);

    while (bytes_to_process > 0){

        int bytes_missing_for_complete_msbc_frame = msbc_frame_size - bd_decoder_state->bytes_in_frame_buffer;
        int bytes_to_append = btstack_min(bytes_to_process, bytes_missing_for_complete_msbc_frame);
        
        append_received_sbc_data(bd_decoder_state, buffer, bytes_to_append);
        // printf("Append %u bytes, now %u in buffer \n", bytes_to_append, bd_decoder_state->bytes_in_frame_buffer);
        buffer           += bytes_to_append;
        bytes_to_process -= bytes_to_append;
        
        if (bd_decoder_state->bytes_in_frame_buffer < msbc_frame_size){
            // printf("not enough data %d > %d\n", msbc_frame_size, bd_decoder_state->bytes_in_frame_buffer);
            if (bytes_to_process){
                printf("SHOULD NOT HAPPEN... not enough bytes, but bytes left to process\n");
            }
            break;
        }
        
        uint16_t bytes_in_buffer_before = bd_decoder_state->bytes_in_frame_buffer;
        uint16_t bytes_processed = 0;
        const OI_BYTE *frame_data = bd_decoder_state->frame_buffer;

        OI_STATUS status = OI_CODEC_SBC_DecodeFrame(&(bd_decoder_state->decoder_context), 
                                                    &frame_data, 
                                                    &(bd_decoder_state->bytes_in_frame_buffer), 
                                                    bd_decoder_state->pcm_plc_data, 
                                                    &(bd_decoder_state->pcm_bytes));

        bytes_processed = bytes_in_buffer_before - bd_decoder_state->bytes_in_frame_buffer;

        // printf("Decode status %u, processed %u, left %u\n", status, bytes_processed, bd_decoder_state->bytes_in_frame_buffer);

        memmove(bd_decoder_state->frame_buffer, bd_decoder_state->frame_buffer + bytes_processed, bd_decoder_state->bytes_in_frame_buffer);

        //printf("frame_count %d, expected frame len %d, bytes in frame %d \n", frame_count, OI_CODEC_SBC_CalculateFramelen(&bd_decoder_state->decoder_context.common.frameInfo), bytes_in_buffer_before);
        switch(status){
            case 0:
                // printf("OK: \n");
                sbc_plc_good_frame(&state->plc_state, bd_decoder_state->pcm_plc_data, bd_decoder_state->pcm_data);
                state->handle_pcm_data(bd_decoder_state->pcm_data, 
                                    sbc_decoder_num_samples_per_frame(state), 
                                    sbc_decoder_num_channels(state), 
                                    sbc_decoder_sample_rate(state), state->context);
                continue;
            case OI_CODEC_SBC_NOT_ENOUGH_HEADER_DATA:
            case OI_CODEC_SBC_NOT_ENOUGH_BODY_DATA:
                // printf("NOT_ENOUGH_DATA: \n");
                // do nothing
                break;

            case OI_CODEC_SBC_NO_SYNCWORD:
            case OI_CODEC_SBC_CHECKSUM_MISMATCH:
                // printf("NO_SYNCWORD or CHECKSUM_MISMATCH\n");
                bd_decoder_state->bytes_in_frame_buffer = 0;

                frame_data = sbc_plc_zero_signal_frame();
                OI_UINT32 bytes_in_frame_buffer = msbc_frame_size;
                
                status = OI_CODEC_SBC_DecodeFrame(&(bd_decoder_state->decoder_context), 
                                                    &frame_data, 
                                                    &bytes_in_frame_buffer, 
                                                    bd_decoder_state->pcm_plc_data, 
                                                    &(bd_decoder_state->pcm_bytes));
                // printf("after bad frame, new status: %d, bytes in frame %d\n", status, bytes_in_frame_buffer);

                if (status != 0) exit(10);
                sbc_plc_bad_frame(&state->plc_state, bd_decoder_state->pcm_plc_data, bd_decoder_state->pcm_data);
                state->handle_pcm_data(bd_decoder_state->pcm_data, 
                                    sbc_decoder_num_samples_per_frame(state), 
                                    sbc_decoder_num_channels(state), 
                                    sbc_decoder_sample_rate(state), state->context);
                
                break;
            default:
                printf("Frame decode error: %d\n", status);
                break;
        }
    }
    // printf ("<<-- exit -->>\n");
}

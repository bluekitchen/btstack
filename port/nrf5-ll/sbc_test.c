/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "classic/btstack_sbc.h"
#include "classic/avdtp.h"
#include "classic/avdtp_source.h"

#define NUM_CHANNELS        2
#define SAMPLE_RATE         44100
#define BYTES_PER_AUDIO_SAMPLE   (2*NUM_CHANNELS)
#define LATENCY             300 // ms

#ifndef M_PI
#define M_PI  3.14159265
#endif
#define TABLE_SIZE_441HZ   100

int16_t source[] = {
     0,   2057,   4106,   6139,   8148,  10125,  12062,  13951,  15785,  17557,
 19259,  20886,  22430,  23886,  25247,  26509,  27666,  28713,  29648,  30465,
 31163,  31737,  32186,  32508,  32702,  32767,  32702,  32508,  32186,  31737,
 31163,  30465,  29648,  28713,  27666,  26509,  25247,  23886,  22430,  20886,
 19259,  17557,  15785,  13951,  12062,  10125,   8148,   6139,   4106,   2057,
     0,  -2057,  -4106,  -6139,  -8148, -10125, -12062, -13951, -15785, -17557,
-19259, -20886, -22430, -23886, -25247, -26509, -27666, -28713, -29648, -30465,
-31163, -31737, -32186, -32508, -32702, -32767, -32702, -32508, -32186, -31737,
-31163, -30465, -29648, -28713, -27666, -26509, -25247, -23886, -22430, -20886,
-19259, -17557, -15785, -13951, -12062, -10125,  -8148,  -6139,  -4106,  -2057,
};

typedef struct {
    int left_phase;
    int right_phase;
} paTestData;

static uint16_t pcm_frame[2*8*16];

static paTestData sin_data;

static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
static btstack_sbc_encoder_state_t sbc_encoder_state;
static btstack_sbc_decoder_state_t sbc_decoder_state;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    UNUSED(data);
    UNUSED(num_samples);
    UNUSED(num_channels);
}

static void fill_sine_frame(void *userData, int num_samples_to_write){
    paTestData *data = (paTestData*)userData;
    int count = 0;
    int offset = 0;
    while (count < num_samples_to_write){
        pcm_frame[offset++] = source[data->left_phase];;
        pcm_frame[offset++] = source[data->right_phase];;
        count++;
        data->left_phase++;
        if (data->left_phase >= TABLE_SIZE_441HZ){
            data->left_phase -= TABLE_SIZE_441HZ;
        }
        data->right_phase++; 
        if (data->right_phase >= TABLE_SIZE_441HZ){
            data->right_phase -= TABLE_SIZE_441HZ;
        } 
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argc;
    (void) argv;
    btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 16, 8, 2, 44100, 53);
                    
    /* initialise sinusoidal wavetable */
    int i;
#if 0
    for (i=0; i<TABLE_SIZE_441HZ; i++){ 
        sin_data.source[i] = sin(((double)i/(double)TABLE_SIZE_441HZ) * M_PI * 2.)*32767;
    }
#endif
    sin_data.left_phase = sin_data.right_phase = 0;
    
    btstack_sbc_decoder_init(&sbc_decoder_state, mode, handle_pcm_data, NULL);
    int num_frames = 1000;
    uint32_t timestamp_start;
    uint32_t encoding_time = 0;
    uint32_t decoding_time = 0;
    
    timestamp_start = btstack_run_loop_get_time_ms();
    for (i=0; i<num_frames; i++){
        // printf("frame %u\n", i);
        fill_sine_frame(&sin_data, 128);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
    }
    encoding_time = btstack_run_loop_get_time_ms() - timestamp_start;

    timestamp_start = btstack_run_loop_get_time_ms();
    for (i=0; i<num_frames; i++){
        fill_sine_frame(&sin_data, 128);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        btstack_sbc_decoder_process_data(&sbc_decoder_state, 0, btstack_sbc_encoder_sbc_buffer(), btstack_sbc_encoder_sbc_buffer_length());
    }
    decoding_time =  btstack_run_loop_get_time_ms() - timestamp_start - encoding_time;

    printf("%u frames encoded in %u ms\n", num_frames, (int) encoding_time);
    printf("%u frames decoded in %u ms\n", num_frames, (int) decoding_time);
}

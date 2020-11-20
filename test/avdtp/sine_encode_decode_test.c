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
#include <portaudio.h>

#include "btstack_ring_buffer.h"
#include "classic/btstack_sbc.h"
#include "wav_util.h"
#include "classic/avdtp.h"
#include "classic/avdtp_source.h"
#include "btstack_stdin.h"

#define NUM_CHANNELS        2
#define SAMPLE_RATE         44100
#define BYTES_PER_AUDIO_SAMPLE   (2*NUM_CHANNELS)
#define LATENCY             300 // ms

#ifndef M_PI
#define M_PI  3.14159265
#endif
#define TABLE_SIZE_441HZ   100

typedef struct {
    int16_t source[TABLE_SIZE_441HZ];
    int left_phase;
    int right_phase;
} paTestData;

static uint8_t pcm_frame[2*8*16*2];

static paTestData sin_data;
// static int total_num_samples = 0;

static char * output_wav_filename = "test_output_sine.wav";
// static char * input_wav_filename = "test_input_sine.wav";

static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
static btstack_sbc_encoder_state_t sbc_encoder_state;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    wav_writer_write_int16(num_samples*num_channels, data);
}

static void fill_sine_frame(void *userData, int num_samples_to_write){
    paTestData *data = (paTestData*)userData;
    int count = 0;
    int offset = 0;
    while (count < num_samples_to_write){
        uint8_t write_data[BYTES_PER_AUDIO_SAMPLE];
        *(int16_t*)&write_data[0] = data->source[data->left_phase];
        *(int16_t*)&write_data[2] = data->source[data->right_phase];
        
        memcpy(pcm_frame+offset, write_data, BYTES_PER_AUDIO_SAMPLE);
        offset += BYTES_PER_AUDIO_SAMPLE;
        count++;

        data->left_phase += 1;
        if (data->left_phase >= TABLE_SIZE_441HZ){
            data->left_phase -= TABLE_SIZE_441HZ;
        }
        data->right_phase += 1; 
        if (data->right_phase >= TABLE_SIZE_441HZ){
            data->right_phase -= TABLE_SIZE_441HZ;
        } 
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argc;
    (void) argv;
    btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 16, 8, SBC_ALLOCATION_METHOD_LOUDNESS, 44100, 53, SBC_CHANNEL_MODE_STEREO);
                    
    /* initialise sinusoidal wavetable */
    int i;
    for (i=0; i<TABLE_SIZE_441HZ; i++){ 
        sin_data.source[i] = sin(((double)i/(double)TABLE_SIZE_441HZ) * M_PI * 2.)*32767;
    }
    sin_data.left_phase = sin_data.right_phase = 0;
    // wav_writer_open(input_wav_filename, NUM_CHANNELS, SAMPLE_RATE);
    wav_writer_open(output_wav_filename, NUM_CHANNELS, SAMPLE_RATE);
    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);

    for (i=0; i<3500; i++){
        fill_sine_frame(&sin_data, 128);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        btstack_sbc_decoder_process_data(&state, 0, btstack_sbc_encoder_sbc_buffer(), btstack_sbc_encoder_sbc_buffer_length());

    }
    wav_writer_close();
    exit(0);
}

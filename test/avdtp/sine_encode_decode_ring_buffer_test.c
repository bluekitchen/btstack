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
#include "btstack_sbc.h"
#include "wav_util.h"
#include "avdtp.h"
#include "avdtp_source.h"
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

static uint32_t fill_audio_ring_buffer_timeout = 50; //ms
static paTestData sin_data;
// static int total_num_samples = 0;

static char * output_wav_filename = "test_output_ring_sine.wav";
// static char * input_wav_filename = "test_input_sine.wav";

static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;

static avdtp_stream_endpoint_t * local_stream_endpoint;


static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    wav_writer_write_int16(num_samples*num_channels, data);
}


static void fill_audio_ring_buffer(void *userData, int num_samples_to_write, avdtp_stream_endpoint_t * stream_endpoint){
    paTestData *data = (paTestData*)userData;
    int count = 0;
    while (btstack_ring_buffer_bytes_free(&stream_endpoint->audio_ring_buffer) >= BYTES_PER_AUDIO_SAMPLE && count < num_samples_to_write){
        uint8_t write_data[BYTES_PER_AUDIO_SAMPLE];
        *(int16_t*)&write_data[0] = data->source[data->left_phase];
        *(int16_t*)&write_data[2] = data->source[data->right_phase];
        
        btstack_ring_buffer_write(&stream_endpoint->audio_ring_buffer, write_data, BYTES_PER_AUDIO_SAMPLE);
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

static void store_sbc_frame_for_transmission(uint8_t * sbc_frame, int sbc_frame_size, avdtp_stream_endpoint_t * stream_endpoint){
    if (btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer) >= (sbc_frame_size + 1)){
        // printf("    store_sbc_frame_for_transmission\n");
        uint8_t size_buffer = sbc_frame_size;
        btstack_ring_buffer_write(&stream_endpoint->sbc_ring_buffer, &size_buffer, 1);
        btstack_ring_buffer_write(&stream_endpoint->sbc_ring_buffer, sbc_frame, sbc_frame_size);
    } else {
        printf("No space in sbc buffer\n");
    }
}


static void avdtp_source_stream_endpoint_run(avdtp_stream_endpoint_t * stream_endpoint){
    // performe sbc encoding
    int total_num_bytes_read = 0;
    int num_audio_samples_to_read = btstack_sbc_encoder_num_audio_frames();
    int audio_bytes_to_read = num_audio_samples_to_read * BYTES_PER_AUDIO_SAMPLE; 

    printf("run: audio samples %u, audio_bytes_to_read: %d\n", num_audio_samples_to_read, audio_bytes_to_read);
    printf("     audio buf, bytes available: %d\n", btstack_ring_buffer_bytes_available(&stream_endpoint->audio_ring_buffer));
    printf("     sbc buf,   bytes free:      %d\n", btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer));

    while (btstack_ring_buffer_bytes_available(&stream_endpoint->audio_ring_buffer) >= audio_bytes_to_read
        && btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer) >= 120){ // TODO use real value
        
        uint32_t number_of_bytes_read = 0;
        uint8_t pcm_frame[256*BYTES_PER_AUDIO_SAMPLE];
        btstack_ring_buffer_read(&stream_endpoint->audio_ring_buffer, pcm_frame, audio_bytes_to_read, &number_of_bytes_read); 
        // printf("     num audio bytes read %d\n", number_of_bytes_read);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        
        uint16_t sbc_frame_bytes = btstack_sbc_encoder_sbc_buffer_length();
        printf("decode %d bytes\n", sbc_frame_bytes);
        total_num_bytes_read += number_of_bytes_read;

        store_sbc_frame_for_transmission(btstack_sbc_encoder_sbc_buffer(), sbc_frame_bytes, stream_endpoint);
        btstack_sbc_decoder_process_data(&state, 0, btstack_sbc_encoder_sbc_buffer(), sbc_frame_bytes);
    }
}

static void test_fill_audio_ring_buffer_timeout_handler(btstack_timer_source_t * timer){
    avdtp_stream_endpoint_t * stream_endpoint = btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&stream_endpoint->fill_audio_ring_buffer_timer, fill_audio_ring_buffer_timeout); // 2 seconds timeout
    btstack_run_loop_add_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = fill_audio_ring_buffer_timeout;
    if (stream_endpoint->time_audio_data_sent > 0){
        update_period_ms = now - stream_endpoint->time_audio_data_sent;
    } 
    uint32_t num_samples = (update_period_ms * 44100) / 1000;
    stream_endpoint->acc_num_missed_samples += (update_period_ms * 44100) % 1000;

    if (stream_endpoint->acc_num_missed_samples >= 1000){
        num_samples++;
        stream_endpoint->acc_num_missed_samples -= 1000;
    }

    fill_audio_ring_buffer(&sin_data, num_samples, stream_endpoint);
    stream_endpoint->time_audio_data_sent = now;

    avdtp_source_stream_endpoint_run(stream_endpoint);
}

static void test_fill_audio_ring_buffer_timer_start(avdtp_stream_endpoint_t * stream_endpoint){
    btstack_run_loop_remove_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
    btstack_run_loop_set_timer_handler(&stream_endpoint->fill_audio_ring_buffer_timer, test_fill_audio_ring_buffer_timeout_handler);
    btstack_run_loop_set_timer_context(&stream_endpoint->fill_audio_ring_buffer_timer, stream_endpoint);
    btstack_run_loop_set_timer(&stream_endpoint->fill_audio_ring_buffer_timer, fill_audio_ring_buffer_timeout); // 50 ms timeout
    btstack_run_loop_add_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
}

static void test_fill_audio_ring_buffer_timer_stop(avdtp_stream_endpoint_t * stream_endpoint){
    btstack_run_loop_remove_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
} 

static void stream_data_start(void){
    test_fill_audio_ring_buffer_timer_start(local_stream_endpoint);
}

static void stream_data_stop(void){
    test_fill_audio_ring_buffer_timer_stop(local_stream_endpoint);
    wav_writer_close();
}

static void show_usage(void){
    printf("\n--- Streaming with ring buffer Test Console ---\n");
    printf("x      - start data stream\n");
    printf("X      - stop  data stream\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    switch (cmd){
        case 'x':
            printf("start streaming sine\n");
            stream_data_start();
            break;
        case 'X':
            printf("stop streaming sine\n");
            stream_data_stop();
            break;
            
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argc;
    (void) argv;
    local_stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, AVDTP_AUDIO);
    btstack_sbc_encoder_init(&(local_stream_endpoint->sbc_encoder_state), SBC_MODE_STANDARD, 16, 8, SBC_ALLOCATION_METHOD_LOUDNESS, 44100, 53, SBC_CHANNEL_MODE_STEREO);
                    
    /* initialise sinusoidal wavetable */
    int i;
    for (i=0; i<TABLE_SIZE_441HZ; i++){ 
        sin_data.source[i] = sin(((double)i/(double)TABLE_SIZE_441HZ) * M_PI * 2.)*32767;
    }
    sin_data.left_phase = sin_data.right_phase = 0;
    // wav_writer_open(input_wav_filename, NUM_CHANNELS, SAMPLE_RATE);
    printf("Outputfile: %s\n", output_wav_filename);
    wav_writer_open(output_wav_filename, NUM_CHANNELS, SAMPLE_RATE);
    btstack_sbc_decoder_init(&state, mode, handle_pcm_data, NULL);
    btstack_stdin_setup(stdin_process);
    return 0;
}

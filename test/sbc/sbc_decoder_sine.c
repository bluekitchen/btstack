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

#include "btstack.h"
#include "classic/btstack_sbc.h"
#include "data_sine_stereo_sbc.h"

#define SAMPLE_RATE 44100

static int total_num_samples = 0;
static int frame_count = 0;

#define NUM_SAMPLES 128
#define NUM_CHANNELS 2
#define BYTES_PER_SAMPLE 2

static uint16_t audio_samples0[NUM_SAMPLES*2];
static uint16_t audio_samples1[NUM_SAMPLES*2];
static volatile int playback_buffer;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
	printf("Samples: num_samples %u, num_channels %u, sample_rate %u\n", num_samples, num_channels, sample_rate);
	// printf_hexdump(data, num_samples * num_channels * 2);
    int i;
    for (i=0;i<num_channels*num_samples;i += 2){
        if ((i%24) == 0) printf("\n");
        printf ("%12d ", data[i]);
    }
    printf("\n");

    total_num_samples+=num_samples*num_channels;
    frame_count++;
}

int btstack_main (int argc, const char * argv[]){
	(void) argc;
	(void) argv;


    btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
    btstack_sbc_decoder_state_t state;
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    btstack_sbc_decoder_test_set_plc_enabled(0);
    uint32_t t_start = btstack_run_loop_get_time_ms();
    playback_buffer = 1;
    uint32_t offset = 0;

    uint32_t sbc_len = data_sine_stereo_sbc_len;
    uint8_t * sbc_data = data_sine_stereo_sbc;

    btstack_sbc_decoder_process_data(&state, 0, &sbc_data[offset], 74);
    offset += 74;

    while (1){

	    btstack_sbc_decoder_process_data(&state, 0, &sbc_data[offset], 74);
	    offset += 74;

	    if (offset >= sbc_len){
	    	offset = 0;
	    }

	    while (playback_buffer == 0){
			__asm__("wfe");
		}

	    btstack_sbc_decoder_process_data(&state, 0, &sbc_data[offset], 74);
	    offset += 74;

	    if (offset >= sbc_len){
	    	offset = 0;
	    }

	    while (playback_buffer == 1){
			__asm__("wfe");
		}

    }
#if 0
    uint32_t t_end = btstack_run_loop_get_time_ms();
    printf("Decoding done. Processed %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", frame_count, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
    printf("Time for 100 frames %u ms\n", t_end - t_start);	// frame len 74 in this sbc test sample
#endif
    return 0;
}

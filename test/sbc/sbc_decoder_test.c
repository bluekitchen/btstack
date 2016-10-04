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

#include "oi_assert.h"

#include "btstack.h"
#include "btstack_sbc.h"

#include "wav_util.h"

static uint8_t read_buffer[24];
static int total_num_samples = 0;
static int frame_count = 0;

static void show_usage(void){
    printf("\n\nUsage: ./sbc_decoder_test input_file msbc|sbc plc_enabled(0|1) corrupt_frame_period\n\n");
}

static ssize_t __read(int fd, void *buf, size_t count){
    ssize_t len, pos = 0;

    while (count > 0) {
        len = read(fd, (int8_t * )buf + pos, count);
        if (len <= 0)
            return pos;

        count -= len;
        pos   += len;
    }
    return pos;
}

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    wav_writer_write_int16(num_samples, data);
    total_num_samples+=num_samples;
    frame_count++;
}

int main (int argc, const char * argv[]){
    if (argc < 4){
        show_usage();
        return -1;
    }
    
    btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
    
    const char * filename = argv[1];

    char sbc_filename[1000];
    char wav_filename[1000];

    strcpy(sbc_filename, filename);
    strcpy(wav_filename, filename);

    const int plc_enabled = atoi(argv[3]);
    const int corrupt_frame_period = atoi(argv[4]);
    
    int sample_rate = 8000;

    printf("\n");
    if (strncmp(argv[2], "msbc", 4) == 0 ){
        mode = SBC_MODE_mSBC;
        printf("Using SBC_MODE_mSBC mode.\n");
        sample_rate = 16000;
    } else {
        printf("Using SBC_MODE_STANDARD mode.\n");
    } 
    
    if (plc_enabled){
        printf("PLC enabled.\n");
    } else {
        printf("PLC disbled.\n"); 
    }

    if (corrupt_frame_period > 0){
        printf("Corrupt frame period: every %d frames.\n", corrupt_frame_period);
    }
    if (mode == SBC_MODE_mSBC){
        strcat(sbc_filename, ".msbc");
    } else {
        strcat(sbc_filename, ".sbc");
    }

    if (plc_enabled){
        strcat(wav_filename, "_plc.wav");
    } else {
        strcat(wav_filename, ".wav");
    }

   
    int fd = open(sbc_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", sbc_filename);
        return -1;
    }
    printf("Open sbc file: %s\n", sbc_filename);
    

    btstack_sbc_decoder_state_t state;
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    wav_writer_open(wav_filename, 1, sample_rate);
    
    if (!plc_enabled){
        btstack_sbc_decoder_test_disable_plc();
    }
    if (corrupt_frame_period > 0){
        btstack_sbc_decoder_test_simulate_corrupt_frames(corrupt_frame_period);
    }

    while (1){
        // get next chunk
        int bytes_read = __read(fd, read_buffer, sizeof(read_buffer));
        if (0 >= bytes_read) break;
        // process chunk
        btstack_sbc_decoder_process_data(&state, 0, read_buffer, bytes_read);
    }

    wav_writer_close();
    close(fd);
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("\nDecoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
    printf("Write %d frames to wav file: %s\n\n", frame_count, wav_filename);

}

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

static uint8_t read_buffer[118];
static int total_num_samples = 0;
static int frame_count = 0;
static int wav_writer_opened = 0;
static char wav_filename[1000];

static uint8_t indices0[] = { 0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c};


static void show_usage(void){
    printf("\n\nUsage: ./sbc_decoder_test input_file [0-sbc|1-msbc] enable_partial_frame_corruption enable_plc corrupt_whole_frame_period \n\n");
    printf("Example: ./sbc_decoder_test data/data/sine-stereo 0 1 0 0\n");
    printf("Example: ./sbc_decoder_test data/data/sine-stereo 1 1 0 0\n\n");
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
    UNUSED(context);
    if (!wav_writer_opened){
        wav_writer_opened = 1;
        wav_writer_open(wav_filename, num_channels, sample_rate);

    }

    // printf("Sampels: num_samples %u, num_channels %u, sample_rate %u\n", num_samples, num_channels, sample_rate);
    // printf_hexdump(data, num_samples * num_channels * 2);
    // int i;
    // for (i=0;i<num_channels*num_samples;i += 2){
    //     if ((i%24) == 0) printf("\n");
    //     printf ("%12d ", data[i]);
    // }
    // printf("\n");
    wav_writer_write_int16(num_samples*num_channels, data);

    total_num_samples+=num_samples*num_channels;
    frame_count++;
}

int main (int argc, const char * argv[]){
    if (argc < 6){
        show_usage();
        return -1;
    }
    
    char sbc_filename[1000];
    int argv_pos = 1;
    const char * filename = argv[argv_pos++];
    
    btstack_sbc_mode_t mode = atoi(argv[argv_pos++]) == 0? SBC_MODE_STANDARD : SBC_MODE_mSBC;
    const int partial_frame_corruption = atoi(argv[argv_pos++]);
    const int plc_enabled = atoi(argv[argv_pos++]);
    const int corrupt_frame_period = atoi(argv[argv_pos++]);
    
    static unsigned g_phase = 0;
    static unsigned g_phase2 = 0;

    strcpy(sbc_filename, filename);
    strcpy(wav_filename, filename);
    
    printf("\n");
    if (mode == SBC_MODE_mSBC){
        printf("Using SBC_MODE_mSBC mode.\n");
        strcat(sbc_filename, ".msbc");
    } else {
        printf("Using SBC_MODE_STANDARD mode.\n");
        strcat(sbc_filename, ".sbc");
    } 
    
    if (plc_enabled){
        printf("PLC enabled.\n");
        strcat(wav_filename, "_decoded_bludroid_plc.wav");
    } else {
        printf("PLC disbled.\n"); 
        strcat(wav_filename, "_decoded_bludroid.wav");
    }

    if (corrupt_frame_period > 0){
        printf("Corrupt frame period: every %d frames.\n", corrupt_frame_period);
    }
    

    int fd = open(sbc_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", sbc_filename);
        return -1;
    }
    printf("Open sbc file: %s\n", sbc_filename);
    

    btstack_sbc_decoder_state_t state;
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    
    if (!plc_enabled){
        btstack_sbc_decoder_test_set_plc_enabled(0);
    }
    if (corrupt_frame_period > 0){
        btstack_sbc_decoder_test_simulate_corrupt_frames(corrupt_frame_period);
    }

    while (1){
        // get next chunk
        int bytes_read = __read(fd, read_buffer, sizeof(read_buffer));
        if (0 >= bytes_read) break;

        if (partial_frame_corruption) {
            // Inject errors every now and then.
            if ((g_phase & 0x7f) >= 128-8){
                uint8_t * data = indices0;
                data[g_phase2] = 0xff;            
                g_phase2++;
            } else {
                g_phase2 = 0;
            }
            g_phase++;
        }

        btstack_sbc_decoder_process_data(&state, 0, indices0, sizeof(indices0));
    }

    wav_writer_close();
    close(fd);
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("WAV Writer: Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n", total_frames_nr, state.good_frames_nr, total_frames_nr - state.good_frames_nr);
    printf("Write %d frames to wav file: %s\n\n", frame_count, wav_filename);

}

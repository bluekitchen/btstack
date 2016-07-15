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
#include "sbc_decoder.h"
 
static uint8_t read_buffer[24];

typedef struct wav_writer_state {
    FILE * wav_file;
    int total_num_samples;
    int frame_count;
} wav_writer_state_t;

static void show_usage(void){
    printf("\n\nUsage: ./sbc_decoder_test input_file msbc|sbc plc_enabled(0|1) corrupt_frame_period\n\n");
}

static ssize_t __read(int fd, void *buf, size_t count){
    ssize_t len, pos = 0;

    while (count > 0) {
        len = read(fd, buf + pos, count);
        if (len <= 0)
            return pos;

        count -= len;
        pos   += len;
    }
    return pos;
}

void little_endian_fstore_16(FILE *wav_file, uint16_t value){
    uint8_t buf[2];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 2, wav_file);
}

void little_endian_fstore_32(FILE *wav_file, uint32_t value){
    uint8_t buf[4];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 4, wav_file);
}


static void write_wav_header(FILE * wav_file,  int total_num_samples, int num_channels, int sample_rate){
    unsigned int bytes_per_sample = 2;
    /* write RIFF header */
    fwrite("RIFF", 1, 4, wav_file);
    // num_samples = blocks * subbands
    uint32_t data_bytes = (uint32_t) (bytes_per_sample * total_num_samples * num_channels);
    little_endian_fstore_32(wav_file, data_bytes + 36); 
    fwrite("WAVE", 1, 4, wav_file);

    int byte_rate = sample_rate * num_channels * bytes_per_sample;
    int bits_per_sample = 8 * bytes_per_sample;
    int block_align = num_channels * bits_per_sample;
    int fmt_length = 16;
    int fmt_format_tag = 1; // PCM

    /* write fmt chunk */
    fwrite("fmt ", 1, 4, wav_file);
    little_endian_fstore_32(wav_file, fmt_length);
    little_endian_fstore_16(wav_file, fmt_format_tag);
    little_endian_fstore_16(wav_file, num_channels);
    little_endian_fstore_32(wav_file, sample_rate);
    little_endian_fstore_32(wav_file, byte_rate);
    little_endian_fstore_16(wav_file, block_align);   
    little_endian_fstore_16(wav_file, bits_per_sample);
    
    /* write data chunk */
    fwrite("data", 1, 4, wav_file); 
    little_endian_fstore_32(wav_file, data_bytes);
}

static void write_wav_data(FILE * wav_file, int num_samples, int num_channels, int16_t * data){
    fwrite(data, num_samples, 2, wav_file);
}

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    wav_writer_state_t * wav_writer_state = (wav_writer_state_t*) context;
    write_wav_data(wav_writer_state->wav_file, num_samples, num_channels, data);
    wav_writer_state->total_num_samples+=num_samples;
    wav_writer_state->frame_count++;
}

int main (int argc, const char * argv[]){
    if (argc < 4){
        show_usage();
        return -1;
    }
    
    sbc_mode_t mode = SBC_MODE_STANDARD;
    
    const char * filename = argv[1];

    char sbc_filename[30];
    char wav_filename[30];

    strcpy(sbc_filename, filename);
    strcpy(wav_filename, filename);

    const int plc_enabled = atoi(argv[3]);
    const int corrupt_frame_period = atoi(argv[4]);
    
    printf("\n");
    if (strncmp(argv[2], "msbc", 4) == 0 ){
        mode = SBC_MODE_mSBC;
        printf("Using SBC_MODE_mSBC mode.\n");
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
    FILE * wav_file = fopen(wav_filename, "wb");
    wav_writer_state_t wav_writer_state;
    wav_writer_state.wav_file = wav_file;
    wav_writer_state.frame_count = 0;
    wav_writer_state.total_num_samples = 0;
   
    sbc_decoder_state_t state;
    sbc_decoder_init(&state, mode, &handle_pcm_data, (void*)&wav_writer_state);
    if (!plc_enabled){
        sbc_decoder_test_disable_plc();
    }
    if (corrupt_frame_period > 0){
        sbc_decoder_test_simulate_corrupt_frames(corrupt_frame_period);
    }

    write_wav_header(wav_writer_state.wav_file, 0, 0, 0);
    
    while (1){
        // get next chunk
        int bytes_read = __read(fd, read_buffer, sizeof(read_buffer));
        if (0 >= bytes_read) break;
        // process chunk
        sbc_decoder_process_data(&state, read_buffer, bytes_read);
    }

    rewind(wav_file);
    write_wav_header(wav_writer_state.wav_file, wav_writer_state.total_num_samples, sbc_decoder_num_channels(&state), sbc_decoder_sample_rate(&state));

    fclose(wav_file);
    close(fd);
    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

    printf("\nDecoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
    printf("Write %d frames to wav file: %s\n\n", wav_writer_state.frame_count, wav_filename);

}

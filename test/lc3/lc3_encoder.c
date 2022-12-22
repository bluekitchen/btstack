/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
// LC3 decoder EHIMA
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "wav_util.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#define MAX_NUM_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480

static uint8_t  write_buffer[200];
static int16_t  samples_buffer[MAX_SAMPLES_PER_FRAME + MAX_NUM_CHANNELS];

static uint32_t frame_count = 0;

static void show_usage(const char * path){
    printf("Usage: %s input.wav output.lc3 frame_duration_ms octets_per_frame\n", path);
    printf("- frame_duration_ms: 7.5 or 10\n");
    printf("- octects_per_frame: 26..155\n");
    printf("\n\n");
}

int main (int argc, const char * argv[]){
    if (argc < 4){
        show_usage(argv[0]);
        return -1;
    }

    uint8_t argv_pos = 1;
    const char * wav_filename = argv[argv_pos++];
    const char * lc3_filename = argv[argv_pos++];

    btstack_lc3_frame_duration_t frame_duration;
    if (strcmp(argv[argv_pos], "10") == 0){
        frame_duration = BTSTACK_LC3_FRAME_DURATION_10000US;
    } else if (strcmp(argv[argv_pos], "7.5") == 0){
        frame_duration = BTSTACK_LC3_FRAME_DURATION_7500US;
    } else {
        printf("Invalid frame duration %s, must be either 7.5 or 10\n", argv[2]);
        return -10;
    }
    argv_pos++;

    uint16_t bytes_per_frame = atoi(argv[argv_pos++]);
    if ((bytes_per_frame < 26) || (bytes_per_frame > 155)){
        printf("Octets per Frame %u out of range [26..155]\n", bytes_per_frame);
        return -10;
    }

    int status = wav_reader_open(wav_filename);
    if (status != 0){
        printf("Could not open wav file %s\n", wav_filename);
        return -10;
    }

    // get wav config
    uint32_t sampling_frequency_hz = wav_reader_get_sampling_rate();
    uint8_t  num_channels = wav_reader_get_num_channels();
    uint16_t number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);

    // init decoder
    uint8_t channel;
    btstack_lc3_encoder_google_t encoder_contexts[MAX_NUM_CHANNELS];
    const btstack_lc3_encoder_t * lc3_encoder;
    for (channel = 0 ; channel < num_channels ; channel++){
        btstack_lc3_encoder_google_t * encoder_context = &encoder_contexts[channel];
        lc3_encoder = btstack_lc3_encoder_google_init_instance(encoder_context);
        lc3_encoder->configure(encoder_context, sampling_frequency_hz, frame_duration, number_samples_per_frame);
    }

    if (number_samples_per_frame > MAX_SAMPLES_PER_FRAME) return -10;

    // calc bitrate from num octets
    uint32_t bitrate_per_channel = ((uint32_t) bytes_per_frame * 80000) / (btstack_lc3_frame_duration_in_us(frame_duration) / 100);
    // fix bitrate for 8_1
    if ((sampling_frequency_hz == 8000) && (frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US)) {
        bitrate_per_channel = 27734;
    }
    // fix bitrate for 441_1 and 441_2
    if (sampling_frequency_hz == 44100){
        switch(frame_duration){
            case BTSTACK_LC3_FRAME_DURATION_7500US:
                bitrate_per_channel = 95060;
                break;
            case BTSTACK_LC3_FRAME_DURATION_10000US:
                bitrate_per_channel = 95500;
                break;
            default:
                btstack_unreachable();
                break;
        }
    }
    uint32_t bitrate = bitrate_per_channel * num_channels;

    // create lc3 file and write header for floating point implementation
    FILE * lc3_file = fopen(lc3_filename, "wb");
    if (!lc3_file) return 1;

    uint16_t frame_duration_100us = (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US) ? 100 : 75;

    uint8_t header[18];
    little_endian_store_16(header, 0, 0xcc1c);
    little_endian_store_16(header, 2, sizeof(header));
    little_endian_store_16(header, 4, sampling_frequency_hz / 100);
    little_endian_store_16(header, 6, bitrate / 100);
    little_endian_store_16(header, 8, num_channels);
    little_endian_store_16(header, 10, frame_duration_100us * 10);
    little_endian_store_16(header, 12, 0);
    little_endian_store_32(header, 14, 0); // num samples need to set later
    fwrite(header, 1, sizeof(header), lc3_file);

    // print format
    printf("WAC file:          %s\n", wav_filename);
    printf("LC3 file:          %s\n", lc3_filename);
    printf("Samplerate:        %u Hz\n", sampling_frequency_hz);
    printf("Channels:          %u\n", num_channels);
    printf("Frame duration:    %s ms\n", (frame_duration == BTSTACK_LC3_FRAME_DURATION_10000US) ? "10" : "7.5");
    printf("Bitrate:           %u\n", bitrate);
    printf("Samples per Frame: %u\n", number_samples_per_frame);

    while (true){
        // process file frame by frame
        memset(samples_buffer, 0, sizeof(samples_buffer));
        // read samples per frame * num channels
        status = wav_reader_read_int16(number_samples_per_frame * num_channels, samples_buffer);

        if (status != 0) break;

        // write len of complete frame
        uint8_t len[2];
        little_endian_store_16(len, 0, num_channels * bytes_per_frame);
        fwrite(len, 1, sizeof(len), lc3_file);

        // encode frame by frame
        for (channel = 0; channel < num_channels ; channel++){
            status = lc3_encoder->encode_signed_16(&encoder_contexts[channel], &samples_buffer[channel], num_channels, write_buffer);
            if (status != ERROR_CODE_SUCCESS){
                printf("Error %u\n", status);
                break;
            }
            fwrite(write_buffer, 1, bytes_per_frame, lc3_file);
        }

        if (status != 0) break;

        frame_count++;
    }

    uint32_t total_samples = frame_count * number_samples_per_frame;
    printf("Total samples: %u\n", total_samples);

    // rewind and store num samples
    little_endian_store_32(header, 14, total_samples); // num samples need to set later
    rewind(lc3_file);
    fwrite(header, 1, sizeof(header), lc3_file);
    fclose(lc3_file);
}

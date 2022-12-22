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
// LC3 decoder Google
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

static uint8_t  read_buffer[200];

static uint32_t frame_count = 0;

static void show_usage(const char * path){
    printf("\n\nUsage: %s input_file.lc3 output_file.wav\n\n", path);
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

int main (int argc, const char * argv[]){
    if (argc < 3){
        show_usage(argv[0]);
        return -1;
    }
    
    const char * lc3_filename = argv[1];
    const char * wav_filename = argv[2];

    int fd = open(lc3_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", lc3_filename);
        return -1;
    }

    // read & parse header
    uint16_t min_header_size = 18;
    int bytes_read = __read(fd, read_buffer, min_header_size);
    if (bytes_read != min_header_size) return -10;
    uint16_t file_id = little_endian_read_16(read_buffer, 0);
    if (file_id != 0xcc1c) return -10;
    uint16_t header_size = little_endian_read_16(read_buffer, 2);
    if (header_size > 100) return -10;
    uint32_t sample_rate_hz = little_endian_read_16(read_buffer, 4) * 100;
    uint32_t bitrate        = little_endian_read_16(read_buffer, 6) * 100;
    uint8_t  num_channels   = little_endian_read_16(read_buffer, 8);
    uint32_t frame_us       = little_endian_read_16(read_buffer, 10) * 10;
    // offset 12: epmode
    // offset 14: signal_len
    // skip addittional fields
    if (header_size > min_header_size){
        __read(fd, read_buffer, header_size - min_header_size);
    }

    if (num_channels > MAX_NUM_CHANNELS) {
        printf("Too much channels: %u\n", num_channels);
        return -10;
    }

    // pick frame duration
    btstack_lc3_frame_duration_t duration2;
    switch (frame_us) {
        case 7500:
            duration2 = BTSTACK_LC3_FRAME_DURATION_7500US;
            break;
        case 10000:
            duration2 = BTSTACK_LC3_FRAME_DURATION_10000US;
            break;
        default:
            return -10;
    }

    uint16_t number_samples_per_frame = btstack_lc3_samples_per_frame(sample_rate_hz, duration2);
    if (number_samples_per_frame > MAX_SAMPLES_PER_FRAME) {
        printf("number samples per frame %u too large\n", number_samples_per_frame);
        return -10;
    }

    // init decoder
    uint8_t channel;
    btstack_lc3_decoder_google_t decoder_contexts[MAX_NUM_CHANNELS];
    const btstack_lc3_decoder_t * lc3_decoder;
    for (channel = 0 ; channel < num_channels ; channel++){
        btstack_lc3_decoder_google_t * decoder_context = &decoder_contexts[channel];
        lc3_decoder = btstack_lc3_decoder_google_init_instance(decoder_context);
        lc3_decoder->configure(decoder_context, sample_rate_hz, duration2, number_samples_per_frame);
    }

    // calc num octets from bitrate
    uint32_t bitrate_per_channel = bitrate / num_channels;
    uint16_t bytes_per_frame = 0;
    if ((sample_rate_hz == 8000) && (bitrate_per_channel == 27700)){
        // fix bitrate for 8_1
        bitrate_per_channel = 27734;
        bitrate = bitrate_per_channel * num_channels;
        bytes_per_frame = 26;
    } else if (sample_rate_hz == 44100){
        // fix bitrate for 441_1 and 441_2
        if ((frame_us == 7500) && (bitrate_per_channel == 95000)) {
            bitrate = 95060;
        }
        if ((frame_us == 10000) && (bitrate_per_channel == 95500)) {
            bytes_per_frame = 130;
        }
    } else {
        bytes_per_frame = bitrate_per_channel / (10000 / (frame_us/100)) / 8;
    }

    // print format
    printf("LC3 file:            %s\n", lc3_filename);
    printf("WAC file:            %s\n", wav_filename);
    printf("Samplerate:          %u Hz\n", sample_rate_hz);
    printf("Channels:            %u\n", num_channels);
    printf("Bitrate              %u bps\n", bitrate);
    uint16_t frame_ms = frame_us / 1000;
    printf("Frame:               %u.%u ms\n", frame_ms, frame_us - (frame_ms * 1000));

    printf("Bytes per frame:     %u\n", bytes_per_frame);
    printf("Samples per frame:   %u\n", number_samples_per_frame);

    // open wav writer
    wav_writer_open(wav_filename, num_channels, sample_rate_hz);

    while (true){

        bool done = false;
        int16_t pcm[MAX_NUM_CHANNELS * MAX_SAMPLES_PER_FRAME];

        // get len of lc3 frames
        int bytes_read = __read(fd, read_buffer, 2);
        if (2 != bytes_read) {
            done = true;
            break;
        }
        uint16_t total_frame_len = little_endian_read_16(read_buffer, 0);
        if (total_frame_len != (bytes_per_frame * num_channels)){
            done = true;
            break;
        }

        for (channel = 0; channel < num_channels; channel++){

            // get next lc3 frame (one channel)
            int bytes_read = __read(fd, read_buffer, bytes_per_frame);
            if (bytes_per_frame != bytes_read) {
                done = true;
                break;
            }

            // process frame
            uint8_t tmp_BEC_detect;
            uint8_t BFI = 0;

            uint8_t status = lc3_decoder->decode_signed_16(&decoder_contexts[channel], read_buffer, BFI, &pcm[channel], num_channels, &tmp_BEC_detect);
            if (status != ERROR_CODE_SUCCESS){
                printf("Error %u\n", status);
                done = true;
                break;
            }
        }

        if (done) break;

        wav_writer_write_int16(num_channels * number_samples_per_frame, pcm);

        frame_count++;
    }

    wav_writer_close();
    close(fd);

    printf("Wrote %d frames / %u samples\n\n", frame_count, frame_count * number_samples_per_frame);
}

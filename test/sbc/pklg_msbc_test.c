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

#define PAKET_TYPE_SCO_OUT 8
#define PAKET_TYPE_SCO_IN  9

static int total_num_samples = 0;
static int frame_count = 0;

static void show_usage(void){
    printf("\n\nUsage: ./pklg_msbc_test input_file \n");
    printf("Example: ./pklg_msbc_test pklg/test1 \n");
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
    (void) context;
    (void) sample_rate;
    wav_writer_write_int16(num_samples*num_channels, data);
    total_num_samples += num_samples*num_channels;
    frame_count++;
}

static void process_file(const char * pklg_path, const char * wav_path, int packet_type, int plc_enabled){

    printf("Processing %s -> %s: PLC enabled: %u, direction %s\n", pklg_path, wav_path, plc_enabled, packet_type == PAKET_TYPE_SCO_OUT ? "Out" : "In");

    int oflags = O_RDONLY;
#ifdef _WIN32
    oflags |= O_BINARY;
#endif
    int fd = open(pklg_path, oflags);
    if (fd < 0) {
        printf("Can't open file %s", pklg_path);
        return;
    }
 
    wav_writer_open(wav_path, 1, 16000);

    total_num_samples = 0;
    frame_count       = 0;

    btstack_sbc_mode_t mode = SBC_MODE_mSBC;

    btstack_sbc_decoder_state_t state;
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    
    btstack_sbc_decoder_test_set_plc_enabled(plc_enabled);

    int sco_packet_counter = 0;
    while (1){
        int bytes_read;
        // get next packet
        uint8_t header[13];
        bytes_read = __read(fd, header, sizeof(header));
        if (0 >= bytes_read) break;
        uint8_t type = header[12];

        uint32_t size = big_endian_read_32(header, 0);

        // auto-detect endianess of size param
        if (size >0xffff){
            size = little_endian_read_32(header, 0);
        }
        // subtract header
        size -= 9;

        uint8_t packet[300];
            
        if (type == packet_type){
            if (size > sizeof(packet) ){
                printf("Error: size %d, packet counter %d \n", size, sco_packet_counter);
                exit(10);
            }
            
            bytes_read = __read(fd, packet, size);
            sco_packet_counter++;
            btstack_sbc_decoder_process_data(&state, (packet[1] >> 4) & 3, packet+3, size-3);
        } else {
            // skip data
            while (size){
                int bytes_to_read = btstack_min(sizeof(packet), size);
                __read(fd, packet, bytes_to_read);
                size -= bytes_to_read;
            }
        }
    }

    wav_writer_close();
    close(fd);

    int good = state.good_frames_nr;
    int bad  = state.bad_frames_nr + state.zero_frames_nr;
    int consecutive;
    int total_frames_nr = good + bad;

    printf("WAV Writer (SBC): Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n", total_frames_nr, good, bad);

    good = state.plc_state.good_frames_nr;
    bad =  state.plc_state.bad_frames_nr;
    consecutive = state.plc_state.max_consecutive_bad_frames_nr;
    total_frames_nr = good + bad;
    printf("WAV Writer (PLC): Decoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d consecutive\n", total_frames_nr, good, bad, consecutive);
    
    printf("Write %d frames to wav file: %s\n\n", frame_count, wav_path);
}


int main (int argc, const char * argv[]){

    char wav_path[1000];
    char pklg_path[1000];

    if (argc < 2){
        show_usage();
        return -1;
    }

    int argv_pos = 1;
    const char * filename = argv[argv_pos++];
    
#ifdef OCTAVE_OUTPUT
    printf("OCTAVE OUTPUT active\n");
    btstack_sbc_plc_octave_set_base_name(filename);
#endif

    btstack_strcpy(pklg_path, sizeof(pklg_path), filename);
    btstack_strcat(pklg_path, sizeof(pklg_path), ".pklg");

    // in file, no plc
    strcpy(wav_path, filename);
    strcat(wav_path, "_in_raw.wav");
    process_file(pklg_path, wav_path, PAKET_TYPE_SCO_IN, 0);

    // in file, plc
    strcpy(wav_path, filename);
    strcat(wav_path, "_in_plc.wav");
    process_file(pklg_path, wav_path, PAKET_TYPE_SCO_IN, 1);

    // out file, no plc
    strcpy(wav_path, filename);
    strcat(wav_path, "_out.wav");
    process_file(pklg_path, wav_path, PAKET_TYPE_SCO_OUT, 0);
}

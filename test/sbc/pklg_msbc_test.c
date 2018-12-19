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

static char wav_filename[1000];
static char pklg_filename[1000];

static int wav_writer_opened;
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
    if (argc < 2){
        show_usage();
        return -1;
    }
    int argv_pos = 1;
    const char * filename = argv[argv_pos++];
    
#ifdef OCTAVE_OUTPUT
    printf("defined OCTAVE OUTPUT\n");
    octave_set_base_name(filename);
#endif

    btstack_sbc_mode_t mode = SBC_MODE_mSBC;

    strcpy(pklg_filename, filename);
    strcat(pklg_filename, ".pklg");

    strcpy(wav_filename, filename);
    strcat(wav_filename, ".wav");


    int fd = open(pklg_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", pklg_filename);
        return -1;
    }
    printf("Open pklg file: %s\n", pklg_filename);
    

    btstack_sbc_decoder_state_t state;
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    
    // btstack_sbc_decoder_test_disable_plc();

    int sco_packet_counter = 0;
    while (1){
        int bytes_read;
        // get next packet
        uint8_t header[13];
        bytes_read = __read(fd, header, sizeof(header));
        if (0 >= bytes_read) break;

        uint8_t packet[256];
        uint32_t size = big_endian_read_32(header, 0) - 9;
        bytes_read = __read(fd, packet, size);

        uint8_t type = header[12];
        if (type != 9) continue;
        sco_packet_counter++;

        // printf("Packet %4u, flags %x: ", sco_packet_counter, (packet[1] >> 4));
        // printf_hexdump(packet+3, size-3);
        btstack_sbc_decoder_process_data(&state, (packet[1] >> 4) & 3, packet+3, size-3);
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
    
    printf("Write %d frames to wav file: %s\n\n", frame_count, wav_filename);
}

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

#include "btstack.h"
#include "classic/btstack_cvsd_plc.h"

#include "wav_util.h"

#define BYTES_PER_FRAME 2

#define PAKET_TYPE_SCO_OUT 8
#define PAKET_TYPE_SCO_IN  9

static btstack_cvsd_plc_state_t plc_state;

static void show_usage(void){
    printf("\n\nUsage: ./pklg_cvsd_test input_file\n");
    printf("Example: ./pklg_cvsd_test pklg/test1\n");
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
 
    wav_writer_open(wav_path, 1, 8000);

    btstack_cvsd_plc_init(&plc_state);
    
    int sco_packet_counter = 0;
    while (1){
        int bytes_read;
        // get next packet
        uint8_t header[13];
        bytes_read = __read(fd, header, sizeof(header));
        if (0 >= bytes_read) break;

        uint32_t size = big_endian_read_32(header, 0);

        // auto-detect endianess of size param
        if (size >0xffff){
            size = little_endian_read_32(header, 0);
        }
        // subtract header
        size -= 9;
        
        uint8_t packet[256];
        uint8_t type = header[12];

        if (type != packet_type) {
            // skip data
            while (size){
                int bytes_to_read = btstack_min(sizeof(packet), size);
                __read(fd, packet, bytes_to_read);
                size -= bytes_to_read;
            }
        } else {
            if (size > sizeof(packet) ){
                printf("Error: size %d, packet counter %d \n", size, sco_packet_counter);
                exit(10);
            }
            bytes_read = __read(fd, packet, size);

            sco_packet_counter++;

            int16_t audio_frame_out[128];    // 

            if (size > sizeof(audio_frame_out)){
                printf("sco_demo_receive_CVSD: SCO packet larger than local output buffer - dropping data.\n");
                break;
            }

            const int audio_bytes_read = size - 3;
            const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

            // check SCO handle -- quick hack, not correct if handle 0x0000 is actually used for SCO
            uint16_t sco_handle = little_endian_read_16(packet, 0) & 0xfff;
            if (sco_handle == 0) continue;

            // convert into host endian
            int16_t audio_frame_in[128];
            int i;
            for (i=0;i<num_samples;i++){
                audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
            }

            if (plc_enabled){
                if (num_samples > 60){
                    btstack_cvsd_plc_process_data(&plc_state, false, audio_frame_in, 60, audio_frame_out);
                    wav_writer_write_int16(60, audio_frame_out);
                    btstack_cvsd_plc_process_data(&plc_state, false, &audio_frame_in[60], num_samples - 60, audio_frame_out);
                    wav_writer_write_int16(num_samples - 60, audio_frame_out);
                } else {
                    btstack_cvsd_plc_process_data(&plc_state, false, audio_frame_in, num_samples, audio_frame_out);
                    wav_writer_write_int16(num_samples, audio_frame_out);
                }
            } else {
                wav_writer_write_int16(num_samples, audio_frame_in);
            }
        }
    }

    wav_writer_close();
    close(fd);

    btstack_cvsd_dump_statistics(&plc_state);
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
    btstack_cvsd_plc_octave_set_base_name(filename);
#endif

    strcpy(pklg_path, filename);
    strcat(pklg_path, ".pklg");

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

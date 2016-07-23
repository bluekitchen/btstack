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
// SBC encoder tests
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
#include "btstack_sbc_encoder.h"
#include "hfp_msbc.h"


static int num_frames = 0;

static int16_t read_buffer[8*16*2];
static uint8_t output_buffer[24];

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

static int read_audio_frame(int wav_fd){
    int i;
    int bytes_read = 0;  
    for (i=0; i < hfp_msbc_num_audio_samples_per_frame() * 2/2; i++){
        uint8_t buf[2];
        bytes_read +=__read(wav_fd, &buf, 2);
        read_buffer[i] = little_endian_read_16(buf, 0);  
    }
    if (bytes_read == hfp_msbc_num_audio_samples_per_frame() * 2 ){
        num_frames++;
    }
    return bytes_read;
}

static void read_wav_header(int wav_fd){
    uint8_t buf[40];
    __read(wav_fd, buf, sizeof(buf));
}

int main (int argc, const char * argv[]){
    if (argc < 3){
        printf("Usage: %s WAV_FILE mSBC_FILE\n", argv[0]);
        return -1;
    }

    const char * wav_filename = argv[1];
    const char * sbc_filename = argv[2];
    
    int wav_fd = open(wav_filename, O_RDONLY); //fopen(wav_filename, "rb");
    if (!wav_fd) {
        printf("Can't open file %s", wav_filename);
        return -1;
    }
    
    FILE * sbc_fd = fopen(sbc_filename, "wb");
    if (!sbc_fd) {
        printf("Can't open file %s", sbc_filename);
        return -1;
    }
    
    read_wav_header(wav_fd);
    hfp_msbc_init();
    
    while (1){
        if (hfp_msbc_can_encode_audio_frame_now()){
            int bytes_read = read_audio_frame(wav_fd);
            if (bytes_read < hfp_msbc_num_audio_samples_per_frame() * 2) break;

            hfp_msbc_encode_audio_frame(read_buffer);
        }
        if (hfp_msbc_num_bytes_in_stream() >= sizeof(output_buffer)){
            hfp_msbc_read_from_stream(output_buffer, sizeof(output_buffer));
            fwrite(output_buffer, 1, sizeof(output_buffer), sbc_fd);
        } 
    }
    

    btstack_sbc_encoder_dump_context();

    printf("Done, frame count %d \n", num_frames);
    close(wav_fd);
    fclose(sbc_fd);
    return 0;
}


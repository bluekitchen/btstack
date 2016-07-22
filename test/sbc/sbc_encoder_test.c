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

#include "btstack.h"
#include "btstack_sbc_encoder.h"

static int16_t read_buffer[8*16*2];
static int num_data_bytes = 0;
static int byte_rate = 0;
static int sampling_frequency = -1;
static int channel_mode = -1;


#define MSBC_FRAME_SIZE 57

static btstack_sbc_encoder_state_t state;
static uint8_t msbc_padding[] = {0,0,0};

static uint8_t msbc_buffer[2*(MSBC_FRAME_SIZE + sizeof(msbc_padding))];
static int msbc_buffer_offset = 0; 

void hfp_msbc_init(void);
int hfp_msbc_can_encode_audio(void);
void hfp_msbc_encode_audio_frame(int16_t * pcm_samples);
void hfp_msbc_read_encoded_stream(uint8_t * buf, int size);

void hfp_msbc_init(void){
    btstack_sbc_encoder_init(&state, SBC_MODE_mSBC, 16, 8, 0, 16000, 26);
    msbc_buffer_offset = 0;
}

int hfp_msbc_can_encode_audio(void){
    return sizeof(msbc_buffer) - msbc_buffer_offset >= MSBC_FRAME_SIZE + sizeof(msbc_padding); 
}

void hfp_msbc_encode_audio_frame(int16_t * pcm_samples){
    if (!hfp_msbc_can_encode_audio()) return;

    btstack_sbc_encoder_process_data(pcm_samples);
    
    memcpy(msbc_buffer + msbc_buffer_offset, msbc_padding, sizeof(msbc_padding));
    msbc_buffer_offset += sizeof(msbc_padding);

    memcpy(msbc_buffer + msbc_buffer_offset, btstack_sbc_encoder_sbc_buffer(), MSBC_FRAME_SIZE);
    msbc_buffer_offset += MSBC_FRAME_SIZE;
    msbc_buffer_free_bytes = sizeof(msbc_buffer) - msbc_buffer_offset;
}

void hfp_msbc_read_encoded_stream(uint8_t * buf, int size){
    int bytes_to_copy = size;
    if (size > msbc_buffer_offset){
        bytes_to_copy = msbc_buffer_offset;
        log_error("sbc frame storage is smaller then the output buffer");
    }

    memcpy(buf, msbc_buffer, bytes_to_copy);
    memmv(msbc_buffer, msbc_buffer + bytes_to_copy, bytes_to_copy);
    msbc_buffer_offset -= bytes_to_copy;
}


static uint16_t little_endian_fread_16(FILE * fd){
    uint8_t buf[2];
    fread(&buf, 1, 2, fd);
    return little_endian_read_16(buf, 0);
}


static uint32_t little_endian_fread_32(FILE * fd){
    uint8_t buf[4];
    fread(&buf, 1, 4, fd);
    return little_endian_read_32(buf, 0);
}

static void read_wav_header(FILE * wav_fd){
    /* write RIFF header */
    uint8_t buf[10];
    fread(&buf, 1, 4, wav_fd); buf[4] = 0; // RIFF
    
    little_endian_fread_32(wav_fd); // 36 + bytes_per_sample * num_samples  * frame_count
    
    fread(&buf, 1, 4, wav_fd);
    buf[4] = 0;
    // printf("%s\n", buf ); // WAVE
    
    fread(&buf, 1, 4, wav_fd);
    buf[4] = 0;
    // printf("%s\n", buf ); // fmt
    
    // int fmt_length = 
    little_endian_fread_32(wav_fd);
    // int fmt_format_tag = 
    little_endian_fread_16(wav_fd);
    
    // printf("fmt_length %d == 16, format %d == 1\n", fmt_length, fmt_format_tag);
    // chanel mode:
    channel_mode = little_endian_fread_16(wav_fd);
    // sampling frequency:
    sampling_frequency = little_endian_fread_32(wav_fd);
    // byte rate:
    byte_rate = little_endian_fread_32(wav_fd);
    
    // int block_align = 
    little_endian_fread_16(wav_fd);    
    // int bits_per_sample = 
    little_endian_fread_16(wav_fd);

    fread(&buf, 1, 4, wav_fd);
    // printf("%s\n", buf ); // data

    num_data_bytes = little_endian_fread_32(wav_fd); 
}

static void read_audio_frame(FILE * wav_fd){
    int i;
    for (i=0; i < btstack_sbc_encoder_num_subband_samples(); i++){
        read_buffer[i] = little_endian_fread_16(wav_fd);  
    }
}

static int sbc_num_frames(){
    int num_all_samples = num_data_bytes / 2;
    return num_all_samples / btstack_sbc_encoder_num_subband_samples(); 
}

int main (int argc, const char * argv[]){
    if (argc < 2){
        return -1;
    }

    const char * wav_filename = argv[1];
    const char * sbc_filename = argv[2];
    
    FILE * wav_fd = fopen(wav_filename, "rb");
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
    btstack_sbc_encoder_init(&state, SBC_MODE_STANDARD, 16, 4, channel_mode, sampling_frequency, 31);
   
    int num_frames = sbc_num_frames();
    int frame_count = 0;
    
    while (frame_count < num_frames){
        read_audio_frame(wav_fd);
        
        btstack_sbc_encoder_process_data(read_buffer);
        fwrite(btstack_sbc_encoder_sbc_buffer(), 1, btstack_sbc_encoder_sbc_buffer_length(), sbc_fd);
        frame_count++;
    }
    btstack_sbc_encoder_dump_context();

    printf("Done, frame count %d\n", frame_count);
    fclose(wav_fd);
    fclose(sbc_fd);
    return 0;
}


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
#include "sbc_encoder.h"

static SBC_ENC_PARAMS context;
static int16_t data[8*16*2];
static int num_data_bytes = 0;
static uint8_t sbc_packet[1000];
static uint8_t buf[4];
    
static uint16_t little_endian_read_16(const uint8_t * buffer, int pos){
    return ((uint16_t) buffer[pos]) | (((uint16_t)buffer[(pos)+1]) << 8);
}

static uint32_t little_endian_read_32(const uint8_t * buffer, int pos){
    return ((uint32_t) buffer[pos]) | (((uint32_t)buffer[(pos)+1]) << 8) | (((uint32_t)buffer[(pos)+2]) << 16) | (((uint32_t) buffer[(pos)+3]) << 24);
}


static uint16_t little_endian_fread_16(FILE * fd){
    fread(&buf, 1, 2, fd);
    return little_endian_read_16(buf, 0);
}


static uint32_t little_endian_fread_32(FILE * fd){
    fread(&buf, 1, 4, fd);
    return little_endian_read_32(buf, 0);
}

static void read_wav_header(FILE * wav_fd, SBC_ENC_PARAMS *context){
    /* write RIFF header */
    uint8_t buf[10];
    fread(&buf, 1, 4, wav_fd);
    buf[4] = 0;
    printf("%s\n", buf ); // RIFF
    
    little_endian_fread_32(wav_fd); // 36 + bytes_per_sample * num_samples  * frame_count
    
    fread(&buf, 1, 4, wav_fd);
    buf[4] = 0;
    printf("%s\n", buf ); // WAVE
    
    fread(&buf, 1, 4, wav_fd);
    buf[4] = 0;
    printf("%s\n", buf ); // fmt
    
    int fmt_length = little_endian_fread_32(wav_fd);
    int fmt_format_tag = little_endian_fread_16(wav_fd);
    
    printf("fmt_length %d == 16, format %d == 1\n", fmt_length, fmt_format_tag);

    context->s16NumOfChannels = little_endian_fread_16(wav_fd);
    if (context->s16NumOfChannels == 1){
        context->s16ChannelMode = SBC_MONO;
    } else {
        context->s16ChannelMode = SBC_STEREO;
    }

    int sample_rate =  little_endian_fread_32(wav_fd);

    if (sample_rate == 16000)
        context->s16SamplingFreq = SBC_sf16000;
    else if (sample_rate == 32000)
        context->s16SamplingFreq = SBC_sf32000;
    else if (sample_rate == 44100)
        context->s16SamplingFreq = SBC_sf44100;
    else
        context->s16SamplingFreq = SBC_sf48000;

    int byte_rate = little_endian_fread_32(wav_fd);
    context->u16BitRate = byte_rate * 8;

    // int block_align = 
    little_endian_fread_16(wav_fd);    
    // int bits_per_sample = 
    little_endian_fread_16(wav_fd);

    fread(&buf, 1, 4, wav_fd);
    printf("%s\n", buf ); // data

    num_data_bytes = little_endian_fread_32(wav_fd); 
}

static void read_audio_frame(FILE * wav_fd, SBC_ENC_PARAMS * context){
    int num_samples = context->s16NumOfSubBands * context->s16NumOfBlocks * context->s16NumOfChannels;
    int i;
    for (i=0; i < num_samples; i++){
        data[i] = little_endian_fread_16(wav_fd);  
    }
    context->ps16PcmBuffer = data;
}

static int sbc_num_frames(SBC_ENC_PARAMS * context){
    int num_subband_samples = context->s16NumOfSubBands * context->s16NumOfBlocks * context->s16NumOfChannels;
    int num_all_samples = num_data_bytes / 2;
    return num_all_samples / num_subband_samples; 
}

static void write_sbc_file(FILE * sbc_fd, SBC_ENC_PARAMS * context){
    int i;
    for (i=0;i<context->u16PacketLength;i++){
        printf("%02x ", context->pu8Packet[i]);
    }
    printf("\n");
    
    fwrite(context->pu8Packet, 1, context->u16PacketLength, sbc_fd);
}

static void SBC_Encoder_Context_Init(SBC_ENC_PARAMS * context, FILE * fd, int blocks, int subbands, int allmethod, int bitpool){
    context->s16NumOfSubBands = subbands;                       
    context->s16NumOfBlocks = blocks;                          
    context->s16AllocationMethod = allmethod;                     
    context->s16BitPool = bitpool;  
    context->pu8Packet = sbc_packet;

    read_wav_header(fd, context);
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
    
    SBC_Encoder_Context_Init(&context, wav_fd, SBC_BLOCK_3, SUB_BANDS_4, SBC_LOUDNESS, 31);
    SBC_Encoder_Init(&context);

    printf("channels %d, subbands %d, blocks %d\n", context.s16NumOfChannels, context.s16NumOfSubBands, context.s16NumOfBlocks);
    int num_frames = sbc_num_frames(&context);
    int frame_count = 0;
    while (frame_count < num_frames){
        printf("frame_count %d\n", frame_count);
        read_audio_frame(wav_fd, &context);
        SBC_Encoder(&context);
        write_sbc_file(sbc_fd, &context);
        frame_count++;
    }
    fclose(wav_fd);
    fclose(sbc_fd);
    return 0;
}


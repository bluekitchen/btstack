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
#include "oi_codec_sbc.h"
#include "oi_assert.h"

static uint8_t read_buffer[6000];
static uint8_t frame_buffer[5000];
static OI_INT16 pcmData[1000];
static OI_UINT32 pcmBytes = sizeof(pcmData);

static uint8_t buf[4];

static OI_UINT32 decoderData[10000];
static OI_CODEC_SBC_DECODER_CONTEXT context;

typedef struct {
    OI_UINT32 bytesRead;
    OI_UINT32 frameBytes;
    OI_UINT32 inputBufferBytes;
    int frame_count;
    const OI_BYTE *frameData;
} sbc_state_t;
static sbc_state_t state;


void OI_AssertFail(char* file, int line, char* reason){
    printf("AssertFail file %s, line %d, reason %s\n", file, line, reason);
}

static void show_usage(void){
    printf("Usage: ./sbc_decoder_test input.sbc");
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

void little_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

void little_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 24;
}

void little_endian_fstore_16(FILE *wav_file, uint16_t value){
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 2, wav_file);
}

void little_endian_fstore_32(FILE *wav_file, uint32_t value){
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 4, wav_file);
}


static void write_wav_header(FILE * wav_file, OI_CODEC_SBC_DECODER_CONTEXT * decoderContext, int frame_count){
    unsigned int bytes_per_sample = 2;
    int num_samples = decoderContext->common.frameInfo.nrof_blocks * decoderContext->common.frameInfo.nrof_subbands;
    int num_channels = decoderContext->common.frameInfo.nrof_channels;
    int sample_rate = decoderContext->common.frameInfo.frequency;

    /* write RIFF header */
    fwrite("RIFF", 1, 4, wav_file);
    // num_samples = blocks * subbands
    uint32_t data_bytes = (uint32_t) (bytes_per_sample * num_samples * frame_count * num_channels);
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

static void write_wav_data(FILE * wav_file, OI_CODEC_SBC_DECODER_CONTEXT * decoderContext, int16_t * data){
    int num_samples = decoderContext->common.frameInfo.nrof_blocks * decoderContext->common.frameInfo.nrof_subbands;
    int num_channels = decoderContext->common.frameInfo.nrof_channels;
    int i;
    for (i=0; i < num_samples; i++){
        little_endian_fstore_16(wav_file, (uint16_t)data[i]);
        if (num_channels == 2){
            little_endian_fstore_16(wav_file, (uint16_t)data);
        }  
    }
}

static void init_sbc_state(sbc_state_t * state){
    state->frameBytes = 0;
    state->bytesRead = 0;
    state->frame_count = 0;
    state->inputBufferBytes = 0;
    state->frameData = NULL;
}

static void append_received_sbc_data(sbc_state_t * state, uint8_t * buffer, int size){
    int numFreeBytes = sizeof(frame_buffer) - state->frameBytes;

    if (size > numFreeBytes){
        printf("sbc data: more bytes read %u than free bytes in buffer %u", size, numFreeBytes);
        exit(10);
    }

    memcpy(frame_buffer + state->frameBytes, buffer, size);
    state->frameBytes += size;
    state->bytesRead = state->frameBytes;
    state->frameData = frame_buffer;
}

// assumption: new data fits into buffer
static void handle_received_sbc_data(sbc_state_t * state, uint8_t * buffer, int size, FILE * wav_file){
    append_received_sbc_data(state, buffer, size);
    while (1){
        
        OI_STATUS status = OI_CODEC_SBC_DecodeFrame(&context, &(state->frameData), &(state->frameBytes), pcmData, &pcmBytes);
    
        if (status != 0){
            if (status != OI_CODEC_SBC_NOT_ENOUGH_HEADER_DATA && status != OI_CODEC_SBC_NOT_ENOUGH_BODY_DATA){
                OI_CODEC_SBC_DumpConfig(&(context. common.frameInfo));
                printf("Frame decode error %d\n", status);
                break;
            }

            printf("Not enough data, read next %u bytes, move %d bytes\n", state->bytesRead-state->frameBytes, state->frameBytes);
            memmove(frame_buffer, frame_buffer + state->bytesRead - state->frameBytes, state->frameBytes);
            break;
        }
        
        if (state->frame_count == 0){
            write_wav_header(wav_file, &context, 0);
        } 

        write_wav_data(wav_file, &context, pcmData);
        state->frame_count++;
    }
}


int main (int argc, const char * argv[]){
    if (argc < 2){
        show_usage();
        return -1;
    }
    
    const char * sbc_filename = argv[1];
    const char * wav_filename = argv[2];
    
    
    int fd = open(sbc_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", sbc_filename);
        return -1;
    }
    printf("Open sbc file: %s\n", sbc_filename);

    //OI_STATUS status = OI_CODEC_SBC_DecoderReset(&context, decoderData, sizeof(decoderData), 1, 1, FALSE);

    OI_STATUS status = OI_CODEC_mSBC_DecoderReset(&context, decoderData, sizeof(decoderData));
    if (status != 0){
        printf("Reset decoder error %d\n", status);
        return -1;
    }
    
    FILE * wav_file = fopen(wav_filename, "wb");
    init_sbc_state(&state);
    
    while (1){

        int bytes_read = __read(fd, read_buffer, sizeof(read_buffer));
        if (0 >= bytes_read) break;

        while (bytes_read > 0){
            int space_in_frame_buffer = sizeof(frame_buffer) - state.frameBytes;
            int bytes_to_append = space_in_frame_buffer > bytes_read ? bytes_read : space_in_frame_buffer;
            handle_received_sbc_data(&state, read_buffer, bytes_to_append, wav_file);
            memmove(read_buffer, read_buffer + bytes_to_append, sizeof(read_buffer) - bytes_to_append);
            bytes_read -= bytes_to_append;
        }
    }

    rewind(wav_file);
    write_wav_header(wav_file, &context, state.frame_count);

    fclose(wav_file);
    printf("Write %d frames to wav file: %s\n", state.frame_count, wav_filename);

    close(fd);
}

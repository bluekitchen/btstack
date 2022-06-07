/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "wav_util.c"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wav_util.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

/* wav reader state */
static FILE * wav_reader_file;
static int wav_reader_bytes_per_sample = 2;
static struct {
    uint8_t num_channels;
    uint32_t sampling_rate;
} wav_reader_state;

/* wav writer state */
static struct {
    FILE * wav_file;
    int total_num_samples;
    int num_channels;
    int sampling_frequency;
    int frame_count;
} wav_writer_state;


static void little_endian_fstore_16(FILE *wav_file, uint16_t value){
    uint8_t buf[2];
    little_endian_store_16(buf, 0, value);
    fwrite(&buf, 1, 2, wav_file);
}

static void little_endian_fstore_32(FILE *wav_file, uint32_t value){
    uint8_t buf[4];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 4, wav_file);
}

static void write_wav_header(FILE * wav_file,  int total_num_samples, int num_channels, int sample_rate){
    unsigned int write_with_bytes_per_sample = 2;
    /* write RIFF header */
    fwrite("RIFF", 1, 4, wav_file);
    // num_samples = blocks * subbands
    uint32_t data_bytes = (uint32_t) (write_with_bytes_per_sample * total_num_samples * num_channels);
    little_endian_fstore_32(wav_file, data_bytes + 36); 
    fwrite("WAVE", 1, 4, wav_file);

    int byte_rate = sample_rate * num_channels * write_with_bytes_per_sample;
    int bits_per_sample = 8 * write_with_bytes_per_sample;
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

int wav_writer_open(const char * filepath, int num_channels, int sampling_frequency){
    FILE * wav_file = fopen(filepath, "wb");
    if (!wav_file) return 1;

    wav_writer_state.wav_file = wav_file;
    wav_writer_state.frame_count = 0;
    wav_writer_state.total_num_samples = 0;
    wav_writer_state.num_channels = num_channels;
    wav_writer_state.sampling_frequency = sampling_frequency;
    write_wav_header(wav_writer_state.wav_file, 0, num_channels, sampling_frequency);
    return 0;
}

int wav_writer_close(void){
    if (wav_writer_state.wav_file == NULL) return 0;
    rewind(wav_writer_state.wav_file);
    write_wav_header(wav_writer_state.wav_file, wav_writer_state.total_num_samples, 
        wav_writer_state.num_channels, wav_writer_state.sampling_frequency);
    fclose(wav_writer_state.wav_file);
    wav_writer_state.wav_file = NULL;
    return 0;
}

int wav_writer_write_int8(int num_samples, int8_t * data){
    btstack_assert(data != NULL);
    if (wav_writer_state.wav_file == NULL) return 1;

    int i = 0;
    int8_t zero_byte = 0;
    for (i=0; i<num_samples; i++){
        fwrite(&zero_byte, 1, 1, wav_writer_state.wav_file);
        uint8_t byte_value = (uint8_t)data[i];
        fwrite(&byte_value, 1, 1, wav_writer_state.wav_file);
    }
    
    wav_writer_state.total_num_samples+=num_samples;
    wav_writer_state.frame_count++;
    return 0;
}

int wav_writer_write_le_int16(int num_samples, int16_t * data){
    btstack_assert(data != NULL);
    if (wav_writer_state.wav_file == NULL) return 1;

    fwrite(data, num_samples, 2, wav_writer_state.wav_file);
    
    wav_writer_state.total_num_samples+=num_samples;
    wav_writer_state.frame_count++;
    return 0;
}

int wav_writer_write_int16(int num_samples, int16_t * data){
    btstack_assert(data != NULL);
    if (wav_writer_state.wav_file == NULL) return 1;

    if (btstack_is_little_endian()){
        return wav_writer_write_le_int16(num_samples, data);
    }

    int i;
    for (i=0;i<num_samples;i++){
        uint16_t sample = btstack_flip_16(data[i]);
        fwrite(&sample, 1, 2, wav_writer_state.wav_file);
    }    

    wav_writer_state.total_num_samples+=num_samples;
    wav_writer_state.frame_count++;
    return 0;
}

/** WAV Reader Implementation */

int wav_reader_open(const char * filepath){
    memset (&wav_reader_state, 0, sizeof(wav_reader_state));
    wav_reader_file = fopen(filepath, "rb");
    if (wav_reader_file == NULL) {
        log_error("Can't open file %s", filepath);
        return 1;
    }

    uint8_t buf[40];
    fread(buf, 1, sizeof(buf), wav_reader_file);

    wav_reader_state.num_channels  = (uint8_t) little_endian_read_16(buf, 22);
    if ((wav_reader_state.num_channels < 1) || (wav_reader_state.num_channels > 2)) {
        log_error("Unexpected num channels %d", wav_reader_state.num_channels);
        return 1;
    }

    wav_reader_state.sampling_rate = little_endian_read_32(buf, 24);

    int block_align = little_endian_read_16(buf, 32);
    wav_reader_bytes_per_sample = block_align / wav_reader_state.num_channels;
    if (wav_reader_bytes_per_sample > 2){
        wav_reader_bytes_per_sample = wav_reader_bytes_per_sample / 8;
    }

    return 0;
}

uint8_t wav_reader_get_num_channels(void){
    return wav_reader_state.num_channels;
}

uint32_t wav_reader_get_sampling_rate(void){
    return wav_reader_state.sampling_rate;
}

int wav_reader_close(void){
    if (wav_reader_file != NULL){
        fclose(wav_reader_file);
    }
    return 0;
}

// Wav data: 8bit is uint8_t; 16bit is int16
int wav_reader_read_int8(int num_samples, int8_t * data){
    if (wav_reader_file == NULL) {
        return 1;
    }
    int i;
    int bytes_read = 0;  

    for (i=0; i<num_samples; i++){
        if (wav_reader_bytes_per_sample == 2){
            uint8_t buf[2];
            bytes_read += (uint16_t) fread(buf, 1, sizeof(buf), wav_reader_file);;
            data[i] = buf[1];
        } else {
            uint8_t buf[1];
            bytes_read += (uint16_t) fread(buf, 1, sizeof(buf), wav_reader_file);;
            data[i] = buf[0] + 128;
        }
    }
    if (bytes_read == num_samples * wav_reader_bytes_per_sample) {
        return 0;
    } else {
        return 1;
    }
}

int wav_reader_read_int16(int num_samples, int16_t * data){
    if (wav_reader_file == NULL) {
        return 1;
    }
    int i;
    int bytes_read = 0;  
    for (i=0; i<num_samples; i++){
        uint8_t buf[2];
        bytes_read += (uint16_t) fread(buf, 1, sizeof(buf), wav_reader_file);;
        data[i] = little_endian_read_16(buf, 0);  
    }
    if (bytes_read == num_samples * wav_reader_bytes_per_sample) {
        return 0;
    } else {
        return 1;
    }
}



#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "wav_utils.h"

#define CVSD_FRAME_SIZE 24

// Wav data: 8bit is uint8_t; 16bit is int16

static int wav_reader_fd;
static int bytes_per_sample = 2;
static int8_t audio_frame_in[CVSD_FRAME_SIZE];

static const uint8_t sine[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};
static int phase = 0;


/* Write wav file utils */
typedef struct wav_writer_state {
    FILE * wav_file;
    int total_num_samples;
    int num_channels;
    int sampling_frequency;
    int frame_count;
} wav_writer_state_t;

wav_writer_state_t wav_writer_state;

static void little_endian_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

static void little_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 24;
}

static void little_endian_fstore_16(FILE *wav_file, uint16_t value){
    uint8_t buf[2];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 2, wav_file);
}

static void little_endian_fstore_32(FILE *wav_file, uint32_t value){
    uint8_t buf[4];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 4, wav_file);
}

void write_wav_header(FILE * wav_file,  int total_num_samples, int num_channels, int sample_rate){
    unsigned int bytes_per_sample = 2;
    /* write RIFF header */
    fwrite("RIFF", 1, 4, wav_file);
    // num_samples = blocks * subbands
    uint32_t data_bytes = (uint32_t) (bytes_per_sample * total_num_samples * num_channels);
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

void init_wav_writer(const char * cvsd_filename){
    FILE * wav_file = fopen(cvsd_filename, "wb");
    wav_writer_state.wav_file = wav_file;
    wav_writer_state.frame_count = 0;
    wav_writer_state.total_num_samples = 0;
    wav_writer_state.num_channels = 1;
    wav_writer_state.sampling_frequency = 8000;
    write_wav_header(wav_writer_state.wav_file, 0, 1, 8000);
}

void close_wav_writer(void){
    rewind(wav_writer_state.wav_file);
    write_wav_header(wav_writer_state.wav_file, wav_writer_state.total_num_samples, 
        wav_writer_state.num_channels, wav_writer_state.sampling_frequency);
    fclose(wav_writer_state.wav_file);
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

static uint16_t little_endian_read_16(const uint8_t * buffer, int pos){
    return ((uint16_t) buffer[pos]) | (((uint16_t)buffer[(pos)+1]) << 8);
}

static void read_wav_header(int wav_fd){
    uint8_t buf[40];
    __read(wav_fd, buf, sizeof(buf));

    int num_channels = little_endian_read_16(buf, 22);
    int block_align = little_endian_read_16(buf, 32);
    bytes_per_sample = block_align/num_channels;
    if (bytes_per_sample > 2){
        bytes_per_sample = bytes_per_sample/8;
    }
    printf("Bytes_per_sample %d\n", bytes_per_sample);
    
}

void init_wav_reader(const char * wav_reader_filename_in){
    wav_reader_fd = open(wav_reader_filename_in, O_RDONLY); //fopen(wav_reader_filename_in, "rb");
    if (!wav_reader_fd) {
        printf("Can't open file %s", wav_reader_filename_in);
    }
    read_wav_header(wav_reader_fd);
}

void close_wav_reader(void){
    close(wav_reader_fd);
}

static int read_audio_frame_int8(int audio_samples_per_frame){
    int i;
    int bytes_read = 0;  

    for (i=0; i < audio_samples_per_frame; i++){
        if (bytes_per_sample == 2){
            uint8_t buf[2];
            bytes_read +=__read(wav_reader_fd, &buf, 2);
            audio_frame_in[i] = buf[1];    
        } else {
            uint8_t buf[1];
            bytes_read +=__read(wav_reader_fd, &buf, 1);
            audio_frame_in[i] = buf[0] + 128;
        }
    }
    return bytes_read;
}

void write_wav_data_int8(int num_samples, int8_t * data){
    if (data == NULL) return;
    int i = 0;
    int8_t zero_byte = 0;
    for (i=0; i<num_samples;i++){
        fwrite(&zero_byte, 1, 1, wav_writer_state.wav_file);
        uint8_t byte_value = (uint8_t)data[i];
        fwrite(&byte_value, 1, 1, wav_writer_state.wav_file);
    }
    
    wav_writer_state.total_num_samples+=num_samples;
    wav_writer_state.frame_count++;
}

int8_t * next_sine_audio_frame(void){
    int i;
    for (i=0;i<CVSD_FRAME_SIZE;i++){
        audio_frame_in[i] = (int8_t)sine[phase];
        phase++;
        if (phase >= sizeof(sine)) phase = 0;
    }  
    return (int8_t *)&audio_frame_in;
}

int8_t * next_audio_frame_int8(void){
    if (!wav_reader_fd) return NULL;

    int bytes_read = read_audio_frame_int8(CVSD_FRAME_SIZE);
    if (bytes_read == CVSD_FRAME_SIZE*bytes_per_sample){
        return (int8_t *)&audio_frame_in;
    }
    return NULL;
}


// void fill_audio_frame_with_error(int8_t * buf, int buf_size, int nr_bad_bytes, int bad_byte_value){
//     int i;
//     for (i=0;i<buf_size;i++){
//         if (i >= buf_size-nr_bad_bytes){
//             buf[i] = bad_byte_value;
//         }
//     }
// }
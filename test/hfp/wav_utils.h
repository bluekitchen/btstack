#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void init_wav_writer(const char * cvsd_filename);
void close_wav_writer(void);
void write_wav_header(FILE * wav_file,  int total_num_samples, int num_channels, int sample_rate);
void write_wav_data_int8(int num_samples, int8_t * data);

void init_wav_reader(const char * wav_reader_filename_in);
void close_wav_reader(void);
int8_t * next_audio_frame_int8(void);

int8_t * next_sine_audio_frame(void);


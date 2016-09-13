#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_cvsd_plc.h"

const  int audio_samples_per_frame = 24;
static int8_t audio_frame_in[audio_samples_per_frame];
static int8_t audio_frame_out[audio_samples_per_frame];
static int last_value = 0;

static uint8_t test_data[][audio_samples_per_frame] = {
    { 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
    { 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05 },
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
    { 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

// input signal: pre-computed sine wave, 160 Hz at 8 kHz
static const uint8_t sine[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};
static int phase = 0;

static btstack_cvsd_plc_state_t plc_state;

static void next_sine_audio_frame(void){
    int i;
    last_value = audio_frame_in[audio_samples_per_frame-1];
    for (i=0;i<audio_samples_per_frame;i++){
        audio_frame_in[i] = (int8_t)sine[phase];
        phase++;
        if (phase >= sizeof(sine)) phase = 0;
    }
}

static void fill_audio_frame_with_error(int nr_zero_bytes, int mark_bad_frame){
    int i;
    int error_value = mark_bad_frame ? 127 : last_value;
    for (i=0;i<audio_samples_per_frame;i++){
        if (i >= audio_samples_per_frame-nr_zero_bytes){
            audio_frame_in[i] = error_value;
        }
    }
}

static int count_equal_bytes(uint8_t * packet){
    int count = 0;
    int temp_count = 1;
    int i;
    for (i = 0; i < audio_samples_per_frame-1; i++){
        if (packet[i] == packet[i+1]){
            temp_count++;
            continue;
        }
        if (count < temp_count){
            count = temp_count;
        }
        temp_count = 1;
    }
    if (temp_count > count + 1){
        count = temp_count;
    }
    return count;
}
/* Write wav file utils */
typedef struct wav_writer_state {
    FILE * wav_file;
    int total_num_samples;
    int num_channels;
    int sampling_frequency;
    int frame_count;
} wav_writer_state_t;

const char * cvsd_filename = "sine_test.wav";
wav_writer_state_t wav_writer_state;

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

static void write_wav_header(FILE * wav_file,  int total_num_samples, int num_channels, int sample_rate){
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

static void init_wav_writer(const char * cvsd_filename){
    printf("Open wav file: %s\n", cvsd_filename);
    FILE * wav_file = fopen(cvsd_filename, "wb");
    wav_writer_state.wav_file = wav_file;
    wav_writer_state.frame_count = 0;
    wav_writer_state.total_num_samples = 0;
    wav_writer_state.num_channels = 1;
    wav_writer_state.sampling_frequency = 8000;
    write_wav_header(wav_writer_state.wav_file, 0, 1, 8000);
}

static void close_wav_writer(void){
    rewind(wav_writer_state.wav_file);
    write_wav_header(wav_writer_state.wav_file, wav_writer_state.total_num_samples, 
        wav_writer_state.num_channels, wav_writer_state.sampling_frequency);
    fclose(wav_writer_state.wav_file);
}

static void write_wav_data(int num_samples, int8_t * data){
    int i = 0;
    for (i=0; i<num_samples;i++){
        fwrite(&data[i], 1, 1, wav_writer_state.wav_file);
        fwrite(&data[i], 1, 1, wav_writer_state.wav_file);
    }
    
    wav_writer_state.total_num_samples+=num_samples;
    wav_writer_state.frame_count++;
}

static int wav_reader_num_frames = 0;
const char * wav_reader_filename_in = "data/fanfare-mono.wav";
int wav_reader_fd;

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

uint16_t little_endian_read_16(const uint8_t * buffer, int pos){
    return ((uint16_t) buffer[pos]) | (((uint16_t)buffer[(pos)+1]) << 8);
}

static int read_audio_frame(int wav_fd){
    int i;
    int bytes_read = 0;  
    for (i=0; i < audio_samples_per_frame; i++){
        uint8_t buf[2];
        bytes_read +=__read(wav_fd, &buf, 2);
        //read_buffer[i] = little_endian_read_16(buf, 0);  
        audio_frame_in[i] = buf[1];
    }

    wav_reader_num_frames++;
    return bytes_read;
}

static void read_wav_header(int wav_fd){
    uint8_t buf[40];
    __read(wav_fd, buf, sizeof(buf));
}

int next_audio_frame(void){
    if (!wav_reader_fd) return -1;
    last_value = audio_frame_in[audio_samples_per_frame-1];
    int bytes_read = read_audio_frame(wav_reader_fd);
    return bytes_read == audio_samples_per_frame*2;
}

static void init_wav_reader(const char * wav_reader_filename_in){
    wav_reader_fd = open(wav_reader_filename_in, O_RDONLY); //fopen(wav_reader_filename_in, "rb");
    if (!wav_reader_fd) {
        printf("Can't open file %s", wav_reader_filename_in);
    }
    read_wav_header(wav_reader_fd);
}

static void close_wav_reader(void){
    close(wav_reader_fd);
}
//

static void process_wav_file_with_error_rate(const char * file_name, int corruption_step, int num_bad_frames, int plc_enabled, int mark_bad_frame){
    btstack_cvsd_plc_init(&plc_state);
    init_wav_writer(file_name);
    init_wav_reader(wav_reader_filename_in);

    int i = 0;
    int num_bf = num_bad_frames;
    printf("Corruption every %dth step\n", corruption_step);
    while (next_audio_frame() > 0){
        if (i > corruption_step && corruption_step > 0 && i%corruption_step == 0) {
            fill_audio_frame_with_error(24, mark_bad_frame);
            btstack_cvsd_plc_bad_frame(&plc_state, audio_frame_out);
            num_bf = num_bad_frames;
        } else if (i > corruption_step && num_bf > 1) {
            fill_audio_frame_with_error(24, mark_bad_frame);
            btstack_cvsd_plc_bad_frame(&plc_state, audio_frame_out);
            num_bf--;
        } else {
            // fill_audio_frame_with_error(0, mark_bad_frame);
            btstack_cvsd_plc_good_frame(&plc_state, audio_frame_in, (int8_t*)audio_frame_out);
        }
        if (plc_enabled){
            write_wav_data(24, audio_frame_out);
        } else {
            write_wav_data(24, audio_frame_in);
        }
        i++;
    } 
    close_wav_writer();
    close_wav_reader();
}

static void process_sine_with_error_rate(const char * file_name, int corruption_step, int num_bad_frames, int plc_enabled, int mark_bad_frame){
    btstack_cvsd_plc_init(&plc_state);
    init_wav_writer(file_name);
    
    int i;
    int num_bf = num_bad_frames;
    printf("Corruption every %dth step\n", corruption_step);
    for (i=0; i<2000; i++){
        next_sine_audio_frame();
        if (i > corruption_step && corruption_step > 0 && i%corruption_step == 0) {
            fill_audio_frame_with_error(24, mark_bad_frame);
            btstack_cvsd_plc_bad_frame(&plc_state, (int8_t*)audio_frame_out);
            num_bf = num_bad_frames;
        } else if (i > corruption_step && num_bf > 1) {
            fill_audio_frame_with_error(24, mark_bad_frame);
            btstack_cvsd_plc_bad_frame(&plc_state, audio_frame_out);
            num_bf--;
        } else {
            fill_audio_frame_with_error(0, mark_bad_frame);
            btstack_cvsd_plc_good_frame(&plc_state, audio_frame_in, audio_frame_out);
        }
        if (plc_enabled){
            write_wav_data(24, audio_frame_out);
        } else {
            write_wav_data(24, audio_frame_in);
        }
    } 
    close_wav_writer();
}



TEST_GROUP(CVSD_PLC){
 
};

TEST(CVSD_PLC, CountEqBytes){
    CHECK_EQUAL(23, count_equal_bytes(test_data[0]));
    CHECK_EQUAL(11, count_equal_bytes(test_data[1]));
    CHECK_EQUAL(12, count_equal_bytes(test_data[2]));
    CHECK_EQUAL(23, count_equal_bytes(test_data[3]));   
}

// TEST(CVSD_PLC, FillAudioFrame){
//     fill_audio_frame_with_error(15, 1);
//     CHECK_EQUAL(15, count_equal_bytes(audio_frame_in));  
// }

// TEST(CVSD_PLC, SineWithErrorRateSingleBadFrame){
//     process_sine_with_error_rate("sine_test_10_no_plc.wav", 10, 0, 0);
//     process_sine_with_error_rate("sine_test_10_plc.wav", 10, 0, 1);
// }

// TEST(CVSD_PLC, SineWithErrorRateMultipleBadFrame){
//     process_sine_with_error_rate("sine_test_10_2_no_plc.wav", 10, 2, 0);
//     process_sine_with_error_rate("sine_test_10_2_plc.wav", 10, 2, 1);
// }


TEST(CVSD_PLC, WavFileWithErrorRateSingleBadFrame){
    // process_wav_file_with_error_rate("fanfare_test.wav", 0, 0, 0);
    // process_sine_with_error_rate("sine_test.wav", 0, 0, 0);
    
    process_wav_file_with_error_rate("fanfare_test_20_no_plc.wav", 20, 0, 0, 1);
    process_wav_file_with_error_rate("fanfare_test_20_1_plc8.wav", 20, 1, 1, 0);
    // process_wav_file_with_error_rate("fanfare_test_20_2_plc8.wav", 20, 2, 1, 0);
    // process_wav_file_with_error_rate("fanfare_test_20_4_plc8.wav", 20, 4, 1, 0);

    process_sine_with_error_rate("sine_test_10_no_plc.wav", 10, 0, 0, 1);
    process_sine_with_error_rate("sine_test_10_1_plc8.wav", 10, 0, 1, 0);
    // process_sine_with_error_rate("sine_test_10_2_plc8.wav", 10, 2, 1, 0);
    // process_sine_with_error_rate("sine_test_10_4_plc8.wav", 10, 4, 1, 0);
}

// TEST(CVSD_PLC, WavFileWithErrorRateMultipleBadFrame){
//     process_wav_file_with_error_rate("fanfare_test_10_2_no_plc.wav", 10, 2, 0);
//     process_wav_file_with_error_rate("fanfare_test_10_2_plc.wav", 10, 2, 1);
// }

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
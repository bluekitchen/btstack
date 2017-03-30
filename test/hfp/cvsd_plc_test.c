#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_cvsd_plc.h"
#include "wav_util.h"

const  int    audio_samples_per_frame = 24;
static int16_t audio_frame_in[audio_samples_per_frame];

static int16_t test_data[][audio_samples_per_frame] = {
    { 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
    { 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05 },
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
    { 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static btstack_cvsd_plc_state_t plc_state;

// input signal: pre-computed sine wave, 160 Hz at 16000 kHz
static const int16_t sine_int16[] = {
     0,    2057,    4107,    6140,    8149,   10126,   12062,   13952,   15786,   17557,
 19260,   20886,   22431,   23886,   25247,   26509,   27666,   28714,   29648,   30466,
 31163,   31738,   32187,   32509,   32702,   32767,   32702,   32509,   32187,   31738,
 31163,   30466,   29648,   28714,   27666,   26509,   25247,   23886,   22431,   20886,
 19260,   17557,   15786,   13952,   12062,   10126,    8149,    6140,    4107,    2057,
     0,   -2057,   -4107,   -6140,   -8149,  -10126,  -12062,  -13952,  -15786,  -17557,
-19260,  -20886,  -22431,  -23886,  -25247,  -26509,  -27666,  -28714,  -29648,  -30466,
-31163,  -31738,  -32187,  -32509,  -32702,  -32767,  -32702,  -32509,  -32187,  -31738,
-31163,  -30466,  -29648,  -28714,  -27666,  -26509,  -25247,  -23886,  -22431,  -20886,
-19260,  -17557,  -15786,  -13952,  -12062,  -10126,   -8149,   -6140,   -4107,   -2057,
};

static int count_equal_samples(int16_t * packet, uint16_t size){
    int count = 0;
    int temp_count = 1;
    int i;
    for (i = 0; i < size-1; i++){
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

// @assumption frame len 24 samples
static int bad_frame(int16_t * frame, uint16_t size){
    return count_equal_samples(frame, size) > 20;
}

static void btstack_cvsd_plc_mark_bad_frame(btstack_cvsd_plc_state_t * state, int16_t * in, uint16_t size, int16_t * out){
    if (size != 24){
        printf("btstack_cvsd_plc_mark_bad_frame: audio frame size is incorrect. Expected %d, got %d\n", CVSD_FS, size);
        return;
    }
    state->frame_count++;
    
    if (bad_frame(in,size)){
        memcpy(out, in, size * 2);
        if (state->good_frames_nr > CVSD_LHIST/CVSD_FS){
            memset(out, 0x33, size * 2);
            state->bad_frames_nr++;
        } 
    } else {
        memcpy(out, in, size);
        state->good_frames_nr++;
        if (state->good_frames_nr == 1){
            printf("First good frame at index %d\n", state->frame_count-1);
        }        
    }
}

static int phase = 0;
static void create_sine_wave_int16_data(int num_samples, int16_t * data){
    int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16[phase++];
        phase++;
        if (phase >= (sizeof(sine_int16) / sizeof(int16_t))){
            phase = 0;
        }
    }
}
    
static int count_equal_bytes(int16_t * packet, uint16_t size){
    int count = 0;
    int temp_count = 1;
    int i;
    for (i = 0; i < size-1; i++){
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

void create_sine_wav(const char * out_filename){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    
    int i;
    for (i=0; i<2000; i++){
        create_sine_wave_int16_data(audio_samples_per_frame, audio_frame_in);
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_in);
    } 
    wav_writer_close();
}

void introduce_bad_frames_to_wav_file(const char * in_filename, const char * out_filename, int corruption_step){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);

    int fc = 0;
    while (wav_reader_read_int16(audio_samples_per_frame, audio_frame_in) == 0){
        if (corruption_step > 0 && fc >= corruption_step && fc%corruption_step == 0){
            memset(audio_frame_in, 50,  audio_samples_per_frame * 2);
        } 
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_in);
        fc++;
    } 
    wav_reader_close();
    wav_writer_close();
}

void process_wav_file_with_plc(const char * in_filename, const char * out_filename){
    // printf("\nProcess %s -> %s\n", in_filename, out_filename);
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);
    
    while (wav_reader_read_int16(audio_samples_per_frame, audio_frame_in) == 0){
        int16_t audio_frame_out[audio_samples_per_frame];
        btstack_cvsd_plc_process_data(&plc_state, audio_frame_in, audio_samples_per_frame, audio_frame_out);
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_out);
    } 
    wav_reader_close();
    wav_writer_close();
    btstack_cvsd_dump_statistics(&plc_state);
}

void mark_bad_frames_wav_file(const char * in_filename, const char * out_filename){
    // printf("\nMark bad frame %s -> %s\n", in_filename, out_filename);
    btstack_cvsd_plc_init(&plc_state);
    CHECK_EQUAL(wav_writer_open(out_filename, 1, 8000), 0);
    CHECK_EQUAL(wav_reader_open(in_filename), 0);
    
    while (wav_reader_read_int16(audio_samples_per_frame, audio_frame_in) == 0){
        int16_t audio_frame_out[audio_samples_per_frame];
        btstack_cvsd_plc_mark_bad_frame(&plc_state, audio_frame_in, audio_samples_per_frame, audio_frame_out);
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_out);
    } 
    wav_reader_close();
    wav_writer_close();
    btstack_cvsd_dump_statistics(&plc_state);
}

TEST_GROUP(CVSD_PLC){
 
};

TEST(CVSD_PLC, CountEqBytes){
    CHECK_EQUAL(23, count_equal_bytes(test_data[0],24));
    CHECK_EQUAL(11, count_equal_bytes(test_data[1],24));
    CHECK_EQUAL(12, count_equal_bytes(test_data[2],24));
    CHECK_EQUAL(23, count_equal_bytes(test_data[3],24));   
}

TEST(CVSD_PLC, TestLiveWavFile){
    int corruption_step = 10;
    introduce_bad_frames_to_wav_file("data/sco_input-16bit.wav", "results/sco_input.wav", 0);
    introduce_bad_frames_to_wav_file("data/sco_input-16bit.wav", "results/sco_input_with_bad_frames.wav", corruption_step);
    
    mark_bad_frames_wav_file("results/sco_input.wav", "results/sco_input_detected_frames.wav");
    process_wav_file_with_plc("results/sco_input.wav", "results/sco_input_after_plc.wav");
    process_wav_file_with_plc("results/sco_input_with_bad_frames.wav", "results/sco_input_with_bad_frames_after_plc.wav");
}

TEST(CVSD_PLC, TestFanfareFile){
    int corruption_step = 10;
    introduce_bad_frames_to_wav_file("data/input/fanfare_mono.wav", "results/fanfare_mono.wav", 0);
    introduce_bad_frames_to_wav_file("results/fanfare_mono.wav", "results/fanfare_mono_with_bad_frames.wav", corruption_step);
    
    mark_bad_frames_wav_file("results/fanfare_mono.wav", "results/fanfare_mono_detected_frames.wav");
    process_wav_file_with_plc("results/fanfare_mono.wav", "results/fanfare_mono_after_plc.wav");
    process_wav_file_with_plc("results/fanfare_mono_with_bad_frames.wav", "results/fanfare_mono_with_bad_frames_after_plc.wav");
}

TEST(CVSD_PLC, TestSineWave){
    int corruption_step = 10;
    create_sine_wav("results/sine_test.wav");
    introduce_bad_frames_to_wav_file("results/sine_test.wav", "results/sine_test_with_bad_frames.wav", corruption_step);
    
    process_wav_file_with_plc("results/sine_test.wav", "results/sine_test_after_plc.wav");
    process_wav_file_with_plc("results/sine_test_with_bad_frames.wav", "results/sine_test_with_bad_frames_after_plc.wav");
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
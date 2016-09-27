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

const  int audio_samples_per_frame = 24;
static int8_t audio_frame_in[audio_samples_per_frame];

static uint8_t test_data[][audio_samples_per_frame] = {
    { 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
    { 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05 },
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
    { 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static btstack_cvsd_plc_state_t plc_state;
    
static int count_equal_bytes(uint8_t * packet, uint16_t size){
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

static void create_sine_wav(const char * out_filename){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    
    int i;
    for (i=0; i<2000; i++){
        wav_synthesize_sine_wave_int8(audio_samples_per_frame, audio_frame_in);
        wav_writer_write_int8(audio_samples_per_frame, audio_frame_in);
    } 
    wav_writer_close();
}

static void introduce_bad_frames_to_wav_file(const char * in_filename, const char * out_filename, int corruption_step){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);

    int fc = 0;
    while (wav_reader_read_int8(audio_samples_per_frame, audio_frame_in)){
        if (corruption_step > 0 && fc >= corruption_step && fc%corruption_step == 0){
            memset(audio_frame_in, 50, audio_samples_per_frame);
        } 
        wav_writer_write_int8(audio_samples_per_frame, audio_frame_in);
        fc++;
    } 
    wav_reader_close();
    wav_writer_close();
}

static void process_wav_file_with_plc(const char * in_filename, const char * out_filename){
    printf("\nProcess %s -> %s\n", in_filename, out_filename);
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);
    
    while (wav_reader_read_int8(audio_samples_per_frame, audio_frame_in)){
        int8_t audio_frame_out[audio_samples_per_frame];
        btstack_cvsd_plc_process_data(&plc_state, audio_frame_in, audio_samples_per_frame, audio_frame_out);
        wav_writer_write_int8(audio_samples_per_frame, audio_frame_out);
    } 
    wav_reader_close();
    wav_writer_close();
    btstack_cvsd_dump_statistics(&plc_state);
}

static void mark_bad_frames_wav_file(const char * in_filename, const char * out_filename){
    printf("\nMark bad frame %s -> %s\n", in_filename, out_filename);
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);
    
    while (wav_reader_read_int8(audio_samples_per_frame, audio_frame_in)){
        int8_t audio_frame_out[audio_samples_per_frame];
        btstack_cvsd_plc_mark_bad_frame(&plc_state, audio_frame_in, audio_samples_per_frame, audio_frame_out);
        wav_writer_write_int8(audio_samples_per_frame, audio_frame_out);
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
    introduce_bad_frames_to_wav_file("data/input/sco_input.wav", "data/sco_input.wav", 0);
    introduce_bad_frames_to_wav_file("data/input/sco_input.wav", "data/sco_input_with_bad_frames.wav", corruption_step);
    
    mark_bad_frames_wav_file("data/sco_input.wav", "data/sco_input_detected_frames.wav");
    process_wav_file_with_plc("data/sco_input.wav", "data/sco_input_after_plc.wav");
    process_wav_file_with_plc("data/sco_input_with_bad_frames.wav", "data/sco_input_with_bad_frames_after_plc.wav");
}

TEST(CVSD_PLC, TestFanfareFile){
    int corruption_step = 10;
    introduce_bad_frames_to_wav_file("data/input/fanfare_mono.wav", "data/fanfare_mono.wav", 0);
    introduce_bad_frames_to_wav_file("data/fanfare_mono.wav", "data/fanfare_mono_with_bad_frames.wav", corruption_step);
    
    mark_bad_frames_wav_file("data/fanfare_mono.wav", "data/fanfare_mono_detected_frames.wav");
    process_wav_file_with_plc("data/fanfare_mono.wav", "data/fanfare_mono_after_plc.wav");
    process_wav_file_with_plc("data/fanfare_mono_with_bad_frames.wav", "data/fanfare_mono_with_bad_frames_after_plc.wav");
}

TEST(CVSD_PLC, TestSineWave){
    int corruption_step = 10;
    create_sine_wav("data/sine_test.wav");
    introduce_bad_frames_to_wav_file("data/sine_test.wav", "data/sine_test_with_bad_frames.wav", corruption_step);
    
    process_wav_file_with_plc("data/sine_test.wav", "data/sine_test_after_plc.wav");
    process_wav_file_with_plc("data/sine_test_with_bad_frames.wav", "data/sine_test_with_bad_frames_after_plc.wav");
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
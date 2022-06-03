#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/btstack_cvsd_plc.h"
#include "wav_util.h"

const  int     audio_samples_per_frame = 60;
static int16_t audio_frame_in[audio_samples_per_frame];

// static int16_t test_data[][audio_samples_per_frame] = {
//     { 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
//     { 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05 },
//     { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
//     { 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
// };

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
    return count_equal_samples(frame, size) > audio_samples_per_frame - 4;
}

static void btstack_cvsd_plc_mark_bad_frame(btstack_cvsd_plc_state_t * state, int16_t * in, uint16_t size, int16_t * out){
    state->frame_count++;
    if (bad_frame(in,size)){
        memcpy(out, in, size * 2);
        if (state->good_frames_nr > CVSD_LHIST/audio_samples_per_frame){
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
        data[i] = sine_int16[phase++] * 90/100;
        phase++;
        if (phase >= (sizeof(sine_int16) / sizeof(int16_t))){
            phase = 0;
        }
    }
}
    
// static int count_equal_bytes(int16_t * packet, uint16_t size){
//     int count = 0;
//     int temp_count = 1;
//     int i;
//     for (i = 0; i < size-1; i++){
//         if (packet[i] == packet[i+1]){
//             temp_count++;
//             continue;
//         }
//         if (count < temp_count){
//             count = temp_count;
//         }
//         temp_count = 1;
//     }
//     if (temp_count > count + 1){
//         count = temp_count;
//     }
//     return count;
// }

static void create_sine_wav(const char * out_filename){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    
    int i;
    for (i=0; i<2000; i++){
        create_sine_wave_int16_data(audio_samples_per_frame, audio_frame_in);
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_in);
    } 
    wav_writer_close();
}

static int introduce_bad_frames_to_wav_file(const char * in_filename, const char * out_filename, int corruption_step){
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);
    int total_corruption_times = 0;
    int fc = 0;
    int start_corruption = 0;

    while (wav_reader_read_int16(audio_samples_per_frame, audio_frame_in) == 0){
        if (corruption_step > 0 && fc >= corruption_step && fc%corruption_step == 0){
            printf("corrupt fc %d, corruption_step %d\n", fc, corruption_step);
            start_corruption = 1;
        } 
        if (start_corruption > 0 && start_corruption < 4){
            memset(audio_frame_in, 50,  audio_samples_per_frame * 2);
            start_corruption++;
        } 
        if (start_corruption > 4){
            start_corruption = 0;
            total_corruption_times++;
            // printf("corupted 3 frames\n");
        }
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_in);
        fc++;
    } 
    wav_reader_close();
    wav_writer_close();
    return total_corruption_times;
}

static void process_wav_file_with_plc(const char * in_filename, const char * out_filename){
    // printf("\nProcess %s -> %s\n", in_filename, out_filename);
    btstack_cvsd_plc_init(&plc_state);
    wav_writer_open(out_filename, 1, 8000);
    wav_reader_open(in_filename);
    
    while (wav_reader_read_int16(audio_samples_per_frame, audio_frame_in) == 0){
        int16_t audio_frame_out[audio_samples_per_frame];
        btstack_cvsd_plc_process_data(&plc_state, false, audio_frame_in, audio_samples_per_frame, audio_frame_out);
        wav_writer_write_int16(audio_samples_per_frame, audio_frame_out);
    } 
    wav_reader_close();
    wav_writer_close();
    btstack_cvsd_dump_statistics(&plc_state);
}

void mark_bad_frames_wav_file(const char * in_filename, const char * out_filename);
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

static void fprintf_array_int16(FILE * oct_file, char * name, int data_len, int16_t * data){
    fprintf(oct_file, "%s = [", name);
    int i;
    for (i = 0; i < data_len - 1; i++){
        fprintf(oct_file, "%d, ", data[i]);
    }
    fprintf(oct_file, "%d", data[i]);
    fprintf(oct_file, "%s", "];\n");
}

static void fprintf_plot_history(FILE * oct_file, char * name, int data_len, int16_t * data){
    fprintf_array_int16(oct_file, name, CVSD_LHIST, plc_state.hist);

    fprintf(oct_file, "y = [min(%s):1000:max(%s)];\n", name, name);
    fprintf(oct_file, "x = zeros(1, size(y,2));\n");
    fprintf(oct_file, "b = [0:500];\n");
    
    int pos = CVSD_FS;
    fprintf(oct_file, "shift_x = x + %d;\n", pos);

    pos = CVSD_LHIST - 1;
    fprintf(oct_file, "lhist_x = x + %d;\n", pos);
    pos += CVSD_OLAL;
    fprintf(oct_file, "lhist_olal1_x = x + %d;\n", pos);
    pos += CVSD_FS - CVSD_OLAL;
    fprintf(oct_file, "lhist_fs_x = x + %d;\n", pos);
    pos += CVSD_OLAL;
    fprintf(oct_file, "lhist_olal2_x = x + %d;\n", pos);
    pos += CVSD_RT;
    fprintf(oct_file, "lhist_rt_x = x + %d;\n", pos);

    fprintf(oct_file, "pattern_window_x = x + %d;\n", CVSD_LHIST - CVSD_M);
    
    fprintf(oct_file, "hold on;\n");
    fprintf(oct_file, "plot(%s); \n", name);
    
    fprintf(oct_file, "plot(shift_x, y, 'k--'); \n"); 
    fprintf(oct_file, "plot(lhist_x, y, 'k'); \n"); 
    fprintf(oct_file, "plot(lhist_olal1_x, y, 'k'); \n");
    fprintf(oct_file, "plot(lhist_fs_x, y, 'k'); \n");
    fprintf(oct_file, "plot(lhist_olal2_x, y, 'k'); \n");
    fprintf(oct_file, "plot(lhist_rt_x, y, 'k');\n");

    int x0 = plc_state.bestlag;
    int x1 = plc_state.bestlag + CVSD_M - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, name, x0, x1);
    
    x0 = plc_state.bestlag + CVSD_M ;
    x1 = plc_state.bestlag + CVSD_M + audio_samples_per_frame - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'kd'); \n", x0, x1, name, x0, x1);
    
    x0 = CVSD_LHIST - CVSD_M;
    x1 = CVSD_LHIST - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, name, x0, x1);
    fprintf(oct_file, "plot(pattern_window_x, y, 'g'); \n");
}

TEST(CVSD_PLC, CountEqBytes){
    // init cvsd_fs in plc_state
    float val, sf;
    int i, x0, x1;

    char * name;
    BTSTACK_CVSD_PLC_SAMPLE_FORMAT out[CVSD_FS];
    BTSTACK_CVSD_PLC_SAMPLE_FORMAT hist[CVSD_LHIST+CVSD_FS+CVSD_RT+CVSD_OLAL];
    FILE * oct_file = fopen("/Users/mringwal/octave/plc.m", "wb");
    if (!oct_file) return;
    fprintf(oct_file, "%s", "1;\n\n");

    int hist_len = sizeof(plc_state.hist)/2;
    create_sine_wave_int16_data(CVSD_LHIST, hist);
    memset(plc_state.hist, hist[CVSD_LHIST-1], sizeof(plc_state.hist));
    memcpy(plc_state.hist, hist, CVSD_LHIST*2);

    // Perform pattern matching to find where to replicate
    plc_state.bestlag = btstack_cvsd_plc_pattern_match(plc_state.hist);
    name = (char *) "hist0";
    fprintf_plot_history(oct_file, name, hist_len, plc_state.hist);
    
    plc_state.bestlag += CVSD_M;
    sf = btstack_cvsd_plc_amplitude_match(&plc_state, audio_samples_per_frame, plc_state.hist, plc_state.bestlag);
    
    for (i=0;i<CVSD_OLAL;i++){
        val = sf*plc_state.hist[plc_state.bestlag+i];
        plc_state.hist[CVSD_LHIST+i] = btstack_cvsd_plc_crop_sample(val);
    }
    name = (char *) "olal1";
    x0 = CVSD_LHIST;
    x1 = x0 + CVSD_OLAL - 1;
    fprintf_array_int16(oct_file, name, CVSD_OLAL, plc_state.hist+x0);
    fprintf(oct_file, "plot(b(%d:%d), %s, 'b.'); \n", x0, x1, name);

    for (;i<CVSD_FS;i++){
        val = sf*plc_state.hist[plc_state.bestlag+i]; 
        plc_state.hist[CVSD_LHIST+i] = btstack_cvsd_plc_crop_sample(val);
    }
    name = (char *)"fs_minus_olal";
    x0  = x1 + 1;
    x1  = x0 + CVSD_FS - CVSD_OLAL - 1;
    fprintf_array_int16(oct_file, name, CVSD_FS - CVSD_OLAL, plc_state.hist+x0);
    fprintf(oct_file, "plot(b(%d:%d), %s, 'b.'); \n", x0, x1, name);
    

    for (;i<CVSD_FS+CVSD_OLAL;i++){
        float left  = sf*plc_state.hist[plc_state.bestlag+i];
        float right = plc_state.hist[plc_state.bestlag+i];
        val = left*btstack_cvsd_plc_rcos(i-CVSD_FS) + right*btstack_cvsd_plc_rcos(CVSD_OLAL-1-i+CVSD_FS);
        plc_state.hist[CVSD_LHIST+i]  = btstack_cvsd_plc_crop_sample(val);
    }
    name = (char *)"olal2";
    x0  = x1 + 1;
    x1  = x0 + CVSD_OLAL - 1;
    fprintf_array_int16(oct_file, name, CVSD_OLAL, plc_state.hist+x0);
    fprintf(oct_file, "plot(b(%d:%d), %s, 'b.'); \n", x0, x1, name);
    
    for (;i<CVSD_FS+CVSD_RT+CVSD_OLAL;i++){
        plc_state.hist[CVSD_LHIST+i] = plc_state.hist[plc_state.bestlag+i];
    }
    name = (char *)"rt";
    x0  = x1 + 1;
    x1  = x0 + CVSD_RT - 1;
    fprintf_array_int16(oct_file, name, CVSD_RT, plc_state.hist+x0);
    fprintf(oct_file, "plot(b(%d:%d), %s, 'b.'); \n", x0, x1, name);
    
    for (i=0;i<CVSD_FS;i++){
        out[i] = plc_state.hist[CVSD_LHIST+i];
    }
    name = (char *)"out";
    x0  = CVSD_LHIST;
    x1  = x0 + CVSD_FS - 1;
    fprintf_array_int16(oct_file, name, CVSD_FS, plc_state.hist+x0);
    fprintf(oct_file, "plot(b(%d:%d), %s, 'cd'); \n", x0, x1, name);
    
    // shift the history buffer 
    for (i=0;i<CVSD_LHIST+CVSD_RT+CVSD_OLAL;i++){
        plc_state.hist[i] = plc_state.hist[i+CVSD_FS];
    }
    fclose(oct_file);
}


// TEST(CVSD_PLC, CountEqBytes){
//     CHECK_EQUAL(23, count_equal_bytes(test_data[0],24));
//     CHECK_EQUAL(11, count_equal_bytes(test_data[1],24));
//     CHECK_EQUAL(12, count_equal_bytes(test_data[2],24));
//     CHECK_EQUAL(23, count_equal_bytes(test_data[3],24));   
// }

// TEST(CVSD_PLC, TestLiveWavFile){
//     int corruption_step = 10;
//     introduce_bad_frames_to_wav_file("data/sco_input-16bit.wav", "results/sco_input.wav", 0);
//     introduce_bad_frames_to_wav_file("data/sco_input-16bit.wav", "results/sco_input_with_bad_frames.wav", corruption_step);
    
//     mark_bad_frames_wav_file("results/sco_input.wav", "results/sco_input_detected_frames.wav");
//     process_wav_file_with_plc("results/sco_input.wav", "results/sco_input_after_plc.wav");
//     process_wav_file_with_plc("results/sco_input_with_bad_frames.wav", "results/sco_input_with_bad_frames_after_plc.wav");
// }

// TEST(CVSD_PLC, TestFanfareFile){
//     int corruption_step = 10;
//     introduce_bad_frames_to_wav_file("data/fanfare_mono.wav", "results/fanfare_mono.wav", 0);
//     introduce_bad_frames_to_wav_file("results/fanfare_mono.wav", "results/fanfare_mono_with_bad_frames.wav", corruption_step);
    
//     mark_bad_frames_wav_file("results/fanfare_mono.wav", "results/fanfare_mono_detected_frames.wav");
//     process_wav_file_with_plc("results/fanfare_mono.wav", "results/fanfare_mono_after_plc.wav");
//     process_wav_file_with_plc("results/fanfare_mono_with_bad_frames.wav", "results/fanfare_mono_with_bad_frames_after_plc.wav");
// }

TEST(CVSD_PLC, TestSineWave){
    int corruption_step = 600;
    create_sine_wav("results/sine_test.wav");
    int total_corruption_times = introduce_bad_frames_to_wav_file("results/sine_test.wav", "results/sine_test_with_bad_frames.wav", corruption_step);
    printf("corruptions %d\n", total_corruption_times);
    process_wav_file_with_plc("results/sine_test.wav", "results/sine_test_after_plc.wav");
    process_wav_file_with_plc("results/sine_test_with_bad_frames.wav", "results/sine_test_with_bad_frames_after_plc.wav");
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

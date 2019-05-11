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

#define BTSTACK_FILE__ "btstack_cvsd_plc.c"

/*
 * btstack_CVSD_plc.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_cvsd_plc.h"
#include "btstack_debug.h"

// static float rcos[CVSD_OLAL] = {
//     0.99148655f,0.96623611f,0.92510857f,0.86950446f,
//     0.80131732f,0.72286918f,0.63683150f,0.54613418f, 
//     0.45386582f,0.36316850f,0.27713082f,0.19868268f, 
//     0.13049554f,0.07489143f,0.03376389f,0.00851345f};

static float rcos[CVSD_OLAL] = {
    0.99148655f,0.92510857f,
    0.80131732f,0.63683150f, 
    0.45386582f,0.27713082f, 
    0.13049554f,0.03376389f};

float btstack_cvsd_plc_rcos(int index){
    if (index > CVSD_OLAL) return 0;
    return rcos[index];
}
// taken from http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi
// Algorithm: Babylonian Method + some manipulations on IEEE 32 bit floating point representation
static float sqrt3(const float x){
    union {
        int i;
        float x;
    } u;
    u.x = x;
    u.i = (1<<29) + (u.i >> 1) - (1<<22); 

    // Two Babylonian Steps (simplified from:)
    // u.x = 0.5f * (u.x + x/u.x);
    // u.x = 0.5f * (u.x + x/u.x);
    u.x =       u.x + x/u.x;
    u.x = 0.25f*u.x + x/u.x;

    return u.x;
}

static float btstack_cvsd_plc_absolute(float x){
     if (x < 0) x = -x;
     return x;
}

static float btstack_cvsd_plc_cross_correlation(BTSTACK_CVSD_PLC_SAMPLE_FORMAT *x, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *y){
    float num = 0;
    float den = 0;
    float x2 = 0;
    float y2 = 0;
    int   m;
    for (m=0;m<CVSD_M;m++){
        num+=((float)x[m])*y[m];
        x2+=((float)x[m])*x[m];
        y2+=((float)y[m])*y[m];
    }
    den = (float)sqrt3(x2*y2);
    return num/den;
}

int btstack_cvsd_plc_pattern_match(BTSTACK_CVSD_PLC_SAMPLE_FORMAT *y){
    float maxCn = -999999.0;  // large negative number
    int   bestmatch = 0;
    float Cn;
    int   n;
    for (n=0;n<CVSD_N;n++){
        Cn = btstack_cvsd_plc_cross_correlation(&y[CVSD_LHIST-CVSD_M], &y[n]); 
        if (Cn>maxCn){
            bestmatch=n;
            maxCn = Cn; 
        }
    }
    return bestmatch;
}

float btstack_cvsd_plc_amplitude_match(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *y, BTSTACK_CVSD_PLC_SAMPLE_FORMAT bestmatch){
    UNUSED(plc_state);
    int   i;
    float sumx = 0;
    float sumy = 0.000001f;
    float sf;
    
    for (i=0;i<num_samples;i++){
        sumx += btstack_cvsd_plc_absolute(y[CVSD_LHIST-num_samples+i]);
        sumy += btstack_cvsd_plc_absolute(y[bestmatch+i]);
    }
    sf = sumx/sumy;
    // This is not in the paper, but limit the scaling factor to something reasonable to avoid creating artifacts 
    if (sf<0.75f) sf=0.75f;
    if (sf>1.0) sf=1.0f;
    return sf;
}

BTSTACK_CVSD_PLC_SAMPLE_FORMAT btstack_cvsd_plc_crop_sample(float val){
    float croped_val = val;
    if (croped_val > 32767.0)  croped_val= 32767.0;
    if (croped_val < -32768.0) croped_val=-32768.0; 
    return (BTSTACK_CVSD_PLC_SAMPLE_FORMAT) croped_val;
}

void btstack_cvsd_plc_init(btstack_cvsd_plc_state_t *plc_state){
    memset(plc_state, 0, sizeof(btstack_cvsd_plc_state_t));
}

#ifdef OCTAVE_OUTPUT
typedef enum {
    OCTAVE_FRAME_TYPE_UNKNOWN = 0,
    OCTAVE_FRAME_TYPE_GOOD,
    OCTAVE_FRAME_TYPE_BAD
} octave_frame_type_t;

static const char * octave_frame_type_name[] = {
    "unknown",
    "good",
    "bad"
};

static octave_frame_type_t octave_frame_type;
static char octave_base_name[1000];

const char * octave_frame_type2str(int index){
    if (index <= 0 || index >= sizeof(octave_frame_type_t)) return octave_frame_type_name[0];
    return octave_frame_type_name[index];
}

void btstack_cvsd_plc_octave_set_base_name(const char * base_name){
    strcpy(octave_base_name, base_name);
    printf("OCTAVE: base name set to %s\n", octave_base_name);
}

static void octave_fprintf_array_int16(FILE * oct_file, char * name, int data_len, int16_t * data){
    fprintf(oct_file, "%s = [", name);
    int i;
    for (i = 0; i < data_len - 1; i++){
        fprintf(oct_file, "%d, ", data[i]);
    }
    fprintf(oct_file, "%d", data[i]);
    fprintf(oct_file, "%s", "];\n");
}

static FILE * open_octave_file(btstack_cvsd_plc_state_t *plc_state, octave_frame_type_t frame_type){
    char oct_file_name[1200];
    octave_frame_type = frame_type;
    sprintf(oct_file_name, "%s_octave_plc_%d_%s.m", octave_base_name, plc_state->frame_count, octave_frame_type2str(octave_frame_type));
    
    FILE * oct_file = fopen(oct_file_name, "wb");
    if (oct_file == NULL){
        printf("OCTAVE: could not open file %s\n", oct_file_name);
        return NULL;
    }
    printf("OCTAVE: opened file %s\n", oct_file_name);
    return oct_file;
}

static void octave_fprintf_plot_history_frame(btstack_cvsd_plc_state_t *plc_state, FILE * oct_file, int frame_nr){
    char title[100];
    char hist_name[10];
    sprintf(hist_name, "hist%d", plc_state->nbf);
            
    octave_fprintf_array_int16(oct_file, hist_name, CVSD_LHIST, plc_state->hist);

    fprintf(oct_file, "y = [min(%s):1000:max(%s)];\n", hist_name, hist_name);
    fprintf(oct_file, "x = zeros(1, size(y,2));\n");
    fprintf(oct_file, "b = [0: %d];\n", CVSD_LHIST+CVSD_FS+CVSD_RT+CVSD_OLAL);
    
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
    
    fprintf(oct_file, "hf = figure();\n");
    sprintf(title, "PLC %s frame %d", octave_frame_type2str(octave_frame_type), frame_nr);
    
    fprintf(oct_file, "hold on;\n");
    fprintf(oct_file, "h1 = plot(%s); \n", hist_name);
    
    fprintf(oct_file, "title(\"%s\");\n", title); 
    
    fprintf(oct_file, "plot(lhist_x, y, 'k'); \n"); 
    fprintf(oct_file, "text(max(lhist_x) - 10, max(y)+1000, 'lhist'); \n"); 

    fprintf(oct_file, "plot(lhist_olal1_x, y, 'k'); \n");
    fprintf(oct_file, "text(max(lhist_olal1_x) - 10, max(y)+1000, 'OLAL'); \n"); 

    fprintf(oct_file, "plot(lhist_fs_x, y, 'k'); \n");
    fprintf(oct_file, "text(max(lhist_fs_x) - 10, max(y)+1000, 'FS'); \n"); 

    fprintf(oct_file, "plot(lhist_olal2_x, y, 'k'); \n");
    fprintf(oct_file, "text(max(lhist_olal2_x) - 10, max(y)+1000, 'OLAL'); \n"); 

    fprintf(oct_file, "plot(lhist_rt_x, y, 'k');\n");
    fprintf(oct_file, "text(max(lhist_rt_x) - 10, max(y)+1000, 'RT'); \n"); 

    if (octave_frame_type == OCTAVE_FRAME_TYPE_GOOD) return;

    int x0 = plc_state->bestlag;
    int x1 = plc_state->bestlag + CVSD_M - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, hist_name, x0, x1);
    fprintf(oct_file, "text(%d - 10, -10, 'bestlag'); \n", x0); 

    x0 = plc_state->bestlag + CVSD_M ;
    x1 = plc_state->bestlag + CVSD_M + CVSD_FS - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'kd'); \n", x0, x1, hist_name, x0, x1);
    
    x0 = CVSD_LHIST - CVSD_M;
    x1 = CVSD_LHIST - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, hist_name, x0, x1);
    fprintf(oct_file, "plot(pattern_window_x, y, 'g'); \n");
    fprintf(oct_file, "text(max(pattern_window_x) - 10, max(y)+1000, 'M'); \n"); 
}

static void octave_fprintf_plot_output(btstack_cvsd_plc_state_t *plc_state, FILE * oct_file){
    if (!oct_file) return;
    char out_name[10];
    sprintf(out_name, "out%d", plc_state->nbf);
    int x0  = CVSD_LHIST;
    int x1  = x0 + CVSD_FS - 1;
    octave_fprintf_array_int16(oct_file, out_name, CVSD_FS, plc_state->hist+x0);
    fprintf(oct_file, "h2 = plot(b(%d:%d), %s, 'cd'); \n", x0, x1, out_name);

    char rest_hist_name[10];
    sprintf(rest_hist_name, "rest%d", plc_state->nbf);
    x0  = CVSD_LHIST + CVSD_FS;
    x1  = x0 + CVSD_OLAL + CVSD_RT - 1;
    octave_fprintf_array_int16(oct_file, rest_hist_name, CVSD_OLAL + CVSD_RT, plc_state->hist+x0);
    fprintf(oct_file, "h3 = plot(b(%d:%d), %s, 'kd'); \n", x0, x1, rest_hist_name);

    char new_hist_name[10];
    sprintf(new_hist_name, "hist%d", plc_state->nbf);
    octave_fprintf_array_int16(oct_file, new_hist_name, CVSD_LHIST, plc_state->hist);
    fprintf(oct_file, "h4 = plot(%s, 'r--'); \n", new_hist_name);

    fprintf(oct_file, "legend ([h1, h2, h3, h4], {\"hist\", \"out\", \"rest\", \"new hist\"}, \"location\", \"northeast\");\n ");

    char fig_name[1200];
    sprintf(fig_name, "../%s_octave_plc_%d_%s", octave_base_name, plc_state->frame_count, octave_frame_type2str(octave_frame_type));
    fprintf(oct_file, "print(hf, \"%s.jpg\", \"-djpg\");", fig_name);
}
#endif

void btstack_cvsd_plc_bad_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *out){
    float val;
    int   i = 0;
    float sf = 1;
    plc_state->nbf++;
    
    if (plc_state->max_consecutive_bad_frames_nr < plc_state->nbf){
        plc_state->max_consecutive_bad_frames_nr = plc_state->nbf;
    }
    if (plc_state->nbf==1){
        // printf("first bad frame\n");
        // Perform pattern matching to find where to replicate
        plc_state->bestlag = btstack_cvsd_plc_pattern_match(plc_state->hist);
    }

#ifdef OCTAVE_OUTPUT
    FILE * oct_file = open_octave_file(plc_state, OCTAVE_FRAME_TYPE_BAD);
    if (oct_file){
        octave_fprintf_plot_history_frame(plc_state, oct_file, plc_state->frame_count);   
    }
#endif

    if (plc_state->nbf==1){
        // the replication begins after the template match
        plc_state->bestlag += CVSD_M; 
        
        // Compute Scale Factor to Match Amplitude of Substitution Packet to that of Preceding Packet
        sf = btstack_cvsd_plc_amplitude_match(plc_state, num_samples, plc_state->hist, plc_state->bestlag);
        for (i=0;i<CVSD_OLAL;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i];
            plc_state->hist[CVSD_LHIST+i] = btstack_cvsd_plc_crop_sample(val);
        }
        
        for (;i<num_samples;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i]; 
            plc_state->hist[CVSD_LHIST+i] = btstack_cvsd_plc_crop_sample(val);
        }
        
        for (;i<num_samples+CVSD_OLAL;i++){
            float left  = sf*plc_state->hist[plc_state->bestlag+i];
            float right = plc_state->hist[plc_state->bestlag+i];
            val = left*rcos[i-num_samples] + right*rcos[CVSD_OLAL-1-i+num_samples];
            plc_state->hist[CVSD_LHIST+i] = btstack_cvsd_plc_crop_sample(val);
        }

        for (;i<num_samples+CVSD_RT+CVSD_OLAL;i++){
            plc_state->hist[CVSD_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    } else {
        for (;i<num_samples+CVSD_RT+CVSD_OLAL;i++){
            plc_state->hist[CVSD_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    }

    for (i=0;i<num_samples;i++){
        out[i] = plc_state->hist[CVSD_LHIST+i];
    }
    
    // shift the history buffer 
    for (i=0;i<CVSD_LHIST+CVSD_RT+CVSD_OLAL;i++){
        plc_state->hist[i] = plc_state->hist[i+num_samples];
    }

#ifdef OCTAVE_OUTPUT
    if (oct_file){
        octave_fprintf_plot_output(plc_state, oct_file);
        fclose(oct_file);
    }
#endif
}

void btstack_cvsd_plc_good_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *in, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *out){
    float val;
    int i = 0;
#ifdef OCTAVE_OUTPUT
    FILE * oct_file = NULL;
    if (plc_state->nbf>0){
        oct_file = open_octave_file(plc_state, OCTAVE_FRAME_TYPE_GOOD);
        if (oct_file){
            octave_fprintf_plot_history_frame(plc_state, oct_file, plc_state->frame_count);
        }
    }
#endif
    if (plc_state->nbf>0){
        for (i=0;i<CVSD_RT;i++){
            out[i] = plc_state->hist[CVSD_LHIST+i];
        }
            
        for (i=CVSD_RT;i<CVSD_RT+CVSD_OLAL;i++){
            float left  = plc_state->hist[CVSD_LHIST+i];
            float right = in[i];
            val = left * rcos[i-CVSD_RT] + right *rcos[CVSD_OLAL+CVSD_RT-1-i];
            out[i] = btstack_cvsd_plc_crop_sample((BTSTACK_CVSD_PLC_SAMPLE_FORMAT)val);
        }
    }

    for (;i<num_samples;i++){
        out[i] = in[i];
    }
    // Copy the output to the history buffer
    for (i=0;i<num_samples;i++){
        plc_state->hist[CVSD_LHIST+i] = out[i];
    }
    // shift the history buffer
    for (i=0;i<CVSD_LHIST;i++){
        plc_state->hist[i] = plc_state->hist[i+num_samples];
    }

#ifdef OCTAVE_OUTPUT
    if (oct_file){
        octave_fprintf_plot_output(plc_state, oct_file);
        fclose(oct_file);
    }
#endif
    plc_state->nbf=0;
}

static int count_equal_samples(BTSTACK_CVSD_PLC_SAMPLE_FORMAT * packet, uint16_t size){
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

static int count_zeros(BTSTACK_CVSD_PLC_SAMPLE_FORMAT * frame, uint16_t size){
    int nr_zeros = 0;
    int i;
    for (i = 0; i < size-1; i++){
        if (frame[i] == 0){
            nr_zeros++;
        }
    }
    return nr_zeros;
}

// note: a zero_frame is currently also a 'bad_frame'
static int zero_frame(BTSTACK_CVSD_PLC_SAMPLE_FORMAT * frame, uint16_t size){
    return count_zeros(frame, size) > (size/2);
}

// more than half the samples are the same -> bad frame
static int bad_frame(btstack_cvsd_plc_state_t *plc_state, BTSTACK_CVSD_PLC_SAMPLE_FORMAT * frame, uint16_t size){
    UNUSED(plc_state);
    return count_equal_samples(frame, size) >= (size / 2);
}


void btstack_cvsd_plc_process_data(btstack_cvsd_plc_state_t * plc_state, BTSTACK_CVSD_PLC_SAMPLE_FORMAT * in, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT * out){
    if (num_samples == 0) return;

    plc_state->frame_count++;

    int is_zero_frame = zero_frame(in, num_samples);
    int is_bad_frame  = bad_frame(plc_state, in, num_samples);

    if (is_bad_frame){
        memcpy(out, in, num_samples * 2);
        if (plc_state->good_samples > CVSD_LHIST){
            btstack_cvsd_plc_bad_frame(plc_state, num_samples, out);
            if (is_zero_frame){
                plc_state->zero_frames_nr++;
            } else {
                plc_state->bad_frames_nr++;
            }
        } else {
            memset(out, 0, num_samples * 2);
        }
    } else {
        btstack_cvsd_plc_good_frame(plc_state, num_samples, in, out);
        plc_state->good_frames_nr++;
        if (plc_state->good_frames_nr == 1){
            log_info("First good frame at index %d\n", plc_state->frame_count-1);
        } 
        plc_state->good_samples += num_samples;
    }
}

void btstack_cvsd_dump_statistics(btstack_cvsd_plc_state_t * state){
    log_info("Good frames: %d\n", state->good_frames_nr);
    log_info("Bad frames: %d\n", state->bad_frames_nr);
    log_info("Zero frames: %d\n", state->zero_frames_nr);
    log_info("Max Consecutive bad frames: %d\n", state->max_consecutive_bad_frames_nr);   
}

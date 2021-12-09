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

#define BTSTACK_FILE__ "btstack_sbc_plc.c"

/*
 * btstack_sbc_plc.c
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef OCTAVE_OUTPUT
#include <stdio.h>
#endif

#include "btstack_sbc_plc.h"
#include "btstack_debug.h"

#define SAMPLE_FORMAT int16_t

// Zero Frame (57 bytes) with padding zeros to avoid out of bound reads
static uint8_t indices0[] = {
    0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d,
    0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6,
    0xdb, 0x77, 0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb,
    0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6, 0xdd, 0xdb, 0x6d,
    0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6,
    0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c,
    /*                padding            */   0x00, 0x00, 0x00
};

/* Raised COSine table for OLA */
static float rcos[SBC_OLAL] = {
    0.99148655f,0.96623611f,0.92510857f,0.86950446f,
    0.80131732f,0.72286918f,0.63683150f,0.54613418f, 
    0.45386582f,0.36316850f,0.27713082f,0.19868268f, 
    0.13049554f,0.07489143f,0.03376389f,0.00851345f
};

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
    u.x =       u.x + (x/u.x);
    u.x = (0.25f*u.x) + (x/u.x);

    return u.x;
}

static float absolute(float x){
     if (x < 0) x = -x;
     return x;
}

static float CrossCorrelation(SAMPLE_FORMAT *x, SAMPLE_FORMAT *y){
    float num = 0;
    float den = 0;
    float x2 = 0;
    float y2 = 0;
    int   m;
    for (m=0;m<SBC_M;m++){
        num+=((float)x[m])*y[m];
        x2+=((float)x[m])*x[m];
        y2+=((float)y[m])*y[m];
    }
    den = (float)sqrt3(x2*y2);
    return num/den;
}

static int PatternMatch(SAMPLE_FORMAT *y){
    float maxCn = -999999.0;  // large negative number
    int   bestmatch = 0;
    float Cn;
    int   n;
    for (n=0;n<SBC_N;n++){
        Cn = CrossCorrelation(&y[SBC_LHIST-SBC_M], &y[n]); 
        if (Cn>maxCn){
            bestmatch=n;
            maxCn = Cn; 
        }
    }
    return bestmatch;
}

static float AmplitudeMatch(SAMPLE_FORMAT *y, SAMPLE_FORMAT bestmatch) {
    int   i;
    float sumx = 0;
    float sumy = 0.000001f;
    float sf;
    
    for (i=0;i<SBC_FS;i++){
        sumx += absolute(y[SBC_LHIST-SBC_FS+i]);
        sumy += absolute(y[bestmatch+i]);
    }
    sf = sumx/sumy;
    // This is not in the paper, but limit the scaling factor to something reasonable to avoid creating artifacts 
    if (sf<0.75f) sf=0.75f;
    if (sf>1.0f) sf=1.0f;
    return sf;
}

static SAMPLE_FORMAT crop_sample(float val){
    float croped_val = val;
    if (croped_val > 32767.0)  croped_val= 32767.0;
    if (croped_val < -32768.0) croped_val=-32768.0; 
    return (SAMPLE_FORMAT) croped_val;
}

uint8_t * btstack_sbc_plc_zero_signal_frame(void){
    return (uint8_t *)&indices0;
}

void btstack_sbc_plc_init(btstack_sbc_plc_state_t *plc_state){
    plc_state->nbf=0;
    plc_state->bestlag=0;
    memset(plc_state->hist,0,sizeof(plc_state->hist));   
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

void btstack_sbc_plc_octave_set_base_name(const char * base_name){
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

static FILE * open_octave_file(btstack_sbc_plc_state_t *plc_state, octave_frame_type_t frame_type){
    char oct_file_name[1200];
    octave_frame_type = frame_type;
    snprintf(oct_file_name, sizeof(oct_file_name), "%s_octave_plc_%d_%s.m",
             octave_base_name, plc_state->frame_count,
             octave_frame_type2str(octave_frame_type));
    oct_file_name[sizeof(oct_file_name) - 1] = 0;
    
    FILE * oct_file = fopen(oct_file_name, "wb");
    if (oct_file == NULL){
        printf("OCTAVE: could not open file %s\n", oct_file_name);
        return NULL;
    }
    printf("OCTAVE: opened file %s\n", oct_file_name);
    return oct_file;
}

static void octave_fprintf_plot_history_frame(btstack_sbc_plc_state_t *plc_state, FILE * oct_file, int frame_nr){
    char title[100];
    char hist_name[10];
    snprintf(hist_name, sizeof(hist_name), "hist%d", plc_state->nbf);
    hist_name[sizeof(hist_name) - 1] = 0;
            
    octave_fprintf_array_int16(oct_file, hist_name, SBC_LHIST, plc_state->hist);

    fprintf(oct_file, "y = [min(%s):1000:max(%s)];\n", hist_name, hist_name);
    fprintf(oct_file, "x = zeros(1, size(y,2));\n");
    fprintf(oct_file, "b = [0: %d];\n", SBC_LHIST+SBC_FS+SBC_RT+SBC_OLAL);
    
    int pos = SBC_FS;
    fprintf(oct_file, "shift_x = x + %d;\n", pos);

    pos = SBC_LHIST - 1;
    fprintf(oct_file, "lhist_x = x + %d;\n", pos);
    pos += SBC_OLAL;
    fprintf(oct_file, "lhist_olal1_x = x + %d;\n", pos);
    pos += SBC_FS - SBC_OLAL;
    fprintf(oct_file, "lhist_fs_x = x + %d;\n", pos);
    pos += SBC_OLAL;
    fprintf(oct_file, "lhist_olal2_x = x + %d;\n", pos);
    pos += SBC_RT;
    fprintf(oct_file, "lhist_rt_x = x + %d;\n", pos);

    fprintf(oct_file, "pattern_window_x = x + %d;\n", SBC_LHIST - SBC_M);
    
    fprintf(oct_file, "hf = figure();\n");
    snprintf(title, sizeof(title), "PLC %s frame %d",
             octave_frame_type2str(octave_frame_type), frame_nr);
    title[sizeof(title) - 1] = 0;
    
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
    int x1 = plc_state->bestlag + SBC_M - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, hist_name, x0, x1);
    fprintf(oct_file, "text(%d - 10, -10, 'bestlag'); \n", x0); 

    x0 = plc_state->bestlag + SBC_M ;
    x1 = plc_state->bestlag + SBC_M + SBC_FS - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'kd'); \n", x0, x1, hist_name, x0, x1);
    
    x0 = SBC_LHIST - SBC_M;
    x1 = SBC_LHIST - 1;
    fprintf(oct_file, "plot(b(%d:%d), %s(%d:%d), 'rd'); \n", x0, x1, hist_name, x0, x1);
    fprintf(oct_file, "plot(pattern_window_x, y, 'g'); \n");
    fprintf(oct_file, "text(max(pattern_window_x) - 10, max(y)+1000, 'M'); \n"); 
}

static void octave_fprintf_plot_output(btstack_sbc_plc_state_t *plc_state, FILE * oct_file){
    if (!oct_file) return;
    char out_name[10];
    snprintf(out_name, sizeof(out_name), "out%d", plc_state->nbf);
    out_name[sizeof(out_name) - 1] = 0;
    int x0  = SBC_LHIST;
    int x1  = x0 + SBC_FS - 1;
    octave_fprintf_array_int16(oct_file, out_name, SBC_FS, plc_state->hist+x0);
    fprintf(oct_file, "h2 = plot(b(%d:%d), %s, 'cd'); \n", x0, x1, out_name);

    char rest_hist_name[10];
    snprintf(rest_hist_name, sizeof(rest_hist_name), "rest%d", plc_state->nbf);
    rest_hist_name[sizeof(rest_hist_name) - 1] = 0;
    x0  = SBC_LHIST + SBC_FS;
    x1  = x0 + SBC_OLAL + SBC_RT - 1;
    octave_fprintf_array_int16(oct_file, rest_hist_name, SBC_OLAL + SBC_RT, plc_state->hist+x0);
    fprintf(oct_file, "h3 = plot(b(%d:%d), %s, 'kd'); \n", x0, x1, rest_hist_name);

    char new_hist_name[10];
    snprintf(new_hist_name, sizeof(new_hist_name), "hist%d", plc_state->nbf);
    new_hist_name[sizeof(new_hist_name) - 1] = 0;
    octave_fprintf_array_int16(oct_file, new_hist_name, SBC_LHIST, plc_state->hist);
    fprintf(oct_file, "h4 = plot(%s, 'r--'); \n", new_hist_name);

    fprintf(oct_file, "legend ([h1, h2, h3, h4], {\"hist\", \"out\", \"rest\", \"new hist\"}, \"location\", \"northeast\");\n ");

    char fig_name[1200];
    snprintf(fig_name, sizeof(fig_name), "../%s_octave_plc_%d_%s",
             octave_base_name, plc_state->frame_count,
             octave_frame_type2str(octave_frame_type));
    fig_name[sizeof(fig_name) - 1] = 0;
    fprintf(oct_file, "print(hf, \"%s.jpg\", \"-djpg\");", fig_name);
}
#endif


void btstack_sbc_plc_bad_frame(btstack_sbc_plc_state_t *plc_state, SAMPLE_FORMAT *ZIRbuf, SAMPLE_FORMAT *out){
    float val;
    int   i;
    float sf = 1;

    plc_state->nbf++;   
    plc_state->bad_frames_nr++;
    plc_state->frame_count++;

    if (plc_state->max_consecutive_bad_frames_nr < plc_state->nbf){
        plc_state->max_consecutive_bad_frames_nr = plc_state->nbf;
    }
    
    if (plc_state->nbf==1){
        // printf("first bad frame\n");
        // Perform pattern matching to find where to replicate
        plc_state->bestlag = PatternMatch(plc_state->hist);
    }

#ifdef OCTAVE_OUTPUT
    FILE * oct_file = open_octave_file(plc_state, OCTAVE_FRAME_TYPE_BAD);
    if (oct_file){
        octave_fprintf_plot_history_frame(plc_state, oct_file, plc_state->frame_count);   
    }
#endif

    if (plc_state->nbf==1){
        // the replication begins after the template match
        plc_state->bestlag += SBC_M; 
        
        // Compute Scale Factor to Match Amplitude of Substitution Packet to that of Preceding Packet
        sf = AmplitudeMatch(plc_state->hist, plc_state->bestlag);
        // printf("sf Apmlitude Match %f, new data %d, bestlag+M %d\n", sf, ZIRbuf[0], plc_state->hist[plc_state->bestlag]);
        for (i=0; i<SBC_OLAL; i++){
            float left  = ZIRbuf[i];
            float right = sf*plc_state->hist[plc_state->bestlag+i];
            val = (left*rcos[i]) + (right*rcos[SBC_OLAL-1-i]);
            // val = sf*plc_state->hist[plc_state->bestlag+i];
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }
        
        for (i=SBC_OLAL; i<SBC_FS; i++){
            val = sf*plc_state->hist[plc_state->bestlag+i]; 
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }
        
        for (i=SBC_FS; i<(SBC_FS+SBC_OLAL); i++){
            float left  = sf*plc_state->hist[plc_state->bestlag+i];
            float right = plc_state->hist[plc_state->bestlag+i];
            val = (left*rcos[i-SBC_FS])+(right*rcos[SBC_OLAL-1-i+SBC_FS]);
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }

        for (i=(SBC_FS+SBC_OLAL); i<(SBC_FS+SBC_RT+SBC_OLAL); i++){
            plc_state->hist[SBC_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    } else {
        // printf("succesive bad frame nr %d\n", plc_state->nbf);
        for (i=0; i<(SBC_FS+SBC_RT+SBC_OLAL); i++){
            plc_state->hist[SBC_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    }
    for (i=0; i<SBC_FS; i++){
        out[i] = plc_state->hist[SBC_LHIST+i];
    }
        
   // shift the history buffer 
    for (i=0; i<(SBC_LHIST+SBC_RT+SBC_OLAL); i++){
        plc_state->hist[i] = plc_state->hist[i+SBC_FS];
    }

#ifdef OCTAVE_OUTPUT
    if (oct_file){
        octave_fprintf_plot_output(plc_state, oct_file);
        fclose(oct_file);
    }
#endif
}

void btstack_sbc_plc_good_frame(btstack_sbc_plc_state_t *plc_state, SAMPLE_FORMAT *in, SAMPLE_FORMAT *out){
    float val;
    int i = 0;
    plc_state->good_frames_nr++;
    plc_state->frame_count++;

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
        for (i=0;i<SBC_RT;i++){
            out[i] = plc_state->hist[SBC_LHIST+i];
        }
            
        for (i = SBC_RT;i<(SBC_RT+SBC_OLAL);i++){
            float left  = plc_state->hist[SBC_LHIST+i];
            float right = in[i];  
            val = (left*rcos[i-SBC_RT]) + (right*rcos[SBC_OLAL+SBC_RT-1-i]);
            out[i] = crop_sample(val);
        }
    }

    for (;i<SBC_FS;i++){
        out[i] = in[i];
    }
    // Copy the output to the history buffer
    for (i=0;i<SBC_FS;i++){
        plc_state->hist[SBC_LHIST+i] = out[i];
    }

    // shift the history buffer
    for (i=0;i<SBC_LHIST;i++){
        plc_state->hist[i] = plc_state->hist[i+SBC_FS];
    }

#ifdef OCTAVE_OUTPUT
    if (oct_file){
        octave_fprintf_plot_output(plc_state, oct_file);
        fclose(oct_file);
    }
#endif
    plc_state->nbf=0;
}

void btstack_sbc_dump_statistics(btstack_sbc_plc_state_t * state){
    log_info("Good frames: %d\n", state->good_frames_nr);
    log_info("Bad frames: %d\n", state->bad_frames_nr);
    log_info("Max Consecutive bad frames: %d\n", state->max_consecutive_bad_frames_nr);   
}

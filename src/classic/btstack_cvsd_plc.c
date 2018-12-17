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

#define __BTSTACK_FILE__ "btstack_cvsd_plc.c"

/*
 * btstack_sbc_plc.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_cvsd_plc.h"
#include "btstack_debug.h"

static float rcos[CVSD_OLAL] = {
    0.99148655f,0.96623611f,0.92510857f,0.86950446f,
    0.80131732f,0.72286918f,0.63683150f,0.54613418f, 
    0.45386582f,0.36316850f,0.27713082f,0.19868268f, 
    0.13049554f,0.07489143f,0.03376389f,0.00851345f};

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

void btstack_cvsd_plc_bad_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *out){
    float val;
    int   i = 0;
    float sf = 1;
    plc_state->nbf++;
    // plc_state->cvsd_fs = CVSD_FS_MAX;
    if (plc_state->nbf==1){
        // Perform pattern matching to find where to replicate
        plc_state->bestlag = btstack_cvsd_plc_pattern_match(plc_state->hist);
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
}

void btstack_cvsd_plc_good_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *in, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *out){
    float val;
    int i = 0;
    if (plc_state->nbf>0){
        for (i=0;i<CVSD_RT;i++){
            out[i] = plc_state->hist[CVSD_LHIST+i];
        }
            
        for (i=CVSD_RT;i<CVSD_RT+CVSD_OLAL;i++){
            float left  = plc_state->hist[CVSD_LHIST+i];
            float right = in[i];
            val = left * rcos[i-CVSD_RT] + right *rcos[CVSD_OLAL+CVSD_RT-1-i];
            out[i] = (BTSTACK_CVSD_PLC_SAMPLE_FORMAT)val;
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
    return count_equal_samples(frame, size) > size / 2;
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
}

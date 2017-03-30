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

#define __BTSTACK_FILE__ "btstack_sbc_plc.c"

/*
 * btstack_sbc_plc.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_sbc_plc.h"

#define SAMPLE_FORMAT int16_t

static uint8_t indices0[] = { 0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c};

/* Raised COSine table for OLA */
static float rcos[SBC_OLAL] = {
    0.99148655f,0.96623611f,0.92510857f,0.86950446f,
    0.80131732f,0.72286918f,0.63683150f,0.54613418f, 
    0.45386582f,0.36316850f,0.27713082f,0.19868268f, 
    0.13049554f,0.07489143f,0.03376389f,0.00851345f};

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
    if (sf>1.2f) sf=1.2f;
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

void btstack_sbc_plc_bad_frame(btstack_sbc_plc_state_t *plc_state, SAMPLE_FORMAT *ZIRbuf, SAMPLE_FORMAT *out){
    float val;
    int   i = 0;
    float sf = 1;
    plc_state->nbf++;
   
    if (plc_state->nbf==1){
        // Perform pattern matching to find where to replicate
        plc_state->bestlag = PatternMatch(plc_state->hist);
        // the replication begins after the template match
        plc_state->bestlag += SBC_M; 
        
        // Compute Scale Factor to Match Amplitude of Substitution Packet to that of Preceding Packet
        sf = AmplitudeMatch(plc_state->hist, plc_state->bestlag);
        for (i=0;i<SBC_OLAL;i++){
            float left  = ZIRbuf[i];
            float right = sf*plc_state->hist[plc_state->bestlag+i];
            val = left*rcos[i] + right*rcos[SBC_OLAL-1-i];
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }
        
        for (;i<SBC_FS;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i]; 
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }
        
        for (;i<SBC_FS+SBC_OLAL;i++){
            float left  = sf*plc_state->hist[plc_state->bestlag+i];
            float right = plc_state->hist[plc_state->bestlag+i];
            val = left*rcos[i-SBC_FS]+right*rcos[SBC_OLAL-1-i+SBC_FS];
            plc_state->hist[SBC_LHIST+i] = crop_sample(val);
        }

        for (;i<SBC_FS+SBC_RT+SBC_OLAL;i++){
            plc_state->hist[SBC_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    } else {
        for (;i<SBC_FS+SBC_RT+SBC_OLAL;i++){
            plc_state->hist[SBC_LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        }
    }
    for (i=0;i<SBC_FS;i++){
        out[i] = plc_state->hist[SBC_LHIST+i];
    }
        
   // shift the history buffer 
    for (i=0;i<SBC_LHIST+SBC_RT+SBC_OLAL;i++){
        plc_state->hist[i] = plc_state->hist[i+SBC_FS];
    }
}

void btstack_sbc_plc_good_frame(btstack_sbc_plc_state_t *plc_state, SAMPLE_FORMAT *in, SAMPLE_FORMAT *out){
    float val;
    int i = 0;
    if (plc_state->nbf>0){
        for (i=0;i<SBC_RT;i++){
            out[i] = plc_state->hist[SBC_LHIST+i];
        }
            
        for (i = SBC_RT;i<SBC_RT+SBC_OLAL;i++){
            float left  = plc_state->hist[SBC_LHIST+i];
            float right = in[i];  
            val = left*rcos[i-SBC_RT] + right*rcos[SBC_OLAL+SBC_RT-1-i];
            out[i] = (SAMPLE_FORMAT)val;
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

    plc_state->nbf=0;
}

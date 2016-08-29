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

/*
 * btstack_sbc_plc.c
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "btstack_cvsd_plc.h"

static float rcos[OLAL] = {
    0.99148655f,0.96623611f,0.92510857f,0.86950446f,
    0.80131732f,0.72286918f,0.63683150f,0.54613418f, 
    0.45386582f,0.36316850f,0.27713082f,0.19868268f, 
    0.13049554f,0.07489143f,0.03376389f,0.00851345f};

static float CrossCorrelation(int8_t *x, int8_t *y);
static int PatternMatch(int8_t *y);
static float AmplitudeMatch(int8_t *y, int8_t bestmatch);

void btstack_cvsd_plc_init(btstack_cvsd_plc_state_t *plc_state){
    int i;
    plc_state->nbf=0;
    plc_state->bestlag=0;
    for (i=0;i<LHIST+CVSDRT;i++){
        plc_state->hist[i] = 0; 
    }    
}

void btstack_cvsd_plc_bad_frame(btstack_cvsd_plc_state_t *plc_state, int8_t *out){
    int   i;
    float val;
    float sf;
    plc_state->nbf++;
    sf=1.0f;

    i=0;
    if (plc_state->nbf==1){
        /* Perform pattern matching to find where to replicate */
        plc_state->bestlag = PatternMatch(plc_state->hist);
        /* the replication begins after the template match */
        plc_state->bestlag += M; 
        
        /* Compute Scale Factor to Match Amplitude of Substitution Packet to that of Preceding Packet */
        sf = AmplitudeMatch(plc_state->hist, plc_state->bestlag);
        for (i=0;i<OLAL;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i];//*rcos[OLAL-i-1];
            if (val > 127.0) val= 127.0;
            if (val < -128.0) val=-128.0; 
            plc_state->hist[LHIST+i] = (int8_t)val;
        }
        
        for (;i<FS;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i]; 
            if (val > 127.0) val= 127.0;
            if (val < -128.0) val=-128.0; 
            plc_state->hist[LHIST+i] = (int8_t)val;
        }
        
        for (;i<FS+OLAL;i++){
            val = sf*plc_state->hist[plc_state->bestlag+i]*rcos[i-FS]
                 +   plc_state->hist[plc_state->bestlag+i]*rcos[OLAL-1-i+FS];
            if (val >  127.0) val= 127.0;
            if (val < -128.0) val=-128.0;
            plc_state->hist[LHIST+i] = (int8_t)val;
        }

        for (;i<FS+CVSDRT+OLAL;i++)
            plc_state->hist[LHIST+i] = plc_state->hist[plc_state->bestlag+i];
    } else {
        for (;i<FS;i++)
            plc_state->hist[LHIST+i] = plc_state->hist[plc_state->bestlag+i];
        for (;i<FS+CVSDRT+OLAL;i++)
            plc_state->hist[LHIST+i] = plc_state->hist[plc_state->bestlag+i];
    }
    for (i=0;i<FS;i++)
        out[i] = plc_state->hist[LHIST+i];
   
   /* shift the history buffer */
   for (i=0;i<LHIST+CVSDRT+OLAL;i++)
        plc_state->hist[i] = plc_state->hist[i+FS];

}

void btstack_cvsd_plc_good_frame(btstack_cvsd_plc_state_t *plc_state, int8_t *in, int8_t *out){
    int i;
    i=0;
    if (plc_state->nbf>0){
        for (i=0;i<CVSDRT;i++)
            out[i] = plc_state->hist[LHIST+i];
        for (;i<CVSDRT+OLAL;i++)
            out[i] = (int8_t)(plc_state->hist[LHIST+i]*rcos[i-CVSDRT] + in[i]*rcos[OLAL-1-i+CVSDRT]);
    }
    for (;i<FS;i++)
        out[i] = in[i];
    /*Copy the output to the history buffer */
    for (i=0;i<FS;i++)
        plc_state->hist[LHIST+i] = out[i];
    /* shift the history buffer */
    for (i=0;i<LHIST;i++)
        plc_state->hist[i] = plc_state->hist[i+FS];
    plc_state->nbf=0;
}


float CrossCorrelation(int8_t *x, int8_t *y){
    int   m;
    float num;
    float den;
    float Cn;
    float x2, y2;
    num=0;
    den=0;
    x2=0.0;
    y2=0.0;
    for (m=0;m<M;m++){
        num+=((float)x[m])*y[m];
        x2+=((float)x[m])*x[m];
        y2+=((float)y[m])*y[m];
    }
    den = (float)sqrt(x2*y2);
    Cn = num/den;
    return(Cn);
}

int PatternMatch(int8_t *y){
    int   n;
    float maxCn;
    float Cn;
    int   bestmatch;
    maxCn=-999999.0;  /* large negative number */
    bestmatch=0;
    for (n=0;n<N;n++){
        Cn = CrossCorrelation(&y[LHIST-M] /* x */, &y[n]); 
        if (Cn>maxCn){
            bestmatch=n;
            maxCn = Cn; 
        }
    }
    return(bestmatch);
}


float AmplitudeMatch(int8_t *y, int8_t bestmatch) {
    int   i;
    float sumx;
    float sumy;
    float sf;
    sumx = 0.0;
    sumy = 0.000001f;
    for (i=0;i<FS;i++){
        sumx += abs(y[LHIST-FS+i]);
        sumy += abs(y[bestmatch+i]);
    }
    sf = sumx/sumy;
    /* This is not in the paper, but limit the scaling factor to something reasonable to avoid creating artifacts */
    if (sf<0.75f) sf=0.75f;
    if (sf>1.2f) sf=1.2f;
    return(sf);
}
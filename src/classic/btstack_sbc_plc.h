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
 * sbc_plc.h
 *
 */

#ifndef __SBC_PLC_H
#define __SBC_PLC_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define FS 120          /* Frame Size */
#define N 256           /* 16ms - Window Length for pattern matching */ 
#define M 64            /* 4ms - Template for matching */
#define LHIST (N+FS-1)  /* Length of history buffer required */ 
#define SBCRT 36        /* SBC Reconvergence Time (samples) */
#define OLAL 16         /* OverLap-Add Length (samples) */

/* PLC State Information */
typedef struct sbc_plc_state {
    int16_t hist[LHIST+FS+SBCRT+OLAL];
    int16_t bestlag;
    int     nbf;
} sbc_plc_state_t;

#if defined __cplusplus
}
#endif

void sbc_plc_init(sbc_plc_state_t *plc_state);
void sbc_plc_bad_frame(sbc_plc_state_t *plc_state, int16_t *ZIRbuf, int16_t *out); 
void sbc_plc_good_frame(sbc_plc_state_t *plc_state, int16_t *in, int16_t *out);
uint8_t * sbc_plc_zero_signal_frame(void);

#endif // __SBC_PLC_H
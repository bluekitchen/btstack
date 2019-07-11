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
 * btstack_sbc_plc.h
 *
 */

#ifndef BTSTACK_SBC_PLC_H
#define BTSTACK_SBC_PLC_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define SBC_FS 120          /* SBC Frame Size */
#define SBC_N 512           /* 32ms - Window Length for pattern matching */ 
#define SBC_M 64            /* 4ms - Template for matching */
#define SBC_LHIST (SBC_N+SBC_FS-1)  /* Length of history buffer required */ 
#define SBC_RT 36           /* SBC Reconvergence Time (samples) */
#define SBC_OLAL 16         /* OverLap-Add Length (samples) */

/* PLC State Information */
typedef struct sbc_plc_state {
    int16_t hist[SBC_LHIST+SBC_FS+SBC_RT+SBC_OLAL];
    int16_t bestlag;
    int     nbf;

    // summary of processed good and bad frames
    int good_frames_nr;
    int bad_frames_nr;
    int frame_count;
    int max_consecutive_bad_frames_nr;
} btstack_sbc_plc_state_t;

// All int16 audio samples are in host endiness
void btstack_sbc_plc_init(btstack_sbc_plc_state_t *plc_state);
void btstack_sbc_plc_bad_frame(btstack_sbc_plc_state_t *plc_state, int16_t *ZIRbuf, int16_t *out); 
void btstack_sbc_plc_good_frame(btstack_sbc_plc_state_t *plc_state, int16_t *in, int16_t *out);
uint8_t * btstack_sbc_plc_zero_signal_frame(void);
void btstack_sbc_dump_statistics(btstack_sbc_plc_state_t * state);

#ifdef OCTAVE_OUTPUT
void btstack_sbc_plc_octave_set_base_name(const char * name);
#endif

#if defined __cplusplus
}
#endif

#endif // BTSTACK_SBC_PLC_H

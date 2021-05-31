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

/**
 * CVSD Packet Loss Concealment 
 *
 */

#ifndef BTSTACK_CVSD_PLC_H
#define BTSTACK_CVSD_PLC_H

#include <stdint.h>
#include "btstack_bool.h"

#if defined __cplusplus
extern "C" {
#endif

#define BTSTACK_CVSD_PLC_SAMPLE_FORMAT int16_t

#define CVSD_FS 60           /* CVSD Frame Size */
#define CVSD_N 256           /* Window Length for pattern matching */ 
#define CVSD_M 32            /* Template for matching */
#define CVSD_LHIST (CVSD_N+CVSD_FS-1)  /* Length of history buffer required */ 
#define CVSD_RT 18            /*  Reconvergence Time (samples) */
#define CVSD_OLAL 8         /*  OverLap-Add Length (samples) */

/* PLC State Information */
typedef struct cvsd_plc_state {
    int16_t hist[CVSD_LHIST+CVSD_FS+CVSD_RT+CVSD_OLAL];
    int16_t bestlag;
    int     nbf;

    // number processed samples
    uint32_t good_samples;

    // summary of processed good and bad frames
    int good_frames_nr;
    int bad_frames_nr;
    int zero_frames_nr;
    int frame_count;
    int max_consecutive_bad_frames_nr;
} btstack_cvsd_plc_state_t;

// All int16 audio samples are in host endiness
void btstack_cvsd_plc_init(btstack_cvsd_plc_state_t *plc_state);
void btstack_cvsd_plc_bad_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, int16_t *out); 
void btstack_cvsd_plc_good_frame(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, int16_t *in, int16_t *out);
void btstack_cvsd_plc_process_data(btstack_cvsd_plc_state_t * state, bool bad_frame, int16_t * in, uint16_t num_samples, int16_t * out);
void btstack_cvsd_dump_statistics(btstack_cvsd_plc_state_t * state);

// testing only
int   btstack_cvsd_plc_pattern_match(BTSTACK_CVSD_PLC_SAMPLE_FORMAT *y);
float btstack_cvsd_plc_amplitude_match(btstack_cvsd_plc_state_t *plc_state, uint16_t num_samples, BTSTACK_CVSD_PLC_SAMPLE_FORMAT *y, BTSTACK_CVSD_PLC_SAMPLE_FORMAT bestmatch);
BTSTACK_CVSD_PLC_SAMPLE_FORMAT btstack_cvsd_plc_crop_sample(float val);
float btstack_cvsd_plc_rcos(int index);

#ifdef OCTAVE_OUTPUT
void btstack_cvsd_plc_octave_set_base_name(const char * name);
#endif

#if defined __cplusplus
}
#endif

#endif // BTSTACK_CVSD_PLC_H

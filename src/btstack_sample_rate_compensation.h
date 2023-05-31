/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

/**
 * @title Sample rate compensation
 *
 * Prevents buffer over/under-run at the audio receiver by compensating for varying/different playback/receiving sample rates.
 *
 * Intended to measure the L2CAP packet sample rate and with the provided playback sample rate calculates a compensation ratio
 * which compensates for drift between playback and reception.
 *
 * Requires the audio interface to provide the current playback sample rate.
 *
 */

#ifndef BTSTACK_SAMPLE_RATE_COMPENSATION_H_
#define BTSTACK_SAMPLE_RATE_COMPENSATION_H_

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

#define FLOAT_TO_Q15(a)   ((signed)((a)*(UINT16_C(1)<<15)+0.5f))
#define FLOAT_TO_Q8(a)    ((signed)((a)*(UINT16_C(1)<<8)+0.5f))
#define FLOAT_TO_Q7(a)    ((signed)((a)*(UINT16_C(1)<<7)+0.5f))

#define Q16_TO_FLOAT(a)   ((float)(a)/(UINT32_C(1)<<16))
#define Q15_TO_FLOAT(a)   ((float)(a)/(UINT32_C(1)<<15))
#define Q8_TO_FLOAT(a)    ((float)(a)/(UINT32_C(1)<<8))
#define Q7_TO_FLOAT(a)    ((float)(a)/(UINT32_C(1)<<7))

//#define DEBUG_RATIO_CALCULATION

typedef struct {
    uint32_t count;         // 17bit are usable to count samples, recommended for max 96kHz
    uint32_t last;          // time stamp of last measurement
    uint32_t rate_state;    // unsigned Q17.8
    uint32_t ratio_state;   // unsigned Q16.16
    uint32_t constant_playback_sample_rate; // playback sample rate if no real one is available
#ifdef DEBUG_RATIO_CALCULATION
    double sample_rate;
    double ratio;
#endif
} btstack_sample_rate_compensation_t;

/**
 * @brief Initialize sample rate compensation
 * @param self pointer to current instance
 * @param time stamp at which to start sample rate measurement
 */
void btstack_sample_rate_compensation_init( btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms, uint32_t sample_rate, uint32_t ratioQ15 );

/**
 * @brief reset sample rate compensation
 * @param self pointer to current instance
 * @param time stamp at which to start sample rate measurement
 */
void btstack_sample_rate_compensation_reset( btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms );

/**
 * @brief update sample rate compensation with the current playback sample rate decoded samples
 * @param self pointer to current instance
 * @param time stamp for current samples
 * @param samples for current time stamp
 * @param playback sample rate
 */
uint32_t btstack_sample_rate_compensation_update( btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms, uint32_t samples, uint32_t playback_sample_rate );

/* API_END */

#if defined __cplusplus
}
#endif

#endif /* BTSTACK_SAMPLE_RATE_COMPENSATION_H_ */

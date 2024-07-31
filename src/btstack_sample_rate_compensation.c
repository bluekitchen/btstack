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

#define BTSTACK_FILE__ "btstack_sample_rate_comnpensation.h"

#include <inttypes.h>

#include "btstack.h"
#include "btstack_sample_rate_compensation.h"

void btstack_sample_rate_compensation_reset(btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms) {
    self->count = 0;
    self->last  = timestamp_ms;
}

void btstack_sample_rate_compensation_init(btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms, uint32_t sample_rate, uint32_t ratio_Q15) {
    btstack_sample_rate_compensation_reset( self, timestamp_ms );
    self->ratio_state = ratio_Q15 << 1; // Q15 to Q16 is one left shift
    self->rate_state = sample_rate << 8;
#ifdef DEBUG_RATIO_CALCULATION
    self->ratio = Q15_TO_FLOAT(ratioQ15);
    self->sample_rate = sample_rate;
#endif
}

uint32_t btstack_sample_rate_compensation_update(btstack_sample_rate_compensation_t *self, uint32_t timestamp_ms, uint32_t samples, uint32_t playback_sample_rate) {
    int32_t delta = timestamp_ms - self->last;
    if( delta >= 1000 ) {
        log_debug("current playback sample rate: %" PRId32 "", playback_sample_rate );

#ifdef DEBUG_RATIO_CALCULATION
        {
            double current_sample_rate = self->count*(1000./delta);
            double current_ratio = self->sample_rate/playback_sample_rate;

            // exponential weighted moving average
            const double rate_decay = 0.025;
            self->sample_rate += rate_decay * (current_sample_rate-self->sample_rate);

            // exponential weighted moving average
            static const double ratio_decay = 1.3;
            self->ratio += ratio_decay * (current_ratio-self->ratio);

            log_debug("current l2cap sample rate: %f (%d %d)", current_sample_rate, delta, self->count );
            log_debug("current ratio:             %f", current_ratio);
            log_debug("calculated ratio:          %f", self->ratio );
        }
#endif
        uint32_t fixed_rate = (self->count*(UINT16_C(1)<<15))/delta*1000;  // sample rate as Q15
        uint32_t fixed_ratio = (self->rate_state<<7)/playback_sample_rate; // Q15
        log_debug("fp current l2cap sample rate: %f (%" PRId32 " %" PRId32 ")", Q15_TO_FLOAT(fixed_rate), delta, self->count);

        self->last = timestamp_ms;
        self->count = 0;

        // fixed point exponential weighted moving average
        const int16_t rate_decay = FLOAT_TO_Q15(0.025f);
        uint32_t rate = self->rate_state >> 8; // integer part only
        self->rate_state += (rate_decay * (int32_t)((fixed_rate>>15)-rate)) >> (15-8); // Q8;

        // fixed point exponential weighted moving average
        const int16_t ratio_decay = FLOAT_TO_Q8(1.3f);
        self->ratio_state += (ratio_decay * (int32_t)((fixed_ratio<<1)-self->ratio_state)) >> (16-8); // Q16

        log_debug("fp current ratio :            %f", Q15_TO_FLOAT(fixed_ratio));
        log_debug("fp calculated ratio:          %f", Q16_TO_FLOAT(self->ratio_state));
        log_debug("scale factor Q16:             %" PRId32 "", self->ratio_state);
    }

    self->count += samples;
    return self->ratio_state;
}

/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef BTSTACK_RESAMPLE_H
#define BTSTACK_RESAMPLE_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/*
 *  btstack_resample.h
 *
 *  Linear resampling for 16-bit audio code samples using 16 bit/16 bit fixed point math
 */

#define BTSTACK_RESAMPLE_MAX_CHANNELS 2

typedef struct {
    uint32_t src_pos;
    uint32_t src_step;
    int16_t  last_sample[BTSTACK_RESAMPLE_MAX_CHANNELS];
    int      num_channels;
} btstack_resample_t;

/**
 * @brief Init resample context
 * @param num_channels
 * @returns btstack_audio implementation
 */
void btstack_resample_init(btstack_resample_t * context, int num_channels);

/**
 * @brief Set resampling factor
 * @param factor as fixed point value, identity is 0x10000
 */
void btstack_resample_set_factor(btstack_resample_t * context, uint32_t factor);

/**
 * @brief Process block of input samples
 * @note size of output buffer is not checked
 * @param input_buffer
 * @param num_frames
 * @param output_buffer
 * @returns number destination frames
 */
uint16_t btstack_resample_block(btstack_resample_t * context, const int16_t * input_buffer, uint32_t num_frames, int16_t * output_buffer);

#if defined __cplusplus
}
#endif

#endif

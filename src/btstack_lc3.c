/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

// *****************************************************************************
//
// LC3 Interface
//
// *****************************************************************************

#define BTSTACK_FILE__ "btstack_lc3.c"

#include "btstack_lc3.h"
#include "btstack_debug.h"

uint16_t btstack_lc3_frame_duration_in_us(btstack_lc3_frame_duration_t frame_duration){
    switch (frame_duration){
        case BTSTACK_LC3_FRAME_DURATION_7500US:
            return 7500;
        case BTSTACK_LC3_FRAME_DURATION_10000US:
            return 10000;
        default:
            return 0;
    }
}

uint16_t btstack_lc3_samples_per_frame(uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration){
    // 44.1 kHz uses longer iso interval and same number of samples as 48 khz
    if (sample_rate == 44100){
        sample_rate = 48000;
    }
    // assume sample rate is x 1000 hz
    return (sample_rate / 1000) * (btstack_lc3_frame_duration_in_us(frame_duration) / 100) / 10;
}

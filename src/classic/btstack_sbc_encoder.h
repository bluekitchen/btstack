/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * btstack_sbc_encoder.h
 *
 */

#ifndef __BTSTACK_SBC_ENCODER_H
#define __BTSTACK_SBC_ENCODER_H

#include <stdint.h>
#include "btstack_sbc.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    // private
    void * encoder_state;
    sbc_mode_t mode;
} btstack_sbc_encoder_state_t;

void btstack_sbc_encoder_init(btstack_sbc_encoder_state_t * state, sbc_mode_t mode, 
                        int blocks, int subbands, int allmethod, int sample_rate, int bitpool);

void btstack_sbc_encoder_process_data(int16_t * input_buffer);


uint8_t * btstack_sbc_encoder_sbc_buffer(void);
uint16_t  btstack_sbc_encoder_sbc_buffer_length(void);

int  btstack_sbc_encoder_num_audio_samples(void);
void btstack_sbc_encoder_dump_context(void);

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_SBC_ENCODER_H
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
 */

/**
 * @title LC3 EHIMA Implementation
 */

#ifndef BTSTACK_LC3_EHIMA_H
#define BTSTACK_LC3_EHIMA_H

#include <stdint.h>
#include "btstack_lc3.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    void *                          decoder;
    uint32_t                        sample_rate;
    btstack_lc3_frame_duration_t    frame_duration;
} lc3_decoder_ehima_t;

typedef struct {
    void *                          encoder;
    uint32_t                        sample_rate;
    btstack_lc3_frame_duration_t    frame_duration;
} lc3_encoder_ehima_t;

/**
 * Init LC3 Decoder Instance
 * @param context for EHIMA LC3 decoder
 */
const btstack_lc3_decoder_t * lc3_decoder_ehima_init_instance(lc3_decoder_ehima_t * context);

/**
 * Init LC3 Decoder Instance
 * @param context for EHIMA LC3 decoder
 */
const btstack_lc3_encoder_t * lc3_encoder_ehima_init_instance(lc3_encoder_ehima_t * context);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // BTSTACK_LC3_EHIMA_H

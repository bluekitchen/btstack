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
 * @title Adapter for Fraunhofer LC3plus Coddec
 * only uses suitable subset for lc3 testing
 */

#ifndef BTSTACK_LC3PLUS_FRAUNHOFER_H
#define BTSTACK_LC3PLUS_FRAUNHOFER_H

#include <stdint.h>
#include "btstack_lc3.h"

#if defined __cplusplus
extern "C" {
#endif

#ifdef HAVE_LC3PLUS

#include "LC3plus/lc3.h"

/* API_START */

typedef struct {
    btstack_lc3_frame_duration_t    frame_duration;
    uint16_t                        octets_per_frame;
    uint32_t                        sample_rate;
    // decoder must be 4-byte aligned
    uint8_t                         decoder[LC3PLUS_DEC_MAX_SIZE];
} btstack_lc3plus_fraunhofer_decoder_t;

typedef struct {
    btstack_lc3_frame_duration_t    frame_duration;
    uint16_t                        octets_per_frame;
    uint32_t                        sample_rate;
    // encoder must be 4-byte aligned
    uint8_t                         encoder[LC3PLUS_ENC_MAX_SIZE];
} btstack_lc3plus_fraunhofer_encoder_t;

/**
 * Init LC3 Decoder Instance
 * @param context for Fraunhofer LC3plus decoder
 */
const btstack_lc3_decoder_t * btstack_lc3plus_fraunhofer_decoder_init_instance(btstack_lc3plus_fraunhofer_decoder_t * context);

/**
 * Init LC3 Encoder Instance
 * @param context for Fraunhofer LC3plus encoder
 */
const btstack_lc3_encoder_t * btstack_lc3plus_fraunhofer_encoder_init_instance(btstack_lc3plus_fraunhofer_encoder_t * context);

/* API_END */

#endif /* HAVE_LC3PLUS */

#if defined __cplusplus
}
#endif
#endif // BTSTACK_LC3_PLUS_FRAUNHOFER_H

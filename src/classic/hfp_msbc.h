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
 
/**
 * @title HFP mSBC Encoder 
 *
 */

#ifndef HFP_MSBC_H
#define HFP_MSBC_H

#include "btstack_config.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 *
 */
void hfp_msbc_init(void);

/**
 *
 */
int  hfp_msbc_num_audio_samples_per_frame(void);

/**
 *
 */
int  hfp_msbc_can_encode_audio_frame_now(void);

/**
 * @param pcm_samples - complete audio frame of hfp_msbc_num_audio_samples_per_frame int16 samples
 */
void hfp_msbc_encode_audio_frame(int16_t * pcm_samples);

/**
 *
 */
int  hfp_msbc_num_bytes_in_stream(void);

/**
 *
 * @param buffer to store stream
 * @param size num bytes to read from stream
 */
void hfp_msbc_read_from_stream(uint8_t * buffer, int size);

/**
 * @brief De-Init HFP mSBC Codec
 */
void hfp_msbc_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif 

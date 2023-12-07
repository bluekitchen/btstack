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
 *  @brief Adapter for Bluedroid SBC implementation
 */

#ifndef BTSTACK_SBC_BLUEDROID_H
#define BTSTACK_SBC_BLUEDROID_H

#include "sbc_encoder.h"
#include "oi_codec_sbc.h"

#include "classic/btstack_sbc.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    btstack_sbc_mode_t       mode;
    SBC_ENC_PARAMS           params;
} btstack_sbc_encoder_bluedroid_t;

#define DECODER_DATA_SIZE (SBC_MAX_CHANNELS*SBC_MAX_BLOCKS*SBC_MAX_BANDS * 4 + SBC_CODEC_MIN_FILTER_BUFFERS*SBC_MAX_BANDS*SBC_MAX_CHANNELS * 2)

typedef struct {
    btstack_sbc_mode_t mode;

    OI_UINT32 bytes_in_frame_buffer;
    OI_CODEC_SBC_DECODER_CONTEXT decoder_context;

    uint8_t   frame_buffer[SBC_MAX_FRAME_LEN];
    int16_t   pcm_plc_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    int16_t   pcm_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    uint32_t  pcm_bytes;
    OI_UINT32 decoder_data[(DECODER_DATA_SIZE+3)/4];
    int       first_good_frame_found;
    int       h2_sequence_nr;
    uint16_t  msbc_bad_bytes;

    void (*handle_pcm_data)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context);
    void * callback_context;

    btstack_sbc_plc_state_t plc_state;

    // summary of processed good, bad and zero frames
    int good_frames_nr;
    int bad_frames_nr;
    int zero_frames_nr;

} btstack_sbc_decoder_bluedroid_t;

/* API_START */

/**
 * Init SBC Encoder Instance
 * @param context for Bluedroid SBC Encoder
 */
const btstack_sbc_encoder_t * btstack_sbc_encoder_bluedroid_init_instance(btstack_sbc_encoder_bluedroid_t * context);

/**
 * Init SBC Decoder Instance
 * @param context for Bluedroid SBC decoder
 */
const btstack_sbc_decoder_t * btstack_sbc_decoder_bluedroid_init_instance(btstack_sbc_decoder_bluedroid_t * context);

/* API_END */

// testing only
void btstack_sbc_decoder_bluedroid_test_set_plc_enabled(int plc_enabled);
void btstack_sbc_decoder_bluedroid_test_simulate_corrupt_frames(int period);

#if defined __cplusplus
}
#endif
#endif // BTSTACK_SBC_BLUEDROID_H

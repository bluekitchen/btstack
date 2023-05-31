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
 * @title HFP Audio Encoder
 * @brief Create SCO packet with H2 Synchronization Header and encoded audio samples
 */

#ifndef HFP_CODEC_H
#define HFP_CODEC_H

#include "btstack_config.h"

#include "hfp.h"    // HFP_CODEC_xxx

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
#include "btstack_sbc.h"
#endif

#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
#include "btstack_lc3_google.h"
#endif

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define SCO_FRAME_SIZE 60

// HFP H2

#define HFP_H2_SYNC_FRAME_SIZE 60

// HFP H2 Framing
typedef struct {
    uint8_t sequence_number;
} hfp_h2_framing_t;

/**
 * @brief Init HFP H2 Framing state
 * @param hfp_h2_framing
 */
void hfp_h2_framing_init(hfp_h2_framing_t * hfp_h2_framing);

/**
 * @brief Add next H2 Header
 * @param hfp_h2_framing
 * @param buffer [2]
 */
void hfp_h2_framing_add_header(hfp_h2_framing_t * hfp_h2_framing, uint8_t * buffer);


struct hfp_codec {
    // to allow for 24 byte USB payloads, we encode up to two SCO packets
    uint8_t sco_packet[2*SCO_FRAME_SIZE];
    uint16_t read_pos;
    uint16_t write_pos;
    uint16_t samples_per_frame;
    hfp_h2_framing_t h2_framing;
    void (*encode)(struct hfp_codec * hfp_codec, int16_t * pcm_samples);
#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
    btstack_sbc_encoder_state_t * msbc_encoder_context;
#endif
#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
    const btstack_lc3_encoder_t * lc3_encoder;
    void * lc3_encoder_context;
#endif
};

/* API_START */

typedef struct hfp_codec hfp_codec_t;

#ifdef ENABLE_HFP_WIDE_BAND_SPEECH
/**
 * @brief Initialize HFP Audio Codec for mSBC
 * @param hfp_codec
 * @param msbc_encoder_context for msbc encoder
 * @return status
 */
void hfp_codec_init_msbc(hfp_codec_t * hfp_codec, btstack_sbc_encoder_state_t * msbc_encoder_context);
#endif

#ifdef ENABLE_HFP_SUPER_WIDE_BAND_SPEECH
/**
 * @brief Initialize HFP Audio Codec for LC3-SWB
 * @param hfp_codec
 * @param lc3_encoder
 * @param lc3_encoder_context for lc3_encoder
 * @return status
 */
void hfp_codec_init_lc3_swb(hfp_codec_t * hfp_codec, const btstack_lc3_encoder_t * lc3_encoder, void * lc3_encoder_context);
#endif

/**
 * @brief Get number of audio samples per HFP SCO frame
 * @param hfp_codec
 * @return num audio samples per 7.5ms SCO packet
 */
uint16_t hfp_codec_num_audio_samples_per_frame(const hfp_codec_t * hfp_codec);

/**
 * @brief Checks if next frame can be encoded
 * @param hfp_codec
 */
bool hfp_codec_can_encode_audio_frame_now(const hfp_codec_t * hfp_codec);

/**
 * Get number of bytes ready for sending
 * @param hfp_codec
 * @return num bytes ready for current packet
 */
uint16_t hfp_codec_num_bytes_available(const hfp_codec_t * hfp_codec);

/**
 * Encode audio samples for HFP SCO packet
 * @param hfp_codec
 * @param pcm_samples - complete audio frame of hfp_msbc_num_audio_samples_per_frame int16 samples
 */
void hfp_codec_encode_audio_frame(hfp_codec_t * hfp_codec, int16_t * pcm_samples);

/**
 * Read from stream into SCO packet buffer
 * @param hfp_codec
 * @param buffer to store stream
 * @param size num bytes to read from stream
 */
void hfp_codec_read_from_stream(hfp_codec_t * hfp_codec, uint8_t * buffer, uint16_t size);

/**
 * @param hfp_codec
 */
void hfp_codec_deinit(hfp_codec_t * hfp_codec);

/* API_END */

#if defined __cplusplus
}
#endif

#endif 

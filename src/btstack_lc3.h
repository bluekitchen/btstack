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
 * @title LC3 Interface
 *
 * Interface for LC3 implementations
 *
 */

#ifndef BTSTACK_LC3_H
#define BTSTACK_LC3_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef enum {
    BTSTACK_LC3_FRAME_DURATION_10000US,
    BTSTACK_LC3_FRAME_DURATION_7500US
} btstack_lc3_frame_duration_t;

typedef struct {

    /**
     * Configure Decoder
     * @param context
     * @param sample_rate
     * @param frame_duration
     * @param context
     * @return status
     */
    uint8_t (*configure)(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration);

    /**
     * Get number of octets per LC3 frame for bitrate
     * @param context
     * @param bitrate
     * @return octets_per_frame
     */
    uint16_t (*get_number_octets_for_bitrate)(void * context, uint32_t bitrate);

    /**
     * Get number of samples per LC3 frame
     * @param context
     * @return number of samples
     */
    uint16_t (*get_number_samples_per_frame)(void * context);

    /**
     * Decode LC3 Frame into signed 16-bit samples
     * @param context
     * @param bytes
     * @param byte_count
     * @param BFI Bad Frame Indication flags
     * @param pcm_out buffer for decoded PCM samples
     * @param stride count between two consecutive samples
     * @param BEC_detect Bit Error Detected flag
     * @return status
     */
     uint8_t (*decode_signed_16)(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI,
                       int16_t* pcm_out, uint16_t stride, uint8_t * BEC_detect);

    /**
     * Decode LC3 Frame into signed 24-bit samples, sign-extended to 32-bit
     * @param context
     * @param bytes
     * @param byte_count
     * @param BFI Bad Frame Indication flags
     * @param pcm_out buffer for decoded PCM samples
     * @param stride count between two consecutive samples
     * @param BEC_detect Bit Error Detected flag
     * @return status
     */
    uint8_t (*decode_signed_24)(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI,
                                int32_t* pcm_out, uint16_t stride, uint8_t * BEC_detect);

} btstack_lc3_decoder_t;

typedef struct {
    /**
     * Configure Decoder
     * @param context
     * @param sample_rate
     * @param frame_duration
     * @param context
     * @return status
     */
    uint8_t (*configure)(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration);

    /**
     * Get bitrate from number of octets per LC3 frame
     * @param context
     * @param number_of_octets
     * @return bitrate
     */
    uint32_t (*get_bitrate_for_number_of_octets)(void * context, uint16_t number_of_octets);

    /**
     * Get number of samples per LC3 frame
     * @param context
    * @return number of samples
    */
    uint16_t (*get_number_samples_per_frame)(void * context);

    /**
     * Encode LC3 Frame with 16-bit signed PCM samples
     * @param context
     * @param pcm_in buffer for decoded PCM samples
     * @param stride count between two consecutive samples
     * @param bytes
     * @param byte_count
     * @return status
     */
    uint8_t (*encode_signed_16)(void * context, const int16_t* pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count);

    /**
     * Encode LC3 Frame with 24-bit signed PCM samples, sign-extended to 32 bit
     * @param context
     * @param pcm_in buffer for decoded PCM samples
     * @param stride count between two consecutive samples
     * @param bytes
     * @param byte_count
     * @return status
     */
    uint8_t (*encode_signed_24)(void * context, const int32_t* pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count);

} btstack_lc3_encoder_t;

/* API_END */

#if defined __cplusplus
}
#endif
#endif // LC3_H

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

/**
 * @title base64 Decoder
 *
 */

#ifndef BTSTACK_BASE_64_DECODER_H
#define BTSTACK_BASE_64_DECODER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t pos;
    uint8_t value;
} btstack_base64_decoder_t;

/* API_START */

#define BTSTACK_BASE64_DECODER_MORE     -1
#define BTSTACK_BASE64_DECODER_COMPLETE -2
#define BTSTACK_BASE64_DECODER_INVALID  -3
#define BTSTACK_BASE64_DECODER_FULL     -4

/**
 * @brief Initialize base64 decoder
 * @param context
 */
void btstack_base64_decoder_init(btstack_base64_decoder_t * context);

/**
 * @brief Decode single byte
 * @brief context
 * @returns value, or BTSTACK_BASE64_DECODER_MORE, BTSTACK_BASE64_DECODER_COMPLETE, BTSTACK_BASE64_DECODER_INVALID
 */
int  btstack_base64_decoder_process_byte(btstack_base64_decoder_t * context, uint8_t c);

/**
 * @brief Decode block
 * @brief input_data base64 encoded message
 * @brief input_size
 * @breif output_buffer
 * @brief output_max_size
 * @return output_size or BTSTACK_BASE64_DECODER_INVALID, BTSTACK_BASE64_DECODER_FULL
 */
int btstack_base64_decoder_process_block(const uint8_t * input_data, uint32_t input_size, uint8_t * output_buffer, uint32_t output_max_size);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_BASE_64_DECODER_H
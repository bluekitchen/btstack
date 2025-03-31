/*
 * Copyright (C) 2025 BlueKitchen GmbH
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
 * @title base64 Encoder
 *
 */

#ifndef BTSTACK_BASE_64_ENCODER_H
#define BTSTACK_BASE_64_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "btstack_bool.h"
#include "btstack_defines.h"    // ssize_t on Winddows

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int bytes;
    uint8_t carry;
} btstack_base64_state_t;

/**
 * @brief Base64 stream encoder init
 * @brief state to be initialized
 *
 * Initializes the given streaming base64 encoder state
 */
void btstack_base64_encoder_stream_init( btstack_base64_state_t *state );

/**
 * @brief Base64 streaming encoder
 * @brief src Data to be encoded
 * @brief len Length of the data to be encoded
 * @brief out Pointer to encoded data
 * @brief out_len Pointer to output length variable, will be updated to the actually written bytes
 * @return -1 on error, the encoded number of bytes otherwise
 *
 * Encodes the given data block into base64 not exceeding out_len. If a value bigger than out_len
 * is returned either through out_len or as return value an overflow would have happened and the function
 * did nothing. On error no data is written.
 */
ssize_t btstack_base64_encoder_stream( btstack_base64_state_t *state, const void *src, size_t len, void *out, size_t *out_len );

/**
 * @brief Base64 stream finalizing
 * @brief out Pointer to encoded data
 * @brief out_len Pointer to output length variable, will be updated to the actually written bytes
 * @return -1 on error, the encoded number of bytes otherwise
 *
 * Finishes the given base64 state and adds padding if required, not exceeding out_len. If a value bigger than out_len
 * is returned either through out_len or as return value an overflow would have happened and the function
 * did nothing. On error no data is written.
 * On success the returned buffer is nul terminated to make it easier to use as a C string.
 */
ssize_t btstack_base64_encoder_stream_final( btstack_base64_state_t *state, void *out, size_t *out_len );

/**
 * @brief Base64 block encoder
 * @brief src Data to be encoded
 * @brief len Length of the data to be encoded
 * @brief out Pointer to encoded data
 * @brief out_len Pointer to output length variable
 * @return true on success
 *
 * Returned buffer is nul terminated to make it easier to use as a C string.
 * The nul terminator is not included in out_len.
 */
bool btstack_base64_encoder_process_block(const void *src, size_t len, void *out, size_t *out_len);

#ifdef __cplusplus
};
#endif

#endif // BTSTACK_BASE_64_ENCODER_H


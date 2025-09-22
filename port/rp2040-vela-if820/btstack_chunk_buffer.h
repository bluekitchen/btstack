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
 * @brief Chunk buffer
 *
 * A chunk buffer allows for sequential reading of a buffer in chunks
 *
 * Usage: setup buffer with btstack_chunk_buffer_init, then read with btstack_chunk_buffer_read until empty
 */

#ifndef BTSTACK_CHUNK_BUFFER_H
#define BTSTACK_CHUNK_BUFFER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    uint8_t * chunk_buffer;
    uint32_t  chunk_size;
} btstack_chunk_buffer_t;

/**
 * @brief Initialize chunk buffer
 * @param context
 * @param chunk_buffer
 * @param chunk_size
 */
void btstack_chunk_buffer_init(btstack_chunk_buffer_t * context, uint8_t * chunk_buffer, uint32_t chunk_size);

/**
 * @brief Get number of bytes available in chunk buffer
 * @param context
 * @return number of bytes available
 */
uint32_t btstack_chunk_buffer_bytes_available(btstack_chunk_buffer_t * context);

/**
 * @brief Read from chunk buffer
 * @param context
 * @param buffer
 * @param buffer_size
 * @return number of bytes read
 */
uint32_t btstack_chunk_buffer_read(btstack_chunk_buffer_t * context, uint8_t * buffer, uint32_t buffer_size);

#if defined __cplusplus
}
#endif
#endif // BTSTACK_CHUNK_BUFFER_H

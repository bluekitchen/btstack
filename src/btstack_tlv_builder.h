/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 *  @brief TODO
 */

#ifndef BTSTACK_TLV_BUILDER_H
#define BTSTACK_TLV_BUILDER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif


/* API_START */

typedef struct {
    uint8_t * buffer;
    uint16_t size;
    uint16_t len_pos;
    uint16_t write_pos;
} btstack_tlv_builder_context_t;

/**
 * @brief Initialize Length/Tag/Value builder with buffer and size for 8-bit tag and length fields
 *
 * @param context
 * @param buffer
 * @param size
 */
void btstack_tlv_builder_init(btstack_tlv_builder_context_t * context, uint8_t * buffer, uint16_t size);

/**
 * @brief Query remaining space in buffer
 *
 * @param context
 * @return number of bytes that can be added
 */
uint16_t btstack_tlv_builder_remaining_space(btstack_tlv_builder_context_t * context);

/**
 * @brief Get constructed event length
 * @param context
 * @return number of bytes in event
 */
uint16_t btstack_tlv_builder_get_length(btstack_tlv_builder_context_t * context);

/**
 * @brief Add tag
 * @param context
 * @return number of bytes in event
 */
void btstack_tlv_builder_add_tag(btstack_tlv_builder_context_t * context, uint8_t tag);

/**
 * @bbrief Add uint8_t to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_08(btstack_tlv_builder_context_t * context, uint8_t value);

/**
 * @bbrief Add uint16_t value to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_big_endian_16(btstack_tlv_builder_context_t * context, uint16_t value);

/**
 * @bbrief Add 24 bit from uint32_t byte to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_big_endian_24(btstack_tlv_builder_context_t * context, uint32_t value);

/**
 * @bbrief Add uint32_t to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_big_endian_32(btstack_tlv_builder_context_t * context, uint32_t value);

/**
 * @bbrief Add byte sequence to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_bytes(btstack_tlv_builder_context_t * context, const uint8_t * data, uint16_t length);

/**
 * @bbrief Add string to current tag
 * @param context
 * @param value
 */
void btstack_tlv_builder_add_string(btstack_tlv_builder_context_t * context, const char * text);

#if defined __cplusplus
}
#endif
#endif // BTSTACK_TLV_BUILDER_H

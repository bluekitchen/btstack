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

#define BTSTACK_FILE__ "btstack_ltv_builder.c"

#include "btstack_ltv_builder.h"

#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdint.h>
#include <string.h>

static void btstack_ltv_builder_increase_tag(btstack_ltv_builder_context_t * context, uint16_t size){
    btstack_assert((context->write_pos + size) < context->size);
    context->write_pos += size;
    btstack_assert((((uint16_t)context->buffer[context->len_pos]) + size) <= 255);
    context->buffer[context->len_pos] += size;
}

void btstack_ltv_builder_init(btstack_ltv_builder_context_t * context, uint8_t * buffer, uint16_t size){
    btstack_assert(buffer != NULL);
    context->buffer = buffer;
    context->size = size;
    context->write_pos = 0;
}

uint16_t btstack_ltv_builder_remaining_space(btstack_ltv_builder_context_t * context){
    return context->size - context->write_pos;
}

uint16_t btstack_ltv_builder_get_length(btstack_ltv_builder_context_t * context){
    return context->write_pos;
}

void btstack_ltv_builder_add_tag(btstack_ltv_builder_context_t * context, uint8_t tag){
    // update write pos
    btstack_assert((context->write_pos + 2) < context->size);
    // track len field position
    context->len_pos = context->write_pos;
    // add empty tag
    context->buffer[context->write_pos++] = 0;
    context->buffer[context->write_pos++] = tag;
}

void btstack_ltv_builder_add_08(btstack_ltv_builder_context_t * context, uint8_t value){
    btstack_assert(context->write_pos != 0);
    uint16_t write_pos = context->write_pos;
    btstack_ltv_builder_increase_tag(context, 1);
    context->buffer[write_pos] = value;
}

void btstack_ltv_builder_add_little_endian_16(btstack_ltv_builder_context_t * context, uint16_t value) {
    btstack_assert(context->write_pos != 0);
    uint16_t write_pos = context->write_pos;
    btstack_ltv_builder_increase_tag(context, 2);
    little_endian_store_16(context->buffer, write_pos, value);
}

void btstack_ltv_builder_add_little_endian_24(btstack_ltv_builder_context_t * context, uint32_t value) {
    btstack_assert(context->write_pos != 0);
    uint16_t write_pos = context->write_pos;
    btstack_ltv_builder_increase_tag(context, 3);
    little_endian_store_24(context->buffer, write_pos, value);
}

void btstack_ltv_builder_add_little_endian_32(btstack_ltv_builder_context_t * context, uint32_t value) {
    btstack_assert(context->write_pos != 0);
    uint16_t write_pos = context->write_pos;
    btstack_ltv_builder_increase_tag(context, 4);
    little_endian_store_32(context->buffer, write_pos, value);
}

void btstack_ltv_builder_add_bytes(btstack_ltv_builder_context_t * context, const uint8_t * data, uint16_t length) {
    btstack_assert(context->write_pos != 0);
    btstack_assert(data != NULL);
    btstack_ltv_builder_increase_tag(context, length);
    memcpy(&context->buffer[context->write_pos], data, length);
    context->write_pos += length;
}

void btstack_ltv_builder_add_string(btstack_ltv_builder_context_t * context, const char * text) {
    uint16_t length = (uint16_t)strlen(text);
    btstack_ltv_builder_add_bytes(context, (const uint8_t *) text, length);
}

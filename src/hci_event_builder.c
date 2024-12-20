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

#define BTSTACK_FILE__ "hci_event_builder.c"

#include "hci_event_builder.h"

#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdint.h>
#include <stddef.h>

static void hci_event_builder_increment_pos(hci_event_builder_context_t * context, uint16_t size){
    btstack_assert((context->pos + size) <= context->size);
    context->pos += size;
    context->buffer[1] = (uint8_t) (context->pos - 2);
}

void hci_event_builder_init(hci_event_builder_context_t * context, uint8_t * buffer, uint16_t size, uint8_t event_type, uint8_t subevent_type){
    uint16_t subevent_overhead = subevent_type == 0 ? 0 : 1;
    btstack_assert(buffer != NULL);
    btstack_assert(size + subevent_overhead >= 2);
    context->buffer = buffer;
    context->size = size;
    context->pos = 0;
    hci_event_builder_add_08(context, event_type);
    context->pos += 1;
    if (subevent_type != 0){
        hci_event_builder_add_08(context, subevent_type);
    }
}

uint16_t hci_event_builder_remaining_space(hci_event_builder_context_t * context){
    return context->size - context->pos;
}

uint16_t hci_event_builder_get_length(hci_event_builder_context_t * context){
    return context->pos;
}

void hci_event_builder_add_08(hci_event_builder_context_t * context, uint8_t value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 1);
    context->buffer[pos] = value;
}

void hci_event_builder_add_16(hci_event_builder_context_t * context, uint16_t value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 2);
    little_endian_store_16(context->buffer, pos, value);
}

void hci_event_builder_add_24(hci_event_builder_context_t * context, uint32_t value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 3);
    little_endian_store_24(context->buffer, pos, value);
}

void hci_event_builder_add_32(hci_event_builder_context_t * context, uint32_t value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 4);
    little_endian_store_32(context->buffer, pos, value);
}

void hci_event_builder_add_64(hci_event_builder_context_t * context, const uint8_t * value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 8);
    reverse_64(value, &context->buffer[pos]);
}

void hci_event_builder_add_128(hci_event_builder_context_t * context, const uint8_t * value){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 16);
    reverse_128(value, &context->buffer[pos]);
}

void hci_event_builder_add_bd_addr(hci_event_builder_context_t * context, bd_addr_t addr){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, 6);
    reverse_bd_addr(addr, &context->buffer[pos]);
}

void hci_event_builder_add_con_handle(hci_event_builder_context_t * context, hci_con_handle_t con_handle){
    hci_event_builder_add_16(context, (uint16_t) con_handle);
}

void hci_event_builder_add_string(hci_event_builder_context_t * context, const char * text){
    uint16_t length = (uint16_t) strlen(text);
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, length + 2);
    context->buffer[pos++] = length;
    memcpy(&context->buffer[pos], text, length);
    pos += length;
    context->buffer[pos] = 0;
}

void hci_event_builder_add_bytes(hci_event_builder_context_t * context, const uint8_t * data, uint16_t length){
    uint16_t pos = context->pos;
    hci_event_builder_increment_pos(context, length);
    memcpy(&context->buffer[pos], data, length);
}

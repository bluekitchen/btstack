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
 * OBEX Iterator
 *
 */

#ifndef OBEX_ITERATOR_H
#define OBEX_ITERATOR_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btstack_bool.h"


/* API_START */

typedef struct obex_iterator {
    const uint8_t * data;
    uint16_t  offset;
    uint16_t  length;
    uint16_t  header_size;
    uint16_t  data_size;
    bool valid;
} obex_iterator_t;

// OBEX packet header iterator
void obex_iterator_init_with_request_packet(obex_iterator_t *context, const uint8_t * packet_data, uint16_t packet_len);
void obex_iterator_init_with_response_packet(obex_iterator_t *context, uint8_t request_opcode, const uint8_t * packet_data, uint16_t packet_len);
int  obex_iterator_has_more(const obex_iterator_t * context);
void obex_iterator_next(obex_iterator_t * context);

// OBEX packet header access functions
// @note BODY/END-OF-BODY headers might be incomplete
uint8_t         obex_iterator_get_hi(const obex_iterator_t * context);
uint8_t         obex_iterator_get_data_8(const obex_iterator_t * context);
uint32_t        obex_iterator_get_data_32(const obex_iterator_t * context);
uint32_t        obex_iterator_get_data_len(const obex_iterator_t * context);
const uint8_t * obex_iterator_get_data(const obex_iterator_t * context);

/* API_END */

// debug
void            obex_dump_packet(uint8_t request_opcode, uint8_t * packet, uint16_t size);

#if defined __cplusplus
}
#endif
#endif

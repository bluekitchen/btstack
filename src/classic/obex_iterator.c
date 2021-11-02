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

#define BTSTACK_FILE__ "obex_iterator.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdlib.h>

#ifdef ENABLE_OBEX_DUMP
#include <stdio.h>
#endif

#include "hci_cmd.h"
#include "btstack_debug.h"
#include "hci.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_event.h"

#include "classic/obex.h"
#include "classic/obex_iterator.h"

static int obex_packet_header_offset_for_opcode(uint8_t opcode){
    switch (opcode){
        case OBEX_OPCODE_SETPATH:
            return 5;
        case OBEX_OPCODE_CONNECT:
            return 7;
        default:
            return 3;
    }
}

static void obex_iterator_reset(obex_iterator_t *context){
    memset(context, 0, sizeof(obex_iterator_t));
    context->valid = false;
}

// check if num bytes are available, and if not, invalidate iterator
static bool obex_iterator_check(obex_iterator_t *context, uint32_t num_bytes){
    if ((context->offset + num_bytes) > context->length){
        context->valid = false;
    }
    return context->valid;
}

static void obex_iterator_prepare(obex_iterator_t *context){
    if (obex_iterator_check(context, 1) == false) return;
    const uint8_t * data = context->data + context->offset;
    uint8_t encoding = data[0] >> 6;
    switch (encoding){
        case 0:
        case 1:
            // 16-bit length info prefixed
            if (obex_iterator_check(context, 3) == false) return;
            context->data_size   = big_endian_read_16(data, 1);
            context->header_size = 3;
            // length includes the header, remove from data size
            if (context->data_size < 3){
                context->valid = false;
                return;
            }
            context->data_size -= 3;
            break;
        case 2:
            // 8-bit value
            context->data_size   = 1;
            context->header_size = 1;
            break;
        case 3:
            // 32-bit value
            context->data_size   = 4;
            context->header_size = 1;
            break;
        default:
            // avoid compiler warning about unused cases (encoding in [0..3])
            break;
    }
    (void) obex_iterator_check(context, context->header_size + context->data_size);
}

static void obex_iterator_init(obex_iterator_t *context, int header_offset, const uint8_t * packet_data, uint16_t packet_len){
    if (header_offset >= packet_len) return;
    context->data   = packet_data + header_offset;
    context->length = packet_len  - header_offset;
    context->valid = true;
    obex_iterator_prepare(context);
}

void obex_iterator_init_with_request_packet(obex_iterator_t *context, const uint8_t * packet_data, uint16_t packet_len){
    obex_iterator_reset(context);
    if (packet_len == 0) return;
    int header_offset = obex_packet_header_offset_for_opcode(packet_data[0]);
    obex_iterator_init(context, header_offset, packet_data, packet_len);
}

void obex_iterator_init_with_response_packet(obex_iterator_t *context, uint8_t request_opcode, const uint8_t * packet_data, uint16_t packet_len){
    obex_iterator_reset(context);
    int header_offset = (request_opcode == OBEX_OPCODE_CONNECT) ? 7 : 3;
    obex_iterator_init(context, header_offset, packet_data, packet_len);
}

int  obex_iterator_has_more(const obex_iterator_t * context){
    return context->valid;
}

void obex_iterator_next(obex_iterator_t * context){
    if (context->valid == false) return;
    context->offset += context->header_size + context->data_size;
    obex_iterator_prepare(context);
}

// OBEX packet header access functions

// @note BODY/END-OF-BODY headers might be incomplete
uint8_t obex_iterator_get_hi(const obex_iterator_t * context){
    return context->data[context->offset];
}
uint8_t obex_iterator_get_data_8(const obex_iterator_t * context){
    return context->data[context->offset+1];
}
uint32_t obex_iterator_get_data_32(const obex_iterator_t * context){
    return big_endian_read_32(context->data, context->offset + 1);
}
uint32_t obex_iterator_get_data_len(const obex_iterator_t * context){
    return context->data_size;
}

const uint8_t * obex_iterator_get_data(const obex_iterator_t * context){
    return &context->data[context->offset + context->header_size];
}

#ifdef ENABLE_OBEX_DUMP
void obex_dump_packet(uint8_t request_opcode, uint8_t * packet, uint16_t size){
    obex_iterator_t it;
    printf("OBEX Opcode: 0x%02x\n", request_opcode);
    int header_offset = (request_opcode == OBEX_OPCODE_CONNECT) ? 7 : 3;
    printf("OBEX Header: ");
    printf_hexdump(packet, header_offset);    
    for (obex_iterator_init_with_response_packet(&it, request_opcode, packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
        uint8_t hi = obex_iterator_get_hi(&it);
        printf("HI: %x - ", hi);
        uint8_t encoding = hi >> 6;
        uint16_t len;
        switch (encoding){
            case 0:
            case 1:
                len = obex_iterator_get_data_len(&it);
                printf_hexdump(obex_iterator_get_data(&it), len);
                break;
            case 2:
                printf("%02x\n", obex_iterator_get_data_8(&it));
                break;
            case 3:
                printf("%08x\n", (int) obex_iterator_get_data_32(&it));
                break;
            default:
                btstack_assert(false);
                break;
        }

    }
}
#endif

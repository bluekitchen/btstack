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

#define BTSTACK_FILE__ "btstack_base64_encoder.c"

#include "btstack_debug.h"
#include "btstack_base64_encoder.h"

#include <string.h>
#include "btstack_util.h"

static const uint8_t base64_table_enc_6bit[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/";

static inline void enc_loop_32_inner( const uint8_t **s, uint8_t **o ) {
    uint32_t src = big_endian_read_24(*s, 0);

    const uint_fast8_t index0 = (src >> 18) & 0x3FU;
    const uint_fast8_t index1 = (src >> 12) & 0x3FU;
    const uint_fast8_t index2 = (src >>  6) & 0x3FU;
    const uint_fast8_t index3 = (src >>  0) & 0x3FU;

    (*o)[0] = base64_table_enc_6bit[index0];
    (*o)[1] = base64_table_enc_6bit[index1];
    (*o)[2] = base64_table_enc_6bit[index2];
    (*o)[3] = base64_table_enc_6bit[index3];

    *s += 3;
    *o += 4;
}

static void enc_loop_32( const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen) {
    if(*slen < 4) {
        return;
    }

    size_t rounds = (*slen - 1) / 3;
    *slen -= rounds * 3;
    *olen += rounds * 4;

    do {
        if(rounds >= 8) {
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            rounds -= 8;
            continue;
        }
        if( rounds >= 4 ) {
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            rounds -= 4;
            continue;
        }
        if( rounds >= 2 ) {
            enc_loop_32_inner(s, o);
            enc_loop_32_inner(s, o);
            rounds -= 2;
            continue;
        }
        enc_loop_32_inner(s, o);
        break;
    } while( rounds > 0 );
}

void btstack_base64_encoder_stream_init( btstack_base64_state_t *state ) {
    state->bytes = 0;
    state->carry = 0;
}

ssize_t btstack_base64_encoder_stream_final( btstack_base64_state_t *state, void *out, size_t *out_len ) {
    if( state == NULL ) {
        return -1;
    }
    if( out == NULL ) {
        return -1;
    }
    if( out_len == NULL ) {
        return -1;
    }
    size_t olen = *out_len;
    uint8_t *o = (uint8_t*)out;
    if(state->bytes == 1 ) {
        if( olen < 4 ) {
            *out_len = 3;
            return 3;
        }
        *o++ = base64_table_enc_6bit[state->carry];
        *o++ = '=';
        *o++ = '=';
        *o = 0;
        *out_len = 3;
        return 3;
    }
    if(state->bytes == 2 ) {
        if( olen < 3 ) {
            *out_len = 2;
            return 2;
        }
        *o++ = base64_table_enc_6bit[state->carry];
        *o++ = '=';
        *o = 0;
        *out_len = 2;
        return 2;
    }
    if( olen < 1 ) {
        *out_len = 0;
        return 0;
    }

    *o = 0; // terminating null character
    *out_len = 0;
    return 0;
}

ssize_t btstack_base64_encoder_stream( btstack_base64_state_t *state, const void *src, size_t len, void *out, size_t *out_len ) {
    const uint8_t *s = (const uint8_t*)src;
    uint8_t *o = (uint8_t*)out;

    if( out_len == NULL ) {
        return -1;
    }

    size_t olen = (len/3)*4;
    if(olen > *out_len) {
        *out_len = olen;
        return olen;
    }

    // nothing to do
    if( len == 0 ) {
        return 0;
    }
    if( src == NULL ) {
        return -1;
    }
    if( out == NULL ) {
        return -1;
    }
    if( state == NULL ) {
        return -1;
    }
    olen = 0;
    size_t slen = len;
    btstack_base64_state_t st;
    st.bytes = state->bytes;
    st.carry = state->carry;

    switch(st.bytes) {
        for(;;)
        {
        case 0:
            enc_loop_32(&s, &slen, &o, &olen);
            if(slen-- == 0) {
                break;
            }
            *o++ = base64_table_enc_6bit[*s >> 2];
            st.carry = (*s++ << 4) & 0x30;
            st.bytes++;
            olen += 1;
            /* fall through */
        case 1:
            if( slen-- == 0) {
                break;
            }
            *o++ = base64_table_enc_6bit[st.carry | (*s >> 4)];
            st.carry = (*s++ << 2) & 0x3C;
            st.bytes++;
            olen += 1;
            /* fall through */
        case 2:
            if( slen-- == 0) {
                break;
            }
            *o++ = base64_table_enc_6bit[st.carry | (*s >> 6)];
            *o++ = base64_table_enc_6bit[*s++ & 0x3F];
            st.bytes = 0;
            olen += 2;
            /* fall through */
        default:
            continue;
        }
    }
    state->bytes = st.bytes;
    state->carry = st.carry;
    *out_len = olen;
    return olen;
}

bool btstack_base64_encoder_process_block( const void *src, size_t len, void *out, size_t *out_len ) {
    uint8_t *o = out;
    size_t sum = 0;
    size_t block_len = *out_len;
    size_t olen = block_len;
    btstack_base64_state_t state;
    btstack_base64_encoder_stream_init( &state );
    ssize_t ret = btstack_base64_encoder_stream( &state, src, len, o, &olen );
    bool ok = ((ret>0) && !(olen>block_len));
    if( ok ) {
        // room for remaining data
        o += olen;
        sum += olen;
        block_len -= olen;
        olen = block_len;
        ret = btstack_base64_encoder_stream_final( &state, o, &olen );
        ok = ((ret>=0) && (olen<block_len));
    }
    if( ok ) {
        sum += olen;
        *out_len = sum;
    }
    return ok;
}


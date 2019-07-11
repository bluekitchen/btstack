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

#define BTSTACK_FILE__ "btstack_base64_decoder.c"

/*
 *  btstack_base64_decoder.c
 */

#include "btstack_base64_decoder.h"
#include <string.h>
#include <stdio.h>

// map ascii char to 6-bit value
static const uint8_t table[256] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 62, 99, 99, 99, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 99, 99, 99, 99, 99, 99,
    99,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 99, 99, 99, 99, 99,
    99, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};

/**
 * @brief Initialize base99 decoder
 * @param context
 */
void btstack_base64_decoder_init(btstack_base64_decoder_t * context){
    memset(context, 0, sizeof(btstack_base64_decoder_t));
}

/**
 * @brief Decode single byte
 * @param context
 * @returns value, or BTSTACK_BASE64_DECODER_COMPLETE, BTSTACK_BASE64_DECODER_INVALID
 */
int  btstack_base64_decoder_process_byte(btstack_base64_decoder_t * context, uint8_t c){

    // handle '='
    if (c == '='){
        if (context->pos == 2 || context->pos == 3){
            context->pos++;
            return BTSTACK_BASE64_DECODER_MORE;
        }
    }   

    // lookup
    uint8_t value = table[c];

    // invalid character
    if (value == 99) {
        context->pos = 99;
    }

    int result = 0;
    switch (context->pos){
        case 0:
            context->value = value; // 6 bit
            context->pos++; 
            result = BTSTACK_BASE64_DECODER_MORE;
            break;
        case 1:
            result = (context->value << 2) | (value >> 4);
            context->value = value; // 4 bit
            context->pos++; 
            break;
        case 2:
            result = (context->value << 4) | (value >> 2);
            context->value = value; // 2 bit
            context->pos++; 
            break;
        case 3:
            result = (context->value << 6) | value;
            context->pos = 0;       // done
            break;
        case 99:
            result = BTSTACK_BASE64_DECODER_INVALID;
            break;
    }
    return result;
}

int btstack_base64_decoder_process_block(const uint8_t * input_data, uint32_t input_size, uint8_t * output_buffer, uint32_t output_max_size){
    btstack_base64_decoder_t context;
    btstack_base64_decoder_init(&context);
    uint32_t size = 0;
    while (input_size){
        uint8_t data = *input_data++;
        int result = btstack_base64_decoder_process_byte(&context, data);
        input_size--;
        switch (result){
            case BTSTACK_BASE64_DECODER_MORE:
                break;
            case BTSTACK_BASE64_DECODER_INVALID:
                return BTSTACK_BASE64_DECODER_INVALID;
            default:
                if (size >= output_max_size){
                    return BTSTACK_BASE64_DECODER_FULL;
                }
                output_buffer[size++] = result;
                break;
        }
    }
    return size;    
}

/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// Advertising Data Parser 
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_parser.h"

typedef enum {
    IncompleteList16 = 0x02, 
    CompleteList16 = 0x03, 
    IncompleteList128 = 0x06, 
    CompleteList128 = 0x07
} UUID_TYPE;

void ad_iterator_init(ad_context_t *context, uint8_t ad_len, uint8_t * ad_data){
    context->data = ad_data;
    context->length = ad_len;
    context->offset = 0;
}

int  ad_iterator_has_more(ad_context_t * context){
    return context->offset < context->length;
}

void ad_iterator_next(ad_context_t * context){
    uint8_t chunk_len = context->data[context->offset];
    context->offset += 1 + chunk_len;
}

uint8_t   ad_iterator_get_data_len(ad_context_t * context){
    return context->data[context->offset] - 1;
}

uint8_t   ad_iterator_get_data_type(ad_context_t * context){
    return context->data[context->offset + 1];
}

uint8_t * ad_iterator_get_data(ad_context_t * context){
    return &context->data[context->offset + 2];
}

int ad_data_contains_uuid16(uint8_t ad_len, uint8_t * ad_data, uint16_t uuid){
    ad_context_t context;
    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        switch (data_type){
            case IncompleteList16:
            case CompleteList16:
                // ... iterate through list of uuids
                break;
            case IncompleteList128:
            case CompleteList128:
                // ...
                break;
            default:
                break;
        }  
    }
    return 0;
}

int ad_data_contains_uuid128(uint8_t ad_len, uint8_t * ad_data, uint8_t * uuid128){
    ad_context_t context;
    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        switch (data_type){
            case IncompleteList16:
            case CompleteList16:
                // ...
                break;
            case IncompleteList128:
            case CompleteList128:
                // ...
                break;
            default:
                break;
        }  
    }
    return 0;
}


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


// *****************************************************************************
//
// Advertising Data Parser 
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/utils.h>
#include <btstack/sdp_util.h>
#include <btstack/hci_cmds.h>

#include "hci.h"
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

int ad_data_contains_uuid16(uint8_t ad_len, uint8_t * ad_data, uint16_t uuid16){
    ad_context_t context;
    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        int i;
        uint8_t ad_uuid128[16], uuid128_bt[16];
                
        switch (data_type){
            case IncompleteList16:
            case CompleteList16:
                for (i=0; i<data_len; i+=2){
                    uint16_t uuid = READ_BT_16(data, i);
                    if ( uuid == uuid16 ) return 1;
                }
                break;
            case IncompleteList128:
            case CompleteList128:
                sdp_normalize_uuid(ad_uuid128, uuid16);
                swap128(ad_uuid128, uuid128_bt);
            
                for (i=0; i<data_len; i+=16){
                    if (memcmp(uuid128_bt, &data[i], 16) == 0) return 1;
                }
                break;
            default:
                break;
        }  
    }
    return 0;
}

int ad_data_contains_uuid128(uint8_t ad_len, uint8_t * ad_data, uint8_t * uuid128){
    ad_context_t context;
    // input in big endian/network order, bluetooth data in little endian
    uint8_t uuid128_le[16];
    swap128(uuid128, uuid128_le);
    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        int i;
        uint8_t ad_uuid128[16];


        switch (data_type){
            case IncompleteList16:
            case CompleteList16:
                for (i=0; i<data_len; i+=2){
                    uint16_t uuid16 = READ_BT_16(data, i);
                    sdp_normalize_uuid(ad_uuid128, uuid16);
                    
                    if (memcmp(ad_uuid128, uuid128_le, 16) == 0) return 1;
                }

                break;
            case IncompleteList128:
            case CompleteList128:
                for (i=0; i<data_len; i+=16){
                    if (memcmp(uuid128_le, &data[i], 16) == 0) return 1;
                }
                break;
            default:
                break;
        }  
    }
    return 0;
}


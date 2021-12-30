/*
 * Copyright (C) 2017 BlueKitchen GmbH
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
 * @title AVRCP Media Item Iterator 
 *
 */

#ifndef AVRCP_MEDIA_ITEM_ITERATOR_H
#define AVRCP_MEDIA_ITEM_ITERATOR_H

#include "btstack_config.h"
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct avrcp_media_item_context {
     const uint8_t * data;
     uint16_t   offset;
     uint16_t   length;
} avrcp_media_item_context_t;

// Media item data iterator
void avrcp_media_item_iterator_init(avrcp_media_item_context_t *context, uint16_t avrcp_media_item_len, const uint8_t * avrcp_media_item_data);
int  avrcp_media_item_iterator_has_more(const avrcp_media_item_context_t * context);
void avrcp_media_item_iterator_next(avrcp_media_item_context_t * context);

// Access functions
uint32_t          avrcp_media_item_iterator_get_attr_id(const avrcp_media_item_context_t * context);
uint16_t         avrcp_media_item_iterator_get_attr_charset(const avrcp_media_item_context_t * context);
uint16_t         avrcp_media_item_iterator_get_attr_value_len(const avrcp_media_item_context_t * context);
const uint8_t *  avrcp_media_item_iterator_get_attr_value(const avrcp_media_item_context_t * context);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // AVRCP_MEDIA_ITEM_ITERATOR_H

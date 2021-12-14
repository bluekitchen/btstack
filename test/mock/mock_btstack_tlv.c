/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 */

#define BTSTACK_FILE__ "btstack_tlv_memory.c"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "btstack_linked_list.h"
#include "btstack_tlv.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include "mock_btstack_tlv.h"

#define MAX_TLV_VALUE_SIZE 200
#define DUMMY_SIZE 4
typedef struct tlv_entry {
    void   * next;
    uint32_t tag;
    uint32_t len;
    uint8_t  value[DUMMY_SIZE];	// dummy size
} tlv_entry_t;

static tlv_entry_t * mock_btstack_tlv_find_entry(mock_btstack_tlv_t * self, uint32_t tag){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &self->entry_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->tag != tag) continue;
        return entry;
    }
    return NULL;
}

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @return size of value
 */
static int mock_btstack_tlv_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){
    mock_btstack_tlv_t * self = (mock_btstack_tlv_t *) context;
    tlv_entry_t * entry = mock_btstack_tlv_find_entry(self, tag);
    // not found
    if (!entry) return 0;
    // return len if buffer = NULL
    if (!buffer) return entry->len;
    // otherwise copy data into buffer
    uint16_t bytes_to_copy = btstack_min(buffer_size, entry->len);
    memcpy(buffer, &entry->value[0], bytes_to_copy);
    return bytes_to_copy;
}

/**
 * Store Tag
 * @param tag
 * @param data
 * @param data_size
 */
static int mock_btstack_tlv_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){
    mock_btstack_tlv_t * self = (mock_btstack_tlv_t *) context;

    // enforce arbitrary max value size
    btstack_assert(data_size <= MAX_TLV_VALUE_SIZE);

    // remove old entry
    tlv_entry_t * old_entry = mock_btstack_tlv_find_entry(self, tag);
    if (old_entry){
        btstack_linked_list_remove(&self->entry_list, (btstack_linked_item_t *) old_entry);
        free(old_entry);
    }

    // create new entry
    uint32_t entry_size = sizeof(tlv_entry_t) - DUMMY_SIZE + data_size;
    tlv_entry_t * new_entry = (tlv_entry_t *) malloc(entry_size);
    if (!new_entry) return 0;
    memset(new_entry, 0, entry_size);
    new_entry->tag = tag;
    new_entry->len = data_size;
    memcpy(&new_entry->value[0], data, data_size);

    // append new entry
    btstack_linked_list_add(&self->entry_list, (btstack_linked_item_t *) new_entry);
    return 0;
}

/**
 * Delete Tag
 * @param tag
 */
static void mock_btstack_tlv_delete_tag(void * context, uint32_t tag){
    mock_btstack_tlv_t * self = (mock_btstack_tlv_t *) context;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &self->entry_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->tag != tag) continue;
        btstack_linked_list_iterator_remove(&it);
        free(entry);
        return;
    }
}

static const btstack_tlv_t mock_btstack_tlv = {
        /* int  (*get_tag)(..);     */ &mock_btstack_tlv_get_tag,
        /* int (*store_tag)(..);    */ &mock_btstack_tlv_store_tag,
        /* void (*delete_tag)(v..); */ &mock_btstack_tlv_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * mock_btstack_tlv_init_instance(mock_btstack_tlv_t * self){
    memset(self, 0, sizeof(mock_btstack_tlv_t));
    return &mock_btstack_tlv;
}

/**
 * Free TLV entries
 * @param self
 */
void mock_btstack_tlv_deinit(mock_btstack_tlv_t * self){
    // free all entries
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &self->entry_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_iterator_remove(&it);
        free(entry);
    }
}

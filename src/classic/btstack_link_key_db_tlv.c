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

#define BTSTACK_FILE__ "btstack_link_key_db_tlv.c"

#include <string.h>
#include <stdlib.h>

#include "classic/btstack_link_key_db_tlv.h"

#include "btstack_debug.h"
#include "btstack_util.h"
#include "classic/core.h"

// NVM_NUM_LINK_KEYS defines number of stored link keys
#ifndef NVM_NUM_LINK_KEYS
#define NVM_NUM_LINK_KEYS 1
#endif

typedef struct {
    const btstack_tlv_t * btstack_tlv_impl;
    void * btstack_tlv_context;
} btstack_link_key_db_tlv_h;

typedef struct link_key_nvm {
    uint32_t seq_nr;    // used for "least recently stored" eviction strategy
    bd_addr_t bd_addr;
    link_key_t link_key;
    link_key_type_t link_key_type;
} link_key_nvm_t;   // sizeof(link_key_nvm_t) = 27 bytes

static btstack_link_key_db_tlv_h singleton;
static btstack_link_key_db_tlv_h * self = &singleton;

static const char tag_0 = 'B';
static const char tag_1 = 'T';
static const char tag_2 = 'L';

static uint32_t btstack_link_key_db_tag_for_index(uint8_t index){
    return (tag_0 << 24) | (tag_1 << 16) | (tag_2 << 8) | index;
}

// Device info
static void btstack_link_key_db_tlv_open(void){
}

static void btstack_link_key_db_tlv_set_bd_addr(bd_addr_t bd_addr){
    (void)bd_addr;
}

static void btstack_link_key_db_tlv_close(void){ 
}

static int btstack_link_key_db_tlv_get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
    int i;
    for (i=0;i<NVM_NUM_LINK_KEYS;i++){
        link_key_nvm_t entry;
        uint32_t tag = btstack_link_key_db_tag_for_index(i);
        int size = self->btstack_tlv_impl->get_tag(self->btstack_tlv_context, tag, (uint8_t*) &entry, sizeof(entry));
        if (size == 0) continue;
        log_info("tag %x, addr %s", tag, bd_addr_to_str(entry.bd_addr));
        if (memcmp(bd_addr, entry.bd_addr, 6)) continue;
        // found, pass back
        memcpy(link_key, entry.link_key, 16);
        *link_key_type = entry.link_key_type;
        return 1;
    }
	return 0;
}

static void btstack_link_key_db_tlv_delete_link_key(bd_addr_t bd_addr){
    int i;
    for (i=0;i<NVM_NUM_LINK_KEYS;i++){
        link_key_nvm_t entry;
        uint32_t tag = btstack_link_key_db_tag_for_index(i);
        int size = self->btstack_tlv_impl->get_tag(self->btstack_tlv_context, tag, (uint8_t*) &entry, sizeof(entry));
        if (size == 0) continue;
        if (memcmp(bd_addr, entry.bd_addr, 6)) continue;
        // found, delete tag
        self->btstack_tlv_impl->delete_tag(self->btstack_tlv_context, tag);
        break;
    }
}

static void btstack_link_key_db_tlv_put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){
    int i;
    uint32_t highest_seq_nr = 0;
    uint32_t lowest_seq_nr = 0;
    uint32_t tag_for_lowest_seq_nr = 0;
    uint32_t tag_for_addr = 0;
    uint32_t tag_for_empty = 0;

    for (i=0;i<NVM_NUM_LINK_KEYS;i++){
        link_key_nvm_t entry;
        uint32_t tag = btstack_link_key_db_tag_for_index(i);
        int size = self->btstack_tlv_impl->get_tag(self->btstack_tlv_context, tag, (uint8_t*) &entry, sizeof(entry));
        // empty/deleted tag
        if (size == 0) {
            tag_for_empty = tag;
            continue;
        }
        // found addr?
        if (memcmp(bd_addr, entry.bd_addr, 6) == 0){
            tag_for_addr = tag;
        }
        // update highest seq nr
        if (entry.seq_nr > highest_seq_nr){
            highest_seq_nr = entry.seq_nr;
        }
        // find entry with lowest seq nr
        if ((tag_for_lowest_seq_nr == 0) || (entry.seq_nr < lowest_seq_nr)){
            tag_for_lowest_seq_nr = tag;
            lowest_seq_nr = entry.seq_nr;
        }
    }

    log_info("tag_for_addr %x, tag_for_empy %x, tag_for_lowest_seq_nr %x", tag_for_addr, tag_for_empty, tag_for_lowest_seq_nr);

    uint32_t tag_to_use = 0;
    if (tag_for_addr){
        tag_to_use = tag_for_addr;
    } else if (tag_for_empty){
        tag_to_use = tag_for_empty;
    } else if (tag_for_lowest_seq_nr){
        tag_to_use = tag_for_lowest_seq_nr;
    } else {
        // should not happen
        return;
    }

    log_info("store with tag %x", tag_to_use);

    link_key_nvm_t entry;
    
    memcpy(entry.bd_addr, bd_addr, 6);
    memcpy(entry.link_key, link_key, 16);
    entry.link_key_type = link_key_type;
    entry.seq_nr = highest_seq_nr + 1;

    self->btstack_tlv_impl->store_tag(self->btstack_tlv_context, tag_to_use, (uint8_t*) &entry, sizeof(entry));
}

static int btstack_link_key_db_tlv_iterator_init(btstack_link_key_iterator_t * it){
    it->context = (void*) 0;
    return 1;
}

static int  btstack_link_key_db_tlv_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type){
    uintptr_t i = (uintptr_t) it->context;
    int found = 0;
    while (i<NVM_NUM_LINK_KEYS){
        link_key_nvm_t entry;
        uint32_t tag = btstack_link_key_db_tag_for_index(i++);
        int size = self->btstack_tlv_impl->get_tag(self->btstack_tlv_context, tag, (uint8_t*) &entry, sizeof(entry));
        if (size == 0) continue;
        memcpy(bd_addr, entry.bd_addr, 6);
        memcpy(link_key, entry.link_key, 16);
        *link_key_type = entry.link_key_type;
        found = 1;
        break;
    }
    it->context = (void*) i;
    return found;
}

static void btstack_link_key_db_tlv_iterator_done(btstack_link_key_iterator_t * it){
    UNUSED(it);
}

static const btstack_link_key_db_t btstack_link_key_db_tlv = {
    btstack_link_key_db_tlv_open,
    btstack_link_key_db_tlv_set_bd_addr,
    btstack_link_key_db_tlv_close,
    btstack_link_key_db_tlv_get_link_key,
    btstack_link_key_db_tlv_put_link_key,
    btstack_link_key_db_tlv_delete_link_key,
    btstack_link_key_db_tlv_iterator_init,
    btstack_link_key_db_tlv_iterator_get_next,
    btstack_link_key_db_tlv_iterator_done,
};

const btstack_link_key_db_t * btstack_link_key_db_tlv_get_instance(const btstack_tlv_t * btstack_tlv_impl, void * btstack_tlv_context){
    self->btstack_tlv_impl = btstack_tlv_impl;
    self->btstack_tlv_context = btstack_tlv_context;
    return &btstack_link_key_db_tlv;
}



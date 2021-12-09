/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_link_key_db_wiced_dct.c"

/*
 *  btstack_link_key_db_wiced_dct.c
 *
 *  Persistent Link Key implemenetation for WICED using DCT mechanism
 */

#include "classic/btstack_link_key_db.h"

#include "stdint.h"
#include "string.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include "wiced.h"

// Link Key Magic
#define LINK_KEY_MAGIC ((uint32_t) 'B' << 24 | 'T' << 16 | 'L' << 8 | 'K')

typedef struct link_key_nvm {
	uint32_t magic;
	uint32_t seq_nr;	// used for "last recently stored" eviction strategy
	bd_addr_t bd_addr;
	link_key_t link_key;
	link_key_type_t link_key_type;
} link_key_nvm_t;

static char link_key_to_str_buffer[LINK_KEY_STR_LEN+1];  // 11223344556677889900112233445566\0
static char *link_key_to_str(link_key_t link_key){
    char * p = link_key_to_str_buffer;
    int i;
    for (i = 0; i < LINK_KEY_LEN ; i++) {
        *p++ = char_for_nibble((link_key[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((link_key[i] >> 0) & 0x0F);
    }
    *p = 0;
    return (char *) link_key_to_str_buffer;
}

static void link_key_db_init(void){
	log_info("Link Key DB initialized for DCT\n");
}

static void link_key_db_close(void){
}

// @return valid
static int link_key_read(int index, link_key_nvm_t * out_entry){

	// calculate address
	uint32_t address = index * sizeof(link_key_nvm_t);

	// read lock
	link_key_nvm_t * entry;
	wiced_dct_read_lock((void*) &entry, WICED_FALSE, DCT_APP_SECTION, address, sizeof(link_key_nvm_t));

	if (entry->magic == LINK_KEY_MAGIC){
		memcpy(out_entry, entry, sizeof(link_key_nvm_t));
	} else {
		memset(out_entry, 0, sizeof(link_key_nvm_t));
	}

	// read unlock
	wiced_dct_read_unlock((void*) entry, WICED_FALSE);

	return out_entry->magic == LINK_KEY_MAGIC;
}

static void link_key_write(int index, link_key_nvm_t * entry){
	// calculate address
	uint32_t address = index * sizeof(link_key_nvm_t);
	// write block
	wiced_dct_write((void*)entry, DCT_APP_SECTION, address, sizeof(link_key_nvm_t));
}

// returns entry index or -1
static int link_key_find_entry_for_address(bd_addr_t address){
	link_key_nvm_t item;
	int i;
	for (i=0;i<NVM_NUM_LINK_KEYS;i++){
		link_key_read(i, &item);
		if (item.magic != LINK_KEY_MAGIC) continue;
		if (memcmp(address, &item.bd_addr, 6) != 0) continue;
		return i;
	}
	return -1;
}

// returns index
static int link_key_find_free_entry(void){
	link_key_nvm_t item;
	int i;
	uint32_t seq_nr = 0;
	int lowest_index = -1;
	for (i=0;i<NVM_NUM_LINK_KEYS;i++){
		link_key_read(i, &item);
		if (item.magic != LINK_KEY_MAGIC) return i;
		if ((lowest_index < 0) || (item.seq_nr < seq_nr)){
			lowest_index = i;
			seq_nr= item.seq_nr;
		}
	}
	return lowest_index;
}

static uint32_t link_key_highest_seq_nr(void){
	link_key_nvm_t item;
	int i;
	uint32_t seq_nr = 0;
	for (i=0;i<NVM_NUM_LINK_KEYS;i++){
		link_key_read(i, &item);
		if (item.magic != LINK_KEY_MAGIC) continue;
		if (item.seq_nr < seq_nr) continue;
		seq_nr = item.seq_nr;
	}
	return seq_nr;	
}

// returns 1 if found
static int link_key_db_get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
	int index = link_key_find_entry_for_address(bd_addr);
	if (index < 0) {
		log_info("link_key_db_get_link_key for %s -> not found\n", bd_addr_to_str(bd_addr));
		return 0;
	}
	link_key_nvm_t item;
	link_key_read(index, &item);
	memcpy(link_key, item.link_key, LINK_KEY_LEN);
	if (link_key_type) {
		*link_key_type = item.link_key_type;
	}
	log_info("link_key_db_get_link_key for %s -> found %s\n", bd_addr_to_str(bd_addr), link_key_to_str(link_key));
	return 1;
}

static void link_key_db_delete_link_key(bd_addr_t bd_addr){
	int index = link_key_find_entry_for_address(bd_addr);
	if (index < 0) return;
	
	link_key_nvm_t item;
	memset(&item, 0, sizeof(item));
	link_key_write(index, &item);	
}


static void link_key_db_put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){
	// check for existing record
	int index = link_key_find_entry_for_address(bd_addr);
	
	// if not found, use new entry
	if (index < 0) {
		index = link_key_find_free_entry();
	}

	log_info("link_key_db_put_link_key for %s - key %s - at index %u\n", bd_addr_to_str(bd_addr),  link_key_to_str(link_key), index);
	
	link_key_nvm_t item;
	item.magic = LINK_KEY_MAGIC;
	item.seq_nr = link_key_highest_seq_nr() + 1;
	memcpy(item.bd_addr, bd_addr, sizeof(bd_addr_t));
	memcpy(item.link_key, link_key, LINK_KEY_LEN);
	item.link_key_type = link_key_type;
	link_key_write(index, &item);
}

static void link_key_db_set_local_bd_addr(bd_addr_t bd_addr){
}

static int link_key_db_tlv_iterator_init(btstack_link_key_iterator_t * it){
    it->context = (void*) 0;
    return 1;
}

static int  link_key_db_tlv_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type){
    uintptr_t i = (uintptr_t) it->context;
    int found = 0;
    while (i<NVM_NUM_LINK_KEYS){
		link_key_nvm_t item;
		link_key_read(i++, &item);
		if (item.magic != LINK_KEY_MAGIC) continue;

        memcpy(bd_addr, item.bd_addr, 6);
        memcpy(link_key, item.link_key, 16);
        *link_key_type = item.link_key_type;
        found = 1;
        break;
    }
    it->context = (void*) i;
    return found;
}

static void link_key_db_tlv_iterator_done(btstack_link_key_iterator_t * it){
    UNUSED(it);
}

const btstack_link_key_db_t btstack_link_key_db_wiced_dct = {
    link_key_db_init,
    link_key_db_set_local_bd_addr,	
    link_key_db_close,
    link_key_db_get_link_key,
    link_key_db_put_link_key,
    link_key_db_delete_link_key,
    link_key_db_tlv_iterator_init,
    link_key_db_tlv_iterator_get_next,
    link_key_db_tlv_iterator_done,
};

// custom function
void btstack_link_key_db_wiced_dct_delete_all(void){
	int i;
	link_key_nvm_t entry;
	memset(&entry, 0, sizeof(link_key_nvm_t));
	for (i=0;i<NVM_NUM_LINK_KEYS;i++){
		link_key_write(i, &entry);
	}
}

int btstack_link_key_db_wiced_dct_get_storage_size(void){
	return NVM_NUM_LINK_KEYS * sizeof(link_key_nvm_t);
}

const btstack_link_key_db_t * btstack_link_key_db_wiced_dct_instance(void){
    return &btstack_link_key_db_wiced_dct;
}

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

#define BTSTACK_FILE__ "btstack_link_key_db_static.c"

/*
 *  btstack_link_key_db_static.c
 *
 *  Static Link Key implementation to use during development/porting:
 *  - Link keys have to be manually added to this file to make them usable
 *  + Link keys are preserved on reflash in constrast to the program flash based link key store
 */

#include "classic/btstack_link_key_db.h"

#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	const char * bd_addr;
	const char * link_key;
	int          link_key_type;
} link_key_entry_t;

// fixed link key db
static const link_key_entry_t link_key_db[] = {
		// Example enry
		{ "11:22:33:44:55:66", "11223344556677889900112233445566", 1},
		// Add new link keys here..
};

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

static int sscanf_link_key(const char * link_key_string, link_key_t link_key){
    uint16_t pos;
    for (pos = 0 ; pos < LINK_KEY_LEN; pos++){
        int high = nibble_for_char(*link_key_string++);
        if (high < 0) return 0;
        int low = nibble_for_char(*link_key_string++);
        if (low < 0) return 0;
        link_key[pos] = (((uint8_t) high) << 4) | ((uint8_t) low);
    }
    return 1;
}

static void link_key_db_init(void){
}

static void link_key_db_close(void){
}


// returns 1 if found
static int link_key_db_get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
	int i;
	int num_entries = sizeof(link_key_db) / sizeof(link_key_entry_t);

	for (i=0;i<num_entries;i++){
		if (strcmp(bd_addr_to_str(bd_addr), link_key_db[i].bd_addr)) continue;
		*link_key_type = (link_key_type_t) link_key_db[i].link_key_type;
		sscanf_link_key(link_key_db[i].link_key, link_key);
		return 1;
	}
	return 0;
}

static void link_key_db_delete_link_key(bd_addr_t bd_addr){
    (void) bd_addr;
}


static void link_key_db_put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){
	log_info("Please add the following line to btstack_link_key_db.c");
	log_info("{ \"%s\", \"%s\", %u },\n", bd_addr_to_str(bd_addr), link_key_to_str(link_key), (int) link_key_type);
}

static void link_key_db_set_local_bd_addr(bd_addr_t bd_addr){
    (void) bd_addr;
}

static int link_key_db_tlv_iterator_init(btstack_link_key_iterator_t * it){
    it->context = (void*) 0;
    return 1;
}

static int  link_key_db_tlv_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type){
    uintptr_t i = (uintptr_t) it->context;

    unsigned int num_entries = sizeof(link_key_db) / sizeof(link_key_entry_t);
    if (i >= num_entries) return 0;

    // fetch values
    *link_key_type = (link_key_type_t) link_key_db[i].link_key_type;
    sscanf_link_key(link_key_db[i].link_key, link_key);
    sscanf_bd_addr(link_key_db[i].bd_addr, bd_addr);

    // next
    it->context = (void*) (i+1);
    return 1;
}

static void link_key_db_tlv_iterator_done(btstack_link_key_iterator_t * it){
    UNUSED(it);
}

static const btstack_link_key_db_t btstack_link_key_db_static = {
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

const btstack_link_key_db_t * btstack_link_key_db_static_instance(void){
    return &btstack_link_key_db_static;
}

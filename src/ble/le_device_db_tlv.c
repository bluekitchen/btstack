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

#define __BTSTACK_FILE__ "le_device_db_tlv.c"
 
#include "ble/le_device_db.h"
#include "ble/le_device_db_tlv.h"

#include "ble/core.h"

#include <stdio.h>
#include <string.h>
#include "btstack_debug.h"

// LE Device DB Implementation storing entries in btstack_tlv

// Local cache is used to keep track of deleted entries in TLV

#define INVALID_ENTRY_ADDR_TYPE 0xff

// Single stored entry
typedef struct le_device_db_entry_t {

    // Identification
    int addr_type;
    bd_addr_t addr;
    sm_key_t irk;

    // Stored pairing information allows to re-establish an enncrypted connection
    // with a peripheral that doesn't have any persistent memory
    sm_key_t ltk;
    uint16_t ediv;
    uint8_t  rand[8];
    uint8_t  key_size;
    uint8_t  authenticated;
    uint8_t  authorized;

#ifdef ENABLE_LE_SIGNED_WRITE
    // Signed Writes by remote
    sm_key_t remote_csrk;
    uint32_t remote_counter;

    // Signed Writes by us
    sm_key_t local_csrk;
    uint32_t local_counter;
#endif

} le_device_db_entry_t;


#ifndef NVM_NUM_DEVICE_DB_ENTRIES 
#error "NVM_NUM_DEVICE_DB_ENTRIES not defined, please define in btstack_config.h"
#endif

#if NVM_NUM_DEVICE_DB_ENTRIES == 0
#error "NVM_NUM_DEVICE_DB_ENTRIES must not be 0, please update in btstack_config.h"
#endif

// only stores if entry present
static uint8_t  entry_map[NVM_NUM_DEVICE_DB_ENTRIES];
static uint32_t num_valid_entries;

static const btstack_tlv_t * le_device_db_tlv_btstack_tlv_impl;
static       void *          le_device_db_tlv_btstack_tlv_context;

static const char tag_0 = 'B';
static const char tag_1 = 'T';
static const char tag_2 = 'D';

static uint32_t le_device_db_tlv_tag_for_index(uint8_t index){
    return (tag_0 << 24) | (tag_1 << 16) | (tag_2 << 8) | index;
}

// @returns success
// @param index = entry_pos
static int le_device_db_tlv_fetch(int index, le_device_db_entry_t * entry){
    if (index < 0 || index >= NVM_NUM_DEVICE_DB_ENTRIES){
	    log_error("le_device_db_tlv_fetch called with invalid index %d", index);
	    return 0;
	}
    uint32_t tag = le_device_db_tlv_tag_for_index(index);
    int size = le_device_db_tlv_btstack_tlv_impl->get_tag(le_device_db_tlv_btstack_tlv_context, tag, (uint8_t*) entry, sizeof(le_device_db_entry_t));
	return size != 0;
}

// @returns success
// @param index = entry_pos
static int le_device_db_tlv_store(int index, le_device_db_entry_t * entry){
    if (index < 0 || index >= NVM_NUM_DEVICE_DB_ENTRIES){
	    log_error("le_device_db_tlv_store called with invalid index %d", index);
	    return 0;
	}
    uint32_t tag = le_device_db_tlv_tag_for_index(index);
    le_device_db_tlv_btstack_tlv_impl->store_tag(le_device_db_tlv_btstack_tlv_context, tag, (uint8_t*) entry, sizeof(le_device_db_entry_t));
	return 1;
}

// @param index = entry_pos
static int le_device_db_tlv_delete(int index){
    if (index < 0 || index >= NVM_NUM_DEVICE_DB_ENTRIES){
	    log_error("le_device_db_tlv_delete called with invalid index %d", index);
	    return 0;
	}
    uint32_t tag = le_device_db_tlv_tag_for_index(index);
    le_device_db_tlv_btstack_tlv_impl->delete_tag(le_device_db_tlv_btstack_tlv_context, tag);
	return 1;
}

void le_device_db_init(void){
    int i;
	num_valid_entries = 0;
	memset(entry_map, 0, sizeof(entry_map));
    for (i=0;i<NVM_NUM_DEVICE_DB_ENTRIES;i++){
    	// lookup entry
    	le_device_db_entry_t entry;
    	if (!le_device_db_tlv_fetch(i, &entry)) continue;

    	entry_map[i] = 1;
        num_valid_entries++;
    }
    log_info("num valid le device entries %u", num_valid_entries);
}

// not used
void le_device_db_set_local_bd_addr(bd_addr_t bd_addr){
    (void)bd_addr;
}

// @returns number of device in db
int le_device_db_count(void){
	return num_valid_entries;
}

void le_device_db_remove(int index){
	// delete entry in TLV
	le_device_db_tlv_delete(index);

	// mark as unused
    entry_map[index] = 0;

    // keep track
    num_valid_entries--;
}

int le_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk){
	// find unused entry in the used list
    int i;
    int index = -1;
    for (i=0;i<NVM_NUM_DEVICE_DB_ENTRIES;i++){
         if (entry_map[i]) continue;
         index = i;
         break;
    }

    // no free entry found
    if (index < 0) return -1;

    log_info("new entry for index %u", index);

    // store entry at index
	le_device_db_entry_t entry;
    log_info("LE Device DB adding type %u - %s", addr_type, bd_addr_to_str(addr));
    log_info_key("irk", irk);

    entry.addr_type = addr_type;
    memcpy(entry.addr, addr, 6);
    memcpy(entry.irk, irk, 16);
#ifdef ENABLE_LE_SIGNED_WRITE
    entry.remote_counter = 0; 
#endif

    // store
    le_device_db_tlv_store(index, &entry);
    
    // set in entry_mape
    entry_map[index] = 1;

    // keep track
    num_valid_entries++;

    return index;
}


// get device information: addr type and address
void le_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    if (addr_type) *addr_type = entry.addr_type;
    if (addr) memcpy(addr, entry.addr, 6);
    if (irk) memcpy(irk, entry.irk, 16);
}

void le_device_db_encryption_set(int index, uint16_t ediv, uint8_t rand[8], sm_key_t ltk, int key_size, int authenticated, int authorized){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

	// update
    log_info("LE Device DB set encryption for %u, ediv x%04x, key size %u, authenticated %u, authorized %u",
        index, ediv, key_size, authenticated, authorized);
    entry.ediv = ediv;
    if (rand) memcpy(entry.rand, rand, 8);
    if (ltk) memcpy(entry.ltk, ltk, 16);
    entry.key_size = key_size;
    entry.authenticated = authenticated;
    entry.authorized = authorized;

    // store
    le_device_db_tlv_store(index, &entry);
}

void le_device_db_encryption_get(int index, uint16_t * ediv, uint8_t rand[8], sm_key_t ltk, int * key_size, int * authenticated, int * authorized){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

	// update user fields
    log_info("LE Device DB encryption for %u, ediv x%04x, keysize %u, authenticated %u, authorized %u",
        index, entry.ediv, entry.key_size, entry.authenticated, entry.authorized);
    if (ediv) *ediv = entry.ediv;
    if (rand) memcpy(rand, entry.rand, 8);
    if (ltk)  memcpy(ltk, entry.ltk, 16);    
    if (key_size) *key_size = entry.key_size;
    if (authenticated) *authenticated = entry.authenticated;
    if (authorized) *authorized = entry.authorized;
}

#ifdef ENABLE_LE_SIGNED_WRITE

// get signature key
void le_device_db_remote_csrk_get(int index, sm_key_t csrk){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    if (csrk) memcpy(csrk, entry.remote_csrk, 16);
}

void le_device_db_remote_csrk_set(int index, sm_key_t csrk){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    if (!csrk) return;

    // update
    memcpy(entry.remote_csrk, csrk, 16);

    // store
    le_device_db_tlv_store(index, &entry);
}

void le_device_db_local_csrk_get(int index, sm_key_t csrk){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    if (!csrk) return;

    // fill
    memcpy(csrk, entry.local_csrk, 16);
}

void le_device_db_local_csrk_set(int index, sm_key_t csrk){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    if (!csrk) return;

    // update
    memcpy(entry.local_csrk, csrk, 16);

    // store
    le_device_db_tlv_store(index, &entry);
}

// query last used/seen signing counter
uint32_t le_device_db_remote_counter_get(int index){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return 0;

    return entry.remote_counter;
}

// update signing counter
void le_device_db_remote_counter_set(int index, uint32_t counter){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

    entry.remote_counter = counter;

    // store
    le_device_db_tlv_store(index, &entry);
}

// query last used/seen signing counter
uint32_t le_device_db_local_counter_get(int index){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return 0;

    return entry.local_counter;
}

// update signing counter
void le_device_db_local_counter_set(int index, uint32_t counter){

	// fetch entry
	le_device_db_entry_t entry;
	int ok = le_device_db_tlv_fetch(index, &entry);
	if (!ok) return;

	// update
    entry.local_counter = counter;

    // store
    le_device_db_tlv_store(index, &entry);
}

#endif

void le_device_db_dump(void){
    log_info("LE Device DB dump, devices: %d", le_device_db_count());
    int i;
    for (i=0;i<num_valid_entries;i++){
		// fetch entry
		le_device_db_entry_t entry;
		le_device_db_tlv_fetch(i, &entry);
        log_info("%u: %u %s", i, entry.addr_type, bd_addr_to_str(entry.addr));
        log_info_key("irk", entry.irk);
#ifdef ENABLE_LE_SIGNED_WRITE
        log_info_key("local csrk", entry.local_csrk);
        log_info_key("remote csrk", entry.remote_csrk);
#endif
    }
}

void le_device_db_tlv_configure(const btstack_tlv_t * btstack_tlv_impl, void * btstack_tlv_context){
	le_device_db_tlv_btstack_tlv_impl = btstack_tlv_impl;
	le_device_db_tlv_btstack_tlv_context = btstack_tlv_context;
}

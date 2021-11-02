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

#define BTSTACK_FILE__ "le_device_db_wiced_dct.c"

/*
 *  le_device_db_wiced_dct.c
 *
 *  Persistent LE Device DB implemenetation for WICED using DCT mechanism
 */

#include "stdint.h"
#include "string.h"
#include "inttypes.h"

#include "wiced.h"

#include "ble/le_device_db.h"
#include "btstack_link_key_db_wiced_dct.h"	// for size of 

#include "btstack_debug.h"
#include "btstack_util.h"

// Link Key Magic
#define LE_DEVICE_MAGIC ((uint32_t) 'L' << 24 | 'E' << 16 | 'D' << 8 | 'B')

typedef struct le_device_nvm {
	uint32_t magic;
	uint32_t seq_nr;	// used for "last recently stored" eviction strategy

    // Identification
    sm_key_t irk;
    bd_addr_t addr;
    uint8_t addr_type;

    // pairing information
    uint8_t  key_size;
    uint8_t  authenticated;
    uint8_t  authorized;
    uint8_t  secure_connection;

    sm_key_t ltk;

    // Stored pairing information allows to re-establish an enncrypted connection
    // with a peripheral that doesn't have any persistent memory
    uint16_t ediv;
    uint8_t  rand[8];

} le_device_nvm_t;

static uint32_t start_of_le_device_db;

// calculate address
static int le_device_db_address_for_absolute_index(int abolute_index){
	return start_of_le_device_db + abolute_index * sizeof(le_device_nvm_t);
}

static int le_device_db_entry_valid(int absolute_index){
	// read lock
	le_device_nvm_t * entry;
	wiced_dct_read_lock((void*) &entry, WICED_FALSE, DCT_APP_SECTION, le_device_db_address_for_absolute_index(absolute_index), sizeof(le_device_nvm_t));
	int valid = entry->magic == LE_DEVICE_MAGIC;
	// read unlock
	wiced_dct_read_unlock((void*) entry, WICED_FALSE);
	return valid;
}

// @return valid
static int le_device_db_entry_read(int absolute_index, le_device_nvm_t * out_entry){

	// read lock
	le_device_nvm_t * entry;
	wiced_dct_read_lock((void*) &entry, WICED_FALSE, DCT_APP_SECTION, le_device_db_address_for_absolute_index(absolute_index), sizeof(le_device_nvm_t));

	if (entry->magic == LE_DEVICE_MAGIC){
		memcpy(out_entry, entry, sizeof(le_device_nvm_t));
	} else {
		memset(out_entry, 0, sizeof(le_device_nvm_t));
	}

	// read unlock
	wiced_dct_read_unlock((void*) entry, WICED_FALSE);

	return out_entry->magic == LE_DEVICE_MAGIC;
}

static void le_device_db_entry_write(int absolute_index, le_device_nvm_t * entry){
	// write block
	wiced_dct_write((void*)entry, DCT_APP_SECTION, le_device_db_address_for_absolute_index(absolute_index), sizeof(le_device_nvm_t));
}

static uint32_t le_device_db_highest_seq_nr(void){
	le_device_nvm_t entry;
	int i;
	uint32_t seq_nr = 0;
	for (i=0;i<NVM_NUM_LE_DEVICES;i++){
		le_device_db_entry_read(i, &entry);
		if (entry.magic != LE_DEVICE_MAGIC) continue;
		if (entry.seq_nr < seq_nr) continue;
		seq_nr = entry.seq_nr;
	}
	return seq_nr;	
}

// returns absoulte index
static int le_device_db_find_free_entry(void){
	le_device_nvm_t entry;
	int i;
	uint32_t seq_nr = 0;
	int lowest_index = -1;
	for (i=0;i<NVM_NUM_LE_DEVICES;i++){
		le_device_db_entry_read(i, &entry);
		if (entry.magic != LE_DEVICE_MAGIC) return i;
		if ((lowest_index < 0) || (entry.seq_nr < seq_nr)){
			lowest_index = i;
			seq_nr= entry.seq_nr;
		}
	}
	return lowest_index;
}

// returns absolute index
static int le_device_db_get_absolute_index_for_device_index(int device_index){
    int i;
    int counter = 0;
    for (i=0;i<NVM_NUM_LE_DEVICES;i++){
    	if (le_device_db_entry_valid(i)) {
    		// found
    		if (counter == device_index) return i;
    		counter++;
    	}
    }
    return 0;
}

// PUBLIC API

void le_device_db_wiced_dct_set_start_address(uint32_t start_address){
	log_info("set start address: %"PRIu32, start_address);	
	start_of_le_device_db = start_address;
}

void le_device_db_init(void){
}

void le_device_db_set_local_bd_addr(bd_addr_t addr){
	(void) addr;
}

// @returns number of device in db
int le_device_db_count(void){
    int i;
    int counter = 0;
    for (i=0;i<NVM_NUM_LE_DEVICES;i++){
    	if (le_device_db_entry_valid(i)) counter++;
    }
    return counter;
}

int le_device_db_max_count(void){
    return NVM_NUM_LE_DEVICES;
}

// get device information: addr type and address
void le_device_db_info(int device_index, int * addr_type, bd_addr_t addr, sm_key_t irk){
	int absolute_index = le_device_db_get_absolute_index_for_device_index(device_index);
    le_device_nvm_t entry;
    int valid = le_device_db_entry_read(absolute_index, &entry);

    // set defaults if not found
    if (!valid) {
        memset(&entry, 0, sizeof(le_device_nvm_t));
        entry.addr_type = BD_ADDR_TYPE_UNKNOWN;
    }

    if (addr_type) *addr_type = entry.addr_type;
    if (addr) memcpy(addr, entry.addr, 6);
    if (irk) memcpy(irk, entry.irk, 16);
}

// free device
void le_device_db_remove(int device_index){
	int absolute_index = le_device_db_get_absolute_index_for_device_index(device_index);
	le_device_nvm_t entry;
	memset(&entry, 0, sizeof(le_device_nvm_t));
	le_device_db_entry_write(absolute_index, &entry);
}

// custom function
void le_device_db_wiced_dct_delete_all(void){
	int i;
	le_device_nvm_t entry;
	memset(&entry, 0, sizeof(le_device_nvm_t));
	for (i=0;i<NVM_NUM_LE_DEVICES;i++){
		le_device_db_entry_write(i, &entry);
	}
}

int le_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk){
    int absolute_index = le_device_db_find_free_entry();
    uint32_t seq_nr = le_device_db_highest_seq_nr() + 1;
    log_info("adding type %u - %s, seq nr %u, as #%u", addr_type, bd_addr_to_str(addr), (int) seq_nr, absolute_index);
    log_info_key("irk", irk);

	le_device_nvm_t entry;
	memset(&entry, 0, sizeof(le_device_nvm_t));

    entry.magic     = LE_DEVICE_MAGIC;
	entry.seq_nr    = seq_nr;
    entry.addr_type = addr_type;
    memcpy(entry.addr, addr, 6);
    memcpy(entry.irk, irk, 16);

    le_device_db_entry_write(absolute_index, &entry);

    return absolute_index;
}

void le_device_db_encryption_set(int device_index, uint16_t ediv, uint8_t rand[8], sm_key_t ltk, int key_size, int authenticated, int authorized, int secure_connection){

	int absolute_index = le_device_db_get_absolute_index_for_device_index(device_index);
    le_device_nvm_t entry;
	le_device_db_entry_read(absolute_index, &entry);

    log_info("LE Device DB set encryption for %u, ediv x%04x, key size %u, authenticated %u, authorized %u, secure connection %u",
        device_index, ediv, key_size, authenticated, authorized, secure_connection);

    entry.ediv = ediv;
    if (rand) memcpy(entry.rand, rand, 8);
    if (ltk) memcpy(entry.ltk, ltk, 16);
    entry.key_size = key_size;
    entry.authenticated = authenticated;
    entry.authorized = authorized;
    entry.secure_connection = secure_connection;

    le_device_db_entry_write(absolute_index, &entry);
}

void le_device_db_encryption_get(int device_index, uint16_t * ediv, uint8_t rand[8], sm_key_t ltk, int * key_size, int * authenticated, int * authorized, int * secure_connection){

	int absolute_index = le_device_db_get_absolute_index_for_device_index(device_index);
    le_device_nvm_t entry;
	le_device_db_entry_read(absolute_index, &entry);

    log_info("LE Device DB encryption for %u, ediv x%04x, keysize %u, authenticated %u, authorized %u, secure connection %u",
        device_index, entry.ediv, entry.key_size, entry.authenticated, entry.authorized, entry.secure_connection);

    if (ediv) *ediv = entry.ediv;
    if (rand) memcpy(rand, entry.rand, 8);
    if (ltk)  memcpy(ltk, entry.ltk, 16);    
    if (key_size) *key_size = entry.key_size;
    if (authenticated) *authenticated = entry.authenticated;
    if (authorized) *authorized = entry.authorized;
    if (secure_connection) *secure_connection = entry.secure_connection;
}

void le_device_db_dump(void){
    log_info("dump, devices: %d", le_device_db_count());
    int i;
    for (i=0;i<NVM_NUM_LE_DEVICES;i++){
	    le_device_nvm_t entry;
		int valid = le_device_db_entry_read(i, &entry);

        if (!valid) continue;

        log_info("#%u: %u %s", i, entry.addr_type, bd_addr_to_str(entry.addr));
        log_info_key("ltk", entry.ltk);
        log_info_key("irk", entry.irk);
    }
}

int le_device_db_wiced_dct_get_storage_size(void){
	return NVM_NUM_LE_DEVICES * sizeof(le_device_nvm_t);
}

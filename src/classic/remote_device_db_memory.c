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

#include <string.h>
#include <stdlib.h>

#include "classic/remote_device_db.h"
#include "btstack_memory.h"
#include "btstack_debug.h"

#include "btstack_util.h"
#include "btstack_linked_list.h"

// This lists should be only accessed by tests.
btstack_linked_list_t db_mem_link_keys = NULL;
btstack_linked_list_t db_mem_names = NULL;

// Device info
static void db_open(void){
}

static void db_close(void){ 
}

static db_mem_device_t * get_item(btstack_linked_list_t list, bd_addr_t bd_addr) {
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) list; it ; it = it->next){
        db_mem_device_t * item = (db_mem_device_t *) it;
        if (BD_ADDR_CMP(item->bd_addr, bd_addr) == 0) {
            return item;
        }
    }
    return NULL;
}

static int get_name(bd_addr_t bd_addr, device_name_t *device_name) {
    db_mem_device_name_t * item = (db_mem_device_name_t *) get_item(db_mem_names, bd_addr);
    
    if (!item) return 0;
    
    strncpy((char*)device_name, item->device_name, MAX_NAME_LEN);
    
	btstack_linked_list_remove(&db_mem_names, (btstack_linked_item_t *) item);
    btstack_linked_list_add(&db_mem_names, (btstack_linked_item_t *) item);
	
	return 1;
}

static int get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
    db_mem_device_link_key_t * item = (db_mem_device_link_key_t *) get_item(db_mem_link_keys, bd_addr);
    
    if (!item) return 0;
    
    memcpy(link_key, item->link_key, LINK_KEY_LEN);
    if (link_key_type) {
        *link_key_type = item->link_key_type;
    }
	btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t *) item);
    btstack_linked_list_add(&db_mem_link_keys, (btstack_linked_item_t *) item);

	return 1;
}

static void delete_link_key(bd_addr_t bd_addr){
    db_mem_device_t * item = get_item(db_mem_link_keys, bd_addr);
    
    if (!item) return;
    
    btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t *) item);
    btstack_memory_db_mem_device_link_key_free((db_mem_device_link_key_t*)item);
}


static void put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){

    // check for existing record and remove if found
    db_mem_device_link_key_t * record = (db_mem_device_link_key_t *) get_item(db_mem_link_keys, bd_addr);
    if (record){
        btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) record);
    }

    // record not found, get new one from memory pool
    if (!record) {
        record = btstack_memory_db_mem_device_link_key_get();
    }

    // if none left, re-use last item and remove from list
    if (!record){
        record = (db_mem_device_link_key_t*)btstack_linked_list_get_last_item(&db_mem_link_keys);
        if (record) {
            btstack_linked_list_remove(&db_mem_link_keys, (btstack_linked_item_t*) record);
        }
    }
        
    if (!record) return;
    
    memcpy(record->device.bd_addr, bd_addr, sizeof(bd_addr_t));
    memcpy(record->link_key, link_key, LINK_KEY_LEN);
    record->link_key_type = link_key_type;
    btstack_linked_list_add(&db_mem_link_keys, (btstack_linked_item_t *) record);
}

static void delete_name(bd_addr_t bd_addr){
    db_mem_device_t * item = get_item(db_mem_names, bd_addr);
    
    if (!item) return;
    
    btstack_linked_list_remove(&db_mem_names, (btstack_linked_item_t *) item);
    btstack_memory_db_mem_device_name_free((db_mem_device_name_t*)item);    
}

static void put_name(bd_addr_t bd_addr, device_name_t *device_name){

    // check for existing record and remove if found
    db_mem_device_name_t * record = (db_mem_device_name_t *) get_item(db_mem_names, bd_addr);
    if (record){
        btstack_linked_list_remove(&db_mem_names, (btstack_linked_item_t*) record);
    }

    // record not found, get new one from memory pool
    if (!record) {
        record = btstack_memory_db_mem_device_name_get();
    }

    // if none left, re-use last item and remove from list
    if (!record){
        record = (db_mem_device_name_t*)btstack_linked_list_get_last_item(&db_mem_names);
        if (record) {
            btstack_linked_list_remove(&db_mem_names, (btstack_linked_item_t*) record);
        }
    }

    if (!record) return;
    
    memcpy(record->device.bd_addr, bd_addr, sizeof(bd_addr_t));
    strncpy(record->device_name, (const char*) device_name, MAX_NAME_LEN);
    btstack_linked_list_add(&db_mem_names, (btstack_linked_item_t *) record);
}

const remote_device_db_t remote_device_db_memory = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name,
};

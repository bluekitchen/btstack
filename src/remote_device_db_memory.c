/*
 * Copyright (C) 2010 by Matthias Ringwald
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
#include "remote_device_db.h"
#include "debug.h"

#include <btstack/linked_list.h>
#include <btstack/utils.h>

#define MAX_NAME_LEN 32

linked_list_t db_mem_devices;
linked_list_t db_mem_services;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;

    // Device's BD_ADDR
    bd_addr_t bd_addr;

    // Link Key
    link_key_t link_key;

    // Device's Name, with maximum length
    char device_name[MAX_NAME_LEN];

} db_mem_devices_t;

// Device info
static void db_open(){
}

static void db_close(){ 
}

static int get_link_key(bd_addr_t *bd_addr, link_key_t *link_key) {
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        print_bd_addr(item->bd_addr);
        if (cmp_bd_addr(item->bd_addr, *bd_addr)) {
            memcpy(link_key, item->link_key, LINK_KEY_LEN);
            print_bd_addr(link_key);
            return 1;
        }
    }
    return 0;
}

static void put_link_key(bd_addr_t *bd_addr, link_key_t *link_key){
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        if (item->bd_addr == *bd_addr){
            memcpy(item->link_key, link_key, LINK_KEY_LEN);
            return;
        }
    }

    // Record not found, create new one for this device
    db_mem_devices_t * newItem = (db_mem_devices_t *) malloc(sizeof(db_mem_devices_t));

    if (newItem) {
        memcpy(newItem->bd_addr, bd_addr, sizeof(bd_addr_t));
        strncpy(newItem->device_name, "", MAX_NAME_LEN);
        memcpy(newItem->link_key, link_key, LINK_KEY_LEN);
	linked_list_add(&db_mem_devices, (linked_item_t *) newItem);
    }
}

static void delete_link_key(bd_addr_t *bd_addr){
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        if (item->bd_addr == *bd_addr){
            // Found record, delete it
            linked_list_remove(&db_mem_devices, (linked_item_t *) item);
            free(item);
            return;
        }
    }
}

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name){
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        if (item->bd_addr == *bd_addr){
            // Found record, ammend it
            strncpy(item->device_name, device_name, MAX_NAME_LEN);
            return;
        }
    }

    // Record not found, create a new one for this device
    db_mem_devices_t * newItem = (db_mem_devices_t *) malloc(sizeof(db_mem_devices_t));

    if (newItem) {
        memcpy(newItem->bd_addr, bd_addr, sizeof(bd_addr_t));
        strncpy(newItem->device_name, device_name, MAX_NAME_LEN);
        memset(newItem->link_key, 0, LINK_KEY_LEN);

	linked_list_add(&db_mem_devices, (linked_item_t *) newItem);
    }
}

static void delete_name(bd_addr_t *bd_addr){
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        if (item->bd_addr == *bd_addr){
            // Found record, delete it
            linked_list_remove(&db_mem_devices, (linked_item_t *) item);
            free(item);
            return;
        }
    }
}

static int  get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
    linked_item_t *it;
    for (it = (linked_item_t *) db_mem_devices; it ; it = it->next){
        db_mem_devices_t * item = (db_mem_devices_t *) it;
        if (item->bd_addr == *bd_addr){
            strncpy(device_name, item->device_name, MAX_NAME_LEN);
            return 1;
        }
    }
    return 0;
}

#pragma mark PERSISTENT RFCOMM CHANNEL ALLOCATION

static uint8_t persistent_rfcomm_channel(char *serviceName){
    // Fake
    return 1;
}


remote_device_db_t remote_device_db_memory = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name,
    persistent_rfcomm_channel
};

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
 
#include "le_device_db.h"

#include <stdio.h>
#include <string.h>
#include "debug.h"

// Central Device db implemenation using static memory
typedef struct le_device_memory_db {

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

    // Signed Writes by remote
    sm_key_t remote_csrk;
    uint32_t remote_counter;

    // Signed Writes by us
    sm_key_t local_csrk;
    uint32_t local_counter;

} le_device_memory_db_t;

#define LE_DEVICE_MEMORY_SIZE 4
#define INVALID_ENTRY_ADDR_TYPE 0xff

static le_device_memory_db_t le_devices[LE_DEVICE_MEMORY_SIZE];

void le_device_db_init(void){
    int i;
    for (i=0;i<LE_DEVICE_MEMORY_SIZE;i++){
        le_device_db_remove(i);
    }
}

// @returns number of device in db
int le_device_db_count(void){
    int i;
    int counter = 0;
    for (i=0;i<LE_DEVICE_MEMORY_SIZE;i++){
        if (le_devices[i].addr_type != INVALID_ENTRY_ADDR_TYPE) counter++;
    }
    return counter;
}

// free device
void le_device_db_remove(int index){
    le_devices[index].addr_type = INVALID_ENTRY_ADDR_TYPE;
}

int le_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk){
    int i;
    int index = -1;
    for (i=0;i<LE_DEVICE_MEMORY_SIZE;i++){
         if (le_devices[i].addr_type == INVALID_ENTRY_ADDR_TYPE){
            index = i;
            break;
         }
    }

    if (index < 0) return -1;

    log_info("Central Device DB adding type %u - %s", addr_type, bd_addr_to_str(addr));
    log_key("irk", irk);

    le_devices[index].addr_type = addr_type;
    memcpy(le_devices[index].addr, addr, 6);
    memcpy(le_devices[index].irk, irk, 16);
    le_devices[index].remote_counter = 0; 

    return index;
}


// get device information: addr type and address
void le_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){
    if (addr_type) *addr_type = le_devices[index].addr_type;
    if (addr) memcpy(addr, le_devices[index].addr, 6);
    if (irk) memcpy(irk, le_devices[index].irk, 16);
}

void le_device_db_encryption_set(int index, uint16_t ediv, uint8_t rand[8], sm_key_t ltk, int key_size, int authenticated, int authorized){
    log_info("Central Device DB set encryption for %u, ediv x%04x, key size %u, authenticated %u, authorized %u",
        index, ediv, key_size, authenticated, authorized);
    le_device_memory_db_t * device = &le_devices[index];
    device->ediv = ediv;
    if (rand) memcpy(device->rand, rand, 8);
    if (ltk) memcpy(device->ltk, ltk, 16);
    device->key_size = key_size;
    device->authenticated = authenticated;
    device->authorized = authorized;
}

void le_device_db_encryption_get(int index, uint16_t * ediv, uint8_t rand[8], sm_key_t ltk, int * key_size, int * authenticated, int * authorized){
    le_device_memory_db_t * device = &le_devices[index];
    log_info("Central Device DB encryption for %u, ediv x%04x, keysize %u, authenticated %u, authorized %u",
        index, device->ediv, device->key_size, device->authenticated, device->authorized);
    if (ediv) *ediv = device->ediv;
    if (rand) memcpy(rand, device->rand, 8);
    if (ltk)  memcpy(ltk, device->ltk, 16);    
    if (key_size) *key_size = device->key_size;
    if (authenticated) *authenticated = device->authenticated;
    if (authorized) *authorized = device->authorized;
}

// get signature key
void le_device_db_remote_csrk_get(int index, sm_key_t csrk){
    if (index < 0 || index >= LE_DEVICE_MEMORY_SIZE){
        log_error("le_device_db_remote_csrk_get called with invalid index %d", index);
        return;
    }
    if (csrk) memcpy(csrk, le_devices[index].remote_csrk, 16);
}

void le_device_db_remote_csrk_set(int index, sm_key_t csrk){
    if (index < 0 || index >= LE_DEVICE_MEMORY_SIZE){
        log_error("le_device_db_remote_csrk_set called with invalid index %d", index);
        return;
    }
    if (csrk) memcpy(le_devices[index].remote_csrk, csrk, 16);
}

void le_device_db_local_csrk_get(int index, sm_key_t csrk){
    if (index < 0 || index >= LE_DEVICE_MEMORY_SIZE){
        log_error("le_device_db_local_csrk_get called with invalid index %d", index);
        return;
    }
    if (csrk) memcpy(csrk, le_devices[index].local_csrk, 16);
}

void le_device_db_local_csrk_set(int index, sm_key_t csrk){
    if (index < 0 || index >= LE_DEVICE_MEMORY_SIZE){
        log_error("le_device_db_local_csrk_set called with invalid index %d", index);
        return;
    }
    if (csrk) memcpy(le_devices[index].local_csrk, csrk, 16);
}

// query last used/seen signing counter
uint32_t le_device_db_remote_counter_get(int index){
    return le_devices[index].remote_counter;
}

// update signing counter
void le_device_db_remote_counter_set(int index, uint32_t counter){
    le_devices[index].remote_counter = counter;
}

// query last used/seen signing counter
uint32_t le_device_db_local_counter_get(int index){
    return le_devices[index].local_counter;
}

// update signing counter
void le_device_db_local_counter_set(int index, uint32_t counter){
    le_devices[index].local_counter = counter;
}

void le_device_db_dump(void){
    log_info("Central Device DB dump, devices: %d", le_device_db_count());
    int i;
    for (i=0;i<LE_DEVICE_MEMORY_SIZE;i++){
        if (le_devices[i].addr_type == INVALID_ENTRY_ADDR_TYPE) continue;
        log_info("%u: %u %s", i, le_devices[i].addr_type, bd_addr_to_str(le_devices[i].addr));
        log_key("irk", le_devices[i].irk);
        log_key("local csrk", le_devices[i].local_csrk);
        log_key("remote csrk", le_devices[i].remote_csrk);
    }
}

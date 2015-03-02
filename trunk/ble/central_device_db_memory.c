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
 
#include "central_device_db.h"

#include <stdio.h>
#include <string.h>
#include "debug.h"

// Central Device db implemenation using static memory
typedef struct central_device_memory_db {

    // Identification
    int addr_type;
    bd_addr_t addr;
    sm_key_t irk;

    // Stored pairing information allows to re-establish an enncrypted connection
    // with a peripheral that doesn't have any persistent memory
    sm_key_t ltk;
    uint16_t ediv;
    uint8_t  rand[8];

    // Signed Writes by remote
    sm_key_t csrk;
    uint32_t remote_counter;

    // Signed Writes to remote (local CSRK is fixed)
    uint32_t local_counter;

} central_device_memory_db_t;

#define CENTRAL_DEVICE_MEMORY_SIZE 4
#define INVALID_ENTRY_ADDR_TYPE 0xff

static central_device_memory_db_t central_devices[CENTRAL_DEVICE_MEMORY_SIZE];

void central_device_db_init(){
    int i;
    for (i=0;i<CENTRAL_DEVICE_MEMORY_SIZE;i++){
        central_device_db_remove(i);
    }
}

// @returns number of device in db
int central_device_db_count(void){
    int i;
    int counter = 0;
    for (i=0;i<CENTRAL_DEVICE_MEMORY_SIZE;i++){
        if (central_devices[i].addr_type != INVALID_ENTRY_ADDR_TYPE) counter++;
    }
    return counter;
}

// free device
void central_device_db_remove(int index){
    central_devices[index].addr_type = INVALID_ENTRY_ADDR_TYPE;
}

int central_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk){
    int i;
    int index = -1;
    for (i=0;i<CENTRAL_DEVICE_MEMORY_SIZE;i++){
         if (central_devices[i].addr_type == INVALID_ENTRY_ADDR_TYPE){
            index = i;
            break;
         }
    }

    if (index < 0) return -1;

    log_info("Central Device DB adding type %u - %s", addr_type, bd_addr_to_str(addr));
    log_key("irk", irk);

    central_devices[index].addr_type = addr_type;
    memcpy(central_devices[index].addr, addr, 6);
    memcpy(central_devices[index].irk, irk, 16);
    central_devices[index].remote_counter = 0; 

    return index;
}


// get device information: addr type and address
void central_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){
    if (addr_type) *addr_type = central_devices[index].addr_type;
    if (addr) memcpy(addr, central_devices[index].addr, 6);
    if (irk) memcpy(irk, central_devices[index].irk, 16);
}

/**
 * @brief set remote encryption info
 * @brief index
 * @brief ediv 
 * @brief rand
 * @brief ltk
 */
void central_device_db_encryption_set(int index, uint16_t ediv, uint8_t rand[8], sm_key_t ltk){
    central_devices[index].ediv = ediv;
    if (rand) memcpy(central_devices[index].rand, rand, 8);
    if (ltk) memcpy(central_devices[index].ltk, ltk, 16);
}

/**
 * @brief get remote encryption info
 * @brief index
 * @brief ediv 
 * @brief rand
 * @brief ltk
 */
void central_device_db_encryption_get(int index, uint16_t * ediv, uint8_t rand[8], sm_key_t ltk){
    if (ediv) *ediv = central_devices[index].ediv;
    if (rand) memcpy(rand, central_devices[index].rand, 8);
    if (ltk)  memcpy(ltk, central_devices[index].ltk, 16);    
}

// get signature key
void central_device_db_csrk_get(int index, sm_key_t csrk){
    if (csrk) memcpy(csrk, central_devices[index].csrk, 16);
}

void central_device_db_csrk_set(int index, sm_key_t csrk){
    if (csrk) memcpy(central_devices[index].csrk, csrk, 16);
}

// query last used/seen signing counter
uint32_t central_device_db_remote_counter_get(int index){
    return central_devices[index].remote_counter;
}

// update signing counter
void central_device_db_remote_counter_set(int index, uint32_t counter){
    central_devices[index].remote_counter = counter;
}

// query last used/seen signing counter
uint32_t central_device_db_local_counter_get(int index){
    return central_devices[index].local_counter;
}

// update signing counter
void central_device_db_local_counter_set(int index, uint32_t counter){
    central_devices[index].local_counter = counter;
}

void central_device_db_dump(){
    log_info("Central Device DB dump, devices: %u", central_device_db_count);
    int i;
    for (i=0;i<CENTRAL_DEVICE_MEMORY_SIZE;i++){
        if (central_devices[i].addr_type == INVALID_ENTRY_ADDR_TYPE) continue;
        log_info("%u: %u %s", i, central_devices[i].addr_type, bd_addr_to_str(central_devices[i].addr));
        log_key("irk", central_devices[i].irk);
        log_key("csrk", central_devices[i].csrk);
    }
}

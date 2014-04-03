/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */
 
#include "central_device_db.h"

#include <stdio.h>
#include <string.h>

// Central Device db implemenation using static memory
typedef struct central_device_memory_db {
    int addr_type;
    bd_addr_t addr;
    sm_key_t csrk;
    sm_key_t irk;
    uint32_t signing_counter;
} central_device_memory_db_t;

#define CENTRAL_DEVICE_MEMORY_SIZE 4
static central_device_memory_db_t central_devices[CENTRAL_DEVICE_MEMORY_SIZE];
static int central_devices_count;

void central_device_db_init(){
    central_devices_count = 0;
}

// @returns number of device in db
int central_device_db_count(void){
    return central_devices_count;
}

// free device - TODO not implemented
void central_device_db_remove(int index){
}

int central_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk, sm_key_t csrk){
    if (central_devices_count >= CENTRAL_DEVICE_MEMORY_SIZE) return -1;

    printf("Central Device DB adding type %u - %s\n", addr_type, bd_addr_to_str(addr));
    print_key("irk", irk);
    print_key("csrk", csrk);

    int index = central_devices_count;
    central_devices_count++;

    central_devices[index].addr_type = addr_type;
    memcpy(central_devices[index].addr, addr, 6);
    memcpy(central_devices[index].csrk, csrk, 16);
    memcpy(central_devices[index].irk, irk, 16);
    central_devices[index].signing_counter = 0; 

    return index;
}


// get device information: addr type and address
void central_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk){
    if (addr_type) *addr_type = central_devices[index].addr_type;
    if (addr) memcpy(addr, central_devices[index].addr, 6);
    if (irk) memcpy(irk, central_devices[index].irk, 16);
}

// get signature key
void central_device_db_csrk(int index, sm_key_t csrk){
    if (csrk) memcpy(csrk, central_devices[index].csrk, 16);
}


// query last used/seen signing counter
uint32_t central_device_db_counter_get(int index){
    return central_devices[index].signing_counter;
}

// update signing counter
void central_device_db_counter_set(int index, uint32_t counter){
    central_devices[index].signing_counter = counter;
}

void central_device_db_dump(){
    printf("Central Device DB dump, devices: %u\n", central_devices_count);
    int i;
    for (i=0;i<central_devices_count;i++){
        printf("%u: %u ", i, central_devices[i].addr_type);
        print_bd_addr(central_devices[i].addr);
        print_key("irk", central_devices[i].irk);
        print_key("csrk", central_devices[i].csrk);
    }
}

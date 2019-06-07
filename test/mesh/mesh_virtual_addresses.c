/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_virtual_addresses.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btstack_util.h"
#include "btstack_memory.h"
#include "mesh_virtual_addresses.h"

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}
// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }

// virtual address management

static mesh_virtual_address_t test_virtual_address;

static void mesh_virtual_address_run(void){
}

uint16_t mesh_virtual_address_register(uint8_t * label_uuid, uint16_t hash){
    // TODO:: check if already exists
    // TODO: calc hash
    test_virtual_address.hash   = hash;
    memcpy(test_virtual_address.label_uuid, label_uuid, 16);
    test_virtual_address.pseudo_dst = 0x8000;
    mesh_virtual_address_run();
    return test_virtual_address.pseudo_dst;
}

void mesh_virtual_address_unregister(uint16_t pseudo_dst){
}

mesh_virtual_address_t * mesh_virtual_address_for_pseudo_dst(uint16_t pseudo_dst){
    if (test_virtual_address.pseudo_dst == pseudo_dst){
        return &test_virtual_address;
    }
    return NULL;
}

mesh_virtual_address_t * mesh_virtual_address_for_label_uuid(uint8_t * label_uuid){
    if (memcmp(label_uuid, test_virtual_address.label_uuid, 16) == 0){
        return &test_virtual_address;
    }
    return NULL;
}
// virtual address iterator

void mesh_virtual_address_iterator_init(mesh_virtual_address_iterator_t * it, uint16_t hash){
    it->first = 1;
    it->hash = hash;
}

int mesh_virtual_address_iterator_has_more(mesh_virtual_address_iterator_t * it){
    if (!it->first) return 0;
    if (it->hash != test_virtual_address.hash) return 0;
    return 1;
}

const mesh_virtual_address_t * mesh_virtual_address_iterator_get_next(mesh_virtual_address_iterator_t * it){
    it->first = 0;
    return &test_virtual_address;
}

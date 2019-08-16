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

#include "mesh/mesh_virtual_addresses.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_util.h"
#include "btstack_memory.h"

// virtual address management

static btstack_linked_list_t mesh_virtual_addresses;
static uint8_t mesh_virtual_addresses_used[MAX_NR_MESH_VIRTUAL_ADDRESSES];

uint16_t mesh_virtual_addresses_get_free_pseudo_dst(void){
    uint16_t i;
    for (i=0;i < MAX_NR_MESH_VIRTUAL_ADDRESSES ; i++){
        if (mesh_virtual_addresses_used[i] == 0){
            return 0x8000+i;
        }
    }
    return MESH_ADDRESS_UNSASSIGNED;
}

void mesh_virtual_address_add(mesh_virtual_address_t * virtual_address){
    mesh_virtual_addresses_used[virtual_address->pseudo_dst-0x8000] = 1;
    virtual_address->ref_count = 0;
    btstack_linked_list_add(&mesh_virtual_addresses, (void *) virtual_address);
}

void mesh_virtual_address_remove(mesh_virtual_address_t * virtual_address){
    btstack_linked_list_remove(&mesh_virtual_addresses, (void *) virtual_address);
    mesh_virtual_addresses_used[virtual_address->pseudo_dst-0x8000] = 0;
}

// helper
mesh_virtual_address_t * mesh_virtual_address_register(uint8_t * label_uuid, uint16_t hash){
    uint16_t pseudo_dst = mesh_virtual_addresses_get_free_pseudo_dst();
    if (pseudo_dst == 0) return NULL;
    mesh_virtual_address_t * virtual_address = btstack_memory_mesh_virtual_address_get();
    if (virtual_address == NULL) return NULL;

    virtual_address->hash = hash;
    virtual_address->pseudo_dst = pseudo_dst;
    memcpy(virtual_address->label_uuid, label_uuid, 16);
    mesh_virtual_address_add(virtual_address);

    return virtual_address;
}

mesh_virtual_address_t * mesh_virtual_address_for_pseudo_dst(uint16_t pseudo_dst){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_virtual_addresses);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_virtual_address_t * item = (mesh_virtual_address_t *) btstack_linked_list_iterator_next(&it);
        if (item->pseudo_dst == pseudo_dst) return item;
    }
    return NULL;
}

mesh_virtual_address_t * mesh_virtual_address_for_label_uuid(uint8_t * label_uuid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_virtual_addresses);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_virtual_address_t * item = (mesh_virtual_address_t *) btstack_linked_list_iterator_next(&it);
        if (memcmp(item->label_uuid, label_uuid, 16) == 0) return item;
    }
    return NULL;
}
// virtual address iterator

void mesh_virtual_address_iterator_init(mesh_virtual_address_iterator_t * it, uint16_t hash){
    btstack_linked_list_iterator_init(&it->it, &mesh_virtual_addresses);
    it->hash = hash;
    it->address = NULL;
}

int mesh_virtual_address_iterator_has_more(mesh_virtual_address_iterator_t * it){
    // find next matching key
    while (1){
        printf("check %p\n", it->address);
        if (it->address && it->address->hash == it->hash) return 1;
        if (!btstack_linked_list_iterator_has_next(&it->it)) break;
        it->address = (mesh_virtual_address_t *) btstack_linked_list_iterator_next(&it->it);
    }
    return 0;
}

const mesh_virtual_address_t * mesh_virtual_address_iterator_get_next(mesh_virtual_address_iterator_t * it){
    mesh_virtual_address_t * address = it->address;
    it->address = NULL;
    return address;
}

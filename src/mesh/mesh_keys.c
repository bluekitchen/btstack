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

#define BTSTACK_FILE__ "mesh_keys.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_keys.h"

#include "btstack_util.h"
#include "btstack_memory.h"
#include "btstack_config.h"

// network key list
static btstack_linked_list_t network_keys;
static uint8_t mesh_network_key_used[MAX_NR_MESH_NETWORK_KEYS];

void mesh_network_key_init(void){
    network_keys = NULL;
}

uint16_t mesh_network_key_get_free_index(void){
    uint16_t i;
    for (i=0;i < MAX_NR_MESH_NETWORK_KEYS ; i++){
        if (mesh_network_key_used[i] == 0){
            return i;
        }
    }
    return MESH_KEYS_INVALID_INDEX;
}

void mesh_network_key_add(mesh_network_key_t * network_key){
    mesh_network_key_used[network_key->internal_index] = 1;
    btstack_linked_list_add_tail(&network_keys, (btstack_linked_item_t *) network_key);
}

bool mesh_network_key_remove(mesh_network_key_t * network_key){
    mesh_network_key_used[network_key->internal_index] = 0;
    return btstack_linked_list_remove(&network_keys, (btstack_linked_item_t *) network_key);
}

mesh_network_key_t * mesh_network_key_list_get(uint16_t netkey_index){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &network_keys);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_network_key_t * item = (mesh_network_key_t *) btstack_linked_list_iterator_next(&it);
        if (item->netkey_index == netkey_index) return item;
    }
    return NULL;
}

int mesh_network_key_list_count(void){
    return btstack_linked_list_count(&network_keys);
}

// mesh network key iterator over all keys
void mesh_network_key_iterator_init(mesh_network_key_iterator_t *it){
    btstack_linked_list_iterator_init(&it->it, &network_keys);
}

int mesh_network_key_iterator_has_more(mesh_network_key_iterator_t *it){
    return btstack_linked_list_iterator_has_next(&it->it);
}

mesh_network_key_t * mesh_network_key_iterator_get_next(mesh_network_key_iterator_t *it){
    return (mesh_network_key_t *) btstack_linked_list_iterator_next(&it->it);
}

// mesh network key iterator for a given nid
void mesh_network_key_nid_iterator_init(mesh_network_key_iterator_t *it, uint8_t nid){
    btstack_linked_list_iterator_init(&it->it, &network_keys);
    it->key = NULL;
    it->nid = nid;
}

int mesh_network_key_nid_iterator_has_more(mesh_network_key_iterator_t *it){
    // find next matching key
    while (true){
        if (it->key && it->key->nid == it->nid) return 1;
        if (!btstack_linked_list_iterator_has_next(&it->it)) break;
        it->key = (mesh_network_key_t *) btstack_linked_list_iterator_next(&it->it);
    }
    return 0;
}

mesh_network_key_t * mesh_network_key_nid_iterator_get_next(mesh_network_key_iterator_t *it){
    mesh_network_key_t * key = it->key;
    it->key = NULL;
    return key;
}


// application key list

// key management
static mesh_transport_key_t   mesh_transport_device_key;
static btstack_linked_list_t  application_keys;

static uint8_t mesh_transport_key_used[MAX_NR_MESH_TRANSPORT_KEYS];

void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.appkey_index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    mesh_transport_device_key.akf   = 0;
    mesh_transport_device_key.netkey_index = 0; // unused
    (void)memcpy(mesh_transport_device_key.key, device_key, 16);

    // use internal slot #0
    mesh_transport_device_key.internal_index = 0;
    mesh_transport_key_used[0] = 1;
}

uint16_t mesh_transport_key_get_free_index(void){
    // find empty slot, skip slot #0 reserved for device key
    uint16_t i;
    for (i=1;i < MAX_NR_MESH_TRANSPORT_KEYS ; i++){
        if (mesh_transport_key_used[i] == 0){
            return i;
        }
    }
    return 0;
}

void mesh_transport_key_add(mesh_transport_key_t * transport_key){
    mesh_transport_key_used[transport_key->internal_index] = 1;
    btstack_linked_list_add_tail(&application_keys, (btstack_linked_item_t *) transport_key);
}

bool mesh_transport_key_remove(mesh_transport_key_t * transport_key){
    mesh_transport_key_used[transport_key->internal_index] = 0;
    return btstack_linked_list_remove(&application_keys, (btstack_linked_item_t *) transport_key);
}

mesh_transport_key_t * mesh_transport_key_get(uint16_t appkey_index){
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        return &mesh_transport_device_key;
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &application_keys);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_transport_key_t * item = (mesh_transport_key_t *) btstack_linked_list_iterator_next(&it);
        if (item->appkey_index == appkey_index) return item;
    }
    return NULL;
}

// key iterator
void mesh_transport_key_iterator_init(mesh_transport_key_iterator_t *it, uint16_t netkey_index){
    btstack_linked_list_iterator_init(&it->it, &application_keys);
    it->netkey_index = netkey_index;
    it->key = NULL;
}

int mesh_transport_key_iterator_has_more(mesh_transport_key_iterator_t *it){
    // find next matching key
    while (true){
        if (it->key && it->key->netkey_index == it->netkey_index) return 1;
        if (!btstack_linked_list_iterator_has_next(&it->it)) break;
        it->key = (mesh_transport_key_t *) btstack_linked_list_iterator_next(&it->it);
    }
    return 0;
}

mesh_transport_key_t * mesh_transport_key_iterator_get_next(mesh_transport_key_iterator_t *it){
    mesh_transport_key_t * key = it->key;
    it->key = NULL;
    return key;
}

void
mesh_transport_key_aid_iterator_init(mesh_transport_key_iterator_t *it, uint16_t netkey_index, uint8_t akf, uint8_t aid) {
    btstack_linked_list_iterator_init(&it->it, &application_keys);
    it->netkey_index = netkey_index;
    it->aid      = aid;
    it->akf      = akf;
    if (it->akf){
        it->key = NULL;
    } else {
        it->key = &mesh_transport_device_key;
    }
}

int mesh_transport_key_aid_iterator_has_more(mesh_transport_key_iterator_t *it){
    if (it->akf == 0){
        return it->key != NULL;
    }
    // find next matching key
    while (true){
        if (it->key && it->key->aid == it->aid && it->key->netkey_index == it->netkey_index) return 1;
        if (!btstack_linked_list_iterator_has_next(&it->it)) break;
        it->key = (mesh_transport_key_t *) btstack_linked_list_iterator_next(&it->it);
    }
    return 0;
}

mesh_transport_key_t * mesh_transport_key_aid_iterator_get_next(mesh_transport_key_iterator_t *it){
    mesh_transport_key_t * key = it->key;
    it->key = NULL;
    return key;
}

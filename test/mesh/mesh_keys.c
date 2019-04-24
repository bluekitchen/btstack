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

#define __BTSTACK_FILE__ "mesh_keys.c"

#include <string.h>
#include <stdio.h>
#include "mesh_keys.h"
#include "btstack_util.h"
#include "btstack_memory.h"


static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}

// network key list
static btstack_linked_list_t network_keys;

void mesh_network_key_init(void){
    network_keys = NULL;
}

void mesh_network_key_add(mesh_network_key_t * network_key){
    btstack_linked_list_add(&network_keys, (btstack_linked_item_t *) network_key);
}

int mesh_network_key_remove(mesh_network_key_t * network_key){
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
    while (1){
        if (it->key && it->key->nid == it->nid) return 1;
        if (!btstack_linked_list_iterator_has_next(&it->it)) break;
        it->key = (mesh_network_key_t *) btstack_linked_list_iterator_next(&it->it);
    }
    return 0;
}

const mesh_network_key_t * mesh_network_key_nid_iterator_get_next(mesh_network_key_iterator_t *it){
    mesh_network_key_t * key = it->key;
    it->key = NULL;
    return key;
}

// ---

void mesh_network_key_list_add_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){

    // get key
    mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();

    // get single instance
    memset(network_key, 0, sizeof(mesh_network_key_t));

    // NetKey
    // memcpy(network_key->net_key, provisioning_data, net_key);

    // IdentityKey
    // memcpy(network_key->identity_key, provisioning_data->identity_key, 16);

    // BeaconKey
    memcpy(network_key->beacon_key, provisioning_data->beacon_key, 16);

    // NID
    network_key->nid = provisioning_data->nid;

    // EncryptionKey
    memcpy(network_key->encryption_key, provisioning_data->encryption_key, 16);

    // PrivacyKey
    memcpy(network_key->privacy_key, provisioning_data->privacy_key, 16);

    // NetworkID
    memcpy(network_key->network_id, provisioning_data->network_id, 8);

    mesh_network_key_add(network_key);
}

// application key list

// key management
static mesh_transport_key_t   test_application_key;
static mesh_transport_key_t   mesh_transport_device_key;

void mesh_application_key_set(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key){
    test_application_key.index = appkey_index;
    test_application_key.aid   = aid;
    test_application_key.akf   = 1;
    memcpy(test_application_key.key, application_key, 16);
}

void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    mesh_transport_device_key.akf   = 0;
    memcpy(mesh_transport_device_key.key, device_key, 16);
}

const mesh_transport_key_t * mesh_transport_key_get(uint16_t appkey_index){
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        return &mesh_transport_device_key;
    }
    if (appkey_index != test_application_key.index) return NULL;
    return &test_application_key;
}

// key iterator


void mesh_transport_key_iterator_init(mesh_transport_key_iterator_t *it, uint8_t akf, uint8_t aid){
    it->aid      = aid;
    it->akf      = akf;
    it->first    = 1;
}

int mesh_transport_key_iterator_has_more(mesh_transport_key_iterator_t *it){
    if (!it->first) return 0;
    if (it->akf){
        return it->aid == test_application_key.aid;
    } else {
        return 1;
    }
}

const mesh_transport_key_t * mesh_transport_key_iterator_get_next(mesh_transport_key_iterator_t *it){
    it->first = 0;
    if (it->akf){
        return &test_application_key;
    } else {
        return &mesh_transport_device_key;
    }
}

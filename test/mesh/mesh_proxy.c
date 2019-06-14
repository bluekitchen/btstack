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

#define __BTSTACK_FILE__ "mesh_proxy.c"

#include <string.h>

#include "ble/mesh/adv_bearer.h"
#include "ble/mesh/mesh_crypto.h"
#include "bluetooth_company_id.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_config.h"
#include "btstack_crypto.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "mesh_proxy.h"
#include "mesh_foundation.h"

#ifdef ENABLE_MESH_PROXY_SERVER

// we only support a single active node id advertisement. when new one is started, an active one is stopped

#define MESH_PROXY_NODE_ID_ADVERTISEMENT_TIMEOUT_MS 60000

static btstack_timer_source_t                           mesh_proxy_node_id_timer;
static btstack_crypto_random_t                          mesh_proxy_node_id_crypto_request_random;
static btstack_crypto_aes128_t                          mesh_proxy_node_id_crypto_request_aes128;
static uint8_t                                          mesh_proxy_node_id_plaintext[16];
static uint8_t                                          mesh_proxy_node_id_hash[16];
static uint8_t                                          mesh_proxy_node_id_random_value[8];

static uint16_t primary_element_address;

// Mesh Proxy, advertise with node id
static adv_bearer_connectable_advertisement_data_item_t connectable_advertisement_item;

static const uint8_t adv_data_with_node_identity_template[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8,
    // Service Data
    0x14, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8, 
          // MESH_IDENTIFICATION_NODE_IDENTIFY_TYPE
          MESH_IDENTIFICATION_NODE_IDENTIFY_TYPE, 
          // Hash - 8 bytes
          // Random - 8 bytes
};

static void mesh_proxy_stop_all_advertising_with_node_id(void){
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        if (network_key->node_id_advertisement_running != 0){
            adv_bearer_advertisements_remove_item(&network_key->advertisement_with_network_id);
            btstack_run_loop_remove_timer(&mesh_proxy_node_id_timer);
            network_key->node_id_advertisement_running = 0;
        }
    }
}

static void mesh_proxy_node_id_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    mesh_proxy_stop_all_advertising_with_node_id();
}

static void mesh_proxy_node_id_handle_get_aes128(void * arg){
    mesh_network_key_t * network_key = (mesh_network_key_t *) arg;

    memcpy(connectable_advertisement_item.adv_data, adv_data_with_node_identity_template, 12);
    memcpy(&connectable_advertisement_item.adv_data[12], &mesh_proxy_node_id_hash[8], 8);
    memcpy(&connectable_advertisement_item.adv_data[20], mesh_proxy_node_id_random_value, 8);
    
    // setup advertisements
    adv_bearer_advertisements_add_item(&connectable_advertisement_item);
    adv_bearer_advertisements_enable(1);

    // set timer
    btstack_run_loop_set_timer_handler(&mesh_proxy_node_id_timer, mesh_proxy_node_id_timeout_handler);
    btstack_run_loop_set_timer(&mesh_proxy_node_id_timer, MESH_PROXY_NODE_ID_ADVERTISEMENT_TIMEOUT_MS);
    btstack_run_loop_add_timer(&mesh_proxy_node_id_timer);

    // mark as active
    network_key->node_id_advertisement_running = 1;
}

static void mesh_proxy_node_id_handle_random(void * arg){
    mesh_network_key_t * network_key = (mesh_network_key_t *) arg;

    // Hash = e(IdentityKey, Padding | Random | Address) mod 2^64
    memset(mesh_proxy_node_id_plaintext, 0, sizeof(mesh_proxy_node_id_plaintext));
    memcpy(&mesh_proxy_node_id_plaintext[6] , mesh_proxy_node_id_random_value, 8);
    big_endian_store_16(mesh_proxy_node_id_plaintext, 14, primary_element_address);
    btstack_crypto_aes128_encrypt(&mesh_proxy_node_id_crypto_request_aes128, network_key->identity_key, mesh_proxy_node_id_plaintext, mesh_proxy_node_id_hash, mesh_proxy_node_id_handle_get_aes128, network_key);
}

static void mesh_proxy_start_advertising_with_node_id(uint16_t netkey_index){
    mesh_proxy_stop_all_advertising_with_node_id();
    // get network key
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL) return;
    log_info("Proxy start advertising with node id, netkey index %04x", netkey_index);
    // setup node id
    btstack_crypto_random_generate(&mesh_proxy_node_id_crypto_request_random, mesh_proxy_node_id_random_value, sizeof(mesh_proxy_node_id_random_value), mesh_proxy_node_id_handle_random, network_key);
}

static void mesh_proxy_stop_advertising_with_node_id(uint16_t netkey_index){
    UNUSED(netkey_index);
    log_info("Proxy stop advertising with node id, netkey index %04x", netkey_index);
    mesh_proxy_stop_all_advertising_with_node_id();
}

// Public API

uint8_t mesh_proxy_get_advertising_with_node_id_status(uint16_t netkey_index, mesh_node_identity_state_t * out_state ){
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        *out_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED;
        return MESH_FOUNDATION_STATUS_SUCCESS;
    }

#ifdef ENABLE_MESH_PROXY_SERVER
    if (network_key->node_id_advertisement_running == 0){
        *out_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED;
    } else {
        *out_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING;
    }
#else
    *out_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED;
#endif

    return MESH_FOUNDATION_STATUS_SUCCESS;
}

uint8_t mesh_proxy_set_advertising_with_node_id(uint16_t netkey_index, mesh_node_identity_state_t state){
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        return  MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    }

#ifdef ENABLE_MESH_PROXY_SERVER
    switch (state){
        case MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED:
            mesh_proxy_stop_advertising_with_node_id(netkey_index);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        case MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING:
            mesh_proxy_start_advertising_with_node_id(netkey_index);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        default:
            break;
    }
#endif

    return MESH_FOUNDATION_STATUS_FEATURE_NOT_SUPPORTED;
}


void mesh_proxy_start_advertising_with_network_id(void){
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        log_info("Proxy start advertising with network id, netkey index %04x", network_key->netkey_index);
        adv_bearer_advertisements_add_item(&network_key->advertisement_with_network_id);
    }
    adv_bearer_advertisements_enable(1);
}

void mesh_proxy_stop_advertising_with_network_id(void){  
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        log_info("Proxy stop advertising with network id, netkey index %04x", network_key->netkey_index);
        adv_bearer_advertisements_remove_item(&network_key->advertisement_with_network_id);
    }
}

void mesh_proxy_init(uint16_t primary_unicast_address){
    primary_element_address = primary_unicast_address;
}

#endif


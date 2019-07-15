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

#define __BTSTACK_FILE__ "mesh.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh.h"

#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_tlv.h"
#include "btstack_memory.h"

#include "mesh/adv_bearer.h"
#include "mesh/beacon.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_configuration_server.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_generic_server.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_peer.h"
#include "mesh/mesh_proxy.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/mesh_virtual_addresses.h"
#include "mesh/pb_adv.h"
#include "mesh/pb_gatt.h"
#include "mesh/provisioning.h"
#include "mesh/provisioning_device.h"

// Persistent storage structures

typedef struct {
    uint16_t netkey_index;

    uint8_t  version;

    // net_key from provisioner or Config Model Client
    uint8_t net_key[16];

    // derived data

    // k1
    uint8_t identity_key[16];
    uint8_t beacon_key[16];

    // k3
    uint8_t network_id[8];

    // k2
    uint8_t nid;
    uint8_t encryption_key[16];
    uint8_t privacy_key[16];
} mesh_persistent_net_key_t;

typedef struct {
    uint16_t netkey_index;
    uint16_t appkey_index;
    uint8_t  aid;
    uint8_t  version;
    uint8_t  key[16];
} mesh_persistent_app_key_t;

typedef struct {
    uint8_t gatt_proxy;
    uint8_t beacon;
    uint8_t default_ttl;
    uint8_t network_transmit;
    uint8_t relay;
    uint8_t relay_retransmit;
    uint8_t friend;
} mesh_persistent_foundation_t;

typedef struct {
    uint32_t iv_index;
    uint32_t seq_number;
} iv_index_and_sequence_number_t;

static btstack_packet_handler_t provisioning_device_packet_handler;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static int provisioned;

// Mandatory Confiuration Server 
static mesh_model_t                 mesh_configuration_server_model;

// Mandatory Health Server
static mesh_model_t                 mesh_health_server_model;
static mesh_configuration_server_model_context_t mesh_configuration_server_model_context;

// Random UUID on start
static btstack_crypto_random_t mesh_access_crypto_random;
static uint8_t random_device_uuid[16];

// TLV
static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

// IV Index persistence
static uint32_t sequence_number_last_stored;
static uint32_t sequence_number_storage_trigger;


void mesh_access_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){

    // set iv_index and iv index update active
    int iv_index_update_active = (provisioning_data->flags & 2) >> 1;
    mesh_iv_index_recovered(iv_index_update_active, provisioning_data->iv_index);

    // set unicast address
    mesh_node_primary_element_address_set(provisioning_data->unicast_address);

    // set device_key
    mesh_transport_set_device_key(provisioning_data->device_key);

    if (provisioning_data->network_key){

        // setup primary network with provisioned netkey
        mesh_network_key_add(provisioning_data->network_key);

        // setup primary network
        mesh_subnet_setup_for_netkey_index(provisioning_data->network_key->netkey_index);

        // start sending Secure Network Beacons
        mesh_subnet_t * provisioned_subnet = mesh_subnet_get_by_netkey_index(provisioning_data->network_key->netkey_index);
        beacon_secure_network_start(provisioned_subnet);
    }

    // Mesh Proxy
#ifdef ENABLE_MESH_PROXY_SERVER
    // Setup Proxy
    mesh_proxy_init(provisioning_data->unicast_address);
    mesh_proxy_start_advertising_with_network_id();
#endif
}

static void mesh_access_setup_unprovisioned_device(void * arg){
    // set random value
    if (arg == NULL){
        mesh_node_set_device_uuid(random_device_uuid);
    }

#ifdef ENABLE_MESH_PB_ADV
    // PB-ADV    
    beacon_unprovisioned_device_start(mesh_node_get_device_uuid(), 0);
#endif
#ifdef ENABLE_MESH_PB_GATT
    mesh_proxy_start_advertising_unprovisioned_device();
#endif
}

void mesh_access_setup_without_provisiong_data(void){
    const uint8_t * device_uuid = mesh_node_get_device_uuid();
    if (device_uuid){
        mesh_access_setup_unprovisioned_device((void *)device_uuid);
    } else{
        btstack_crypto_random_generate(&mesh_access_crypto_random, random_device_uuid, 16, &mesh_access_setup_unprovisioned_device, NULL);
    }
}

static void mesh_provisioning_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    mesh_provisioning_data_t provisioning_data;

    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_PROV_COMPLETE:
                    // get provisioning data
                    provisioning_device_data_get(&provisioning_data);

                    // and store in TLV
                    mesh_node_store_provisioning_data(&provisioning_data);

                    // setup node after provisioned
                    mesh_access_setup_from_provisioning_data(&provisioning_data);

                    // start advertising with node id after provisioning
                    mesh_proxy_set_advertising_with_node_id(provisioning_data.network_key->netkey_index, MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING);

                    provisioned = 1;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (provisioning_device_packet_handler == NULL) return;

    // forward
    (*provisioning_device_packet_handler)(packet_type, channel, packet, size);
}

static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // get TLV instance
                    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);

                    // startup from provisioning data stored in TLV
                    provisioned = mesh_node_startup_from_tlv();
                    break;
                
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // enable PB_GATT
                    if (provisioned == 0){
                        mesh_proxy_start_advertising_unprovisioned_device();
                    } else {
#ifdef ENABLE_MESH_PROXY_SERVER
                        mesh_proxy_start_advertising_with_network_id();
#endif
                    }
                    break;
                    
                case HCI_EVENT_LE_META:
                    if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
                    // disable PB_GATT
                    mesh_proxy_stop_advertising_unprovisioned_device();
                    break;
                default:
                    break;
            }
            break;
    }
}

// Foundation state
static const uint32_t mesh_foundation_state_tag = ((uint32_t) 'M' << 24) | ((uint32_t) 'F' << 16)  | ((uint32_t) 'N' << 8) | ((uint32_t) 'D' << 8);

void mesh_foundation_state_load(void){
    mesh_persistent_foundation_t data;

    int foundation_state_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
    if (foundation_state_len != sizeof(data)) return;

    mesh_foundation_gatt_proxy_set(data.gatt_proxy);
    mesh_foundation_beacon_set(data.gatt_proxy);
    mesh_foundation_default_ttl_set(data.default_ttl);
    mesh_foundation_friend_set(data.friend);
    mesh_foundation_network_transmit_set(data.network_transmit);
    mesh_foundation_relay_set(data.relay);
    mesh_foundation_relay_retransmit_set(data.relay_retransmit);
}

void mesh_foundation_state_store(void){
    mesh_persistent_foundation_t data;
    data.gatt_proxy       = mesh_foundation_gatt_proxy_get();
    data.gatt_proxy       = mesh_foundation_beacon_get();
    data.default_ttl      = mesh_foundation_default_ttl_get();
    data.friend           = mesh_foundation_friend_get();
    data.network_transmit = mesh_foundation_network_transmit_get();
    data.relay            = mesh_foundation_relay_get();
    data.relay_retransmit = mesh_foundation_relay_retransmit_get();
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
}

// Mesh Network Keys
static uint32_t mesh_network_key_tag_for_internal_index(uint16_t internal_index){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'N' << 16) | ((uint32_t) internal_index);
}

void mesh_store_network_key(mesh_network_key_t * network_key){
    mesh_persistent_net_key_t data;
    printf("Store NetKey: internal index 0x%x, NetKey Index 0x%06x, NID %02x: ", network_key->internal_index, network_key->netkey_index, network_key->nid);
    printf_hexdump(network_key->net_key, 16);
    uint32_t tag = mesh_network_key_tag_for_internal_index(network_key->internal_index);
    data.netkey_index = network_key->netkey_index;
    memcpy(data.net_key, network_key->net_key, 16);
    memcpy(data.identity_key, network_key->identity_key, 16);
    memcpy(data.beacon_key, network_key->beacon_key, 16);
    memcpy(data.network_id, network_key->network_id, 8);
    data.nid = network_key->nid;
    data.version = network_key->version;
    memcpy(data.encryption_key, network_key->encryption_key, 16);
    memcpy(data.privacy_key, network_key->privacy_key, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(mesh_persistent_net_key_t));
}

void mesh_delete_network_key(uint16_t internal_index){
    uint32_t tag = mesh_network_key_tag_for_internal_index(internal_index);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}


void mesh_load_network_keys(void){
    printf("Load Network Keys\n");
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_NETWORK_KEYS; internal_index++){
        mesh_persistent_net_key_t data;
        uint32_t tag = mesh_network_key_tag_for_internal_index(internal_index);
        int netkey_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (netkey_len != sizeof(mesh_persistent_net_key_t)) continue;
        
        mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();
        if (network_key == NULL) return;

        network_key->netkey_index = data.netkey_index;
        memcpy(network_key->net_key, data.net_key, 16);
        memcpy(network_key->identity_key, data.identity_key, 16);
        memcpy(network_key->beacon_key, data.beacon_key, 16);
        memcpy(network_key->network_id, data.network_id, 8);
        network_key->nid = data.nid;
        network_key->version = data.version;
        memcpy(network_key->encryption_key, data.encryption_key, 16);
        memcpy(network_key->privacy_key, data.privacy_key, 16);

#ifdef ENABLE_GATT_BEARER
        // setup advertisement with network id
        network_key->advertisement_with_network_id.adv_length = mesh_proxy_setup_advertising_with_network_id(network_key->advertisement_with_network_id.adv_data, network_key->network_id);
#endif

        mesh_network_key_add(network_key);

        mesh_subnet_setup_for_netkey_index(network_key->netkey_index);

        printf("- internal index 0x%x, NetKey Index 0x%06x, NID %02x: ", network_key->internal_index, network_key->netkey_index, network_key->nid);
        printf_hexdump(network_key->net_key, 16);
    }
}

void mesh_delete_network_keys(void){
    printf("Delete Network Keys\n");
    
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_NETWORK_KEYS; internal_index++){
        mesh_delete_network_key(internal_index);
    }
}

// Mesh App Keys

static uint32_t mesh_transport_key_tag_for_internal_index(uint16_t internal_index){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'A' << 16) | ((uint32_t) internal_index);
}

void mesh_store_app_key(mesh_transport_key_t * app_key){
    mesh_persistent_app_key_t data;
    printf("Store AppKey: internal index 0x%x, AppKey Index 0x%06x, AID %02x: ", app_key->internal_index, app_key->appkey_index, app_key->aid);
    printf_hexdump(app_key->key, 16);
    uint32_t tag = mesh_transport_key_tag_for_internal_index(app_key->internal_index);
    data.netkey_index = app_key->netkey_index;
    data.appkey_index = app_key->appkey_index;
    data.aid = app_key->aid;
    data.version = app_key->version;
    memcpy(data.key, app_key->key, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
}

void mesh_delete_app_key(uint16_t internal_index){
    uint32_t tag = mesh_transport_key_tag_for_internal_index(internal_index);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_app_keys(void){
    printf("Load App Keys\n");
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_TRANSPORT_KEYS; internal_index++){
        mesh_persistent_app_key_t data;
        uint32_t tag = mesh_transport_key_tag_for_internal_index(internal_index);
        int app_key_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (app_key_len == 0) continue;
        
        mesh_transport_key_t * key = btstack_memory_mesh_transport_key_get();
        if (key == NULL) return;

        key->internal_index = internal_index;
        key->appkey_index = data.appkey_index;
        key->netkey_index = data.netkey_index;
        key->aid          = data.aid;
        key->akf          = 1;
        key->version      = data.version;
        memcpy(key->key, data.key, 16);
        mesh_transport_key_add(key);
        printf("- internal index 0x%x, AppKey Index 0x%06x, AID %02x: ", key->internal_index, key->appkey_index, key->aid);
        printf_hexdump(key->key, 16);
    }
}

void mesh_delete_app_keys(void){
    printf("Delete App Keys\n");
    
    uint16_t internal_index;
    for (internal_index = 0; internal_index < MAX_NR_MESH_TRANSPORT_KEYS; internal_index++){
        mesh_delete_app_key(internal_index);
    }
}


// Model to Appkey List

static uint32_t mesh_model_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'B' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_load_appkey_list(mesh_model_t * model){
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->appkey_indices, sizeof(model->appkey_indices));
}

static void mesh_store_appkey_list(mesh_model_t * model){
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->appkey_indices, sizeof(model->appkey_indices));
}

static void mesh_delete_appkey_list(mesh_model_t * model){
    uint32_t tag = mesh_model_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_appkey_lists(void){
    printf("Load Appkey Lists\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_load_appkey_list(model);
        }
    }
}

void mesh_delete_appkey_lists(void){
    printf("Delete Appkey Lists\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_delete_appkey_list(model);
        }
    }
}

void mesh_model_reset_appkeys(mesh_model_t * mesh_model){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
    }
}

uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) return MESH_FOUNDATION_STATUS_SUCCESS;
    }
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == MESH_APPKEY_INVALID) {
            mesh_model->appkey_indices[i] = appkey_index;
            mesh_store_appkey_list(mesh_model);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        }
    }
    return MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
}

void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) {
            mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
            mesh_store_appkey_list(mesh_model);
        }
    }
}

int mesh_model_contains_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) return 1;
    }
    return 0;
}

void mesh_access_netkey_finalize(mesh_network_key_t * network_key){
    mesh_network_key_remove(network_key);
    mesh_delete_network_key(network_key->internal_index);
    btstack_memory_mesh_network_key_free(network_key);
}

void mesh_access_appkey_finalize(mesh_transport_key_t * transport_key){
    mesh_transport_key_remove(transport_key);
    mesh_delete_app_key(transport_key->appkey_index);
    btstack_memory_mesh_transport_key_free(transport_key);
}

void mesh_access_key_refresh_revoke_keys(mesh_subnet_t * subnet){
    // delete old netkey index
    mesh_access_netkey_finalize(subnet->old_key);
    subnet->old_key = subnet->new_key;
    subnet->new_key = NULL;

    // delete old appkeys, if any
    mesh_transport_key_iterator_t it;
    mesh_transport_key_iterator_init(&it, subnet->netkey_index);
    while (mesh_transport_key_iterator_has_more(&it)){
        mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
        if (transport_key->old_key == 0) continue;
        mesh_access_appkey_finalize(transport_key);
    }
}

// Mesh IV Index
static uint32_t mesh_tag_for_iv_index_and_seq_number(void){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'F' << 16) | ((uint32_t) 'I' << 9) | ((uint32_t) 'S');
}

void mesh_store_iv_index_after_provisioning(uint32_t iv_index){
    iv_index_and_sequence_number_t data;
    uint32_t tag = mesh_tag_for_iv_index_and_seq_number();
    data.iv_index   = iv_index;
    data.seq_number = 0;
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));

    sequence_number_last_stored = data.seq_number;
    sequence_number_storage_trigger = sequence_number_last_stored + MESH_SEQUENCE_NUMBER_STORAGE_INTERVAL;
}

void mesh_store_iv_index_and_sequence_number(void){
    iv_index_and_sequence_number_t data;
    uint32_t tag = mesh_tag_for_iv_index_and_seq_number();
    data.iv_index   = mesh_get_iv_index();
    data.seq_number = mesh_sequence_number_peek();
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));

    sequence_number_last_stored = data.seq_number;
    sequence_number_storage_trigger = sequence_number_last_stored + MESH_SEQUENCE_NUMBER_STORAGE_INTERVAL;
}

int mesh_load_iv_index_and_sequence_number(uint32_t * iv_index, uint32_t * sequence_number){
    iv_index_and_sequence_number_t data;
    uint32_t tag = mesh_tag_for_iv_index_and_seq_number();
    uint32_t len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
    if (len == sizeof(iv_index_and_sequence_number_t)){
        *iv_index = data.iv_index;
        *sequence_number = data.seq_number;
        return 1;
    }
    return 0;
}

// higher layer
static void mesh_persist_iv_index_and_sequence_number(void){
    if (mesh_sequence_number_peek() >= sequence_number_storage_trigger){
        mesh_store_iv_index_and_sequence_number();
    }
}


static void mesh_node_setup_default_models(void){
    // configure Config Server
    mesh_configuration_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
    mesh_configuration_server_model.model_data       = &mesh_configuration_server_model_context;
    mesh_configuration_server_model.operations       = mesh_configuration_server_get_operations();    
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_configuration_server_model);

    // Config Health Server
    mesh_health_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER);
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_health_server_model);
}

void mesh_init(void){

    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // ADV Bearer also used for GATT Proxy Advertisements and PB-GATT
    adv_bearer_init();

#ifdef ENABLE_MESH_GATT_BEARER
    // Setup GATT bearer
    gatt_bearer_init();
#endif

#ifdef ENABLE_MESH_ADV_BEARER
    // Setup Unprovisioned Device Beacon
    beacon_init();
#endif

    provisioning_device_init();

    // Node Configuration
    mesh_node_init();

    // Network layer
    mesh_network_init();

    // Transport layers (lower + upper))
    mesh_lower_transport_init();
    mesh_upper_transport_init();

    // Access layer
    mesh_access_init();

    // Add mandatory models: Config Server and Health Server
    mesh_node_setup_default_models();

    // register for seq number updates
    mesh_sequence_number_set_update_callback(&mesh_persist_iv_index_and_sequence_number);
}

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler){
    provisioning_device_packet_handler = packet_handler;
    provisioning_device_register_packet_handler(&mesh_provisioning_message_handler);
}

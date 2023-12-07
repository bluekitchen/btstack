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

#define BTSTACK_FILE__ "mesh.c"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "mesh/mesh.h"

#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_tlv.h"
#include "btstack_memory.h"
#include "btstack_debug.h"

#include "mesh/adv_bearer.h"
#include "mesh/beacon.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_configuration_server.h"
#include "mesh/mesh_health_server.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_generic_model.h"
#include "mesh/mesh_generic_on_off_server.h"
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
#include "mesh/provisioning_provisioner.h"

static void mesh_node_store_provisioning_data(mesh_provisioning_data_t * provisioning_data);
static int mesh_node_startup_from_tlv(void);

// Persistent storage structures

typedef struct {
    uint16_t  hash;
    uint8_t   label_uuid[16];
} mesh_persistent_virtual_address_t;

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
    uint16_t publish_address;
    uint16_t appkey_index;
    uint8_t  friendship_credential_flag;
    uint8_t  publish_period;
    uint8_t  publish_ttl;
    uint8_t  publish_retransmit;
} mesh_persistent_publication_t;

typedef struct {
    uint32_t iv_index;
    uint32_t seq_number;
} iv_index_and_sequence_number_t;

static btstack_packet_handler_t provisioning_device_packet_handler;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static int provisioned;

// Mandatory Confiuration Server 
static mesh_model_t                 mesh_configuration_server_model;
static mesh_configuration_server_model_context_t mesh_configuration_server_model_context;

// Mandatory Health Server
static mesh_publication_model_t     mesh_health_server_publication;
static mesh_model_t                 mesh_health_server_model;
static mesh_health_state_t  mesh_health_server_model_context;

// Random UUID on start
static btstack_crypto_random_t mesh_access_crypto_random;
static uint8_t random_device_uuid[16];

// TLV
static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

// IV Index persistence
static uint32_t sequence_number_last_stored;
static uint32_t sequence_number_storage_trigger;

// Attention Timer
static uint8_t                attention_timer_timeout_s;
static btstack_timer_source_t attention_timer_timer;

// used to log all keys to packet log for log viewer
// mesh-appkey-0xxx: 1234567890abcdef1234567890abcdef
static void mesh_log_key(const char * prefix, uint16_t id, const uint8_t * key){
    char line[60];
    strcpy(line, prefix);
    uint16_t pos = strlen(line);
    if (id != 0xffff){
        line[pos++] = '-';
        line[pos++] = '0';
        line[pos++] = char_for_nibble((id >> 8) & 0x0f);
        line[pos++] = char_for_nibble((id >> 4) & 0x0f);
        line[pos++] = char_for_nibble( id       & 0x0f);
    }
    line[pos++] = ':';
    line[pos++] = ' ';
    uint16_t i;
    for (i=0;i<16;i++){
        line[pos++] = char_for_nibble((key[i] >> 4) & 0x0f);
        line[pos++] = char_for_nibble( key[i]       & 0x0f);
    }
    line[pos++] = 0;
    hci_dump_log(HCI_DUMP_LOG_LEVEL_INFO, "%s", line);
}

static void mesh_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){

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

// Attention Timer state
static void mesh_emit_attention_timer_event(uint8_t timer_s){
    if (!provisioning_device_packet_handler) return;
    uint8_t event[4] = { HCI_EVENT_MESH_META, 4, MESH_SUBEVENT_ATTENTION_TIMER};
    event[3] = timer_s;
    provisioning_device_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void mesh_attention_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    attention_timer_timeout_s--;
    mesh_emit_attention_timer_event(attention_timer_timeout_s);
    if (attention_timer_timeout_s == 0) return;

    btstack_run_loop_set_timer(&attention_timer_timer, 1000);
    btstack_run_loop_add_timer(&attention_timer_timer);
}

void mesh_attention_timer_set(uint8_t timer_s){
    // stop old timer if running
    if (attention_timer_timeout_s){
        btstack_run_loop_remove_timer(&attention_timer_timer);
    }

    attention_timer_timeout_s = timer_s;
    mesh_emit_attention_timer_event(attention_timer_timeout_s);

    if (attention_timer_timeout_s){
        btstack_run_loop_set_timer_handler(&attention_timer_timer, &mesh_attention_timer_handler);
        btstack_run_loop_set_timer(&attention_timer_timer, 1000);
        btstack_run_loop_add_timer(&attention_timer_timer);
    }
}

uint8_t mesh_attention_timer_get(void){
    return attention_timer_timeout_s;
}

static void mesh_provisioning_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    mesh_provisioning_data_t provisioning_data;
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_PROV_ATTENTION_TIMER:
                    mesh_attention_timer_set(mesh_subevent_pb_prov_attention_timer_get_attention_time(packet));
                    break;
                case MESH_SUBEVENT_PB_PROV_COMPLETE:
                    // get provisioning data
                    provisioning_device_data_get(&provisioning_data);

                    // and store in TLV
                    mesh_node_store_provisioning_data(&provisioning_data);

                    // setup node after provisioned
                    mesh_setup_from_provisioning_data(&provisioning_data);

#ifdef ENABLE_MESH_PROXY_SERVER
                    // start advertising with node id after provisioning
                    mesh_proxy_set_advertising_with_node_id(provisioning_data.network_key->netkey_index, MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING);
#endif
                    
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
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            // get TLV instance
            btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);

            // startup from static provisioning data stored in TLV
            provisioned = mesh_node_startup_from_tlv();
            break;
        
#ifdef ENABLE_MESH_PROXY_SERVER
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // enable PB_GATT
            if (provisioned == 0){
                mesh_proxy_start_advertising_unprovisioned_device();
            } else {
                mesh_proxy_start_advertising_with_network_id();
            }
            break;
            
        case HCI_EVENT_META_GAP:
            if (hci_event_gap_meta_get_subevent_code(packet) !=  GAP_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            // disable PB_GATT
            mesh_proxy_stop_advertising_unprovisioned_device();
            break;
#endif
        default:
            break;
    }
}

static void report_store_error(int result, const char * type){
    if (result != 0){
        log_error("Store %s data failed", type);
    }
}

// Foundation state
static const uint32_t mesh_foundation_state_tag = ((uint32_t) 'M' << 24) | ((uint32_t) 'F' << 16)  | ((uint32_t) 'N' << 8) | ((uint32_t) 'D' << 8);

void mesh_foundation_state_load(void){
    mesh_persistent_foundation_t data;

    int foundation_state_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
    if (foundation_state_len != sizeof(data)) return;

    mesh_foundation_gatt_proxy_set(data.gatt_proxy);
    mesh_foundation_beacon_set(data.beacon);
    mesh_foundation_default_ttl_set(data.default_ttl);
    mesh_foundation_friend_set(data.friend);
    mesh_foundation_network_transmit_set(data.network_transmit);
    mesh_foundation_relay_set(data.relay);
    mesh_foundation_relay_retransmit_set(data.relay_retransmit);
}

void mesh_foundation_state_store(void){
    mesh_persistent_foundation_t data;
    data.gatt_proxy       = mesh_foundation_gatt_proxy_get();
    data.beacon           = mesh_foundation_beacon_get();
    data.default_ttl      = mesh_foundation_default_ttl_get();
    data.friend           = mesh_foundation_friend_get();
    data.network_transmit = mesh_foundation_network_transmit_get();
    data.relay            = mesh_foundation_relay_get();
    data.relay_retransmit = mesh_foundation_relay_retransmit_get();
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_foundation_state_tag, (uint8_t *) &data, sizeof(data));
    report_store_error(result, "foundation");
}

// Mesh Virtual Address Management
static uint32_t mesh_virtual_address_tag_for_pseudo_dst(uint16_t pseudo_dst){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'V' << 16) | ((uint32_t) pseudo_dst);
}

static void mesh_store_virtual_address(uint16_t pseudo_dest, uint16_t hash, const uint8_t * label_uuid){
    mesh_persistent_virtual_address_t data;
    uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dest);
    data.hash = hash;
    (void)memcpy(data.label_uuid, label_uuid, 16);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
    report_store_error(result, "virtual address");
}

static void mesh_delete_virtual_address(uint16_t pseudo_dest){
    uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dest);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_virtual_addresses(void){
    uint16_t pseudo_dst;
    for (pseudo_dst = 0x8000; pseudo_dst < (0x8000 + MAX_NR_MESH_VIRTUAL_ADDRESSES); pseudo_dst++){
        mesh_virtual_address_tag_for_pseudo_dst(pseudo_dst);
        mesh_persistent_virtual_address_t data;
        uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dst);
        int virtual_address_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (virtual_address_len == 0) continue;
        
        mesh_virtual_address_t * virtual_address = btstack_memory_mesh_virtual_address_get();
        if (virtual_address == NULL) return;

        virtual_address->pseudo_dst = pseudo_dst;
        virtual_address->hash = data.hash;
        (void)memcpy(virtual_address->label_uuid, data.label_uuid, 16);
        mesh_virtual_address_add(virtual_address);
    }
}

void mesh_delete_virtual_addresses(void){
    uint16_t pseudo_dest;
    for (pseudo_dest = 0x8000; pseudo_dest < (0x8000 + MAX_NR_MESH_VIRTUAL_ADDRESSES); pseudo_dest++){
        mesh_delete_virtual_address(pseudo_dest);
    }
}

void mesh_virtual_address_decrease_refcount(mesh_virtual_address_t * virtual_address){
    if (virtual_address == NULL){
        log_error("virtual_address == NULL");
    }
    // decrease refcount
    virtual_address->ref_count--;
    // Free virtual address if ref count reaches zero
    if (virtual_address->ref_count > 0) return;
    // delete from TLV
    mesh_delete_virtual_address(virtual_address->pseudo_dst);
    // remove from list
    mesh_virtual_address_remove(virtual_address);
    // free memory
    btstack_memory_mesh_virtual_address_free(virtual_address);
}

void mesh_virtual_address_increase_refcount(mesh_virtual_address_t * virtual_address){
    if (virtual_address == NULL){
        log_error("virtual_address == NULL");
    }
    virtual_address->ref_count++;
    if (virtual_address->ref_count > 1) return;
    // store in TLV
    mesh_store_virtual_address(virtual_address->pseudo_dst, virtual_address->hash, virtual_address->label_uuid);
}

// Mesh Subscriptions
static uint32_t mesh_model_subscription_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'S' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_model_load_subscriptions(mesh_model_t * mesh_model){
    uint32_t tag = mesh_model_subscription_tag_for_index(mesh_model->mid);
    btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &mesh_model->subscriptions, sizeof(mesh_model->subscriptions));
    // update ref count

    // increase ref counts for virtual subscriptions
    uint16_t i;
    for (i = 0; i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL ; i++){
        uint16_t src = mesh_model->subscriptions[i];
        if (mesh_network_address_virtual(src)){
            mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_pseudo_dst(src);
            mesh_virtual_address_increase_refcount(virtual_address);
        }
    }
}

void mesh_model_store_subscriptions(mesh_model_t * model){
    uint32_t tag = mesh_model_subscription_tag_for_index(model->mid);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->subscriptions, sizeof(model->subscriptions));
    report_store_error(result, "subscription");
}

static void mesh_model_delete_subscriptions(mesh_model_t * model){
    uint32_t tag = mesh_model_subscription_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_subscriptions(void){
    printf("Load Model Subscription Lists\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_model_load_subscriptions(model);
        }
    }
}

void mesh_delete_subscriptions(void){
    printf("Delete Model Subscription Lists\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_model_delete_subscriptions(model);
        }
    }
}

// Model Publication

static uint32_t mesh_model_publication_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'P' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_model_load_publication(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication = mesh_model->publication_model;
    if (publication == NULL) return;

    mesh_persistent_publication_t data;
    uint32_t tag = mesh_model_publication_tag_for_index(mesh_model->mid);
    btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(mesh_persistent_publication_t));

    publication->address                    = data.publish_address;
    publication->appkey_index               = data.appkey_index;
    publication->friendship_credential_flag = data.friendship_credential_flag;
    publication->ttl                        = data.publish_ttl;
    publication->period                     = data.publish_period;
    publication->retransmit                 = data.publish_retransmit;

    // increase ref counts for current virtual publicataion address
    uint16_t src = publication->address;
    if (mesh_network_address_virtual(src)){
        mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_pseudo_dst(src);
        if (virtual_address){
            mesh_virtual_address_increase_refcount(virtual_address);
        }
    }

    mesh_model_publication_start(mesh_model);
}

void mesh_model_store_publication(mesh_model_t * mesh_model){
    mesh_publication_model_t * publication = mesh_model->publication_model;
    if (publication == NULL) return;

    mesh_persistent_publication_t data;
    data.publish_address            = publication->address;
    data.appkey_index               = publication->appkey_index;
    data.friendship_credential_flag = publication->friendship_credential_flag;
    data.publish_ttl                = publication->ttl;
    data.publish_period             = publication->period;
    data.publish_retransmit         = publication->retransmit;
    uint32_t tag = mesh_model_publication_tag_for_index(mesh_model->mid);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(mesh_persistent_publication_t));
    report_store_error(result, "publication");
}

static void mesh_model_delete_publication(mesh_model_t * mesh_model){
    if (mesh_model->publication_model == NULL) return;
    uint32_t tag = mesh_model_publication_tag_for_index(mesh_model->mid);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_publications(void){
    printf("Load Model Publications\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_model_load_publication(model);
        }
    }
}

void mesh_delete_publications(void){
    printf("Delete Model Publications\n");
    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            mesh_model_delete_publication(model);
        }
    }
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
    (void)memcpy(data.net_key, network_key->net_key, 16);
    (void)memcpy(data.identity_key, network_key->identity_key, 16);
    (void)memcpy(data.beacon_key, network_key->beacon_key, 16);
    (void)memcpy(data.network_id, network_key->network_id, 8);
    data.nid = network_key->nid;
    data.version = network_key->version;
    (void)memcpy(data.encryption_key, network_key->encryption_key, 16);
    (void)memcpy(data.privacy_key, network_key->privacy_key, 16);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(mesh_persistent_net_key_t));
    report_store_error(result, "network key");
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

        network_key->internal_index = internal_index;
        network_key->netkey_index = data.netkey_index;
        (void)memcpy(network_key->net_key, data.net_key, 16);
        (void)memcpy(network_key->identity_key, data.identity_key, 16);
        (void)memcpy(network_key->beacon_key, data.beacon_key, 16);
        (void)memcpy(network_key->network_id, data.network_id, 8);
        network_key->nid = data.nid;
        network_key->version = data.version;
        (void)memcpy(network_key->encryption_key, data.encryption_key, 16);
        (void)memcpy(network_key->privacy_key, data.privacy_key, 16);

#ifdef ENABLE_GATT_BEARER
        // setup advertisement with network id
        network_key->advertisement_with_network_id.adv_length = mesh_proxy_setup_advertising_with_network_id(network_key->advertisement_with_network_id.adv_data, network_key->network_id);
#endif

        mesh_network_key_add(network_key);

        mesh_subnet_setup_for_netkey_index(network_key->netkey_index);

        printf("- internal index 0x%x, NetKey Index 0x%06x, NID %02x: ", network_key->internal_index, network_key->netkey_index, network_key->nid);
        printf_hexdump(network_key->net_key, 16);

        // dump into packet log
        mesh_log_key("mesh-netkey",  network_key->netkey_index, network_key->net_key);
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
    (void)memcpy(data.key, app_key->key, 16);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
    report_store_error(result, "app key");
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
        (void)memcpy(key->key, data.key, 16);
        mesh_transport_key_add(key);
        printf("- internal index 0x%x, AppKey Index 0x%06x, AID %02x: ", key->internal_index, key->appkey_index, key->aid);
        printf_hexdump(key->key, 16);

        // dump into packet log
        mesh_log_key("mesh-appkey",  key->appkey_index, key->key);
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
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->appkey_indices, sizeof(model->appkey_indices));
    report_store_error(result, "appkey list");
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
static const uint32_t mesh_tag_for_iv_index_and_seq_number = ((uint32_t) 'M' << 24) | ((uint32_t) 'F' << 16) | ((uint32_t) 'I' << 9) | ((uint32_t) 'S');

static int mesh_load_iv_index_and_sequence_number(uint32_t * iv_index, uint32_t * sequence_number){
    iv_index_and_sequence_number_t data;
    uint32_t len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_tag_for_iv_index_and_seq_number, (uint8_t *) &data, sizeof(data));
    if (len == sizeof(iv_index_and_sequence_number_t)){
        *iv_index = data.iv_index;
        *sequence_number = data.seq_number;
        return 1;
    }
    return 0;
}

static void mesh_store_iv_index_and_sequence_number(uint32_t iv_index, uint32_t sequence_number){
    iv_index_and_sequence_number_t data;
    data.iv_index   = iv_index;
    data.seq_number = sequence_number;
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_tag_for_iv_index_and_seq_number, (uint8_t *) &data, sizeof(data));
    report_store_error(result, "index and sequence number");

    sequence_number_last_stored = data.seq_number;
    sequence_number_storage_trigger = sequence_number_last_stored + MESH_SEQUENCE_NUMBER_STORAGE_INTERVAL;
}

static void mesh_persist_iv_index_and_sequence_number(void){
    mesh_store_iv_index_and_sequence_number(mesh_get_iv_index(), mesh_sequence_number_peek());
}


// higher layer - only store if sequence number is higher than trigger
static void mesh_persist_iv_index_and_sequence_number_if_needed(void){
    if (mesh_sequence_number_peek() >= sequence_number_storage_trigger){
        mesh_persist_iv_index_and_sequence_number();
    }
}

static void mesh_access_secure_network_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != MESH_BEACON_PACKET) return;

    // lookup subnet and netkey by network id
    uint8_t * beacon_network_id = &packet[2];
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    mesh_subnet_t * subnet = NULL;
    uint8_t new_key = 0;
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * item = mesh_subnet_iterator_get_next(&it);
        if (memcmp(item->old_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
        }
        if (item->new_key != NULL && memcmp(item->new_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
            new_key = 1;
        }
        break;
    }
    if (subnet == NULL) return;

    uint8_t flags = packet[1];

    // Key refresh via secure network beacons that are authenticated with new netkey
    if (new_key){
        // either first or second phase (in phase 0, new key is not set)
        int key_refresh_flag = flags & 1;
        if (key_refresh_flag){
            //  transition to phase 3 from either phase 1 or 2
            switch (subnet->key_refresh){
                case MESH_KEY_REFRESH_FIRST_PHASE:
                case MESH_KEY_REFRESH_SECOND_PHASE:
                    mesh_access_key_refresh_revoke_keys(subnet);
                    subnet->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;
                    break;
                default:
                    break;
            }
        } else {
            //  transition to phase 2 from either phase 1
            switch (subnet->key_refresh){
                case MESH_KEY_REFRESH_FIRST_PHASE:
                    // -- update state
                    subnet->key_refresh = MESH_KEY_REFRESH_SECOND_PHASE;
                    break;
                default:
                    break;
            }
        }
    }

    // IV Update

    int     beacon_iv_update_active = flags & 2;
    int     local_iv_update_active = mesh_iv_update_active();
    uint32_t beacon_iv_index = big_endian_read_32(packet, 10);
    uint32_t local_iv_index = mesh_get_iv_index();

    int32_t iv_index_delta = (int32_t)(beacon_iv_index - local_iv_index);

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index less than the last known IV Index or greater than
    //  the last known IV Index + 42, the Secure Network beacon shall be ignored."
    if (iv_index_delta < 0 || iv_index_delta > 42){
        return;
    }

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index equal to the last known IV index+1 and
    //  the IV Update Flag set to 0, the node may update its IV without going to the IV Update in Progress state, or it may initiate
    //  an IV Index Recovery procedure (Section 3.10.6), or it may ignore the Secure Network beacon. The node makes the choice depending
    //  on the time since last IV update and the likelihood that the node has missed the Secure Network beacons with the IV update Flag set to 1.""
    if (local_iv_update_active == 0 && beacon_iv_update_active == 0 && iv_index_delta == 1){
        // instant iv update
        mesh_set_iv_index( beacon_iv_index );
        // store updated iv index
        mesh_persist_iv_index_and_sequence_number();
        return;
    }

    // "If this node is a member of a primary subnet and receives a Secure Network beacon on a secondary subnet with an IV Index greater than
    //  the last known IV Index of the primary subnet, the Secure Network beacon shall be ignored."
    int member_of_primary_subnet = mesh_subnet_get_by_netkey_index(0) != NULL;
    int beacon_on_secondary_subnet = subnet->netkey_index != 0;
    if (member_of_primary_subnet && beacon_on_secondary_subnet && iv_index_delta > 0){
        return;
    }

    // "If a node in Normal Operation receives a Secure Network beacon with an IV index greater than the last known IV Index + 1..."
    // "... it may initiate an IV Index Recovery procedure, see Section 3.10.6."
    if (local_iv_update_active == 0 && iv_index_delta > 1){
        // "Upon receiving and successfully authenticating a Secure Network beacon for a primary subnet... "
        int beacon_on_primary_subnet = subnet->netkey_index == 0;
        if (!beacon_on_primary_subnet) return;
        // "... whose IV Index is 1 or more higher than the current known IV Index, the node shall "
        // " set its current IV Index and its current IV Update procedure state from the values in this Secure Network beacon."
        mesh_iv_index_recovered(beacon_iv_update_active, beacon_iv_index);
        // store updated iv index if in normal mode
        if (beacon_iv_update_active == 0){
            mesh_persist_iv_index_and_sequence_number();
        }
        return;
    }

    if (local_iv_update_active == 0){
        if (beacon_iv_update_active){
            mesh_trigger_iv_update();
        }
    } else {
        if (beacon_iv_update_active == 0){
            // " At the point of transition, the node shall reset the sequence number to 0x000000."
            mesh_sequence_number_set(0);
            mesh_iv_update_completed();
            // store updated iv index 
            mesh_persist_iv_index_and_sequence_number();
        }
    }
}

static const uint32_t mesh_tag_for_prov_data = ((uint32_t) 'P' << 24) | ((uint32_t) 'R' << 16) | ((uint32_t) 'O' <<  8) | (uint32_t)'V';

typedef struct {
    uint16_t unicast_address;
    uint8_t  flags;
    uint8_t  device_key[16];

} mesh_persistent_provisioning_data_t;

static void mesh_node_store_provisioning_data(mesh_provisioning_data_t * provisioning_data){

    // fill persistent prov data
    mesh_persistent_provisioning_data_t persistent_provisioning_data;

    persistent_provisioning_data.unicast_address = provisioning_data->unicast_address;
    persistent_provisioning_data.flags = provisioning_data->flags;
    (void)memcpy(persistent_provisioning_data.device_key,
                 provisioning_data->device_key, 16);

    // store in tlv
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
    int result = btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data, (uint8_t *) &persistent_provisioning_data, sizeof(mesh_persistent_provisioning_data_t));
    report_store_error(result, "provisioning");

    // store IV Index and sequence number
    mesh_store_iv_index_and_sequence_number(provisioning_data->iv_index, 0);

    // store primary network key
    mesh_store_network_key(provisioning_data->network_key);
}

static void mesh_access_setup_unprovisioned_device(const uint8_t * device_uuid){
#ifdef ENABLE_MESH_PB_ADV
    // PB-ADV
    beacon_unprovisioned_device_start(device_uuid, 0);
#else
    UNUSED(device_uuid);;
#endif
#ifdef ENABLE_MESH_PB_GATT
    mesh_proxy_start_advertising_unprovisioned_device();
#endif
}

static void mesh_access_setup_without_provisiong_data_random(void * arg){
    UNUSED(arg);
    // set random value
    mesh_node_set_device_uuid(random_device_uuid);
    mesh_access_setup_unprovisioned_device(random_device_uuid);
}

void mesh_node_reset(void){
    // PROV
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data);
    // everything else
    mesh_delete_network_keys();
    mesh_delete_app_keys();
    mesh_delete_appkey_lists();
    mesh_delete_virtual_addresses();
    mesh_delete_subscriptions();
    mesh_delete_publications();
    // also reset iv index + sequence number
    mesh_set_iv_index(0);
    mesh_sequence_number_set(0);
    // start advertising as unprovisioned device
    mesh_access_setup_unprovisioned_device(mesh_node_get_device_uuid());
}

static void mesh_access_setup_with_provisiong_data_random(void * arg){
    UNUSED(arg);
    // set random value
    mesh_node_set_device_uuid(random_device_uuid);
}

static int mesh_node_startup_from_tlv(void){
    
    mesh_persistent_provisioning_data_t persistent_provisioning_data;
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);

    // load provisioning data
    uint32_t prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data, (uint8_t *) &persistent_provisioning_data, sizeof(mesh_persistent_provisioning_data_t));
    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
    int prov_data_valid = prov_len == sizeof(mesh_persistent_provisioning_data_t);
    if (prov_data_valid){

        // copy into mesh_provisioning_data
        mesh_provisioning_data_t provisioning_data;
        (void)memcpy(provisioning_data.device_key,
                     persistent_provisioning_data.device_key, 16);
        provisioning_data.unicast_address = persistent_provisioning_data.unicast_address;
        provisioning_data.flags = persistent_provisioning_data.flags;
        provisioning_data.network_key = NULL;
        printf("Provisioning Data: Flags %x, unicast_address %04x\n", persistent_provisioning_data.flags, provisioning_data.unicast_address);
        
        // try load iv index and sequence number
        uint32_t iv_index        = 0;
        uint32_t sequence_number = 0;
        (void) mesh_load_iv_index_and_sequence_number(&iv_index, &sequence_number);

        // bump sequence number to account for interval updates
        sequence_number += MESH_SEQUENCE_NUMBER_STORAGE_INTERVAL;
        mesh_store_iv_index_and_sequence_number(iv_index, sequence_number);

        mesh_set_iv_index(iv_index);
        mesh_sequence_number_set(sequence_number);
        provisioning_data.iv_index = iv_index;
        printf("IV Index: %08x, Sequence Number %08x\n", (int) iv_index, (int) sequence_number);

        // setup iv update, node address, device key ...
        mesh_setup_from_provisioning_data(&provisioning_data);

        // load network keys
        mesh_load_network_keys();

        // load app keys
        mesh_load_app_keys();

        // load foundation state
        mesh_foundation_state_load();

        // load model to appkey bindings
        mesh_load_appkey_lists();

        // load virtual addresses
        mesh_load_virtual_addresses();

        // load model subscriptions
        mesh_load_subscriptions();

        // load model publications
        mesh_load_publications();

#if defined(ENABLE_MESH_ADV_BEARER) || defined(ENABLE_MESH_PB_ADV)
        // start sending Secure Network Beacon
        mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(0);
        if (subnet){
            beacon_secure_network_start(subnet);
        }
#endif
        // create random uuid if not already set
        if (mesh_node_get_device_uuid() == NULL){
            btstack_crypto_random_generate(&mesh_access_crypto_random, random_device_uuid, 16, &mesh_access_setup_with_provisiong_data_random, NULL);
        }

        // dump into packet log
        hci_dump_log(HCI_DUMP_LOG_LEVEL_INFO, "mesh-iv-index: %08" PRIx32,  iv_index);
        mesh_log_key("mesh-devkey",  0xffff, persistent_provisioning_data.device_key);

    } else {

        const uint8_t * device_uuid = mesh_node_get_device_uuid();
        if (device_uuid){
            mesh_access_setup_unprovisioned_device(device_uuid);
        } else{
            btstack_crypto_random_generate(&mesh_access_crypto_random, random_device_uuid, 16, &mesh_access_setup_without_provisiong_data_random, NULL);
        }

    }
    
    return prov_data_valid;
}

static void mesh_control_message_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    UNUSED(callback_type);
    UNUSED(status);

    // get opcode 
    uint8_t opcode = mesh_pdu_control_opcode(pdu);
    printf("MESH Control Message, Opcode: 0x%02x + ", opcode);
    printf_hexdump(mesh_pdu_data(pdu), mesh_pdu_len(pdu));

    uint8_t init_ttl;
    uint8_t hops = 0;
    uint16_t features = 0;
    switch(opcode){
        case 0x0a:
            // read params
            init_ttl = (*mesh_pdu_data(pdu)) & 0x7fu;
            features = big_endian_read_16(mesh_pdu_data(pdu), 1);
            // calculates hops
            hops     = init_ttl - mesh_pdu_ttl(pdu) + 1;
            // process heartbeat info
            mesh_configuration_server_process_heartbeat(&mesh_configuration_server_model, mesh_pdu_src(pdu), mesh_pdu_dst(pdu), hops, features);
            break;
        default:
            break;
    }
    mesh_upper_transport_message_processed_by_higher_layer(pdu);
}

static void mesh_node_setup_default_models(void){
    // configure Config Server
    mesh_configuration_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
    mesh_configuration_server_model.model_data       = &mesh_configuration_server_model_context;
    mesh_configuration_server_model.operations       = mesh_configuration_server_get_operations();    
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_configuration_server_model);

    // Config Health Server
    mesh_health_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER);
    mesh_health_server_model.model_data       = &mesh_health_server_model_context;
    mesh_health_server_model.operations       = mesh_health_server_get_operations();
    mesh_health_server_model.publication_model = &mesh_health_server_publication;
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_health_server_model);
    mesh_health_server_set_publication_model(&mesh_health_server_model, &mesh_health_server_publication);
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
    provisioning_device_register_packet_handler(&mesh_provisioning_message_handler);

#ifdef ENABLE_MESH_PROVISIONER
    provisioning_provisioner_init();
    provisioning_provisioner_register_packet_handler(&mesh_provisioning_message_handler);
#endif

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

    // register for secure network beacons
    beacon_register_for_secure_network_beacons(&mesh_access_secure_network_beacon_handler);

    // register for seq number updates
    mesh_sequence_number_set_update_callback(&mesh_persist_iv_index_and_sequence_number_if_needed);

    // register for control messages
    mesh_upper_transport_register_control_message_handler(&mesh_control_message_handler);
}

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler){
    provisioning_device_packet_handler = packet_handler;
}

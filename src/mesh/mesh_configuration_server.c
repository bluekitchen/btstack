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

#define __BTSTACK_FILE__ "mesh_configuration_server.c"

#include <string.h>
#include <stdio.h>

#include "mesh/mesh_configuration_server.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_tlv.h"
#include "btstack_util.h"

#include "mesh/beacon.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_crypto.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_node.h"
#include "mesh/mesh_proxy.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/mesh_virtual_addresses.h"

#define MESH_HEARTBEAT_FEATURES_SUPPORTED_MASK 0x000f

typedef struct {
    uint16_t  hash;
    uint8_t   label_uuid[16];
} mesh_persistent_virtual_address_t;


// current access pdu
static mesh_pdu_t * access_pdu_in_process;

// data from current pdu
static uint16_t                     configuration_server_element_address;
static uint32_t                     configuration_server_model_identifier;
static mesh_model_t               * configuration_server_target_model;
static mesh_publication_model_t     configuration_server_publication_model;

// cmac for virtual address hash and netkey derive
static btstack_crypto_aes128_cmac_t configuration_server_cmac_request;

// used to setup virtual addresses
static uint8_t                      configuration_server_label_uuid[16];
static uint16_t                     configuration_server_hash;

// heartbeat publication and subscription state for all Configuration Server models - there is only one
// static mesh_heartbeat_subscription_t mesh_heartbeat_subscription;

// for PTS testing
static int config_netkey_list_max = 0;


// Heartbeat (helper)
static uint16_t heartbeat_pwr2(uint8_t value){
    if (!value)                         return 0x0000;
    if (value == 0xff || value == 0x11) return 0xffff;
    return 1 << (value-1);
}

static uint8_t heartbeat_count_log(uint16_t value){
    if (!value)          return 0x00;
    if (value == 0x01)   return 0x01;
    if (value == 0xffff) return 0xff;
    // count leading zeros, supported by clang and gcc
    return 32 - __builtin_clz(value - 1) + 1;
}

// TLV

static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

static uint32_t mesh_virtual_address_tag_for_pseudo_dst(uint16_t pseudo_dst){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'V' << 16) | ((uint32_t) pseudo_dst);
}

static void mesh_configuration_server_setup_tlv(void){
    if (btstack_tlv_singleton_impl) return;
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
}

static void mesh_store_virtual_address(uint16_t pseudo_dest, uint16_t hash, const uint8_t * label_uuid){
    mesh_configuration_server_setup_tlv();

    mesh_persistent_virtual_address_t data;
    uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dest);
    data.hash = hash;
    memcpy(data.label_uuid, label_uuid, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
}

static void mesh_delete_virtual_address(uint16_t pseudo_dest){
    mesh_configuration_server_setup_tlv();

    uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dest);
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, tag);
}

void mesh_load_virtual_addresses(void){
    mesh_configuration_server_setup_tlv();
    uint16_t pseudo_dst;
    for (pseudo_dst = 0x8000; pseudo_dst < (0x8000 + MAX_NR_MESH_VIRTUAL_ADDRESSES); pseudo_dst++){
        mesh_virtual_address_tag_for_pseudo_dst(pseudo_dst);
        mesh_persistent_virtual_address_t data;
        uint32_t tag = mesh_virtual_address_tag_for_pseudo_dst(pseudo_dst);
        int virtual_address_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &data, sizeof(data));
        if (virtual_address_len == 0) return;
        
        mesh_virtual_address_t * virtual_address = btstack_memory_mesh_virtual_address_get();
        if (virtual_address == NULL) return;

        virtual_address->pseudo_dst = pseudo_dst;
        virtual_address->hash = data.hash;
        memcpy(virtual_address->label_uuid, data.label_uuid, 16);
        mesh_virtual_address_add(virtual_address);
    }
}

void mesh_delete_virtual_addresses(void){
    uint16_t pseudo_dest;
    for (pseudo_dest = 0x8000; pseudo_dest < (0x8000 + MAX_NR_MESH_VIRTUAL_ADDRESSES); pseudo_dest++){
        mesh_delete_virtual_address(pseudo_dest);
    }
}

static void mesh_virtual_address_decrease_refcount(mesh_virtual_address_t * virtual_address){
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

static void mesh_virtual_address_increase_refcount(mesh_virtual_address_t * virtual_address){
    if (virtual_address == NULL){
        log_error("virtual_address == NULL");
    }
    virtual_address->ref_count++;
    if (virtual_address->ref_count > 1) return;
    // store in TLV
    mesh_store_virtual_address(virtual_address->pseudo_dst, virtual_address->hash, virtual_address->label_uuid);
}

static int mesh_model_is_configuration_server(uint32_t model_identifier){
    return mesh_model_is_bluetooth_sig(model_identifier) && (mesh_model_get_model_id(model_identifier) == MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
}

// Configuration Model Subscriptions (helper)

// Model to Appkey List

static uint32_t mesh_model_subscription_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'S' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_model_load_subscriptions(mesh_model_t * mesh_model){
    mesh_configuration_server_setup_tlv();
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

static void mesh_model_store_subscriptions(mesh_model_t * model){
    mesh_configuration_server_setup_tlv();
    uint32_t tag = mesh_model_subscription_tag_for_index(model->mid);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &model->subscriptions, sizeof(model->subscriptions));
}

static void mesh_model_delete_subscriptions(mesh_model_t * model){
    mesh_configuration_server_setup_tlv();
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
    mesh_configuration_server_setup_tlv();
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

static uint8_t mesh_model_add_subscription(mesh_model_t * mesh_model, uint16_t address){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == address) return MESH_FOUNDATION_STATUS_SUCCESS;
    }
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == MESH_ADDRESS_UNSASSIGNED) {
            mesh_model->subscriptions[i] = address;
            return MESH_FOUNDATION_STATUS_SUCCESS;
        }
    }
    return MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
}

static void mesh_model_delete_subscription(mesh_model_t * mesh_model, uint16_t address){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == address) {
            mesh_model->subscriptions[i] = MESH_ADDRESS_UNSASSIGNED;
        }
    }
}

static void mesh_model_delete_all_subscriptions(mesh_model_t * mesh_model){
    uint16_t i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        mesh_model->subscriptions[i] = MESH_ADDRESS_UNSASSIGNED;
    }
}

static void mesh_subcription_decrease_virtual_address_ref_count(mesh_model_t *mesh_model){
    // decrease ref counts for current virtual subscriptions
    uint16_t i;
    for (i = 0; i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL ; i++){
        uint16_t src = mesh_model->subscriptions[i];
        if (mesh_network_address_virtual(src)){
            mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_pseudo_dst(src);
            mesh_virtual_address_decrease_refcount(virtual_address);
        }
    }
}

// Model Publication

static uint32_t mesh_model_publication_tag_for_index(uint16_t internal_model_id){
    return ((uint32_t) 'M' << 24) | ((uint32_t) 'P' << 16) | ((uint32_t) internal_model_id);
}

static void mesh_model_load_publication(mesh_model_t * mesh_model){
    mesh_configuration_server_setup_tlv();
    if (mesh_model->publication_model == NULL) return;

    uint32_t tag = mesh_model_publication_tag_for_index(mesh_model->mid);
    btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &mesh_model->publication_model, sizeof(mesh_publication_model_t));

    // increase ref counts for current virtual publicataion address
    uint16_t src = mesh_model->publication_model->address;
    if (mesh_network_address_virtual(src)){
        mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_pseudo_dst(src);
        mesh_virtual_address_increase_refcount(virtual_address);
    }

    mesh_model_publication_start(mesh_model);
}

static void mesh_model_store_publication(mesh_model_t * mesh_model){
    mesh_configuration_server_setup_tlv();
    if (mesh_model->publication_model == NULL) return;
    uint32_t tag = mesh_model_publication_tag_for_index(mesh_model->mid);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, tag, (uint8_t *) &mesh_model->subscriptions, sizeof(mesh_publication_model_t));
}

static void mesh_model_delete_publication(mesh_model_t * mesh_model){
    mesh_configuration_server_setup_tlv();
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
    mesh_configuration_server_setup_tlv();
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

// AppKeys Helper
static void mesh_configuration_server_delete_appkey(mesh_transport_key_t * transport_key){
    uint16_t appkey_index = transport_key->appkey_index;

    // iterate over elements and models
    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);
        mesh_model_iterator_t model_it;
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * mesh_model = mesh_model_iterator_next(&model_it);

            // remove from Model to AppKey List
            mesh_model_unbind_appkey(mesh_model, appkey_index);

            // stop publishing if this AppKey was used
            if (mesh_model->publication_model != NULL){
                mesh_publication_model_t * publication_model = mesh_model->publication_model;
                if (publication_model->appkey_index == appkey_index){
                    publication_model->address = MESH_ADDRESS_UNSASSIGNED;
                    publication_model->appkey_index = MESH_APPKEY_INVALID;
                    mesh_model_store_publication(mesh_model);
                }
            }
        }
    }

    mesh_access_appkey_finalize(transport_key);
}

// Foundatiopn Message

const mesh_access_message_t mesh_foundation_config_beacon_status = {
        MESH_FOUNDATION_OPERATION_BEACON_STATUS, "1"
};
const mesh_access_message_t mesh_foundation_config_default_ttl_status = {
        MESH_FOUNDATION_OPERATION_DEFAULT_TTL_STATUS, "1"
};
const mesh_access_message_t mesh_foundation_config_friend_status = {
        MESH_FOUNDATION_OPERATION_FRIEND_STATUS, "1"
};
const mesh_access_message_t mesh_foundation_config_gatt_proxy_status = {
        MESH_FOUNDATION_OPERATION_GATT_PROXY_STATUS, "1"
};
const mesh_access_message_t mesh_foundation_config_relay_status = {
        MESH_FOUNDATION_OPERATION_RELAY_STATUS, "11"
};
const mesh_access_message_t mesh_foundation_config_model_publication_status = {
        MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_STATUS, "1222111m"
};
const mesh_access_message_t mesh_foundation_config_model_subscription_status = {
        MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_STATUS, "122m"
};
const mesh_access_message_t mesh_foundation_config_netkey_status = {
        MESH_FOUNDATION_OPERATION_NETKEY_STATUS, "12"
};
const mesh_access_message_t mesh_foundation_config_appkey_status = {
        MESH_FOUNDATION_OPERATION_APPKEY_STATUS, "13"
};
const mesh_access_message_t mesh_foundation_config_model_app_status = {
        MESH_FOUNDATION_OPERATION_MODEL_APP_STATUS, "122m"
};
const mesh_access_message_t mesh_foundation_node_reset_status = {
        MESH_FOUNDATION_OPERATION_NODE_RESET_STATUS, ""
};
const mesh_access_message_t mesh_foundation_config_heartbeat_publication_status = {
        MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_STATUS, "1211122"
};
const mesh_access_message_t mesh_foundation_config_network_transmit_status = {
        MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_STATUS, "11"
};
const mesh_access_message_t mesh_foundation_node_identity_status = {
        MESH_FOUNDATION_OPERATION_NODE_IDENTITY_STATUS, "121"
};
const mesh_access_message_t mesh_key_refresh_phase_status = {
        MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_STATUS, "121"
};
const mesh_access_message_t mesh_foundation_low_power_node_poll_timeout_status = {
        MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_STATUS, "23"
};
const mesh_access_message_t mesh_foundation_config_heartbeat_subscription_status = {
        MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_STATUS, "1221111"
};

static void config_server_send_message(uint16_t netkey_index, uint16_t dest, mesh_pdu_t *pdu){
    // Configuration Server is on primary element and can only use DeviceKey
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint16_t src          = mesh_node_get_primary_element_address();
    uint8_t  ttl          = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_access_send_unacknowledged_pdu(pdu);
}

static void config_composition_data_status(uint16_t netkey_index, uint16_t dest){

    printf("Received Config Composition Data Get -> send Config Composition Data Status\n");

    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_STATUS);
    if (!transport_pdu) return;

    // page 0
    mesh_access_transport_add_uint8(transport_pdu, 0);

    // CID
    mesh_access_transport_add_uint16(transport_pdu, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH);
    // PID
    mesh_access_transport_add_uint16(transport_pdu, 0);
    // VID
    mesh_access_transport_add_uint16(transport_pdu, 0);
    // CRPL - number of protection list entries
    mesh_access_transport_add_uint16(transport_pdu, 1);
    // Features - Relay, Proxy, Friend, Lower Power, ...
    mesh_access_transport_add_uint16(transport_pdu, 0);

    mesh_element_iterator_t element_it;
    mesh_element_iterator_init(&element_it);
    while (mesh_element_iterator_has_next(&element_it)){
        mesh_element_t * element = mesh_element_iterator_next(&element_it);

        // Loc
        mesh_access_transport_add_uint16(transport_pdu, element->loc);
        // NumS
        mesh_access_transport_add_uint8( transport_pdu, element->models_count_sig);
        // NumV
        mesh_access_transport_add_uint8( transport_pdu, element->models_count_vendor);

        mesh_model_iterator_t model_it;
        
        // SIG Models
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            if (!mesh_model_is_bluetooth_sig(model->model_identifier)) continue;
            mesh_access_transport_add_uint16(transport_pdu, model->model_identifier);
        }
        // Vendor Models
        mesh_model_iterator_init(&model_it, element);
        while (mesh_model_iterator_has_next(&model_it)){
            mesh_model_t * model = mesh_model_iterator_next(&model_it);
            if (mesh_model_is_bluetooth_sig(model->model_identifier)) continue;
            mesh_access_transport_add_uint32(transport_pdu, model->model_identifier);
        }
    }
    
    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_composition_data_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    UNUSED(mesh_model);
    config_composition_data_status(mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_beacon_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_beacon_status,
                                                                               mesh_foundation_beacon_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_beacon_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_model_beacon_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_beacon_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t beacon_enabled = mesh_access_parser_get_u8(&parser);

    // beacon valid
    if (beacon_enabled >= MESH_FOUNDATION_STATE_NOT_SUPPORTED) return;

    // store
    mesh_foundation_beacon_set(beacon_enabled);
    mesh_foundation_state_store();

    //
    config_model_beacon_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_default_ttl_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_default_ttl_status, mesh_foundation_default_ttl_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_default_ttl_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_model_default_ttl_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_default_ttl_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t new_ttl = mesh_access_parser_get_u8(&parser);

    // ttl valid
    if (new_ttl > 0x7f || new_ttl == 0x01) return;
    // store
    mesh_foundation_default_ttl_set(new_ttl);
    mesh_foundation_state_store();

    //
    config_model_default_ttl_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_friend_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_friend_status, mesh_foundation_friend_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_friend_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_friend_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_friend_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t new_friend_state = mesh_access_parser_get_u8(&parser);

    // validate
    if (new_friend_state >= MESH_FOUNDATION_STATE_NOT_SUPPORTED) return;

    // store if supported
    if (mesh_foundation_friend_get() != MESH_FOUNDATION_STATE_NOT_SUPPORTED){
        mesh_foundation_friend_set(new_friend_state);
        mesh_foundation_state_store();
    }

    //
    config_friend_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_gatt_proxy_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_gatt_proxy_status, mesh_foundation_gatt_proxy_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_gatt_proxy_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_model_gatt_proxy_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_gatt_proxy_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t enabled = mesh_access_parser_get_u8(&parser);

    // ttl valid
    if (enabled > 1) return;
    // store
    mesh_foundation_gatt_proxy_set(enabled);
    mesh_foundation_state_store();

    //
    config_model_gatt_proxy_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);

    // trigger heartbeat emit on change
    mesh_configuration_server_feature_changed();
}

static void config_model_relay_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_relay_status,
                                                                               mesh_foundation_relay_get(),
                                                                               mesh_foundation_relay_retransmit_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_relay_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_model_relay_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_relay_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // check if valid
    uint8_t relay            = mesh_access_parser_get_u8(&parser);
    uint8_t relay_retransmit = mesh_access_parser_get_u8(&parser);

    // check if valid
    if (relay > 1) return;

    // only update if supported
    if (mesh_foundation_relay_get() != MESH_FOUNDATION_STATE_NOT_SUPPORTED){
        mesh_foundation_relay_set(relay);
        mesh_foundation_relay_retransmit_set(relay_retransmit);
        mesh_foundation_state_store();
    }

    //
    config_model_relay_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);

    // trigger heartbeat emit on change
    mesh_configuration_server_feature_changed();
}

static void config_model_network_transmit_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_network_transmit_status, mesh_foundation_network_transmit_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_network_transmit_get_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    config_model_network_transmit_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_network_transmit_set_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint8_t new_ttl = mesh_access_parser_get_u8(&parser);

    // store
    mesh_foundation_network_transmit_set(new_ttl);
    mesh_foundation_state_store();

    //
    config_model_network_transmit_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

// NetKey List

void config_nekey_list_set_max(uint16_t max){
    config_netkey_list_max = max;
}

static void config_netkey_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t new_netkey_index){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_netkey_status, status, new_netkey_index);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_netkey_list(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest) {
    UNUSED(mesh_model);

    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(MESH_FOUNDATION_OPERATION_NETKEY_LIST);
    if (!transport_pdu) return;

    // add list of netkey indexes
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        mesh_access_transport_add_uint16(transport_pdu, network_key->netkey_index);
    }

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_netkey_add_derived(void * arg){
    mesh_subnet_t * subnet = (mesh_subnet_t *) arg;

    // store network key
    mesh_store_network_key(subnet->old_key);

    // add key to NetKey List
    mesh_network_key_add(subnet->old_key);

    // add subnet
    mesh_subnet_add(subnet);

#ifdef ENABLE_MESH_PROXY_SERVER
    mesh_proxy_start_advertising_with_network_id();
#endif

    config_netkey_status(mesh_model_get_configuration_server(), mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), MESH_FOUNDATION_STATUS_SUCCESS, subnet->netkey_index);
    mesh_access_message_processed(access_pdu_in_process);
}

static void config_netkey_add_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // get params
    uint8_t new_netkey[16];
    uint16_t new_netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_access_parser_get_key(&parser, new_netkey);

    uint8_t status;

    const mesh_subnet_t * existing_subnet = mesh_subnet_get_by_netkey_index(new_netkey_index);
    if (existing_subnet == NULL){

        // check limit for pts
        uint16_t internal_index = mesh_network_key_get_free_index();
        if (internal_index == 0 || (config_netkey_list_max && mesh_network_key_list_count() >= config_netkey_list_max)){
            status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
        } else {

            // allocate new key and subnet
            mesh_network_key_t * new_network_key = btstack_memory_mesh_network_key_get();
            mesh_subnet_t * new_subnet = btstack_memory_mesh_subnet_get();

            if (new_network_key == NULL || new_subnet == NULL){
                if (new_network_key != NULL){
                    btstack_memory_mesh_network_key_free(new_network_key);
                }
                if (new_subnet != NULL){
                    btstack_memory_mesh_subnet_free(new_subnet);
                }
                status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;

            } else {

                // setup key
                new_network_key->internal_index = internal_index;
                new_network_key->netkey_index   = new_netkey_index;
                memcpy(new_network_key->net_key, new_netkey, 16);

                // setup subnet
                new_subnet->old_key = new_network_key;
                new_subnet->netkey_index = new_netkey_index;
                new_subnet->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;

                // derive other keys
                access_pdu_in_process = pdu;
                mesh_network_key_derive(&configuration_server_cmac_request, new_network_key, config_netkey_add_derived, new_network_key);
                return;
            }
        }

    } else {
        // network key for netkey index already exists
        if (memcmp(existing_subnet->old_key->net_key, new_netkey, 16) == 0){
            // same netkey
            status = MESH_FOUNDATION_STATUS_SUCCESS;
        } else {
            // different netkey
            status = MESH_FOUNDATION_STATUS_KEY_INDEX_ALREADY_STORED;
        }
    }

    // report status    
    config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, new_netkey_index);
    mesh_access_message_processed(access_pdu_in_process);
}

static void config_netkey_update_derived(void * arg){
    mesh_subnet_t * subnet = (mesh_subnet_t *) arg;

    // store network key
    mesh_store_network_key(subnet->new_key);

    // add key to NetKey List
    mesh_network_key_add(subnet->new_key);

    // update subnet - Key Refresh Phase 1
    subnet->key_refresh = MESH_KEY_REFRESH_FIRST_PHASE;

    // report status    
    config_netkey_status(mesh_model_get_configuration_server(), mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), MESH_FOUNDATION_STATUS_SUCCESS, subnet->netkey_index);
    mesh_access_message_processed(access_pdu_in_process);
}

static void config_netkey_update_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);

    // get params
    uint8_t new_netkey[16];
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_access_parser_get_key(&parser, new_netkey);

    // get existing subnet
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (subnet == NULL){
        config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX, netkey_index);
        mesh_access_message_processed(access_pdu_in_process);
        return;
    }

    // setup new key
    uint16_t internal_index = mesh_network_key_get_free_index();
    mesh_network_key_t * new_network_key = btstack_memory_mesh_network_key_get();
    if (internal_index == 0 || new_network_key == NULL){
        config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES, netkey_index);
        mesh_access_message_processed(access_pdu_in_process);
        return;
    }

    // setup new key
    new_network_key->internal_index = internal_index;
    new_network_key->netkey_index   = netkey_index;
    new_network_key->version        = (uint8_t)(subnet->old_key->version + 1);
    memcpy(new_network_key->net_key, new_netkey, 16);

    // store in subnet (not active yet)
    subnet->new_key = new_network_key;    

    // derive other keys
    access_pdu_in_process = pdu;
    mesh_network_key_derive(&configuration_server_cmac_request, new_network_key, config_netkey_update_derived, subnet);
}

static void config_netkey_delete_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);

    // get params
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);

    // get existing network_key
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;

    // remove subnet
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (subnet != NULL){
        // A NetKey shall not be deleted from the NetKey List using a message secured with this NetKey.
        if (mesh_subnet_list_count() > 1 && subnet->netkey_index != netkey_index){

            // remove all appkeys for this netkey
            mesh_transport_key_iterator_t it;
            mesh_transport_key_iterator_init(&it, netkey_index);
            while (mesh_transport_key_iterator_has_more(&it)){
                mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
                mesh_configuration_server_delete_appkey(transport_key);
            }

            // delete old/current key
            mesh_access_netkey_finalize(subnet->old_key);
            subnet->old_key = NULL;

            // delete new key
            if (subnet->new_key != NULL){
                mesh_access_netkey_finalize(subnet->new_key);
                subnet->new_key = NULL;
            }

            // remove subnet
            mesh_subnet_remove(subnet);
            btstack_memory_mesh_subnet_free(subnet);

        } else {
            // we cannot remove the last network key
            status = MESH_FOUNDATION_STATUS_CANNOT_REMOVE;
        }
    }
    config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index);
}

static void config_netkey_get_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    config_netkey_list(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

// AppKey List

static void config_appkey_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint32_t netkey_and_appkey_index, uint8_t status){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_appkey_status,
                                                                               status, netkey_and_appkey_index);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_appkey_list(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint32_t netkey_index_of_list){
    UNUSED(mesh_model);

    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(MESH_FOUNDATION_OPERATION_APPKEY_LIST);
    if (!transport_pdu) return;

    // check netkey_index is valid
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index_of_list);
    uint8_t status;
    if (network_key == NULL){
        status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    } else {
        status = MESH_FOUNDATION_STATUS_SUCCESS;
    }
    mesh_access_transport_add_uint8(transport_pdu, status);
    mesh_access_transport_add_uint16(transport_pdu, netkey_index_of_list);

    // add list of appkey indexes
    mesh_transport_key_iterator_t it;
    mesh_transport_key_iterator_init(&it, netkey_index_of_list);
    while (mesh_transport_key_iterator_has_more(&it)){
        mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
        mesh_access_transport_add_uint16(transport_pdu, transport_key->appkey_index);
    }

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_appkey_add_or_update_aid(void *arg){
    mesh_transport_key_t * transport_key = (mesh_transport_key_t *) arg;

    printf("Config Appkey Add/Update: NetKey Index 0x%04x, AppKey Index 0x%04x, AID %02x: ", transport_key->netkey_index, transport_key->appkey_index, transport_key->aid);
    printf_hexdump(transport_key->key, 16);

    // store in TLV
    mesh_store_app_key(transport_key);

    // add app key
    mesh_transport_key_add(transport_key);

    uint32_t netkey_and_appkey_index = (transport_key->appkey_index << 12) | transport_key->netkey_index;
    config_appkey_status(mesh_model_get_configuration_server(),  mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);

    mesh_access_message_processed(access_pdu_in_process);
}

static void config_appkey_add_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // netkey and appkey index
    uint32_t netkey_and_appkey_index = mesh_access_parser_get_u24(&parser);
    uint16_t netkey_index = netkey_and_appkey_index & 0xfff;
    uint16_t appkey_index = netkey_and_appkey_index >> 12;

    // actual key
    uint8_t  appkey[16];
    mesh_access_parser_get_key(&parser, appkey);

    // check netkey_index is valid
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    // check if appkey already exists
    mesh_transport_key_t * transport_key = mesh_transport_key_get(appkey_index);
    if (transport_key){
        uint8_t status;
        if (transport_key->netkey_index != netkey_index){
            // already stored but with different netkey
            status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
        } else if (memcmp(transport_key->key, appkey, 16) == 0){
            // key identical
            status = MESH_FOUNDATION_STATUS_SUCCESS;
        } else {
            // key differs
            status = MESH_FOUNDATION_STATUS_KEY_INDEX_ALREADY_STORED;
        }
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, status);
        mesh_access_message_processed(pdu);
        return;
    }

    // create app key (first get free slot in transport key table)
    mesh_transport_key_t * app_key = NULL;
    uint16_t internal_index = mesh_transport_key_get_free_index();
    if (internal_index > 0){
        app_key = btstack_memory_mesh_transport_key_get();
    }    
    if (app_key == NULL) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES);
        mesh_access_message_processed(pdu);
        return;
    }

    // store data
    app_key->internal_index = internal_index;
    app_key->akf = 1;
    app_key->appkey_index = appkey_index;
    app_key->netkey_index = netkey_index;
    app_key->version = 0;
    app_key->old_key = 0;

    memcpy(app_key->key, appkey, 16);

    // calculate AID
    access_pdu_in_process = pdu;
    mesh_transport_key_calc_aid(&configuration_server_cmac_request, app_key, config_appkey_add_or_update_aid, app_key);
}

static void config_appkey_update_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // netkey and appkey index
    uint32_t netkey_and_appkey_index = mesh_access_parser_get_u24(&parser);
    uint16_t netkey_index = netkey_and_appkey_index & 0xfff;
    uint16_t appkey_index = netkey_and_appkey_index >> 12;

    // actual key
    uint8_t  appkey[16];
    mesh_access_parser_get_key(&parser, appkey);


// for PTS testing
    // check netkey_index is valid
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    // check if appkey already exists
    mesh_transport_key_t * existing_app_key = mesh_transport_key_get(appkey_index);
    if (!existing_app_key) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    if (existing_app_key->netkey_index != netkey_index){
        // already stored but with different netkey
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_BINDING);
        mesh_access_message_processed(pdu);
        return;
    }

    if (memcmp(existing_app_key->key, appkey, 16) == 0){
        // key identical
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);
        mesh_access_message_processed(pdu);
        return;
    }

    // create app key
    mesh_transport_key_t * new_app_key = NULL;
    uint16_t internal_index = mesh_transport_key_get_free_index();
    if (internal_index > 0){
        new_app_key = btstack_memory_mesh_transport_key_get();
    }    
    if (new_app_key == NULL) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES);
        mesh_access_message_processed(pdu);
        return;
    }

    // store data
    new_app_key->internal_index = internal_index;
    new_app_key->appkey_index = appkey_index;
    new_app_key->netkey_index = netkey_index;
    new_app_key->key_refresh  = 1;
    new_app_key->version = (uint8_t)(existing_app_key->version + 1);
    memcpy(new_app_key->key, appkey, 16);

    // mark old key
    existing_app_key->old_key = 1;

    // calculate AID
    access_pdu_in_process = pdu;
    mesh_transport_key_calc_aid(&configuration_server_cmac_request, new_app_key, config_appkey_add_or_update_aid, new_app_key);
}

static void config_appkey_delete_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);

    // netkey and appkey index
    uint32_t netkey_and_appkey_index = mesh_access_parser_get_u24(&parser);
    uint16_t netkey_index = netkey_and_appkey_index & 0xfff;
    uint16_t appkey_index = netkey_and_appkey_index >> 12;

    // check netkey_index is valid
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    // check if appkey already exists
    mesh_transport_key_t * transport_key = mesh_transport_key_get(appkey_index);
    if (transport_key){
        mesh_configuration_server_delete_appkey(transport_key);
    }

    config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);
    mesh_access_message_processed(pdu);
}

static void config_appkey_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);

    config_appkey_list(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_index);
    mesh_access_message_processed(pdu);
}

// Configuration Model Subscriptions (messages)

static void config_model_subscription_list_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint32_t model_identifier, mesh_model_t * target_model){
    UNUSED(mesh_model);

    uint16_t opcode;
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        opcode = MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_LIST;
    } else {
        opcode = MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_LIST;
    }

    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(opcode);
    if (!transport_pdu) return;

    // setup segmented message
    mesh_access_transport_add_uint8(transport_pdu, status);
    mesh_access_transport_add_uint16(transport_pdu, element_address);
    mesh_access_transport_add_model_identifier(transport_pdu, model_identifier);

    if (target_model != NULL){
        uint16_t i;
        for (i = 0; i < MAX_NR_MESH_SUBSCRIPTION_PER_MODEL; i++){
            if (target_model->subscriptions[i] == MESH_ADDRESS_UNSASSIGNED) continue;
            if (mesh_network_address_virtual(target_model->subscriptions[i])) continue;
            mesh_access_transport_add_uint16(transport_pdu, target_model->subscriptions[i]);
        }
    }   
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_subscription_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    config_model_subscription_list_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, model_identifier, target_model);
    mesh_access_message_processed(pdu);
}

static void config_model_subscription_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint16_t address, uint32_t model_identifier){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_model_subscription_status,
                                                                                status, element_address, address, model_identifier);
    if (!transport_pdu) return;
    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_subscription_add_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    
    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    uint16_t address          = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if (target_model != NULL){
        if (mesh_network_address_group(address) && !mesh_network_address_all_nodes(address)){
            status = mesh_model_add_subscription(target_model, address);
            if (status == MESH_FOUNDATION_STATUS_SUCCESS){
                mesh_model_store_subscriptions(target_model);
            }
        } else {
            status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        }
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, address, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_subscription_virtual_address_add_hash(void *arg){
    mesh_model_t * target_model = (mesh_model_t*) arg;
    mesh_model_t * mesh_model = mesh_model_get_configuration_server();

    // add if not exists
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(configuration_server_label_uuid);
    if (virtual_address == NULL){
        virtual_address = mesh_virtual_address_register(configuration_server_label_uuid, configuration_server_hash);
    }

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    uint16_t pseudo_dst = MESH_ADDRESS_UNSASSIGNED;
    if (virtual_address == NULL){
        status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
    } else {
        pseudo_dst = virtual_address->pseudo_dst;
        if (!mesh_model_contains_subscription(target_model, pseudo_dst)){
            status = mesh_model_add_subscription(target_model, pseudo_dst);
            if (status == MESH_FOUNDATION_STATUS_SUCCESS){
                mesh_virtual_address_increase_refcount(virtual_address);
                mesh_model_store_subscriptions(target_model);
            }
        }
    }

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, configuration_server_element_address, pseudo_dst, target_model->model_identifier);
    mesh_access_message_processed(access_pdu_in_process);
    return;
}

static void config_model_subscription_virtual_address_add_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    configuration_server_element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, configuration_server_label_uuid);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(configuration_server_element_address, model_identifier, &status);

    if (target_model == NULL){
        config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, configuration_server_element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
        mesh_access_message_processed(pdu);
        return;
    }

    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, configuration_server_label_uuid, &configuration_server_hash, &config_model_subscription_virtual_address_add_hash, target_model);
}

static void config_model_subscription_overwrite_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    uint16_t address          = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if (target_model != NULL){
        if (mesh_network_address_group(address) && !mesh_network_address_all_nodes(address)){
            mesh_subcription_decrease_virtual_address_ref_count(target_model);
            mesh_model_delete_all_subscriptions(mesh_model);
            mesh_model_add_subscription(mesh_model, address);
            mesh_model_store_subscriptions(target_model);
        } else {
            status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        } 
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, address, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_subscription_virtual_address_overwrite_hash(void *arg){
    mesh_model_t * target_model = (mesh_model_t*) arg;
    mesh_model_t * mesh_model = mesh_model_get_configuration_server();

    // add if not exists
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(configuration_server_label_uuid);
    if (virtual_address == NULL){
        virtual_address = mesh_virtual_address_register(configuration_server_label_uuid, configuration_server_hash);
    }

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    uint16_t pseudo_dst = MESH_ADDRESS_UNSASSIGNED;
    if (virtual_address == NULL){
        status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
    } else {

        // increase refcount first to avoid flash delete + add in a row
        mesh_virtual_address_increase_refcount(virtual_address);

        // decrease ref counts for virtual addresses in subscription
        mesh_subcription_decrease_virtual_address_ref_count(target_model);

        // clear subscriptions
        mesh_model_delete_all_subscriptions(target_model);

        // add new subscription (successfull if MAX_NR_MESH_SUBSCRIPTION_PER_MODEL > 0)
        pseudo_dst = virtual_address->pseudo_dst;
        mesh_model_add_subscription(target_model, pseudo_dst);

        mesh_model_store_subscriptions(target_model);
    }

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, configuration_server_element_address, pseudo_dst, target_model->model_identifier);
    mesh_access_message_processed(access_pdu_in_process);
    return;
}

static void config_model_subscription_virtual_address_overwrite_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    configuration_server_element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, configuration_server_label_uuid);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(configuration_server_element_address, model_identifier, &status);

    if (target_model == NULL){
        config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, configuration_server_element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
        mesh_access_message_processed(pdu);
        return;
    }
    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, configuration_server_label_uuid, &configuration_server_hash, &config_model_subscription_virtual_address_overwrite_hash, target_model);
}

static void config_model_subscription_delete_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    uint16_t address          = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if (target_model != NULL){
        if (mesh_network_address_group(address) && !mesh_network_address_all_nodes(address)){
            mesh_model_delete_subscription(target_model, address);
            mesh_model_store_subscriptions(target_model);
        } else {
            status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        }
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, address, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_subscription_virtual_address_delete_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    mesh_access_parser_get_label_uuid(&parser, configuration_server_label_uuid);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(configuration_server_label_uuid);
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if ((target_model != NULL) && (virtual_address != NULL) && mesh_model_contains_subscription(target_model, virtual_address->pseudo_dst)){
        mesh_virtual_address_decrease_refcount(virtual_address);
        mesh_model_delete_subscription(target_model, virtual_address->pseudo_dst);
        mesh_model_store_subscriptions(target_model);
     }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, virtual_address->hash, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_subscription_delete_all_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address  = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if (target_model != NULL){
        mesh_subcription_decrease_virtual_address_ref_count(target_model);
        mesh_model_delete_all_subscriptions(target_model);
        mesh_model_store_subscriptions(target_model);
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
    mesh_access_message_processed(pdu);
}

// Configuration Model to AppKey List

static void config_model_app_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint16_t appkey_index, uint32_t model_identifier){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_model_app_status,
                                                            status, element_address, appkey_index, model_identifier);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_app_list(mesh_model_t * config_server_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint32_t model_identifier, mesh_model_t * mesh_model){
    UNUSED(config_server_model);

    uint16_t opcode;
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        opcode = MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_LIST;
    } else {
        opcode = MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_LIST;
    }
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(opcode);
    if (!transport_pdu) return;

    mesh_access_transport_add_uint8(transport_pdu, status);
    mesh_access_transport_add_uint16(transport_pdu, element_address);
    if (mesh_model_is_bluetooth_sig(model_identifier)) {
        mesh_access_transport_add_uint16(transport_pdu, mesh_model_get_model_id(model_identifier));
    } else {
        mesh_access_transport_add_uint32(transport_pdu, model_identifier);
    }
    
    // add list of appkey indexes
    if (mesh_model){
        uint16_t i;
        for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
            uint16_t appkey_index = mesh_model->appkey_indices[i];
            if (appkey_index == MESH_APPKEY_INVALID) continue;
            mesh_access_transport_add_uint16(transport_pdu, appkey_index);
        }
    }

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_app_bind_handler(mesh_model_t *config_server_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address = mesh_access_parser_get_u16(&parser);
    uint16_t appkey_index    = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    do {
        // validate address and look up model
        mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);
        if (target_model == NULL) break;

        // validate app key exists
        mesh_transport_key_t * app_key = mesh_transport_key_get(appkey_index);
        if (!app_key){
            status = MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX;
            break;
        }

        // Configuration Server only allows device keys
        if (mesh_model_is_configuration_server(model_identifier)){
            status = MESH_FOUNDATION_STATUS_CANNOT_BIND;
            break;
        }
        status = mesh_model_bind_appkey(target_model, appkey_index);

    } while (0);

    config_model_app_status(config_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, appkey_index, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_app_unbind_handler(mesh_model_t *config_server_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address = mesh_access_parser_get_u16(&parser);
    uint16_t appkey_index    = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    do {
        // validate address and look up model
        mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);
        if (target_model == NULL) break;

        // validate app key exists
        mesh_transport_key_t * app_key = mesh_transport_key_get(appkey_index);
        if (!app_key){
            status = MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX;
            break;
        }
        mesh_model_unbind_appkey(target_model, appkey_index);
        status = MESH_FOUNDATION_STATUS_SUCCESS;
    } while (0);

    config_model_app_status(config_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, appkey_index, model_identifier);
    mesh_access_message_processed(pdu);
}

static void config_model_app_get_handler(mesh_model_t *config_server_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);
    config_model_app_list(config_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, model_identifier, target_model);
    mesh_access_message_processed(pdu);
}

// Model Publication

static void config_model_publication_changed(mesh_model_t *mesh_model, mesh_publication_model_t * new_publication_model){

    // stop publication
    mesh_model_publication_stop(mesh_model);

    // update model publication state
    memcpy(mesh_model->publication_model, &configuration_server_publication_model, sizeof(mesh_publication_model_t));

    // store
    mesh_model_store_publication(mesh_model);

    // start publication if address is set (nothing happens if period = 0 and retransmit = 0)
    if (new_publication_model->address == MESH_ADDRESS_UNSASSIGNED) return;

    // start to publish
    mesh_model_publication_start(mesh_model);
}


static void
config_model_publication_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status,
                                    uint16_t element_address, uint32_t model_identifier, mesh_publication_model_t *publication_model) {
    UNUSED(mesh_model);

    // setup message
    uint16_t app_key_index_and_credential_flag = (publication_model->friendship_credential_flag << 12) | publication_model->appkey_index;
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_model_app_status, status, element_address, publication_model->address,
            app_key_index_and_credential_flag,
            publication_model->ttl, publication_model->period, publication_model->retransmit, model_identifier);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void
config_model_publication_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {

    // set defaults
    memset(&configuration_server_publication_model, 0, sizeof(mesh_publication_model_t));

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    uint16_t element_address = mesh_access_parser_get_u16(&parser);

    // PublishAddress, 16 bit
    configuration_server_publication_model.address = mesh_access_parser_get_u16(&parser);

    // AppKeyIndex (12), CredentialFlag (1), RFU (3)
    uint16_t temp = mesh_access_parser_get_u16(&parser);
    configuration_server_publication_model.appkey_index = temp & 0x0fff;
    configuration_server_publication_model.friendship_credential_flag = (temp >> 12) & 1;

    // TTL
    configuration_server_publication_model.ttl        = mesh_access_parser_get_u8(&parser);

    // Period
    configuration_server_publication_model.period     = mesh_access_parser_get_u8(&parser);

    // Retransmit
    configuration_server_publication_model.retransmit = mesh_access_parser_get_u8(&parser);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);
    uint8_t status;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    // TODO validate params

    if (target_model){
        if (target_model->publication_model == NULL){
            status = MESH_FOUNDATION_STATUS_CANNOT_SET;
        } else {

            // decrease ref count if old virtual address
            if (mesh_network_address_virtual(configuration_server_publication_model.address)) {
                mesh_virtual_address_t * current_virtual_address = mesh_virtual_address_for_pseudo_dst(configuration_server_publication_model.address);
                mesh_virtual_address_decrease_refcount(current_virtual_address);
            }

            // restart publication
            config_model_publication_changed(target_model, &configuration_server_publication_model);
        }
    }

    // send status
    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, model_identifier, &configuration_server_publication_model);
    mesh_access_message_processed(pdu);
}

static void config_model_publication_virtual_address_set_hash(void *arg){
    mesh_model_t *mesh_model = (mesh_model_t*) arg;

    // add if not exist
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(configuration_server_label_uuid);
    if (virtual_address == NULL){
        virtual_address = mesh_virtual_address_register(configuration_server_label_uuid, configuration_server_hash);
    }

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    if (virtual_address == NULL){
        status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
    } else {

        // increase ref count if new virtual address
        mesh_virtual_address_increase_refcount(virtual_address);

        // decrease ref count if old virtual address
        if (mesh_network_address_virtual(configuration_server_publication_model.address)) {
            mesh_virtual_address_t * current_virtual_address = mesh_virtual_address_for_pseudo_dst(configuration_server_publication_model.address);
            mesh_virtual_address_decrease_refcount(current_virtual_address);
        }

        configuration_server_publication_model.address = virtual_address->pseudo_dst;
        mesh_virtual_address_increase_refcount(virtual_address);

        // restart publication
        config_model_publication_changed(configuration_server_target_model, &configuration_server_publication_model);
    }

    // send status
    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, configuration_server_element_address, configuration_server_model_identifier, &configuration_server_publication_model);

    mesh_access_message_processed(access_pdu_in_process);
}

static void 
config_model_publication_virtual_address_set_handler(mesh_model_t *mesh_model,
                                                     mesh_pdu_t * pdu) {

    // set defaults
    memset(&configuration_server_publication_model, 0, sizeof(mesh_publication_model_t));

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element
    configuration_server_element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, configuration_server_label_uuid);

    // AppKeyIndex (12), CredentialFlag (1), RFU (3)
    uint16_t temp = mesh_access_parser_get_u16(&parser);
    configuration_server_publication_model.appkey_index = temp & 0x0fff;
    configuration_server_publication_model.friendship_credential_flag = (temp >> 12) & 1;
    configuration_server_publication_model.ttl        = mesh_access_parser_get_u8(&parser);
    configuration_server_publication_model.period     = mesh_access_parser_get_u8(&parser);
    configuration_server_publication_model.retransmit = mesh_access_parser_get_u8(&parser);

    // Model Identifier
    configuration_server_model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    configuration_server_target_model = mesh_access_model_for_address_and_model_identifier(configuration_server_element_address, configuration_server_model_identifier, &status);

    // model exists, but no publication model
    if (configuration_server_target_model != NULL && configuration_server_target_model->publication_model == NULL){
        status = MESH_FOUNDATION_STATUS_CANNOT_SET;
    }

    // on error, no need to calculate virtual address hash
    if (status != MESH_FOUNDATION_STATUS_SUCCESS){
        config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, configuration_server_element_address, configuration_server_model_identifier, &configuration_server_publication_model);
        mesh_access_message_processed(pdu);
        return;
    }

    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, configuration_server_label_uuid, &configuration_server_publication_model.address, &config_model_publication_virtual_address_set_hash, mesh_model);
}

static void
config_model_publication_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {


    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    uint16_t element_address = mesh_access_parser_get_u16(&parser);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    mesh_publication_model_t * publication_model;

    if (target_model == NULL){
        // use defaults
        memset(&configuration_server_publication_model, 0, sizeof(mesh_publication_model_t));
        publication_model = &configuration_server_publication_model;
    } else {
        publication_model = target_model->publication_model;
    }

    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, configuration_server_element_address, model_identifier, publication_model);
    mesh_access_message_processed(pdu);
}

// Heartbeat Publication

static void config_heartbeat_publication_emit(mesh_heartbeat_publication_t * mesh_heartbeat_publication){

    printf("CONFIG_SERVER_HEARTBEAT: Emit (dest %04x, count %u, period %u ms)\n",
        mesh_heartbeat_publication->destination, 
        mesh_heartbeat_publication->count, 
        mesh_heartbeat_publication->period_ms);
    
    // active features
    mesh_heartbeat_publication->active_features = mesh_foundation_get_features();

    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (network_pdu){
        uint8_t data[3];
        data[0] = mesh_heartbeat_publication->ttl;
        big_endian_store_16(data, 1, mesh_heartbeat_publication->active_features);
        mesh_upper_transport_setup_control_pdu((mesh_pdu_t *) network_pdu, mesh_heartbeat_publication->netkey_index,
                mesh_heartbeat_publication->ttl, mesh_node_get_primary_element_address(), mesh_heartbeat_publication->destination,
                MESH_TRANSPORT_OPCODE_HEARTBEAT, data, sizeof(data));
        mesh_upper_transport_send_control_pdu((mesh_pdu_t *) network_pdu);
    }

    // forever
    if (mesh_heartbeat_publication->count < 0xffffu) {
        mesh_heartbeat_publication->count--;
    }
}
void mesh_configuration_server_feature_changed(void){
    mesh_model_t * mesh_model = mesh_model_get_configuration_server();
    mesh_heartbeat_publication_t * mesh_heartbeat_publication = &((mesh_configuration_server_model_context_t*) mesh_model->model_data)->heartbeat_publication;

    // active features
    uint16_t active_features = mesh_foundation_get_features();
    if (mesh_heartbeat_publication->active_features == active_features) return;
    config_heartbeat_publication_emit(mesh_heartbeat_publication);
}

static void config_heartbeat_publication_timeout_handler(btstack_timer_source_t * ts){

    mesh_heartbeat_publication_t * mesh_heartbeat_publication = (mesh_heartbeat_publication_t*) ts;

    // emit beat
    config_heartbeat_publication_emit(mesh_heartbeat_publication);

    // all sent?
    if (mesh_heartbeat_publication->count == 0) return;

    // periodic publication?
    if (mesh_heartbeat_publication->period_ms == 0) return;

    btstack_run_loop_set_timer(ts, mesh_heartbeat_publication->period_ms);
    btstack_run_loop_add_timer(ts);
}

static void config_heartbeat_publication_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, mesh_heartbeat_publication_t * mesh_heartbeat_publication){
    UNUSED(mesh_model);

    // setup message
    uint8_t count_log = heartbeat_count_log(mesh_heartbeat_publication->count);
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_heartbeat_publication_status,
            status,
            mesh_heartbeat_publication->destination,
            count_log,
            mesh_heartbeat_publication->period_log,
            mesh_heartbeat_publication->ttl,
            mesh_heartbeat_publication->features,
            mesh_heartbeat_publication->netkey_index);
    if (!transport_pdu) return;
    printf("MESH config_heartbeat_publication_status count = %u => count_log = %u\n", mesh_heartbeat_publication->count, count_log);

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_heartbeat_publication_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {

    mesh_heartbeat_publication_t requested_publication;

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // Destination address for Heartbeat messages
    requested_publication.destination = mesh_access_parser_get_u16(&parser);
    // Number of Heartbeat messages to be sent
    requested_publication.count = heartbeat_pwr2(mesh_access_parser_get_u8(&parser));
    //  Period for sending Heartbeat messages
    requested_publication.period_log = mesh_access_parser_get_u8(&parser);
    //  TTL to be used when sending Heartbeat messages
    requested_publication.ttl = mesh_access_parser_get_u8(&parser);
    // Bit field indicating features that trigger Heartbeat messages when changed
    requested_publication.features = mesh_access_parser_get_u16(&parser) & MESH_HEARTBEAT_FEATURES_SUPPORTED_MASK;
    // NetKey Index
    requested_publication.netkey_index = mesh_access_parser_get_u16(&parser);

    // store period as ms
    requested_publication.period_ms = heartbeat_pwr2(requested_publication.period_log) * 1000;

    mesh_heartbeat_publication_t * mesh_heartbeat_publication = &((mesh_configuration_server_model_context_t*) mesh_model->model_data)->heartbeat_publication;

    // validate fields
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_network_key_t * network_key = mesh_network_key_list_get(requested_publication.netkey_index);
    if (network_key == NULL){
        status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    } else {
        printf("MESH config_heartbeat_publication_set, destination %x, count = %x, period = %u s\n",
            requested_publication.destination, requested_publication.count, requested_publication.period_ms);

        // accept update
        memcpy(mesh_heartbeat_publication, &requested_publication, sizeof(mesh_heartbeat_publication_t));
    }

    config_heartbeat_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, mesh_heartbeat_publication);

    mesh_access_message_processed(pdu);

    if (status != MESH_FOUNDATION_STATUS_SUCCESS) return;

    // check if heartbeats should be disabled
    if (mesh_heartbeat_publication->destination == MESH_ADDRESS_UNSASSIGNED || mesh_heartbeat_publication->period_log == 0) {
        btstack_run_loop_remove_timer(&mesh_heartbeat_publication->timer);
        printf("MESH config_heartbeat_publication_set, disable\n");
        return;
    }

    // NOTE: defer first heartbeat to allow config status getting sent first
    btstack_run_loop_set_timer_handler(&mesh_heartbeat_publication->timer, config_heartbeat_publication_timeout_handler);
    btstack_run_loop_set_timer_context(&mesh_heartbeat_publication->timer, mesh_heartbeat_publication);
    btstack_run_loop_set_timer(&mesh_heartbeat_publication->timer, 2000);
    btstack_run_loop_add_timer(&mesh_heartbeat_publication->timer);
}

static void config_heartbeat_publication_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_heartbeat_publication_t * mesh_heartbeat_publication = &((mesh_configuration_server_model_context_t*) mesh_model->model_data)->heartbeat_publication;
    config_heartbeat_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_SUCCESS, mesh_heartbeat_publication);
    mesh_access_message_processed(pdu);
}

// Heartbeat Subscription

static void config_heartbeat_subscription_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, mesh_heartbeat_subscription_t * mesh_heartbeat_subscription){
    UNUSED(mesh_model);

    // setup message
    uint8_t count_log = heartbeat_count_log(mesh_heartbeat_subscription->count_log);
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_heartbeat_subscription_status,
            status,
            mesh_heartbeat_subscription->source,
            mesh_heartbeat_subscription->destination,
            mesh_heartbeat_subscription->period_log,
            count_log,
            mesh_heartbeat_subscription->min_hops,
            mesh_heartbeat_subscription->max_hops);
    if (!transport_pdu) return;
    printf("MESH config_heartbeat_subscription_status count = %u => count_log = %u\n", mesh_heartbeat_subscription->count_log, count_log);

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}
static int config_heartbeat_subscription_enabled(mesh_heartbeat_subscription_t * mesh_heartbeat_subscription){
    return mesh_network_address_unicast(mesh_heartbeat_subscription->source) && mesh_heartbeat_subscription->period_log > 0 &&
        (mesh_network_address_unicast(mesh_heartbeat_subscription->destination) || mesh_network_address_group(mesh_heartbeat_subscription->destination));
}

static void config_heartbeat_subscription_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    mesh_heartbeat_subscription_t requested_subscription;

    // Destination address for Heartbeat messages
    requested_subscription.source = mesh_access_parser_get_u16(&parser);
    // Destination address for Heartbeat messages
    requested_subscription.destination = mesh_access_parser_get_u16(&parser);
    //  Period for sending Heartbeat messages
    requested_subscription.period_log = mesh_access_parser_get_u8(&parser);
    
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    if (requested_subscription.period_log > 0x11u){
        status = MESH_FOUNDATION_STATUS_CANNOT_SET;
    } else if ((requested_subscription.source != MESH_ADDRESS_UNSASSIGNED)       && 
               !mesh_network_address_unicast(requested_subscription.source)){
        status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
    } else if ((requested_subscription.destination != MESH_ADDRESS_UNSASSIGNED)  && 
               !mesh_network_address_unicast(requested_subscription.destination) &&
               !mesh_network_address_group(requested_subscription.destination)){
        status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
    } 

    if (status != MESH_FOUNDATION_STATUS_SUCCESS){
        config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, &requested_subscription);
        mesh_access_message_processed(pdu);
        return;
    }

    mesh_heartbeat_subscription_t * mesh_heartbeat_subscription = &((mesh_configuration_server_model_context_t*) mesh_model->model_data)->heartbeat_subscription;

    if (config_heartbeat_subscription_enabled(mesh_heartbeat_subscription)){
        requested_subscription.count_log  = 0u;
        requested_subscription.min_hops   = 0x7Fu;
        requested_subscription.max_hops   = 0u;
    } else {
        requested_subscription.source = MESH_ADDRESS_UNSASSIGNED;
        requested_subscription.destination = MESH_ADDRESS_UNSASSIGNED;
        requested_subscription.period_log = 0u;
        requested_subscription.count_log  = 0u;
        requested_subscription.min_hops   = 0u;
        requested_subscription.max_hops   = 0u;
    }
    
    // accept request
    memcpy(mesh_heartbeat_subscription, &requested_subscription, sizeof(mesh_heartbeat_subscription_t));

    printf("MESH config_heartbeat_subscription_set, destination %x, count = %x, period = %u s\n",
            mesh_heartbeat_subscription->destination, mesh_heartbeat_subscription->count_log, heartbeat_pwr2(mesh_heartbeat_subscription->period_log));

    config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, mesh_heartbeat_subscription);
    mesh_access_message_processed(pdu);
}


static void config_heartbeat_subscription_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_heartbeat_subscription_t * mesh_heartbeat_subscription = &((mesh_configuration_server_model_context_t*) mesh_model->model_data)->heartbeat_subscription;
    config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_SUCCESS, mesh_heartbeat_subscription);
    mesh_access_message_processed(pdu);
}

// KeyRefresh Phase

static void config_key_refresh_phase_status(mesh_model_t *mesh_model, uint16_t netkey_index_dest, uint16_t dest, uint8_t status, uint16_t netkey_index,
    mesh_key_refresh_state_t key_refresh_state){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_key_refresh_phase_status,
        status,
        netkey_index,
        key_refresh_state);
    if (!transport_pdu) return;
    
    // send as segmented access pdu
    config_server_send_message(netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_key_refresh_phase_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);

    uint8_t status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    mesh_key_refresh_state_t key_refresh_state = MESH_KEY_REFRESH_NOT_ACTIVE;
    
    if (subnet != NULL){
        status = MESH_FOUNDATION_STATUS_SUCCESS;
        key_refresh_state = subnet->key_refresh;
    }

    config_key_refresh_phase_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, key_refresh_state);
    mesh_access_message_processed(pdu);
}

static void config_key_refresh_phase_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    uint8_t  key_refresh_phase_transition = mesh_access_parser_get_u8(&parser);
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);

    uint8_t status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    
    if (subnet != NULL){
        status = MESH_FOUNDATION_STATUS_SUCCESS;
        
        switch (key_refresh_phase_transition){
            case 0x02:
                switch (subnet->key_refresh){
                    case MESH_KEY_REFRESH_FIRST_PHASE:
                    case MESH_KEY_REFRESH_SECOND_PHASE:
                        subnet->key_refresh = MESH_KEY_REFRESH_SECOND_PHASE;
                        break;
                    default:
                        break;
                }
                break;
            case 0x03:
                switch (subnet->key_refresh){
                    case MESH_KEY_REFRESH_FIRST_PHASE:
                    case MESH_KEY_REFRESH_SECOND_PHASE:
                        // key refresh phase 3 entered
                        mesh_access_key_refresh_revoke_keys(subnet);
                        subnet->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;
                        break;
                    default:
                        break;
                }
                break;
            default:
                status = MESH_FOUNDATION_STATUS_CANNOT_SET;
                break;
        }
    }

    config_key_refresh_phase_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, subnet->key_refresh);
    mesh_access_message_processed(pdu);
}


static void config_node_reset_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_node_reset_status);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_node_reset_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_node_reset();
    config_node_reset_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static void low_power_node_poll_timeout_status(mesh_model_t *mesh_model, uint16_t netkey_index_dest, uint16_t dest, uint8_t status){
    UNUSED(mesh_model);

    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_foundation_low_power_node_poll_timeout_status,
        status,
        0,  // The unicast address of the Low Power node
        0); // The current value of the PollTimeout timer of the Low Power node
    if (!transport_pdu) return;
    printf("TODO: send unicast address of the Low Power node and the current value of the PollTimeout timer, instead of 0s\n");
    // send as segmented access pdu
    config_server_send_message(netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_low_power_node_poll_timeout_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    printf("TODO: implement get the current value of PollTimeout timer of the Low Power node within a Friend node\n");
    low_power_node_poll_timeout_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_SUCCESS);
    mesh_access_message_processed(pdu);
}

static void config_node_identity_status(mesh_model_t *mesh_model, uint16_t netkey_index_dest, uint16_t dest, uint8_t status, uint16_t netkey_index, 
    mesh_node_identity_state_t node_identity_state){
    UNUSED(mesh_model);

    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_foundation_node_identity_status,
        status,
        netkey_index,
        node_identity_state);
    if (!transport_pdu) return;
    
    // send as segmented access pdu
    config_server_send_message(netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_node_identity_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);

    mesh_node_identity_state_t node_identity_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED;
    uint8_t status = mesh_proxy_get_advertising_with_node_id_status(netkey_index, &node_identity_state);
       
    config_node_identity_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, node_identity_state);
    mesh_access_message_processed(pdu);
}

static void config_node_identity_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_node_identity_state_t node_identity_state = (mesh_node_identity_state_t) mesh_access_parser_get_u8(&parser);

    uint8_t status = mesh_proxy_set_advertising_with_node_id(netkey_index, node_identity_state);

    config_node_identity_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, node_identity_state);
    mesh_access_message_processed(pdu);
}

//

const static mesh_operation_t mesh_configuration_server_model_operations[] = {
    { MESH_FOUNDATION_OPERATION_APPKEY_ADD,                                  19, config_appkey_add_handler },
    { MESH_FOUNDATION_OPERATION_APPKEY_DELETE,                                3, config_appkey_delete_handler },
    { MESH_FOUNDATION_OPERATION_APPKEY_GET,                                   2, config_appkey_get_handler },
    { MESH_FOUNDATION_OPERATION_APPKEY_UPDATE,                               19, config_appkey_update_handler },
    { MESH_FOUNDATION_OPERATION_NETKEY_ADD,                                  18, config_netkey_add_handler },
    { MESH_FOUNDATION_OPERATION_NETKEY_UPDATE,                               18, config_netkey_update_handler },
    { MESH_FOUNDATION_OPERATION_NETKEY_DELETE,                                2, config_netkey_delete_handler },
    { MESH_FOUNDATION_OPERATION_NETKEY_GET,                                   0, config_netkey_get_handler },
    { MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_GET,                         1, config_composition_data_get_handler },
    { MESH_FOUNDATION_OPERATION_BEACON_GET,                                   0, config_beacon_get_handler },
    { MESH_FOUNDATION_OPERATION_BEACON_SET,                                   1, config_beacon_set_handler },
    { MESH_FOUNDATION_OPERATION_DEFAULT_TTL_GET,                              0, config_default_ttl_get_handler },
    { MESH_FOUNDATION_OPERATION_DEFAULT_TTL_SET,                              1, config_default_ttl_set_handler },
    { MESH_FOUNDATION_OPERATION_FRIEND_GET,                                   0, config_friend_get_handler },
    { MESH_FOUNDATION_OPERATION_FRIEND_SET,                                   1, config_friend_set_handler },
    { MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_GET,                         0, config_model_network_transmit_get_handler },
    { MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_SET,                         1, config_model_network_transmit_set_handler },
    { MESH_FOUNDATION_OPERATION_GATT_PROXY_GET,                               0, config_gatt_proxy_get_handler },
    { MESH_FOUNDATION_OPERATION_GATT_PROXY_SET,                               1, config_gatt_proxy_set_handler },
    { MESH_FOUNDATION_OPERATION_RELAY_GET,                                    0, config_relay_get_handler },
    { MESH_FOUNDATION_OPERATION_RELAY_SET,                                    1, config_relay_set_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_ADD,                       6, config_model_subscription_add_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_ADD,      20, config_model_subscription_virtual_address_add_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE,                    6, config_model_subscription_delete_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE,   20, config_model_subscription_virtual_address_delete_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE,                 6, config_model_subscription_overwrite_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE,20, config_model_subscription_virtual_address_overwrite_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL,                4, config_model_subscription_delete_all_handler },
    { MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET,                   4, config_model_subscription_get_handler },
    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET,                6, config_model_subscription_get_handler },
    { MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_GET,                            4, config_model_app_get_handler },
    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_GET,                         6, config_model_app_get_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET,                       11, config_model_publication_set_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET,       25, config_model_publication_virtual_address_set_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET,                        4, config_model_publication_get_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_APP_BIND,                               6, config_model_app_bind_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_APP_UNBIND,                             6, config_model_app_unbind_handler },
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_GET,                    0, config_heartbeat_publication_get_handler },
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_SET,                    9, config_heartbeat_publication_set_handler },
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_GET,                   0, config_heartbeat_subscription_get_handler},
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_SET,                   5, config_heartbeat_subscription_set_handler},
    { MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_GET,                        2, config_key_refresh_phase_get_handler },
    { MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_SET,                        3, config_key_refresh_phase_set_handler },
    { MESH_FOUNDATION_OPERATION_NODE_RESET,                                   0, config_node_reset_handler },
    { MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_GET,              2, config_low_power_node_poll_timeout_get_handler },
    { MESH_FOUNDATION_OPERATION_NODE_IDENTITY_GET,                            2, config_node_identity_get_handler },
    { MESH_FOUNDATION_OPERATION_NODE_IDENTITY_SET,                            3, config_node_identity_set_handler },
    { 0, 0, NULL }
};

const mesh_operation_t * mesh_configuration_server_get_operations(void){
    return mesh_configuration_server_model_operations;
}

static uint32_t mesh_tag_for_prov_data(void){
    return ((uint32_t) 'P' << 24) | ((uint32_t) 'R' << 16) | ((uint32_t) 'O' <<  8) | (uint32_t)'V';
}

void mesh_node_reset(void){
    mesh_configuration_server_setup_tlv();

    // PROV
    btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data());
    // everything else
    mesh_delete_network_keys();
    mesh_delete_app_keys();
    mesh_delete_appkey_lists();
    mesh_delete_virtual_addresses();
    mesh_delete_subscriptions();
    mesh_delete_publications();
}

typedef struct {
    uint16_t unicast_address;
    uint8_t  flags;
    uint8_t  device_key[16];

} mesh_persistent_provisioning_data_t;

void mesh_node_store_provisioning_data(mesh_provisioning_data_t * provisioning_data){

    // fill persistent prov data
    mesh_persistent_provisioning_data_t persistent_provisioning_data;

    persistent_provisioning_data.unicast_address = provisioning_data->unicast_address;
    memcpy(persistent_provisioning_data.device_key, provisioning_data->device_key, 16);

    // store in tlv
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data(), (uint8_t *) &persistent_provisioning_data, sizeof(mesh_persistent_provisioning_data_t));

    // store IV Index and sequence number
    mesh_store_iv_index_after_provisioning(provisioning_data->iv_index);

    // store primary network key
    mesh_store_network_key(provisioning_data->network_key);
}

int mesh_node_startup_from_tlv(void){

    mesh_persistent_provisioning_data_t persistent_provisioning_data;
    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);

    // load provisioning data
    uint32_t prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, mesh_tag_for_prov_data(), (uint8_t *) &persistent_provisioning_data, sizeof(mesh_persistent_provisioning_data_t));
    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
    if (prov_len){

        // copy into mesh_provisioning_data
        mesh_provisioning_data_t provisioning_data;
        memcpy(provisioning_data.device_key, persistent_provisioning_data.device_key, 16);
        provisioning_data.unicast_address = persistent_provisioning_data.unicast_address;
        provisioning_data.flags = persistent_provisioning_data.flags;
        provisioning_data.network_key = NULL;
        
        // load iv index
        mesh_restore_iv_index_and_sequence_number();
        // load network keys
        mesh_load_network_keys();
        // load app keys
        mesh_load_app_keys();
        // load model to appkey bindings
        mesh_load_appkey_lists();
        // load virtual addresses
        mesh_load_virtual_addresses();
        // load model subscriptions
        mesh_load_subscriptions();
        // load model publications
        mesh_load_publications();
        // load foundation state
        mesh_foundation_state_load();

        mesh_access_setup_from_provisioning_data(&provisioning_data);

#if defined(ENABLE_MESH_ADV_BEARER) || defined(ENABLE_MESH_PB_ADV)
        // start sending Secure Network Beacon
        mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(0);
        if (subnet){
            beacon_secure_network_start(subnet);
        }
#endif
        return 1;
    } else {
        mesh_access_setup_without_provisiong_data();
        return 0;
    }
}


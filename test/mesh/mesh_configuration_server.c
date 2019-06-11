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
#include "mesh_configuration_server.h"
#include "btstack_util.h"
#include "ble/mesh/mesh_network.h"
#include "mesh_keys.h"
#include "mesh_transport.h"
#include "mesh_access.h"
#include "mesh_foundation.h"
#include "bluetooth_company_id.h"
#include "btstack_memory.h"
#include "ble/mesh/mesh_crypto.h"
#include "mesh_virtual_addresses.h"
#include "btstack_debug.h"

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

typedef enum {
    MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED = 0,
    MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING,
    MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED
} mesh_node_identity_state_t;

typedef struct  {
    btstack_timer_source_t timer;
    uint16_t destination;
    uint16_t count_log;
    uint8_t  period_log;
    uint8_t  ttl;
    uint16_t features;
    uint16_t netkey_index;
} mesh_heartbeat_publication_t;

typedef struct  {
    btstack_timer_source_t timer;
    uint16_t source;
    uint16_t destination;
    uint8_t  period_log;
    uint8_t  count_log;
    uint8_t  min_hops;
    uint8_t  max_hops;
} mesh_heartbeat_subscription_t;

typedef struct {
    mesh_heartbeat_publication_t heartbeat_publication;
} mesh_configuration_server_model_context;


static btstack_crypto_aes128_cmac_t configuration_server_cmac_request;

static mesh_pdu_t * access_pdu_in_process;

static btstack_crypto_aes128_cmac_t mesh_cmac_request;

static uint16_t                 model_subscription_hash;
static uint8_t                  model_subscription_label_uuid[16];
static uint16_t                 model_subscription_element_address;


static mesh_heartbeat_publication_t  mesh_heartbeat_publication;
static mesh_heartbeat_subscription_t mesh_heartbeat_subscription;

static int mesh_model_is_configuration_server(uint32_t model_identifier){
    return mesh_model_is_bluetooth_sig(model_identifier) && (mesh_model_get_model_id(model_identifier) == MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
}

static void config_server_send_message(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest,
                                                 mesh_pdu_t *pdu){
    if (mesh_model == NULL){
        log_error("mesh_model == NULL"); 
    }
    if (mesh_model->element == NULL){
        log_error("mesh_model->element == NULL"); 
    }
    uint16_t src  = mesh_model->element->unicast_address;
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint8_t  ttl  = mesh_foundation_default_ttl_get();
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, 0);
    mesh_upper_transport_send_access_pdu(pdu);
}


static void config_composition_data_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){

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

    mesh_element_iterator_t it;
    mesh_element_iterator_init(&it);
    while (mesh_element_iterator_has_next(&it)){
        mesh_element_t * element = mesh_element_iterator_next(&it);

        // Loc
        mesh_access_transport_add_uint16(transport_pdu, element->loc);
        // NumS
        mesh_access_transport_add_uint8( transport_pdu, element->models_count_sig);
        // NumV
        mesh_access_transport_add_uint8( transport_pdu, element->models_count_vendor);

        mesh_model_iterator_t it;
        
        // SIG Models
        mesh_model_iterator_init(&it, element);
        while (mesh_model_iterator_has_next(&it)){
            mesh_model_t * model = mesh_model_iterator_next(&it);
            if (!mesh_model_is_bluetooth_sig(model->model_identifier)) continue;
            mesh_access_transport_add_uint16(transport_pdu, model->model_identifier);
        }
        // Vendor Models
        mesh_model_iterator_init(&it, element);
        while (mesh_model_iterator_has_next(&it)){
            mesh_model_t * model = mesh_model_iterator_next(&it);
            if (mesh_model_is_bluetooth_sig(model->model_identifier)) continue;
            mesh_access_transport_add_uint32(transport_pdu, model->model_identifier);
        }
    }
    
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_composition_data_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    config_composition_data_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_beacon_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_beacon_status,
                                                                               mesh_foundation_becaon_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    //
    config_model_beacon_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_default_ttl_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_default_ttl_status, mesh_foundation_default_ttl_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    //
    config_model_default_ttl_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_friend_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_friend_status, mesh_foundation_friend_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    }

    //
    config_friend_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_gatt_proxy_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_gatt_proxy_status, mesh_foundation_gatt_proxy_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    //
    config_model_gatt_proxy_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_relay_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_relay_status,
                                                                               mesh_foundation_relay_get(),
                                                                               mesh_foundation_relay_retransmit_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    }

    //
    config_model_relay_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

static void config_model_network_transmit_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_network_transmit_status, mesh_foundation_network_transmit_get());
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    //
    config_model_network_transmit_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);
}

// NetKey List

static int config_netkey_list_max = 0;

void config_nekey_list_set_max(uint16_t max){
    config_netkey_list_max = max;
}

static void config_netkey_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t new_netkey_index){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_netkey_status, status, new_netkey_index);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_netkey_list(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest) {
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
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_netkey_add_or_update_derived(void * arg){

#ifdef ENABLE_MESH_PROXY_SERVER
    mesh_proxy_stop_advertising_with_network_id();
#endif

    mesh_network_key_t * network_key = (mesh_network_key_t *) arg;

#ifdef ENABLE_MESH_PROXY_SERVER
    // setup advertisement with network id
    network_key->advertisement_with_network_id.adv_length = gatt_bearer_setup_advertising_with_network_id(network_key->advertisement_with_network_id.adv_data, network_key->network_id);
#endif

    mesh_network_key_add(network_key);

#ifdef ENABLE_MESH_PROXY_SERVER
    mesh_proxy_start_advertising_with_network_id();
#endif
    
    config_netkey_status(NULL, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), MESH_FOUNDATION_STATUS_SUCCESS, network_key->netkey_index);
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

    const mesh_network_key_t * existing_network_key = mesh_network_key_list_get(new_netkey_index);
    if (existing_network_key){
        // network key for netkey index already exists
        if (memcmp(existing_network_key->net_key, new_netkey, 16) == 0){
            // same netkey
            status = MESH_FOUNDATION_STATUS_SUCCESS;
        } else {
            // different netkey
            status = MESH_FOUNDATION_STATUS_KEY_INDEX_ALREADY_STORED;
        }
    } else {
        // check limit for pts
        if (config_netkey_list_max && mesh_network_key_list_count() >= config_netkey_list_max){
            status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
        } else {
            // setup new key
            mesh_network_key_t * new_network_key = btstack_memory_mesh_network_key_get();
            if (new_network_key == NULL){
                status = MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
            } else {
                access_pdu_in_process = pdu;
                new_network_key->netkey_index = new_netkey_index;
                memcpy(new_network_key->net_key, new_netkey, 16);
                mesh_network_key_derive(&configuration_server_cmac_request, new_network_key, config_netkey_add_or_update_derived, new_network_key);
                return;
            }
        }
    }
    config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, new_netkey_index);
    mesh_access_message_processed(access_pdu_in_process);
}

static void config_netkey_get_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu){
    config_netkey_list(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static void config_netkey_update_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);

    // get params
    uint8_t new_netkey[16];
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_access_parser_get_key(&parser, new_netkey);

    // get existing network_key
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX, netkey_index);
        mesh_access_message_processed(access_pdu_in_process);
        return;
    }

    // setup new key
    mesh_network_key_t * new_network_key = btstack_memory_mesh_network_key_get();
    if (new_network_key == NULL){
        config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES, netkey_index);
        mesh_access_message_processed(access_pdu_in_process);
        return;
    }

    access_pdu_in_process = pdu;
    new_network_key->netkey_index = netkey_index;
    new_network_key->key_refresh  = 1;
    memcpy(new_network_key->net_key, new_netkey, 16);
    mesh_network_key_derive(&configuration_server_cmac_request, new_network_key, config_netkey_add_or_update_derived, new_network_key);
}

static void config_netkey_delete_handler(mesh_model_t * mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t *) pdu);

    // get params
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);

    // get existing network_key
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key){
        if (mesh_network_key_list_count() > 1){
            // remove netkey
            mesh_network_key_remove(network_key);
            // remove all appkeys for this netkey
            mesh_transport_key_iterator_t it;
            mesh_transport_key_iterator_init(&it, netkey_index);
            while (mesh_transport_key_iterator_has_more(&it)){
                mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
                uint16_t appkey_index = transport_key->appkey_index;
                mesh_transport_key_remove(transport_key);
                mesh_delete_app_key(appkey_index);
            }
        } else {
            // we cannot remove the last network key
            status = MESH_FOUNDATION_STATUS_CANNOT_REMOVE;
        }
    }
    config_netkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index);
}

static void config_appkey_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint32_t netkey_and_appkey_index, uint8_t status){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_appkey_status,
                                                                               status, netkey_and_appkey_index);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_appkey_list(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint32_t netkey_index_of_list){
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
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_appkey_add_or_udpate_aid(void *arg){
    mesh_transport_key_t * transport_key = (mesh_transport_key_t *) arg;

    printf("Config Appkey Add/Update: NetKey Index 0x%04x, AppKey Index 0x%04x, AID %02x: ", transport_key->netkey_index, transport_key->appkey_index, transport_key->aid);
    printf_hexdump(transport_key->key, 16);

    // store in TLV
    mesh_store_app_key(transport_key->internal_index, transport_key->netkey_index, transport_key->appkey_index, transport_key->aid, transport_key->key);

    // add app key
    mesh_transport_key_add(transport_key);

    uint32_t netkey_and_appkey_index = (transport_key->appkey_index << 12) | transport_key->netkey_index;
    config_appkey_status(NULL,  mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);

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
    memcpy(app_key->key, appkey, 16);

    // calculate AID
    access_pdu_in_process = pdu;
    mesh_transport_key_calc_aid(&mesh_cmac_request, app_key, config_appkey_add_or_udpate_aid, app_key);
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

    // check netkey_index is valid
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    // check if appkey already exists
    mesh_transport_key_t * transport_key = mesh_transport_key_get(appkey_index);
    if (!transport_key) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX);
        mesh_access_message_processed(pdu);
        return;
    }

    if (transport_key->netkey_index != netkey_index){
        // already stored but with different netkey
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INVALID_BINDING);
        mesh_access_message_processed(pdu);
        return;
    }

    if (memcmp(transport_key->key, appkey, 16) == 0){
        // key identical
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);
        mesh_access_message_processed(pdu);
        return;
    }

    // TODO: Don't create new key. We create a new app_key becaue key refresh is not implemented yet, by adding a new one, both are available

    // create app key
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
    app_key->appkey_index = appkey_index;
    app_key->netkey_index = netkey_index;
    app_key->key_refresh  = 1;
    memcpy(app_key->key, appkey, 16);

    // calculate AID
    access_pdu_in_process = pdu;
    mesh_transport_key_calc_aid(&mesh_cmac_request, app_key, config_appkey_add_or_udpate_aid, app_key);
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
        mesh_delete_app_key(transport_key->internal_index);
        mesh_transport_key_remove(transport_key);
        btstack_memory_mesh_transport_key_free(transport_key);
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

// Configuration Model Subscriptions (Virtual Address Helper)

static void mesh_virtual_address_decrease_refcount(mesh_virtual_address_t * virtual_address){
    virtual_address->ref_count--;
    // Free virtual address if ref count reaches zero
    if (virtual_address->ref_count > 0) return;
    mesh_virtual_address_remove(virtual_address);
    btstack_memory_mesh_virtual_address_free(virtual_address);
    // TODO: delete from TLV
}

static void mesh_virtual_address_increase_refcount(mesh_virtual_address_t * virtual_address){
    virtual_address->ref_count++;
    if (virtual_address->ref_count > 1) return;
    // TODO: store virtual address in TLV
}

// Configuration Model Subscriptions (helper)

static uint8_t mesh_model_add_subscription(mesh_model_t * mesh_model, uint16_t address){
    int i;
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
    int i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        if (mesh_model->subscriptions[i] == address) {
            mesh_model->subscriptions[i] = MESH_ADDRESS_UNSASSIGNED;
        }
    }
}

static void mesh_model_delete_all_subscriptions(mesh_model_t * mesh_model){
    int i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        mesh_model->subscriptions[i] = MESH_ADDRESS_UNSASSIGNED;
    }
}

static void mesh_subcription_decrease_virtual_address_ref_count(mesh_model_t *mesh_model){
    // decrease ref counts for current virtual subscriptions
    uint16_t i;
    for (i = 0; i <= MAX_NR_MESH_SUBSCRIPTION_PER_MODEL ; i++){
        uint16_t src = mesh_model->subscriptions[i];
        if (mesh_network_address_virtual(src)){
            mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_pseudo_dst(src);
            if (virtual_address == NULL) continue;
            mesh_virtual_address_decrease_refcount(virtual_address);
        }
    }
}

// Configuration Model Subscriptions (messages)

static void config_model_subscription_list_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint32_t model_identifier, mesh_model_t * target_model){
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
        int i;
        for (i = 0; i < MAX_NR_MESH_SUBSCRIPTION_PER_MODEL; i++){
            if (target_model->subscriptions[i] == MESH_ADDRESS_UNSASSIGNED) continue;
            if (mesh_network_address_virtual(target_model->subscriptions[i])) continue;
            mesh_access_transport_add_uint16(transport_pdu, target_model->subscriptions[i]);
        }
    }   
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_model_subscription_status,
                                                                                status, element_address, address, model_identifier);
    if (!transport_pdu) return;
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(model_subscription_label_uuid);
    if (virtual_address == NULL){
        virtual_address = mesh_virtual_address_register(model_subscription_label_uuid, model_subscription_hash);
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
            }
        }
    }

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, model_subscription_element_address, pseudo_dst, target_model->model_identifier);
    mesh_access_message_processed(access_pdu_in_process);
    return;
}

static void config_model_subscription_virtual_address_add_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    model_subscription_element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, model_subscription_label_uuid);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(model_subscription_element_address, model_identifier, &status);

    if (target_model == NULL){
        config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, model_subscription_element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
        mesh_access_message_processed(pdu);
        return;
    }
    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, model_subscription_label_uuid, &model_subscription_hash, &config_model_subscription_virtual_address_add_hash, target_model);
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
            status = mesh_model_add_subscription(mesh_model, address);
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
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(model_subscription_label_uuid);
    if (virtual_address == NULL){
        virtual_address = mesh_virtual_address_register(model_subscription_label_uuid, model_subscription_hash);
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
    }

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, model_subscription_element_address, pseudo_dst, target_model->model_identifier);
    mesh_access_message_processed(access_pdu_in_process);
    return;
}

static void config_model_subscription_virtual_address_overwrite_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    model_subscription_element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, model_subscription_label_uuid);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(model_subscription_element_address, model_identifier, &status);

    if (target_model == NULL){
        config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, model_subscription_element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
        mesh_access_message_processed(pdu);
        return;
    }
    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, model_subscription_label_uuid, &model_subscription_hash, &config_model_subscription_virtual_address_overwrite_hash, target_model);
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
    mesh_access_parser_get_label_uuid(&parser, model_subscription_label_uuid);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    mesh_virtual_address_t * virtual_address = mesh_virtual_address_for_label_uuid(model_subscription_label_uuid);
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    if ((target_model != NULL) && (virtual_address != NULL) && mesh_model_contains_subscription(target_model, virtual_address->pseudo_dst)){
        mesh_virtual_address_decrease_refcount(virtual_address);
        mesh_model_delete_subscription(target_model, virtual_address->pseudo_dst);
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
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, MESH_ADDRESS_UNSASSIGNED, model_identifier);
    mesh_access_message_processed(pdu);
}

// Configuration Model to AppKey List

static void config_model_app_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint16_t appkey_index, uint32_t model_identifier){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_model_app_status,
                                                            status, element_address, appkey_index, model_identifier);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_model_app_list(mesh_model_t * config_server_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint32_t model_identifier, mesh_model_t * mesh_model){
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
        int i;
        for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
            uint16_t appkey_index = mesh_model->appkey_indices[i];
            if (appkey_index == MESH_APPKEY_INVALID) continue;
            mesh_access_transport_add_uint16(transport_pdu, appkey_index);
        }
    }

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
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

static void config_model_app_get(mesh_model_t *config_server_model, mesh_pdu_t * pdu, int sig_model){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    uint16_t element_address = mesh_access_parser_get_u16(&parser);
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);
    config_model_app_list(config_server_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, model_identifier, target_model);
    mesh_access_message_processed(pdu);
}

static void config_sig_model_app_get_handler(mesh_model_t *config_server_model, mesh_pdu_t * pdu) {
    config_model_app_get(config_server_model, pdu, 1);
}

static void config_vendor_model_app_get_handler(mesh_model_t *config_server_model, mesh_pdu_t * pdu) {
    config_model_app_get(config_server_model, pdu, 0);
}

static void
config_model_publication_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status,
                                    uint32_t model_id, mesh_publication_model_t *publication_model) {
    // setup message
    uint16_t app_key_index_and_credential_flag = (publication_model->friendship_credential_flag << 12) | publication_model->appkey_index;
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_model_app_status, status, mesh_access_get_primary_element_address(), publication_model->address,
            app_key_index_and_credential_flag,
            publication_model->ttl, publication_model->period, publication_model->retransmit, model_id);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

// TODO: avoid temp storage
static mesh_publication_model_t publication_model;
static mesh_model_t           * config_model_publication_model;
static uint32_t                 config_model_publication_model_identifier;
static uint8_t                  model_publication_label_uuid[16];

static void
config_model_publication_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    uint16_t element_address = mesh_access_parser_get_u16(&parser);

    // PublishAddress, 16 bit
    publication_model.address = mesh_access_parser_get_u16(&parser);

    // AppKeyIndex (12), CredentialFlag (1), RFU (3)
    uint16_t temp = mesh_access_parser_get_u16(&parser);
    publication_model.appkey_index = temp & 0x0fff;
    publication_model.friendship_credential_flag = (temp >> 12) & 1;

    // TTL
    publication_model.ttl        = mesh_access_parser_get_u8(&parser);

    // Period
    publication_model.period     = mesh_access_parser_get_u8(&parser);

    // Retransmit
    publication_model.retransmit = mesh_access_parser_get_u8(&parser);

    // Model Identifier
    uint32_t model_identifier = mesh_access_parser_get_model_identifier(&parser);
    uint8_t status;
    mesh_model_t * target_model = mesh_access_model_for_address_and_model_identifier(element_address, model_identifier, &status);

    // TODO validate params

    if (target_model){
        if (target_model->publication_model == NULL){
            status = MESH_FOUNDATION_STATUS_CANNOT_SET;
        } else {
            if (publication_model.address == MESH_ADDRESS_UNSASSIGNED){
                // unpublish
                memset(&publication_model, 0, sizeof(publication_model));
            }
            memcpy(target_model->publication_model, &publication_model, sizeof(mesh_publication_model_t));
        }
    }

    // TODO: validate params

    // send status
    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, model_identifier, &publication_model);
    mesh_access_message_processed(pdu);
}

static void config_model_publication_virtual_address_set_hash(void *arg){
    mesh_model_t *mesh_model = (mesh_model_t*) arg;
    printf("Virtual Address Hash: %04x\n", publication_model.address);

    // TODO: find a way to get netkey_index
    uint16_t netkey_index = 0;

    // update
    if (publication_model.address == MESH_ADDRESS_UNSASSIGNED){
        // unpublish
        memset(&publication_model, 0, sizeof(publication_model));
    }
    memcpy(config_model_publication_model->publication_model, &publication_model, sizeof(mesh_publication_model_t));


    // send status
    uint8_t status = 0;
    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), status, config_model_publication_model_identifier, &publication_model);

    mesh_access_message_processed(access_pdu_in_process);
}

static void 
config_model_publication_virtual_address_set_handler(mesh_model_t *mesh_model,
                                                     mesh_pdu_t * pdu) {

    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // ElementAddress - Address of the element - should be us
    uint16_t element_address = mesh_access_parser_get_u16(&parser);

    // store label uuid
    mesh_access_parser_get_label_uuid(&parser, model_publication_label_uuid);

    // AppKeyIndex (12), CredentialFlag (1), RFU (3)
    uint16_t temp = mesh_access_parser_get_u16(&parser);
    publication_model.appkey_index = temp & 0x0fff;
    publication_model.friendship_credential_flag = (temp >> 12) & 1;
    publication_model.ttl        = mesh_access_parser_get_u8(&parser);
    publication_model.period     = mesh_access_parser_get_u8(&parser);
    publication_model.retransmit = mesh_access_parser_get_u8(&parser);

    // Model Identifier
    config_model_publication_model_identifier = mesh_access_parser_get_model_identifier(&parser);

    uint8_t status;
    config_model_publication_model = mesh_access_model_for_address_and_model_identifier(element_address, config_model_publication_model_identifier, &status);

    // on error, no need to calculate virtual address hash
    if (status != MESH_FOUNDATION_STATUS_SUCCESS){
        config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, config_model_publication_model_identifier, &publication_model);
        mesh_access_message_processed(pdu);
        return;
    }

    // model exists, but no publication model
    if (config_model_publication_model->publication_model == NULL){
        config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), MESH_FOUNDATION_STATUS_CANNOT_SET, config_model_publication_model_identifier, &publication_model);
        mesh_access_message_processed(pdu);
    }

    access_pdu_in_process = pdu;
    mesh_virtual_address(&configuration_server_cmac_request, model_publication_label_uuid, &publication_model.address, &config_model_publication_virtual_address_set_hash, mesh_model);
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

    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, model_identifier, target_model->publication_model);
    mesh_access_message_processed(pdu);
}


// Heartbeat Publication
#define MESH_HEARTBEAT_FEATURES_SUPPORTED_MASK 0x000f

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

static void config_heartbeat_publication_emit(btstack_timer_source_t * ts){
    if (mesh_heartbeat_publication.count_log == 0) return;

    uint32_t time_ms = heartbeat_pwr2(mesh_heartbeat_publication.period_log) * 1000;
    printf("CONFIG_SERVER_HEARTBEAT: Emit (dest %04x, count %u, period %u ms, seq %x)\n", mesh_heartbeat_publication.destination, mesh_heartbeat_publication.count_log, time_ms, mesh_lower_transport_peek_seq());
    mesh_heartbeat_publication.count_log--;

    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (network_pdu){
        uint8_t data[3];
        data[0] = mesh_heartbeat_publication.ttl;
        big_endian_store_16(data, 1, mesh_heartbeat_publication.features);
        mesh_upper_transport_setup_control_pdu((mesh_pdu_t *) network_pdu, mesh_heartbeat_publication.netkey_index,
                mesh_heartbeat_publication.ttl, mesh_access_get_primary_element_address(), mesh_heartbeat_publication.destination,
                MESH_TRANSPORT_OPCODE_HEARTBEAT, data, sizeof(data));
        mesh_upper_transport_send_control_pdu((mesh_pdu_t *) network_pdu);
    }
    btstack_run_loop_set_timer(ts, time_ms);
    btstack_run_loop_add_timer(ts);
}

static void config_heartbeat_publication_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest){

    // setup message
    uint8_t status = 0;
    uint8_t count_log = heartbeat_count_log(mesh_heartbeat_publication.count_log);
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_heartbeat_publication_status,
            status,
            mesh_heartbeat_publication.destination,
            count_log,
            mesh_heartbeat_publication.period_log,
            mesh_heartbeat_publication.ttl,
            mesh_heartbeat_publication.features,
            mesh_heartbeat_publication.netkey_index);
    if (!transport_pdu) return;
    printf("MESH config_heartbeat_publication_status count = %u => count_log = %u\n", mesh_heartbeat_publication.count_log, count_log);

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_heartbeat_publication_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // TODO: validate fields

    // Destination address for Heartbeat messages
    mesh_heartbeat_publication.destination = mesh_access_parser_get_u16(&parser);
    // Number of Heartbeat messages to be sent
    mesh_heartbeat_publication.count_log = heartbeat_pwr2(mesh_access_parser_get_u8(&parser));
    //  Period for sending Heartbeat messages
    mesh_heartbeat_publication.period_log = mesh_access_parser_get_u8(&parser);
    //  TTL to be used when sending Heartbeat messages
    mesh_heartbeat_publication.ttl = mesh_access_parser_get_u8(&parser);
    // Bit field indicating features that trigger Heartbeat messages when changed
    mesh_heartbeat_publication.features = mesh_access_parser_get_u16(&parser) & MESH_HEARTBEAT_FEATURES_SUPPORTED_MASK;
    // NetKey Index
    mesh_heartbeat_publication.netkey_index = mesh_access_parser_get_u16(&parser);

    printf("MESH config_heartbeat_publication_set, destination %x, count = %x, period = %u s\n",
            mesh_heartbeat_publication.destination, mesh_heartbeat_publication.count_log, heartbeat_pwr2(mesh_heartbeat_publication.period_log));

    config_heartbeat_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));

    mesh_access_message_processed(pdu);

    // check if we should enable heartbeats
    if (mesh_heartbeat_publication.destination == MESH_ADDRESS_UNSASSIGNED) {
        btstack_run_loop_remove_timer(&mesh_heartbeat_publication.timer);
        printf("MESH config_heartbeat_publication_set, disable\n");
        return;
    }

    // NOTE: defer first heartbeat to allow config status getting sent first
    // TODO: check if heartbeat was off before
    btstack_run_loop_set_timer_handler(&mesh_heartbeat_publication.timer, config_heartbeat_publication_emit);
    btstack_run_loop_set_timer(&mesh_heartbeat_publication.timer, 2000);
    btstack_run_loop_add_timer(&mesh_heartbeat_publication.timer);
}

static void config_heartbeat_publication_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    config_heartbeat_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static void config_heartbeat_subscription_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    uint8_t count_log = heartbeat_count_log(mesh_heartbeat_subscription.count_log);
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
            &mesh_foundation_config_heartbeat_subscription_status,
            status,
            mesh_heartbeat_subscription.source,
            mesh_heartbeat_subscription.destination,
            mesh_heartbeat_subscription.period_log,
            count_log,
            mesh_heartbeat_subscription.min_hops,
            mesh_heartbeat_subscription.max_hops);
    if (!transport_pdu) return;
    printf("MESH config_heartbeat_subscription_status count = %u => count_log = %u\n", mesh_heartbeat_subscription.count_log, count_log);

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}
static int config_heartbeat_subscription_enabled(void){
    return mesh_network_address_unicast(mesh_heartbeat_subscription.source) && mesh_heartbeat_subscription.period_log > 0 &&
        (mesh_network_address_unicast(mesh_heartbeat_subscription.destination) || mesh_network_address_group(mesh_heartbeat_subscription.destination));
}

static void config_heartbeat_subscription_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);

    // Destination address for Heartbeat messages
    mesh_heartbeat_subscription.source = mesh_access_parser_get_u16(&parser);
    // Destination address for Heartbeat messages
    mesh_heartbeat_subscription.destination = mesh_access_parser_get_u16(&parser);
    //  Period for sending Heartbeat messages
    mesh_heartbeat_subscription.period_log = mesh_access_parser_get_u8(&parser);
    
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    if (mesh_heartbeat_subscription.period_log > 0x11u){
        status = MESH_FOUNDATION_STATUS_CANNOT_SET;
    } else if ((mesh_heartbeat_subscription.source != MESH_ADDRESS_UNSASSIGNED)       && 
               !mesh_network_address_unicast(mesh_heartbeat_subscription.source)){
        status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
    } else if ((mesh_heartbeat_subscription.destination != MESH_ADDRESS_UNSASSIGNED)  && 
               !mesh_network_address_unicast(mesh_heartbeat_subscription.destination) &&
               !mesh_network_address_group(mesh_heartbeat_subscription.destination)){
        status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
    } 

    if (status != MESH_FOUNDATION_STATUS_SUCCESS){
        config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
        mesh_access_message_processed(pdu);
        return;
    }

    if (config_heartbeat_subscription_enabled()){
        mesh_heartbeat_subscription.count_log  = 0u;
        mesh_heartbeat_subscription.min_hops   = 0x7Fu;
        mesh_heartbeat_subscription.max_hops   = 0u;
    } else {
        mesh_heartbeat_subscription.source = MESH_ADDRESS_UNSASSIGNED;
        mesh_heartbeat_subscription.destination = MESH_ADDRESS_UNSASSIGNED;
        mesh_heartbeat_subscription.period_log = 0u;
        // TODO: check if we need to reset these three params
        mesh_heartbeat_subscription.count_log  = 0u;
        mesh_heartbeat_subscription.min_hops   = 0u;
        mesh_heartbeat_subscription.max_hops   = 0u;
    }
    
    printf("MESH config_heartbeat_subscription_set, destination %x, count = %x, period = %u s\n",
            mesh_heartbeat_subscription.destination, mesh_heartbeat_subscription.count_log, heartbeat_pwr2(mesh_heartbeat_subscription.period_log));

    config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);

    // TODO: implement subcription behaviour
}


static void config_heartbeat_subscription_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    config_heartbeat_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static void config_key_refresh_phase_status(mesh_model_t *mesh_model, uint16_t netkey_index_dest, uint16_t dest, uint8_t status, uint16_t netkey_index,
    mesh_key_refresh_state_t key_refresh_state){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_key_refresh_phase_status,
        status,
        netkey_index_dest,
        key_refresh_state);
    if (!transport_pdu) return;
    
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_key_refresh_phase_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);

    uint8_t status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    mesh_key_refresh_state_t key_refresh_state = MESH_KEY_REFRESH_NOT_ACTIVE;
    
    if (network_key != NULL){
        status = MESH_FOUNDATION_STATUS_SUCCESS;
        key_refresh_state = network_key->key_refresh;
    }

    config_key_refresh_phase_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, key_refresh_state);
    mesh_access_message_processed(pdu);
}

static void config_key_refresh_phase_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    uint8_t  key_refresh_phase_transition = mesh_access_parser_get_u8(&parser);
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);

    uint8_t status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    
    if (network_key != NULL){
        status = MESH_FOUNDATION_STATUS_SUCCESS;
        
        switch (key_refresh_phase_transition){
            case 0x02:
                switch (network_key->key_refresh){
                    case MESH_KEY_REFRESH_FIRST_PHASE:
                    case MESH_KEY_REFRESH_SECOND_PHASE:
                        network_key->key_refresh = MESH_KEY_REFRESH_SECOND_PHASE;
                        break;
                    default:
                        break;
                }
                break;
            case 0x03:
                switch (network_key->key_refresh){
                    case MESH_KEY_REFRESH_FIRST_PHASE:
                    case MESH_KEY_REFRESH_SECOND_PHASE:
                        // TODO:  invoke Key Refresh Phase 3, then
                        //        set network_key->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;
                        printf("TODO: invoke Key Refresh Phase 3, then set key refresh phase to MESH_KEY_REFRESH_NOT_ACTIVE\n");
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

    config_key_refresh_phase_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, network_key->key_refresh);
    mesh_access_message_processed(pdu);
}


static void config_node_reset_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_node_reset_status);
    if (!transport_pdu) return;

    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_node_reset_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_foundation_node_reset();
    config_node_reset_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu));
    mesh_access_message_processed(pdu);
}

static void low_power_node_poll_timeout_status(mesh_model_t *mesh_model, uint16_t netkey_index_dest, uint16_t dest, uint8_t status){
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_foundation_low_power_node_poll_timeout_status,
        status,
        0,  // The unicast address of the Low Power node
        0); // The current value of the PollTimeout timer of the Low Power node
    if (!transport_pdu) return;
    printf("TODO: send unicast address of the Low Power node and the current value of the PollTimeout timer, instead of 0s\n");
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
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
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(
        &mesh_foundation_node_identity_status,
        status,
        netkey_index_dest,
        node_identity_state);
    if (!transport_pdu) return;
    
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index_dest, dest, (mesh_pdu_t *) transport_pdu);
}

static void config_node_identity_get_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
   
    uint8_t status = MESH_FOUNDATION_STATUS_SUCCESS;
    mesh_node_identity_state_t node_identity_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED;
    
    if (network_key == NULL){
        status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    } else {
#ifdef ENABLE_MESH_PROXY_SERVER
        if (network_key->node_id_advertisement_running == 0){
            node_identity_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED;
        } else {
            node_identity_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING;
        }
#endif
    }

    config_node_identity_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, netkey_index, node_identity_state);
    mesh_access_message_processed(pdu);
}

static void config_node_identity_set_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu){
    mesh_access_parser_state_t parser;
    mesh_access_parser_init(&parser, (mesh_pdu_t*) pdu);
    uint16_t netkey_index = mesh_access_parser_get_u16(&parser);
    mesh_node_identity_state_t node_identity_state = (mesh_node_identity_state_t) mesh_access_parser_get_u8(&parser);
    
    uint8_t status = MESH_FOUNDATION_STATUS_FEATURE_NOT_SUPPORTED;
    
    mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (network_key == NULL){
        status = MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    } else {
#ifdef ENABLE_MESH_PROXY_SERVER
        switch (node_identity_state){
            case MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED:
                network_key->node_id_advertisement_running = 0;
                status = MESH_FOUNDATION_STATUS_SUCCESS;
                break;
            case MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING:
                network_key->node_id_advertisement_running = 1;
                status = MESH_FOUNDATION_STATUS_SUCCESS;
                break;
            default:
                break;
        }
#endif
    }

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
    { MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_GET,                            4, config_sig_model_app_get_handler },
    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_GET,                         6, config_vendor_model_app_get_handler },
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

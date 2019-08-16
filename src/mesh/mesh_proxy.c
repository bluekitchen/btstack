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

#include "mesh/mesh_proxy.h"

#include <string.h>

#include "bluetooth_company_id.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_config.h"
#include "btstack_crypto.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"

#include "mesh/adv_bearer.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_crypto.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_node.h"

static void mesh_proxy_start_advertising_with_node_id_next_subnet(void);
static void mesh_proxy_start_advertising_with_node_id_for_subnet(mesh_subnet_t * mesh_subnet);

#ifdef ENABLE_MESH_PROXY_SERVER

// we only support a single active node id advertisement. when new one is started, an active one is stopped

#define MESH_PROXY_NODE_ID_ADVERTISEMENT_TIMEOUT_MS 60000

static btstack_timer_source_t                           mesh_proxy_node_id_timer;
static btstack_crypto_random_t                          mesh_proxy_node_id_crypto_request_random;
static btstack_crypto_aes128_t                          mesh_proxy_node_id_crypto_request_aes128;
static uint8_t                                          mesh_proxy_node_id_plaintext[16];
static uint8_t                                          mesh_proxy_node_id_hash[16];
static uint8_t                                          mesh_proxy_node_id_random_value[8];
static uint8_t                                          mesh_proxy_node_id_all_subnets;

static uint16_t primary_element_address;

// Mesh Proxy, advertise unprovisioned device
static adv_bearer_connectable_advertisement_data_item_t connectable_advertisement_unprovisioned_device;

static const uint8_t adv_data_with_node_id_template[] = {
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

static const uint8_t adv_data_with_network_id_template[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8,
    // Service Data
    0x0C, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8, 
          // MESH_IDENTIFICATION_NETWORK_ID_TYPE
          MESH_IDENTIFICATION_NETWORK_ID_TYPE 
};

#ifdef ENABLE_MESH_PB_GATT

// Mesh Provisioning
static const uint8_t adv_data_unprovisioned_template[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING >> 8,
    // Service Data (22)
    0x15, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING >> 8, 
          // UUID - 16 bytes
          // OOB information - 2 bytes
};
const uint8_t adv_data_unprovisioned_template_len = sizeof(adv_data_unprovisioned_template);

static void mesh_proxy_setup_advertising_unprovisioned(adv_bearer_connectable_advertisement_data_item_t * advertisement_item, const uint8_t * device_uuid) {
    // store in advertisement item
    memset(advertisement_item, 0, sizeof(adv_bearer_connectable_advertisement_data_item_t));
    advertisement_item->adv_length = adv_data_unprovisioned_template_len + 18;
    memcpy(advertisement_item->adv_data, (uint8_t*) adv_data_unprovisioned_template, adv_data_unprovisioned_template_len);
    // dynamically store device uuid into adv data
    memcpy(&advertisement_item->adv_data[11], device_uuid, 16);
    little_endian_store_16(advertisement_item->adv_data, 27, 0);
}

void mesh_proxy_start_advertising_unprovisioned_device(void){
    const uint8_t * device_uuid = mesh_node_get_device_uuid();
    mesh_proxy_setup_advertising_unprovisioned(&connectable_advertisement_unprovisioned_device, device_uuid);
    // setup advertisements
    adv_bearer_advertisements_add_item(&connectable_advertisement_unprovisioned_device);
    adv_bearer_advertisements_enable(1);
}

/**
 * @brief Start Advertising Unprovisioned Device with Device ID
 * @param device_uuid
 */
void mesh_proxy_stop_advertising_unprovisioned_device(void){
    adv_bearer_advertisements_remove_item(&connectable_advertisement_unprovisioned_device);
}

#endif

static uint8_t mesh_proxy_setup_advertising_with_network_id(uint8_t * buffer, uint8_t * network_id){
    memcpy(&buffer[0], adv_data_with_network_id_template, 12);
    memcpy(&buffer[12], network_id, 8);
    return 20;
}

static void mesh_proxy_stop_advertising_with_node_id_for_subnet(mesh_subnet_t * mesh_subnet){
    if (mesh_subnet->node_id_advertisement_running != 0){
        adv_bearer_advertisements_remove_item(&mesh_subnet->advertisement_with_node_id);
        mesh_subnet->node_id_advertisement_running = 0;
    }
}

static void mesh_proxy_stop_all_advertising_with_node_id(void){
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * mesh_subnet = mesh_subnet_iterator_get_next(&it);
        mesh_proxy_stop_advertising_with_node_id_for_subnet(mesh_subnet);
    }
    btstack_run_loop_remove_timer(&mesh_proxy_node_id_timer);
    mesh_proxy_node_id_all_subnets = 0;
}

static void mesh_proxy_node_id_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    mesh_proxy_stop_all_advertising_with_node_id();
}

static void mesh_proxy_node_id_handle_get_aes128(void * arg){
    mesh_subnet_t * mesh_subnet = (mesh_subnet_t *) arg;

    memcpy(mesh_subnet->advertisement_with_node_id.adv_data, adv_data_with_node_id_template, 12);
    memcpy(&mesh_subnet->advertisement_with_node_id.adv_data[12], &mesh_proxy_node_id_hash[8], 8);
    memcpy(&mesh_subnet->advertisement_with_node_id.adv_data[20], mesh_proxy_node_id_random_value, 8);
    mesh_subnet->advertisement_with_node_id.adv_length = 28;
    
    // setup advertisements
    adv_bearer_advertisements_add_item(&mesh_subnet->advertisement_with_node_id);
    adv_bearer_advertisements_enable(1);

    // set timer
    btstack_run_loop_set_timer_handler(&mesh_proxy_node_id_timer, mesh_proxy_node_id_timeout_handler);
    btstack_run_loop_set_timer(&mesh_proxy_node_id_timer, MESH_PROXY_NODE_ID_ADVERTISEMENT_TIMEOUT_MS);
    btstack_run_loop_add_timer(&mesh_proxy_node_id_timer);

    // mark as active
    mesh_subnet->node_id_advertisement_running = 1;

    // next one
    if (mesh_proxy_node_id_all_subnets == 0) return;
    mesh_proxy_start_advertising_with_node_id_next_subnet();
}

static void mesh_proxy_node_id_handle_random(void * arg){
    mesh_subnet_t * mesh_subnet = (mesh_subnet_t *) arg;

    // Hash = e(IdentityKey, Padding | Random | Address) mod 2^64
    memset(mesh_proxy_node_id_plaintext, 0, sizeof(mesh_proxy_node_id_plaintext));
    memcpy(&mesh_proxy_node_id_plaintext[6] , mesh_proxy_node_id_random_value, 8);
    big_endian_store_16(mesh_proxy_node_id_plaintext, 14, primary_element_address);
    // TODO: old vs. new key
    btstack_crypto_aes128_encrypt(&mesh_proxy_node_id_crypto_request_aes128, mesh_subnet->old_key->identity_key, mesh_proxy_node_id_plaintext, mesh_proxy_node_id_hash, mesh_proxy_node_id_handle_get_aes128, mesh_subnet);
}

static void mesh_proxy_start_advertising_with_node_id_for_subnet(mesh_subnet_t * mesh_subnet){
    if (mesh_subnet->node_id_advertisement_running) return;
    log_info("Proxy start advertising with node id, netkey index %04x", mesh_subnet->netkey_index);
    // setup node id
    btstack_crypto_random_generate(&mesh_proxy_node_id_crypto_request_random, mesh_proxy_node_id_random_value, sizeof(mesh_proxy_node_id_random_value), mesh_proxy_node_id_handle_random, mesh_subnet);
}

// Public API

uint8_t mesh_proxy_get_advertising_with_node_id_status(uint16_t netkey_index, mesh_node_identity_state_t * out_state ){
    mesh_subnet_t * mesh_subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (mesh_subnet == NULL){
        *out_state = MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED;
        return MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    }

#ifdef ENABLE_MESH_PROXY_SERVER
    if (mesh_subnet->node_id_advertisement_running == 0){
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
    mesh_subnet_t * mesh_subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (mesh_subnet == NULL){
        return  MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX;
    }

#ifdef ENABLE_MESH_PROXY_SERVER
    switch (state){
        case MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED:
            mesh_proxy_stop_advertising_with_node_id_for_subnet(mesh_subnet);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        case MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING:
            mesh_proxy_start_advertising_with_node_id_for_subnet(mesh_subnet);
            return MESH_FOUNDATION_STATUS_SUCCESS;
        default:
            break;
    }
#endif

    return MESH_FOUNDATION_STATUS_FEATURE_NOT_SUPPORTED;
}

static void mesh_proxy_start_advertising_with_node_id_next_subnet(void){
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * subnet = mesh_subnet_iterator_get_next(&it);
        if (subnet->node_id_advertisement_running != 0) continue;

        mesh_proxy_start_advertising_with_node_id_for_subnet(subnet);
        return;
    }
}

void mesh_proxy_start_advertising_with_node_id(void){
    mesh_proxy_node_id_all_subnets = 1;
    adv_bearer_advertisements_enable(1);
    
    // start advertising on first subnet that is not already advertising with node id    
    mesh_proxy_start_advertising_with_node_id_next_subnet();
}

void mesh_proxy_stop_advertising_with_node_id(void){
    mesh_proxy_stop_all_advertising_with_node_id();
}

void mesh_proxy_start_advertising_with_network_id(void){
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * subnet = mesh_subnet_iterator_get_next(&it);
        log_info("Proxy start advertising with network id, netkey index %04x", subnet->netkey_index);
        // setup advertisement with network id (used by proxy)
        mesh_network_key_t * network_key = mesh_subnet_get_outgoing_network_key(subnet);
        subnet->advertisement_with_network_id.adv_length = mesh_proxy_setup_advertising_with_network_id(subnet->advertisement_with_network_id.adv_data, network_key->network_id);
        adv_bearer_advertisements_add_item(&subnet->advertisement_with_network_id);
    }
    adv_bearer_advertisements_enable(1);
}

void mesh_proxy_stop_advertising_with_network_id(void){  
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * network_key = mesh_subnet_iterator_get_next(&it);
        log_info("Proxy stop advertising with network id, netkey index %04x", network_key->netkey_index);
        adv_bearer_advertisements_remove_item(&network_key->advertisement_with_network_id);
    }
}

// Mesh Proxy Configuration

typedef enum {
    MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_SET_FILTER_TYPE = 0,
    MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_ADD_ADDRESSES,
    MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_REMOVE_ADDRESSES,
    MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_FILTER_STATUS
} mesh_proxy_configuration_message_opcode_t;

typedef enum {
    MESH_PROXY_CONFIGURATION_FILTER_TYPE_SET_WHITE_LIST = 0,
    MESH_PROXY_CONFIGURATION_FILTER_TYPE_BLACK_LIST
} mesh_proxy_configuration_filter_type_t;

static mesh_network_pdu_t * encrypted_proxy_configuration_ready_to_send;

// Used to answer configuration request
static uint16_t proxy_configuration_filter_list_len;
static mesh_proxy_configuration_filter_type_t proxy_configuration_filter_type;
static uint16_t primary_element_address;

static void request_can_send_now_proxy_configuration_callback_handler(mesh_network_pdu_t * network_pdu){
    encrypted_proxy_configuration_ready_to_send = network_pdu;
    gatt_bearer_request_can_send_now_for_mesh_proxy_configuration();
}

static void proxy_configuration_message_handler(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * received_network_pdu){
    mesh_proxy_configuration_message_opcode_t opcode;
    uint8_t data[4];
    mesh_network_pdu_t * network_pdu;
    uint8_t * network_pdu_data;

    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("proxy_configuration_message_handler: MESH_PROXY_PDU_RECEIVED\n");
            network_pdu_data = mesh_network_pdu_data(received_network_pdu);
            opcode = network_pdu_data[0];
            switch (opcode){
                case MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_SET_FILTER_TYPE:{
                    switch (network_pdu_data[1]){
                        case MESH_PROXY_CONFIGURATION_FILTER_TYPE_SET_WHITE_LIST:
                        case MESH_PROXY_CONFIGURATION_FILTER_TYPE_BLACK_LIST:
                            proxy_configuration_filter_type = network_pdu_data[1];
                            break;
                    }
                    
                    uint8_t  ctl          = 1;
                    uint8_t  ttl          = 0;
                    uint16_t src          = primary_element_address;
                    uint16_t dest         = 0; // unassigned address
                    uint32_t seq          = mesh_sequence_number_next();
                    uint8_t  nid          = mesh_network_nid(received_network_pdu);
                    uint16_t netkey_index = received_network_pdu->netkey_index; 
                    printf("netkey index 0x%02x\n", netkey_index);

                    network_pdu = btstack_memory_mesh_network_pdu_get();
                    int pos = 0;
                    data[pos++] = MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_FILTER_STATUS;
                    data[pos++] = proxy_configuration_filter_type;
                    big_endian_store_16(data, pos, proxy_configuration_filter_list_len);

                    mesh_network_setup_pdu(network_pdu, netkey_index, nid, ctl, ttl, seq, src, dest, data, sizeof(data));
                    mesh_network_encrypt_proxy_configuration_message(network_pdu, &request_can_send_now_proxy_configuration_callback_handler);
                    
                    // received_network_pdu is processed
                    btstack_memory_mesh_network_pdu_free(received_network_pdu);
                    break;
                }
                default:
                    printf("proxy config not implemented, opcode %d\n", opcode);
                    break;
            }
            break;
        case MESH_NETWORK_CAN_SEND_NOW:
            printf("MESH_SUBEVENT_CAN_SEND_NOW mesh_netework_gatt_bearer_handle_proxy_configuration len %d\n", encrypted_proxy_configuration_ready_to_send->len);
            printf_hexdump(encrypted_proxy_configuration_ready_to_send->data, encrypted_proxy_configuration_ready_to_send->len);
            gatt_bearer_send_mesh_proxy_configuration(encrypted_proxy_configuration_ready_to_send->data, encrypted_proxy_configuration_ready_to_send->len); 
            break;
        case MESH_NETWORK_PDU_SENT:
            // printf("test MESH_PROXY_PDU_SENT\n");
            // mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_SENT, network_pdu);
            break;
        default:
            break;
    }
}

void mesh_proxy_init(uint16_t primary_unicast_address){
    primary_element_address = primary_unicast_address;

    // mesh proxy configuration
    mesh_network_set_proxy_message_handler(proxy_configuration_message_handler);
}
#endif


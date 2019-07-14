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

    mesh_node_setup_default_models();
}

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler){
    provisioning_device_packet_handler = packet_handler;
    provisioning_device_register_packet_handler(&mesh_provisioning_message_handler);
}

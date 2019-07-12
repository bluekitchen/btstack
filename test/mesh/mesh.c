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

#include "mesh.h"
#include "btstack_util.h"
#include "btstack_config.h"
#include "btstack_event.h"

#include "mesh/adv_bearer.h"
#include "mesh/beacon.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/pb_adv.h"
#include "mesh/pb_gatt.h"
#include "mesh_access.h"
#include "mesh_configuration_server.h"
#include "mesh_foundation.h"
#include "mesh_generic_model.h"
#include "mesh_generic_server.h"
#include "mesh_iv_index_seq_number.h"
#include "mesh_peer.h"
#include "mesh_proxy.h"
#include "mesh_virtual_addresses.h"
#include "provisioning.h"
#include "provisioning_device.h"

static btstack_packet_handler_t provisioning_device_packet_handler;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static int provisioned;

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
}

/**
 * Register for Mesh Provisioning Device events
 * @param packet_handler
 */
void mesh_register_provisioning_device_packet_handler(btstack_packet_handler_t packet_handler){
    provisioning_device_packet_handler = packet_handler;
    provisioning_device_register_packet_handler(&mesh_provisioning_message_handler);
}

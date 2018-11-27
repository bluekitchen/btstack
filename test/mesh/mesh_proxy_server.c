/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_proxy_server.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble/mesh/gatt_bearer.h"
#include "ble/gatt-service/mesh_proxy_service_server.h"
#include "mesh_proxy_server.h"
#include "btstack_config.h"
#include "btstack.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "btstack_tlv.h"

static mesh_provisioning_data_t provisioning_data;
static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;
static uint16_t con_handle;
static uint16_t adv_int_min = 0x0030;
static uint16_t adv_int_max = 0x0030;

static btstack_crypto_random_t crypto_request_random;
static btstack_crypto_aes128_t crypto_request_aes128;
static uint8_t plaintext[16];
static uint8_t hash[8];
static uint8_t random_value[8];

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t adv_data_with_network_id[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8,
    // Service Data
    0x0C, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8, 
          // MESH_IDENTIFICATION_NETWORK_ID_TYPE
          MESH_IDENTIFICATION_NETWORK_ID_TYPE, 
          // Network ID
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t adv_data_with_node_identity[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8,
    // Service Data
    0x14, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8, 
          // MESH_IDENTIFICATION_NODE_IDENTIFY_TYPE
          MESH_IDENTIFICATION_NODE_IDENTIFY_TYPE, 
          // Hash
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          // Random
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t adv_data_with_network_id_len = sizeof(adv_data_with_network_id);
const uint8_t adv_data_with_node_identity_len = sizeof(adv_data_with_node_identity);


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Mesh Provisioning Device Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    switch (cmd){
        case 't':
            printf("disconnect \n");
            gap_disconnect(con_handle);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

static void setup_advertising_with_network_id(mesh_provisioning_data_t * prov_data){
    // dynamically store network ID into adv data 
    // skip flipping for now ... (check if provisioner or BlueNRG-MESH has a bug)
    // uint8_t netid_flipped[8];
    // reverse_64(provisioning_data.network_id, netid_flipped);
    // memcpy(&adv_data_with_network_id[12], netid_flipped, sizeof(netid_flipped));
    memcpy(&adv_data_with_network_id[12], prov_data->network_id, sizeof(prov_data->network_id));
     // setup advertisements
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint8_t adv_type = 0;   // AFV_IND
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_with_network_id_len, (uint8_t*) adv_data_with_network_id);
    gap_advertisements_enable(1);
}

static void mesh_proxy_handle_get_aes128(void * arg){
    UNUSED(arg);
    memcpy(&adv_data_with_node_identity[12], hash, 8);
    memcpy(&adv_data_with_node_identity[20], random_value, 8);
    printf("Calculated Hash\n");
    printf_hexdump(hash, sizeof(hash));

    printf("\nAdv\n");
    printf_hexdump(adv_data_with_node_identity, sizeof(adv_data_with_node_identity));
     // setup advertisements
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint8_t adv_type = 0;   // AFV_IND
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_with_network_id_len, (uint8_t*) adv_data_with_network_id);
    gap_advertisements_enable(1);
}
 
static void setup_advertising_with_node_identity(mesh_provisioning_data_t * prov_data){
    // Hash = e(IdentityKey, Padding | Random | Address) mod 2^64
    memset(plaintext, 0, sizeof(plaintext));
    memcpy(&plaintext[6] , random_value, 8);
    big_endian_store_16(plaintext, 14, prov_data->unicast_address);
    btstack_crypto_aes128_encrypt(&crypto_request_aes128, prov_data->identity_key, plaintext, hash, mesh_proxy_handle_get_aes128, NULL);
}  

static void mesh_proxy_handle_get_random(void * arg){
    UNUSED(arg);
    printf("Received random value\n");
    printf_hexdump(random_value, sizeof(random_value));
    setup_advertising_with_node_identity(&provisioning_data);
}

static uint8_t mesh_salt_nhbk[16];
static btstack_crypto_aes128_cmac_t crypto_aes128_cmac_request;
static const char salt[] = {'n','k','i','k'};

static void mesh_proxy_handle_get_salt_nhbk(void * arg){
    UNUSED(arg);
    printf("Received salt_nhbk\n");
    printf_hexdump(mesh_salt_nhbk, sizeof(mesh_salt_nhbk));
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int prov_len;
    if (packet_type != HCI_EVENT_PACKET) return;
 
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:{
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            // get tlv
            btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
            // load provisioning data
            prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
            printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
            
            if (!prov_len) break;
            // setup_advertising_with_network_id(&provisioning_data);
            btstack_crypto_random_generate(&crypto_request_random, random_value, sizeof(random_value), mesh_proxy_handle_get_random, NULL);
            // btstack_crypto_aes128_cmac_zero(&crypto_aes128_cmac_request, 4, (const uint8_t *)salt,  mesh_salt_nhbk, mesh_proxy_handle_get_salt_nhbk, NULL);
            break;
        }
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("connected handle 0x%02x\n", con_handle);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void){
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // crypto
    btstack_crypto_init();

    // l2cap
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    

    // Setup GATT bearere
    gatt_bearer_init();
#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

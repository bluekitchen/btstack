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
#include "ble/mesh/mesh_network.h"
#include "btstack_config.h"
#include "btstack.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "btstack_tlv.h"

#define BEACON_TYPE_SECURE_NETWORK 1

#define USE_ADVERTISING_WITH_NETWORK_ID
// #define USE_ADVERTISING_WITH_NODE_IDENTITY

#if defined(USE_ADVERTISING_WITH_NETWORK_ID) && defined(USE_ADVERTISING_WITH_NODE_IDENTITY) 
#error "USE_ADVERTISING_WITH_NETWORK_ID and USE_ADVERTISING_WITH_NODE_IDENTITY cannot be defined at the same time"
#endif

#if !defined(USE_ADVERTISING_WITH_NETWORK_ID) && !defined(USE_ADVERTISING_WITH_NODE_IDENTITY) 
#error "USE_ADVERTISING_WITH_NETWORK_ID or USE_ADVERTISING_WITH_NODE_IDENTITY must be defined"
#endif

static mesh_provisioning_data_t provisioning_data;
static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;
static uint16_t con_handle;
static uint16_t adv_int_min = 0x0030;
static uint16_t adv_int_max = 0x0030;

#ifdef USE_ADVERTISING_WITH_NODE_IDENTITY 
static btstack_crypto_random_t crypto_request_random;
static btstack_crypto_aes128_t crypto_request_aes128;
static uint8_t plaintext[16];
static uint8_t hash[8];
static uint8_t random_value[8];

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

const uint8_t adv_data_with_node_identity_len = sizeof(adv_data_with_node_identity);
#endif

#ifdef USE_ADVERTISING_WITH_NETWORK_ID 
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

const uint8_t adv_data_with_network_id_len = sizeof(adv_data_with_network_id);
#endif

static btstack_crypto_aes128_cmac_t mesh_cmac_request;
static uint8_t   mesh_secure_network_beacon[22];
static uint8_t   mesh_secure_network_beacon_auth_value[16];
static uint8_t   mesh_flags;
static uint8_t   network_id[8];
static uint8_t   beacon_key[16];
static uint8_t * network_pdu_data;
static uint8_t   network_pdu_len;

static btstack_packet_callback_registration_t hci_event_callback_registration;

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

#ifdef USE_ADVERTISING_WITH_NODE_IDENTITY 
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
    gap_advertisements_set_data(adv_data_with_node_identity_len, (uint8_t*) adv_data_with_node_identity);
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
#endif



#ifdef USE_ADVERTISING_WITH_NETWORK_ID
static void setup_advertising_with_network_id(mesh_provisioning_data_t * prov_data){
    // dynamically store network ID into adv data 
    memcpy(&adv_data_with_network_id[12], prov_data->network_id, sizeof(prov_data->network_id));
    // copy beacon key and network id
    memcpy(beacon_key, prov_data->beacon_key, 16);
    memcpy(network_id, prov_data->network_id, 8);
    
    printf_hexdump(prov_data->network_id, 8);
    mesh_flags = prov_data->flags;

     // setup advertisements
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint8_t adv_type = 0;   // AFV_IND
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_with_network_id_len, (uint8_t*) adv_data_with_network_id);
    gap_advertisements_enable(1);
}
#endif

static void mesh_provisioning_dump(const mesh_provisioning_data_t * data){
    printf("UnicastAddr:   0x%02x\n", data->unicast_address);
    printf("NID:           0x%02x\n", data->nid);
    printf("IV Index:      0x%08x\n", data->iv_index);
    printf("NetworkID:     "); printf_hexdump(data->network_id, 8);
    printf("BeaconKey:     "); printf_hexdump(data->beacon_key, 16);
    printf("EncryptionKey: "); printf_hexdump(data->encryption_key, 16);
    printf("PrivacyKey:    "); printf_hexdump(data->privacy_key, 16);
    printf("DevKey:        "); printf_hexdump(data->device_key, 16);
}

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    UNUSED(arg);
    memcpy(&mesh_secure_network_beacon[14], mesh_secure_network_beacon_auth_value, 8);
    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
    printf("Secure Network Beacon done\n");
    gatt_bearer_request_can_send_now_for_mesh_beacon();
}

static void packet_handler_for_mesh_network_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet_handler_for_mesh_network_pdu\n");
    
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            printf("Received network PDU\n");
            printf_hexdump(packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_PB_TRANSPORT_LINK_OPEN:
                            printf("mesh_proxy_server: MESH_PB_TRANSPORT_LINK_OPEN\n");
                            printf("+ Setup Secure Network Beacon\n");
                            mesh_secure_network_beacon[0] = BEACON_TYPE_SECURE_NETWORK;
                            mesh_secure_network_beacon[1] = mesh_flags;
                            memcpy(&mesh_secure_network_beacon[2], network_id, 8);
                            big_endian_store_32(mesh_secure_network_beacon, 10, mesh_get_iv_index());
                            btstack_crypto_aes128_cmac_message(&mesh_cmac_request, beacon_key, 13,
                                &mesh_secure_network_beacon[1], mesh_secure_network_beacon_auth_value, &mesh_secure_network_beacon_auth_value_calculated, NULL);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void packet_handler_for_mesh_beacon(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet_handler_for_mesh_beacon\n");
    
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            printf("Received beacon\n");
            printf_hexdump(packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            gatt_bearer_send_mesh_beacon(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void packet_handler_for_mesh_proxy_configuration(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet_handler_for_mesh_proxy_configuration\n");
            
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            printf("Received proxy configuration\n");
            printf_hexdump(packet, size);
            mesh_network_process_proxy_message(packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            printf("MESH_SUBEVENT_CAN_SEND_NOW packet_handler_for_mesh_proxy_configuration\n");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void mesh_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){
    // add to network key list
    mesh_network_key_list_add_from_provisioning_data(provisioning_data);
    // set unicast address
    mesh_network_set_primary_element_address(provisioning_data->unicast_address);
    // mesh_upper_transport_set_primary_element_address(provisioning_data->unicast_address);
    // primary_element_address = provisioning_data->unicast_address;
    // set iv_index
    mesh_set_iv_index(provisioning_data->iv_index);
    // set device_key
    // mesh_transport_set_device_key(provisioning_data->device_key);
    // copy beacon key and network id
    // memcpy(beacon_key, provisioning_data->beacon_key, 16);
    // memcpy(network_id, provisioning_data->network_id, 8);
    // for secure beacon
    // mesh_flags = provisioning_data->flags;
    // dump data
    mesh_provisioning_dump(provisioning_data);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int prov_len;
    
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            printf("MESH_PROXY_DATA_PACKET \n");
            printf_hexdump(packet, size);

            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case BTSTACK_EVENT_STATE:{
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // get tlv
                    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
                    // load provisioning data
                    prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
                    if (!prov_len) break;

                    mesh_setup_from_provisioning_data(&provisioning_data);

#ifdef USE_ADVERTISING_WITH_NETWORK_ID
                    setup_advertising_with_network_id(&provisioning_data);
#endif
#ifdef USE_ADVERTISING_WITH_NODE_IDENTITY 
                    btstack_crypto_random_generate(&crypto_request_random, random_value, sizeof(random_value), mesh_proxy_handle_get_random, NULL);
#endif
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
            break;
        default:
            break;
    }
}

void proxy_configuration_message_handler(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu){
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("proxy_configuration_message_handler: MESH_PROXY_PDU_RECEIVED\n");
            network_pdu_len = mesh_network_pdu_len(network_pdu);
            network_pdu_data = mesh_network_pdu_data(network_pdu);
            printf_hexdump(network_pdu_data, network_pdu_len);
            break;
        case MESH_NETWORK_PDU_SENT:
            // printf("test MESH_PROXY_PDU_SENT\n");
            // mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_SENT, network_pdu);
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
    gatt_bearer_register_for_mesh_network_pdu(&packet_handler_for_mesh_network_pdu);
    gatt_bearer_register_for_mesh_beacon(&packet_handler_for_mesh_beacon);

    gatt_bearer_register_for_mesh_proxy_configuration(&packet_handler_for_mesh_proxy_configuration);
    mesh_network_set_proxy_message_handler(proxy_configuration_message_handler);
    
#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

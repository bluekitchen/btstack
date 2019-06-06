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

#define __BTSTACK_FILE__ "mesh.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ble/mesh/adv_bearer.h"
#include "ble/mesh/gatt_bearer.h"
#include "ble/mesh/beacon.h"
#include "ble/mesh/mesh_crypto.h"
#include "ble/mesh/mesh_lower_transport.h"
#include "ble/mesh/pb_adv.h"
#include "ble/mesh/pb_gatt.h"
#include "ble/gatt-service/mesh_provisioning_service_server.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "mesh_transport.h"
#include "mesh_foundation.h"
#include "mesh.h"
#include "btstack.h"
#include "btstack_tlv.h"

// #define ENABLE_MESH_ADV_BEARER
// #define ENABLE_MESH_PB_ADV
#define ENABLE_MESH_PROXY_SERVER
#define ENABLE_MESH_PB_GATT


#define BEACON_TYPE_SECURE_NETWORK 1
#define PTS_DEFAULT_TTL 10

static void show_usage(void);

const static uint8_t device_uuid[] = { 0x00, 0x1B, 0xDC, 0x08, 0x10, 0x21, 0x0B, 0x0E, 0x0A, 0x0C, 0x00, 0x0B, 0x0E, 0x0A, 0x0C, 0x00 };

// Mesh Provisioning
static uint8_t adv_data_unprovisioned[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING >> 8,
    // Service Data (22)
    0x15, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING >> 8, 
          // UUID
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          // OOB information
          0x00, 0x00
};
const uint8_t adv_data_unprovisioned_len = sizeof(adv_data_unprovisioned);

// Mesh Proxy, advertise with node id
static adv_bearer_connectable_advertisement_data_item_t connectable_advertisement_item;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint8_t mesh_flags;

static uint16_t pb_transport_cid = MESH_PB_TRANSPORT_INVALID_CID;

// pin entry
static int ui_chars_for_pin; 
static uint8_t ui_pin[17];
static int ui_pin_offset;

static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

static uint8_t beacon_key[16];
static uint8_t identity_key[16];
static uint8_t network_id[8];
static uint16_t primary_element_address;

static int provisioned;

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
     printf("%-20s ", name);
     printf_hexdump(data, len);
}

// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }

#ifdef ENABLE_MESH_PROXY_SERVER

// we only support a single active node id advertisement. when new one is started, an active one is stopped

#define MESH_PROXY_NODE_ID_ADVERTISEMENT_TIMEOUT_MS 60000

static adv_bearer_connectable_advertisement_data_item_t mesh_proxy_node_id_advertisement_item;
static btstack_timer_source_t                           mesh_proxy_node_id_timer;
static btstack_crypto_random_t                          mesh_proxy_node_id_crypto_request_random;
static btstack_crypto_aes128_t                          mesh_proxy_node_id_crypto_request_aes128;
static uint8_t                                          mesh_proxy_node_id_plaintext[16];
static uint8_t                                          mesh_proxy_node_id_hash[16];
static uint8_t                                          mesh_proxy_node_id_random_value[8];

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


static void mesh_proxy_start_advertising_with_network_id(void){
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        log_info("Proxy start advertising with network id, netkey index %04x", network_key->netkey_index);
        adv_bearer_advertisements_add_item(&network_key->advertisement_with_network_id);
    }
}

static void mesh_proxy_stop_advertising_with_network_id(void){  
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        log_info("Proxy stop advertising with network id, netkey index %04x", network_key->netkey_index);
        adv_bearer_advertisements_remove_item(&network_key->advertisement_with_network_id);
    }
}

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
    UNUSED(arg);
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
}

static void mesh_proxy_node_id_handle_random(void * arg){
    // Hash = e(IdentityKey, Padding | Random | Address) mod 2^64
    memset(mesh_proxy_node_id_plaintext, 0, sizeof(mesh_proxy_node_id_plaintext));
    memcpy(&mesh_proxy_node_id_plaintext[6] , mesh_proxy_node_id_random_value, 8);
    big_endian_store_16(mesh_proxy_node_id_plaintext, 14, primary_element_address);
    btstack_crypto_aes128_encrypt(&mesh_proxy_node_id_crypto_request_aes128,identity_key, mesh_proxy_node_id_plaintext, mesh_proxy_node_id_hash, mesh_proxy_node_id_handle_get_aes128, NULL);
}

static void mesh_proxy_start_advertising_with_node_id(uint16_t netkey_index){
    mesh_proxy_stop_all_advertising_with_node_id();
    log_info("Proxy start advertising with node id, netkey index %04x", netkey_index);
    // setup node id
    btstack_crypto_random_generate(&mesh_proxy_node_id_crypto_request_random, mesh_proxy_node_id_random_value, sizeof(mesh_proxy_node_id_random_value), mesh_proxy_node_id_handle_random, NULL);
}

static void mesh_proxy_stop_advertising_with_node_id(uint16_t netkey_index){
    UNUSED(netkey_index);
    log_info("Proxy stop advertising with node id, netkey index %04x", netkey_index);
    mesh_proxy_stop_all_advertising_with_node_id();
}
#endif

static void mesh_provisioning_dump(const mesh_provisioning_data_t * data){
    printf("UnicastAddr:   0x%02x\n", data->unicast_address);
    printf("IV Index:      0x%08x\n", data->iv_index);
    printf("DevKey:        "); printf_hexdump(data->device_key, 16);
    printf("NetKey:        "); printf_hexdump(data->net_key, 16);
    printf("NID:           0x%02x\n", data->nid);
    printf("NetworkID:     "); printf_hexdump(data->network_id, 8);
    printf("BeaconKey:     "); printf_hexdump(data->beacon_key, 16);
    printf("EncryptionKey: "); printf_hexdump(data->encryption_key, 16);
    printf("PrivacyKey:    "); printf_hexdump(data->privacy_key, 16);
    printf("IdentityKey:   "); printf_hexdump(data->identity_key, 16);
}

static void mesh_network_key_add_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){

    // get key
    mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();

    // get single instance
    memset(network_key, 0, sizeof(mesh_network_key_t));

    // NetKey
    memcpy(network_key->net_key, provisioning_data->net_key, 16);

    // IdentityKey
    memcpy(network_key->identity_key, provisioning_data->identity_key, 16);

    // BeaconKey
    memcpy(network_key->beacon_key, provisioning_data->beacon_key, 16);

    // NID
    network_key->nid = provisioning_data->nid;

    // EncryptionKey
    memcpy(network_key->encryption_key, provisioning_data->encryption_key, 16);

    // PrivacyKey
    memcpy(network_key->privacy_key, provisioning_data->privacy_key, 16);

    // NetworkID
    memcpy(network_key->network_id, provisioning_data->network_id, 8);

    // setup advertisement with network id
    network_key->advertisement_with_network_id.adv_length = gatt_bearer_setup_advertising_with_network_id(network_key->advertisement_with_network_id.adv_data, network_key->network_id);

    // finally add
    mesh_network_key_add(network_key);
}

static void mesh_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){
    provisioned = 1;

    // add to network key list
    mesh_network_key_add_from_provisioning_data(provisioning_data);
    // set unicast address
    mesh_network_set_primary_element_address(provisioning_data->unicast_address);
    mesh_lower_transport_set_primary_element_address(provisioning_data->unicast_address);
    mesh_upper_transport_set_primary_element_address(provisioning_data->unicast_address);
    primary_element_address = provisioning_data->unicast_address;
    // set iv_index
    mesh_set_iv_index(provisioning_data->iv_index);
    // set device_key
    mesh_transport_set_device_key(provisioning_data->device_key);
    // copy beacon key and network id
    memcpy(identity_key, provisioning_data->identity_key, 16);
    memcpy(beacon_key, provisioning_data->beacon_key, 16);
    memcpy(network_id, provisioning_data->network_id, 8);
    // for secure beacon
    mesh_flags = provisioning_data->flags;
    // dump data
    mesh_provisioning_dump(provisioning_data);
    
    // Mesh Proxy
#ifdef ENABLE_MESH_PROXY_SERVER
    printf("Advertise Mesh Proxy Service with Network ID\n");
    mesh_proxy_start_advertising_with_network_id();
#endif
}

#ifdef ENABLE_MESH_PB_GATT
static void setup_advertising_unprovisioned(void) {
    printf("Advertise Mesh Provisioning Service\n");
    // dynamically store device uuid into adv data
    memcpy(&adv_data_unprovisioned[11], device_uuid, sizeof(device_uuid));
    // store in advertisement item
    memset(&connectable_advertisement_item, 0, sizeof(connectable_advertisement_item));
    connectable_advertisement_item.adv_length = adv_data_unprovisioned_len;
    memcpy(connectable_advertisement_item.adv_data, (uint8_t*) adv_data_unprovisioned, adv_data_unprovisioned_len);
    
    // setup advertisements
    adv_bearer_advertisements_add_item(&connectable_advertisement_item);
    adv_bearer_advertisements_enable(1);
}
#endif


static void mesh_setup_without_provisiong_data(void){
    provisioned = 0;

#ifdef ENABLE_MESH_PB_ADV
    // PB-ADV    
    printf("Starting Unprovisioned Device Beacon\n");
    beacon_unprovisioned_device_start(device_uuid, 0);
#endif
#ifdef ENABLE_MESH_PB_GATT
    // PB_GATT
    setup_advertising_unprovisioned();
#endif
}



static mesh_transport_key_t   test_application_key;

static void mesh_application_key_set(uint16_t netkey_index, uint16_t appkey_index, uint8_t aid, const uint8_t *application_key) {
    test_application_key.netkey_index = netkey_index;
    test_application_key.appkey_index = appkey_index;
    test_application_key.aid   = aid;
    test_application_key.akf   = 1;
    memcpy(test_application_key.key, application_key, 16);
    mesh_transport_key_add(&test_application_key);
}

static void mesh_load_app_keys(void){
/*
    uint8_t data[2+1+16];
    int app_key_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'APPK', (uint8_t *) &data, sizeof(data));
    if (app_key_len){
        uint16_t appkey_index = little_endian_read_16(data, 0);
        uint8_t  aid          = data[2];
        uint8_t * application_key = &data[3];
        mesh_application_key_set(0, appkey_index, aid, application_key);
        printf("Load AppKey: AppKey Index 0x%06x, AID %02x: ", appkey_index, aid);
        printf_hexdump(application_key, 16);
    }  else {
        printf("No Appkey stored\n");
    }
*/
}

void mesh_store_app_key(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key){
/*
    printf("Store AppKey: AppKey Index 0x%06x, AID %02x: ", appkey_index, aid);
    printf_hexdump(application_key, 16);
    uint8_t data[2+1+16];
    little_endian_store_16(data, 0, appkey_index);
    data[2] = aid;
    memcpy(&data[3], application_key, 16);
    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, 'APPK', (uint8_t *) &data, sizeof(data));
*/
}

// helper network layer, temp
static uint8_t mesh_network_send(uint16_t netkey_index, uint8_t ctl, uint8_t ttl, uint32_t seq, uint16_t src, uint16_t dest, const uint8_t * transport_pdu_data, uint8_t transport_pdu_len){

    // "3.4.5.2: The output filter of the interface connected to advertising or GATT bearers shall drop all messages with TTL value set to 1."
    // if (ttl <= 1) return 0;

    // TODO: check transport_pdu_len depending on ctl

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 0;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (!network_pdu) return 0;

    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, ctl, ttl, seq, src, dest, transport_pdu_data, transport_pdu_len);

    // send network_pdu
    mesh_lower_transport_send_pdu((mesh_pdu_t *) network_pdu);
    return 0;
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t addr;
    int i;
    int prov_len;
    mesh_provisioning_data_t provisioning_data;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // dump bd_addr in pts format
                    gap_local_bd_addr(addr);
                    printf("Local addr: %s - ", bd_addr_to_str(addr));
                    for (i=0;i<6;i++) {
                        printf("%02x", addr[i]);
                    }
                    printf("\n");
                    // get tlv
                    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
                    // load provisioning data
                    prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
                    if (prov_len){
                        mesh_setup_from_provisioning_data(&provisioning_data);
                    } else {
                        mesh_setup_without_provisiong_data();
                    }
                    // load app keys
                    mesh_load_app_keys();
#if defined(ENABLE_MESH_ADV_BEARER) || defined(ENABLE_MESH_PB_ADV)
                    // setup scanning
                    gap_set_scan_parameters(0, 0x300, 0x300);
                    gap_start_scan();
#endif
                    //
                    show_usage();
                    break;
                
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // enable PB_GATT
                    if (provisioned == 0){
                         setup_advertising_unprovisioned();
                    } else {
#ifdef ENABLE_MESH_PROXY_SERVER
                        printf("Advertise Mesh Proxy Service with Network ID\n");
                        mesh_proxy_start_advertising_with_network_id();
#endif
                    }
                    break;
                    
                case HCI_EVENT_LE_META:
                    if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
                    // disable PB_GATT
                    printf("Connected, stop advertising gatt service\n");
                    adv_bearer_advertisements_remove_item(&connectable_advertisement_item);
                    break;
                default:
                    break;
            }
            break;
    }
}

static void mesh_provisioning_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    mesh_provisioning_data_t provisioning_data;
    mesh_network_key_t * network_key;
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                    printf("Provisioner link opened");
                    pb_transport_cid = mesh_subevent_pb_transport_link_open_get_pb_transport_cid(packet);
                    break;
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED:
                    pb_transport_cid = MESH_PB_TRANSPORT_INVALID_CID;
                    break;
                case MESH_SUBEVENT_PB_PROV_ATTENTION_TIMER:
                    printf("Attention Timer: %u\n", packet[3]);
                    break;
                case MESH_SUBEVENT_PB_PROV_INPUT_OOB_REQUEST:
                    printf("Enter passphrase: ");
                    fflush(stdout);
                    ui_chars_for_pin = 1;
                    ui_pin_offset = 0;
                    break;
                case MESH_SUBEVENT_PB_PROV_COMPLETE:
                    printf("Provisioning complete\n");

                    memcpy(provisioning_data.device_key, provisioning_device_data_get_device_key(), 16);
                    provisioning_data.iv_index = provisioning_device_data_get_iv_index();
                    provisioning_data.flags = provisioning_device_data_get_flags();
                    provisioning_data.unicast_address = provisioning_device_data_get_unicast_address();

                    network_key = provisioning_device_data_get_network_key();
                    memcpy(provisioning_data.net_key,        network_key->net_key, 16);
                    memcpy(provisioning_data.network_id,     network_key->network_id, 8);
                    memcpy(provisioning_data.identity_key,   network_key->identity_key, 16);
                    memcpy(provisioning_data.beacon_key,     network_key->beacon_key, 16);
                    memcpy(provisioning_data.encryption_key, network_key->encryption_key, 16);
                    memcpy(provisioning_data.privacy_key,    network_key->privacy_key, 16);
                    provisioning_data.nid = network_key->nid;

                    // store in TLV
                    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));

                    // setup after provisioned
                    mesh_setup_from_provisioning_data(&provisioning_data);

                    // start advertising with node id after provisioning
                    mesh_proxy_start_advertising_with_node_id(network_key->netkey_index);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void mesh_unprovisioned_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != MESH_BEACON_PACKET) return;
    uint8_t  device_uuid[16];
    uint16_t oob;
    memcpy(device_uuid, &packet[1], 16);
    oob = big_endian_read_16(packet, 17);
    printf("received unprovisioned device beacon, oob data %x, device uuid: ", oob);
    printf_hexdump(device_uuid, 16);
    pb_adv_create_link(device_uuid);
}

uint8_t      pts_device_uuid[16];
const char * pts_device_uuid_string = "001BDC0810210B0E0A0C000B0E0A0C00";

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

static int btstack_parse_hex(const char * string, uint16_t len, uint8_t * buffer){
    int i;
    for (i = 0; i < len; i++) {
        int single_byte = scan_hex_byte(string);
        if (single_byte < 0) return 0;
        string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' && separator == '-' && separator == ' ') {
            string++;
        }
    }
    return 1;
}

static void btstack_print_hex(const uint8_t * data, uint16_t len, char separator){
    int i;
    for (i=0;i<len;i++){
        printf("%02x", data[i]);
        if (separator){
            printf("%c", separator);
        }
    }
    printf("\n");
}
static uint16_t pts_proxy_dst;
static int      pts_type;

static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

static uint8_t      prov_public_key_data[64];
static const char * prov_public_key_string = "F465E43FF23D3F1B9DC7DFC04DA8758184DBC966204796ECCF0D6CF5E16500CC0201D048BCBBD899EEEFC424164E33C201C2B010CA6B4D43A8A155CAD8ECB279";
static uint8_t      prov_private_key_data[32];
static const char * prov_private_key_string = "529AA0670D72CD6497502ED473502B037E8803B5C60829A5A3CAA219505530BA";

static btstack_crypto_aes128_cmac_t mesh_cmac_request;
static uint8_t mesh_secure_network_beacon[22];
static uint8_t mesh_secure_network_beacon_auth_value[16];

static void load_pts_app_key(void){
    // PTS app key
    uint8_t application_key[16];
    const char * application_key_string = "3216D1509884B533248541792B877F98";
    btstack_parse_hex(application_key_string, 16, application_key);
    mesh_application_key_set(0, 0, 0x38, application_key);
    printf("PTS Application Key (AID %02x): ", 0x38);
    printf_hexdump(application_key, 16);
}

static void send_pts_network_messsage(int type){
    uint8_t lower_transport_pdu_data[16];

    uint16_t src = 0x0028;
    uint16_t dst = 0x0001;
    uint32_t seq = 0x00;
    uint8_t ttl = 0;
    uint8_t ctl = 0;

    switch (type){
        case 0:
            ttl = 0;
            dst = 0x001;
            printf("unicast ttl=0\n");
            break;
        case 1:
            dst = 0x001;
            ttl = PTS_DEFAULT_TTL;
            printf("unicast ttl=10\n");
            break;
        case 2:
            dst = 0x001;
            ttl = 0x7f;
            printf("unicast ttl=0x7f\n");
            break;
        case 3:
            printf("virtual\n");
            break;
        case 4:
            printf("group\n");
            break;
        case 5:
            printf("all-proxies\n");
            break;
        case 6:
            printf("all-friends\n");
            break;
        case 7:
            printf("all-relays\n");
            break;
        case 8:
            printf("all-nodes\n");
            break;
        default:
            return;
    }
    int lower_transport_pdu_len = 16;
    memset(lower_transport_pdu_data, 0x55, lower_transport_pdu_len);
    mesh_network_send(0, ctl, ttl, seq, src, dst, lower_transport_pdu_data, lower_transport_pdu_len);
}


static void send_pts_unsegmented_access_messsage(void){
    uint8_t access_pdu_data[16];

    load_pts_app_key();

    uint16_t src = primary_element_address;
    uint16_t dest = 0x0001;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 1;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0; // MESH_DEVICE_KEY_INDEX;

    // send as unsegmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t*) mesh_network_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_upper_transport_send_access_pdu(pdu);
}

static void send_pts_segmented_access_messsage_unicast(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = primary_element_address;
    uint16_t dest = 0x0001;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0; // MESH_DEVICE_KEY_INDEX;

    // send as segmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t *) mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_upper_transport_send_access_pdu(pdu);
}

static void send_pts_segmented_access_messsage_group(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = primary_element_address;
    uint16_t dest = 0xd000;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;

    // send as segmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t *) mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_upper_transport_send_access_pdu(pdu);
}

static void send_pts_segmented_access_messsage_virtual(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = primary_element_address;
    uint16_t dest = pts_proxy_dst;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;

    // send as segmented access pdu
    mesh_transport_pdu_t * transport_pdu = mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu((mesh_pdu_t*) transport_pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_upper_transport_send_access_pdu((mesh_pdu_t*) transport_pdu);
}

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    UNUSED(arg);
    memcpy(&mesh_secure_network_beacon[14], mesh_secure_network_beacon_auth_value, 8);
    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
    adv_bearer_send_mesh_beacon(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Mesh Console at %s ---\n", bd_addr_to_str(iut_address));
    printf("1      - Send Unsegmented Access Message\n");
    printf("2      - Send   Segmented Access Message - Unicast\n");
    printf("3      - Send   Segmented Access Message - Group   D000\n");
    printf("4      - Send   Segmented Access Message - Virtual 9779\n");
    printf("6      - Clear Replay Protection List\n");
    printf("7      - Load PTS App key\n");
    printf("8      - Delete provisioning data\n");
    printf("\n");
}

static void stdin_process(char cmd){
    if (ui_chars_for_pin){
        printf("%c", cmd);
        fflush(stdout);
        if (cmd == '\n'){
            printf("\nSending Pin '%s'\n", ui_pin);
            provisioning_device_input_oob_complete_alphanumeric(1, ui_pin, ui_pin_offset);
            ui_chars_for_pin = 0;
        } else {
            ui_pin[ui_pin_offset++] = cmd;
        }
        return;
    }
    switch (cmd){
        case '0':
            send_pts_network_messsage(pts_type++);
            break;
        case '1':
            send_pts_unsegmented_access_messsage();
            break;
        case '2':
            send_pts_segmented_access_messsage_unicast();
            break;
        case '3':
            send_pts_segmented_access_messsage_group();
            break;
        case '4':
            send_pts_segmented_access_messsage_virtual();
            break;
        case '6':
            printf("Clearing Replay Protection List\n");
            mesh_seq_auth_reset();
            break;
        case '7':
            load_pts_app_key();
            break;
        case '8':
            btstack_tlv_singleton_impl->delete_tag(btstack_tlv_singleton_context, 'PROV');
            printf("Provisioning data deleted\n");
            setup_advertising_unprovisioned();
            break;
        case 'p':
            printf("+ Public Key OOB Enabled\n");
            btstack_parse_hex(prov_public_key_string, 64, prov_public_key_data);
            btstack_parse_hex(prov_private_key_string, 32, prov_private_key_data);
            provisioning_device_set_public_key_oob(prov_public_key_data, prov_private_key_data);
            break;
        case 'o':
            printf("+ Output OOB Enabled\n");
            provisioning_device_set_output_oob_actions(0x08, 0x08);
            break;
        case 'i':
            printf("+ Input OOB Enabled\n");
            provisioning_device_set_input_oob_actions(0x08, 0x08);
            break;
        case 's':
            printf("+ Static OOB Enabled\n");
            btstack_parse_hex(prov_static_oob_string, 16, prov_static_oob_data);
            provisioning_device_set_static_oob(16, prov_static_oob_data);
            break;
        case 'b':
            printf("+ Setup Secure Network Beacon\n");
            mesh_secure_network_beacon[0] = BEACON_TYPE_SECURE_NETWORK;
            mesh_secure_network_beacon[1] = mesh_flags;
            memcpy(&mesh_secure_network_beacon[2], network_id, 8);
            big_endian_store_32(mesh_secure_network_beacon, 10, mesh_get_iv_index());
            btstack_crypto_aes128_cmac_message(&mesh_cmac_request, beacon_key, 13,
                &mesh_secure_network_beacon[1], mesh_secure_network_beacon_auth_value, &mesh_secure_network_beacon_auth_value_calculated, NULL);
            break;
        case ' ':
            show_usage();
            break;
        default:
            printf("Command: '%c' not implemented\n", cmd);
            show_usage();
            break;
    }
}

// Foundation Model Operations
#define MESH_FOUNDATION_OPERATION_APPKEY_ADD                                      0x00
#define MESH_FOUNDATION_OPERATION_APPKEY_UPDATE                                   0x01
#define MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_STATUS                         0x02
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET                           0x03
#define MESH_FOUNDATION_OPERATION_HEALTH_CURRENT_STATUS                           0x04
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS                             0x05
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_STATUS                    0x06
#define MESH_FOUNDATION_OPERATION_APPKEY_DELETE                                 0x8000
#define MESH_FOUNDATION_OPERATION_APPKEY_GET                                    0x8001
#define MESH_FOUNDATION_OPERATION_APPKEY_LIST                                   0x8002
#define MESH_FOUNDATION_OPERATION_APPKEY_STATUS                                 0x8003
#define MESH_FOUNDATION_OPERATION_ATTENTION_GET                                 0x8004
#define MESH_FOUNDATION_OPERATION_ATTENTION_SET                                 0x8005
#define MESH_FOUNDATION_OPERATION_ATTENTION_SET_UNACKNOWLEDGED                  0x8006
#define MESH_FOUNDATION_OPERATION_ATTENTION_STATUS                              0x8007
#define MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_GET                          0x8008
#define MESH_FOUNDATION_OPERATION_BEACON_GET                                    0x8009
#define MESH_FOUNDATION_OPERATION_BEACON_SET                                    0x800a
#define MESH_FOUNDATION_OPERATION_BEACON_STATUS                                 0x800b
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_GET                               0x800c
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_SET                               0x800d
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_STATUS                            0x800e
#define MESH_FOUNDATION_OPERATION_FRIEND_GET                                    0x800f
#define MESH_FOUNDATION_OPERATION_FRIEND_SET                                    0x8010
#define MESH_FOUNDATION_OPERATION_FRIEND_STATUS                                 0x8011
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_GET                                0x8012
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_SET                                0x8013
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_STATUS                             0x8014
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_GET                         0x8015
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_SET                         0x8016
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_STATUS                      0x8017
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET                         0x8018
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_STATUS                      0x8019
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET         0x801a
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_ADD                        0x801b
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE                     0x801c
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL                 0x801d
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE                  0x801e
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_STATUS                     0x801f
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_ADD        0x8020
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE     0x8021
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE  0x8022
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_GET                          0x8023
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_SET                          0x8024
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_STATUS                       0x8025
#define MESH_FOUNDATION_OPERATION_RELAY_GET                                     0x8026
#define MESH_FOUNDATION_OPERATION_RELAY_SET                                     0x8027
#define MESH_FOUNDATION_OPERATION_RELAY_STATUS                                  0x8028
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET                    0x8029
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_LIST                   0x802a
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET                 0x802b
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_LIST                0x802c
#define MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_GET               0x802d
#define MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_STATUS            0x802e
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR                            0x802f
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR_UNACKNOWLEDGED             0x8030
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_GET                              0x8031
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST                             0x8032
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST_UNACKNOWLEDGED              0x8033
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_GET                             0x8034
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET                             0x8035
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET_UNACKNOWLEDGED              0x8036
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_STATUS                          0x8037
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_GET                     0x8038
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_SET                     0x8039
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_GET                    0x803a
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_SET                    0x803b
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_STATUS                 0x803c
#define MESH_FOUNDATION_OPERATION_MODEL_APP_BIND                                0x803d
#define MESH_FOUNDATION_OPERATION_MODEL_APP_STATUS                              0x803e
#define MESH_FOUNDATION_OPERATION_MODEL_APP_UNBIND                              0x803f
#define MESH_FOUNDATION_OPERATION_NETKEY_ADD                                    0x8040
#define MESH_FOUNDATION_OPERATION_NETKEY_DELETE                                 0x8041
#define MESH_FOUNDATION_OPERATION_NETKEY_GET                                    0x8042
#define MESH_FOUNDATION_OPERATION_NETKEY_LIST                                   0x8043
#define MESH_FOUNDATION_OPERATION_NETKEY_STATUS                                 0x8044
#define MESH_FOUNDATION_OPERATION_NETKEY_UPDATE                                 0x8045
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_GET                             0x8046
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_SET                             0x8047
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_STATUS                          0x8048
#define MESH_FOUNDATION_OPERATION_NODE_RESET                                    0x8049
#define MESH_FOUNDATION_OPERATION_NODE_RESET_STATUS                             0x804a
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_GET                             0x804b
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_LIST                            0x804c
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_GET                          0x804d
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_LIST                         0x804e

// Foundation Models Status Codes
#define MESH_FOUNDATION_STATUS_SUCCESS                                           0x00
#define MESH_FOUNDATION_STATUS_INVALID_ADDRESS                                   0x01
#define MESH_FOUNDATION_STATUS_INVALID_MODEL                                     0x02
#define MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX                              0x03
#define MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX                              0x04
#define MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES                            0x05
#define MESH_FOUNDATION_STATUS_KEY_INDEX_ALREADY_STORED                          0x06
#define MESH_FOUNDATION_STATUS_INVALID_PUBLISH_PARAMETER                         0x07
#define MESH_FOUNDATION_STATUS_NOT_A_SUBSCRIPTION_MODEL                          0x08
#define MESH_FOUNDATION_STATUS_STORAGE_FAILURE                                   0x09
#define MESH_FOUNDATION_STATUS_FEATURE_NOT_SUPPORTED                             0x0a
#define MESH_FOUNDATION_STATUS_CANNOT_UPDATE                                     0x0b
#define MESH_FOUNDATION_STATUS_CANNOT_REMOVE                                     0x0c
#define MESH_FOUNDATION_STATUS_CANNOT_BIND                                       0x0d
#define MESH_FOUNDATION_STATUS_TEMPORARILY_UNABLE_TO_CHANGE_STATE                0x0e
#define MESH_FOUNDATION_STATUS_CANNOT_SET                                        0x0f
#define MESH_FOUNDATION_STATUS_UNSPECIFIED_ERROR                                 0x10
#define MESH_FOUNDATION_STATUS_INVALID_BINDING                                   0x11

// Foundatiopn Message

typedef struct {
    uint32_t     opcode;
    const char * format;
} mesh_access_message_t;

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

// Model Identifier utilities

static uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id){
    return (vendor_id << 16) | model_id;
}

static uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id){
    return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | model_id;
}

static uint16_t mesh_model_get_model_id(uint32_t model_identifier){
    return model_identifier & 0xFFFFu;
}

static uint16_t mesh_model_get_vendor_id(uint32_t model_identifier){
    return model_identifier >> 16;
}

static int mesh_model_is_bluetooth_sig(uint32_t model_identifier){
    return mesh_model_get_vendor_id(model_identifier) == BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC;
}

// message parser

static int mesh_access_get_opcode(uint8_t * buffer, uint16_t buffer_size, uint32_t * opcode, uint16_t * opcode_size){
    switch (buffer[0] >> 6){
        case 0:
        case 1:
            if (buffer[0] == 0x7f) return 0;
            *opcode = buffer[0];
            *opcode_size = 1;
            return 1;
        case 2:
            if (buffer_size < 2) return 0;
            *opcode = big_endian_read_16(buffer, 0);
            *opcode_size = 2;
            return 1;
        case 3:
            if (buffer_size < 3) return 0;
            *opcode = (buffer[0] << 16) | little_endian_read_16(buffer, 1);
            *opcode_size = 3;
            return 1;
        default:
            return 0;
    }
}

static int mesh_access_transport_get_opcode(mesh_transport_pdu_t * transport_pdu, uint32_t * opcode, uint16_t * opcode_size){
    return mesh_access_get_opcode(transport_pdu->data, transport_pdu->len, opcode, opcode_size);
}

static int mesh_access_network_get_opcode(mesh_network_pdu_t * network_pdu, uint32_t * opcode, uint16_t * opcode_size){
    // TransMIC already removed by mesh_upper_transport_validate_unsegmented_message_ccm
    return mesh_access_get_opcode(&network_pdu->data[10], network_pdu->len - 10, opcode, opcode_size);
}

static int mesh_access_pdu_get_opcode(mesh_pdu_t * pdu, uint32_t * opcode, uint16_t * opcode_size){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_access_transport_get_opcode((mesh_transport_pdu_t*) pdu, opcode, opcode_size);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_access_network_get_opcode((mesh_network_pdu_t *) pdu, opcode, opcode_size);
        default:
            return 0;
    }
}

static uint16_t mesh_pdu_src(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_src((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_src((mesh_network_pdu_t *) pdu);
        default:
            return MESH_ADDRESS_UNSASSIGNED;
    }
}

static uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->netkey_index;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->netkey_index;
        default:
            return 0;
    }
}

static uint16_t mesh_pdu_len(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->len;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->len - 10;
        default:
            return 0;
    }
}

typedef struct {
    uint32_t opcode;
    uint8_t * data;
    uint16_t len;
} mesh_access_parser_state_t;

static void mesh_access_parser_skip(mesh_access_parser_state_t * state, uint16_t bytes_to_skip){
    state->data += bytes_to_skip;
    state->len  -= bytes_to_skip;
}

static int mesh_access_parser_init(mesh_access_parser_state_t * state, mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            state->data =  ((mesh_transport_pdu_t*) pdu)->data;
            state->len  = ((mesh_transport_pdu_t*) pdu)->len;
            break;
        case MESH_PDU_TYPE_NETWORK:
            state->data =  &((mesh_transport_pdu_t*) pdu)->data[10];
            state->len  = ((mesh_network_pdu_t *) pdu)->len - 10;
            break;
        default:
            return 0;
    }
    uint16_t opcode_size = 0;
    int ok = mesh_access_get_opcode(state->data, state->len, &state->opcode, &opcode_size);
    if (ok){
        mesh_access_parser_skip(state, opcode_size);
    }
    return ok;
}

static uint16_t mesh_access_parser_available(mesh_access_parser_state_t * state){
    return state->len;
}

static uint8_t mesh_access_parser_get_u8(mesh_access_parser_state_t * state){
    uint8_t value = *state->data;
    mesh_access_parser_skip(state, 1);
    return value;
}

static uint16_t mesh_access_parser_get_u16(mesh_access_parser_state_t * state){
    uint16_t value = little_endian_read_16(state->data, 0);
    mesh_access_parser_skip(state, 2);
    return value;
}

static uint32_t mesh_access_parser_get_u24(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 3);
    return value;
}

static uint32_t mesh_access_parser_get_u32(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 4);
    return value;
}

static void mesh_access_parser_get_u128(mesh_access_parser_state_t * state, uint8_t * dest){
    reverse_128( state->data, dest);
    mesh_access_parser_skip(state, 16);
}

static void mesh_access_parser_get_label_uuid(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

static void mesh_access_parser_get_key(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

static uint32_t mesh_access_parser_get_model_identifier(mesh_access_parser_state_t * parser){
    if (mesh_access_parser_available(parser) == 4){
        return mesh_access_parser_get_u32(parser);
    } else {
        return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | mesh_access_parser_get_u16(parser);
    }
}

// message builder
static int mesh_access_setup_opcode(uint8_t * buffer, uint32_t opcode){
    if (opcode < 0x100){
        buffer[0] = opcode;
        return 1;
    }
    if (opcode < 0x10000){
        big_endian_store_16(buffer, 0, opcode);
        return 2;
    }
    buffer[0] = opcode >> 16;
    little_endian_store_16(buffer, 1, opcode & 0xffff);
    return 3;
}

static mesh_transport_pdu_t * mesh_access_transport_init(uint32_t opcode){
    mesh_transport_pdu_t * pdu = mesh_transport_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(pdu->data, opcode);
    return pdu;
}

static void mesh_access_transport_add_uint8(mesh_transport_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

static void mesh_access_transport_add_uint16(mesh_transport_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

static void mesh_access_transport_add_uint24(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

static void mesh_access_transport_add_uint32(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}

static mesh_network_pdu_t * mesh_access_network_init(uint32_t opcode){
    mesh_network_pdu_t * pdu = mesh_network_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(&pdu->data[10], opcode) + 10;
    return pdu;
}

static void mesh_access_network_add_uint8(mesh_network_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

static void mesh_access_network_add_uint16(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

static void mesh_access_network_add_uint24(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

static void mesh_access_network_add_uint32(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}

static void mesh_access_network_add_model_identifier(mesh_network_pdu_t * pdu, uint32_t model_identifier){
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        mesh_access_network_add_uint16( pdu, mesh_model_get_model_id(model_identifier) );
    } else {
        mesh_access_network_add_uint32( pdu, model_identifier );
    }
}

// access message template
#include <stdarg.h>

static mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *template, ...){
    mesh_network_pdu_t * network_pdu = mesh_access_network_init(template->opcode);
    if (!network_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint8( network_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint16( network_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint24( network_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint32( network_pdu, longword);
                break;
            case 'm':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_model_identifier( network_pdu, longword);
                break;
            default:
                log_error("Unsupported mesh message format specifier '%c", *format);
                break;
        }
        format++;
    }

    va_end(argptr);

    return network_pdu;
}

static mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *template, ...){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(template->opcode);
    if (!transport_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format++){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint8( transport_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint16( transport_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint24( transport_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint32( transport_pdu, longword);
                break;
            default:
                break;
        }
    }

    va_end(argptr);

    return transport_pdu;
}

// to sort

// move to btstack_config.h
#define MAX_NR_MESH_APPKEYS_PER_MODEL           3u
#define MAX_NR_MESH_SUBSCRIPTION_PER_MODEL      3u
#define MESH_APPKEY_INVALID                     0xffffu
#define MESH_SIG_MODEL_ID_CONFIGURATION_SERVER  0x0000u
#define MESH_SIG_MODEL_ID_CONFIGURATION_CLIENT  0x0001u
#define MESH_SIG_MODEL_ID_HEALTH_SERVER         0x0002u
#define MESH_SIG_MODEL_ID_HEALTH_CLIENT         0x0003u
#define MESH_SIG_MODEL_ID_GENERIC_ON_OFF_SERVER 0x1000u
#define MESH_SIG_MODEL_ID_GENERIC_ON_OFF_CLIENT 0x1001u
#define MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER   0x0000u

typedef enum {
    MESH_NODE_IDENTITY_STATE_ADVERTISING_STOPPED = 0,
    MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING,
    MESH_NODE_IDENTITY_STATE_ADVERTISING_NOT_SUPPORTED
} mesh_node_identity_state_t;


static btstack_crypto_aes128_cmac_t configuration_server_cmac_request;

static mesh_pdu_t * access_pdu_in_process;

static mesh_transport_key_t new_app_key;

typedef struct  {
    btstack_timer_source_t timer;
    uint16_t destination;
    uint16_t count;
    uint8_t  period_log;
    uint8_t  ttl;
    uint16_t features;
    uint16_t netkey_index;
} mesh_heartbeat_publication_t;

typedef struct {
    mesh_heartbeat_publication_t heartbeat_publication;
} mesh_configuration_server_model_context;

typedef struct {
    uint16_t address;
    uint16_t appkey_index;
    uint8_t  friendship_credential_flag;
    uint8_t  period;
    uint8_t  ttl;
    uint8_t  retransmit;
} mesh_publication_model_t;

typedef struct {
    // linked list item
    btstack_linked_item_t item;

    // vendor_id << 16 | model id, use BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC for SIG models
    uint32_t model_identifier;

    // model operations

    // publication model if supported
    mesh_publication_model_t * publication_model;

    // data
    void * model_data;

    // bound appkeys
    uint16_t appkey_indices[MAX_NR_MESH_APPKEYS_PER_MODEL];

    // subscription list
    uint16_t subscriptions[MAX_NR_MESH_SUBSCRIPTION_PER_MODEL];
} mesh_model_t;

typedef struct {
    // linked list item
    btstack_linked_item_t item;
    
    // unicast address
    uint16_t unicast_address;
    
    // LOC
    uint16_t loc;
    
    // models
    btstack_linked_list_t models;
    uint16_t models_count_sig;
    uint16_t models_count_vendor;

} mesh_element_t;

typedef struct {
    btstack_linked_list_iterator_t it;
} mesh_model_iterator_t;

static btstack_linked_list_t elements;

static mesh_heartbeat_publication_t mesh_heartbeat_publication;

static void mesh_model_reset_appkeys(mesh_model_t * mesh_model){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
    }
}

static uint8_t mesh_model_bind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) return MESH_FOUNDATION_STATUS_SUCCESS;
    }
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == MESH_APPKEY_INVALID) {
            mesh_model->appkey_indices[i] = appkey_index;
            return MESH_FOUNDATION_STATUS_SUCCESS;
        }
    }
    return MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES;
}

static void mesh_model_unbind_appkey(mesh_model_t * mesh_model, uint16_t appkey_index){
    int i;
    for (i=0;i<MAX_NR_MESH_APPKEYS_PER_MODEL;i++){
        if (mesh_model->appkey_indices[i] == appkey_index) {
            mesh_model->appkey_indices[i] = MESH_APPKEY_INVALID;
        }
    }
}

static void mesh_model_reset_subscriptions(mesh_model_t * mesh_model){
    int i;
    for (i=0;i<MAX_NR_MESH_SUBSCRIPTION_PER_MODEL;i++){
        mesh_model->subscriptions[i] = MESH_ADDRESS_UNSASSIGNED;
    }
}

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

static int mesh_model_is_configuration_server(uint32_t model_identifier){
    return mesh_model_is_bluetooth_sig(model_identifier) && (mesh_model_get_model_id(model_identifier) == MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
}

static mesh_element_t   primary_element;
static btstack_linked_list_t mesh_elements;

static mesh_element_t * mesh_primary_element(void){
    return &primary_element;
}

static void mesh_element_add(mesh_element_t * element){
    btstack_linked_list_add_tail(&mesh_elements, (void*) element);
}

static mesh_element_t * mesh_element_for_unicast_address(uint16_t unicast_address){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);
        if (element->unicast_address != unicast_address) continue;
        return element;
    }
    return NULL;
}

static void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model){
    if (mesh_model_is_bluetooth_sig(mesh_model->model_identifier)){
        element->models_count_sig++;
    } else {
        element->models_count_vendor++;
    }
    btstack_linked_list_add_tail(&element->models, (btstack_linked_item_t *) mesh_model);
}

static void mesh_model_iterator_init(mesh_model_iterator_t * iterator, mesh_element_t * element){
    btstack_linked_list_iterator_init(&iterator->it, &element->models);
}

static int mesh_model_iterator_has_next(mesh_model_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

static mesh_model_t * mesh_model_iterator_get_next(mesh_model_iterator_t * iterator){
    return (mesh_model_t *) btstack_linked_list_iterator_next(&iterator->it);
}

static mesh_model_t * mesh_model_get_by_identifier(mesh_element_t * element, uint32_t model_identifier){
    mesh_model_iterator_t it;
    mesh_model_iterator_init(&it, element);
    while (mesh_model_iterator_has_next(&it)){
        mesh_model_t * model = mesh_model_iterator_get_next(&it);
        if (model->model_identifier != model_identifier) continue;
        return model;
    }
    return NULL;
}

static void mesh_access_message_processed(mesh_pdu_t * pdu){
    mesh_upper_transport_message_processed_by_higher_layer(pdu);
}

static void config_server_send_message(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest,
                                                 mesh_pdu_t *pdu){
    UNUSED(mesh_model);
    // TODO: use addr from element this model belongs to
    uint16_t src  = primary_element_address;
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

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);

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
            mesh_model_t * model = mesh_model_iterator_get_next(&it);
            if (!mesh_model_is_bluetooth_sig(model->model_identifier)) continue;
            mesh_access_transport_add_uint16(transport_pdu, model->model_identifier);
        }
        // Vendor Models
        mesh_model_iterator_init(&it, element);
        while (mesh_model_iterator_has_next(&it)){
            mesh_model_t * model = mesh_model_iterator_get_next(&it);
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

static void config_nekey_list_set_max(uint16_t max){
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
                mesh_transport_key_remove(transport_key);
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
    mesh_store_app_key(transport_key->appkey_index, transport_key->aid, transport_key->key);

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

    // create app key
    mesh_transport_key_t * app_key = btstack_memory_mesh_transport_key_get();
    if (app_key == NULL) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES);
        mesh_access_message_processed(pdu);
        return;
    }

    // store data
    app_key->appkey_index = appkey_index;
    app_key->netkey_index = netkey_index;
    memcpy(app_key->key, appkey, 16);

    // calculate AID
    access_pdu_in_process = pdu;
    mesh_transport_key_calc_aid(&mesh_cmac_request, app_key, config_appkey_add_or_udpate_aid, app_key);
}

static void config_appkey_update_aid(void * arg){
    mesh_transport_key_t * transport_key = (mesh_transport_key_t *) arg;

    printf("Config Appkey Update: NetKey Index 0x%04x, AppKey Index 0x%04x, AID %02x: ", transport_key->netkey_index, transport_key->appkey_index, transport_key->aid);
    printf_hexdump(transport_key->key, 16);

    // store in TLV
    mesh_store_app_key(transport_key->appkey_index, transport_key->aid, transport_key->key);

    uint32_t netkey_and_appkey_index = (transport_key->appkey_index << 12) | transport_key->netkey_index;
    config_appkey_status(NULL,  mesh_pdu_netkey_index(access_pdu_in_process), mesh_pdu_src(access_pdu_in_process), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_SUCCESS);

    mesh_access_message_processed(access_pdu_in_process);
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

    // create app key
    mesh_transport_key_t * app_key = btstack_memory_mesh_transport_key_get();
    if (app_key == NULL) {
        config_appkey_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), netkey_and_appkey_index, MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES);
        mesh_access_message_processed(pdu);
        return;
    }

    // store data
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
        mesh_transport_key_remove(transport_key);
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

static void config_model_subscription_status(mesh_model_t * mesh_model, uint16_t netkey_index, uint16_t dest, uint8_t status, uint16_t element_address, uint16_t address, uint32_t model_identifier){
    // setup message
    mesh_transport_pdu_t * transport_pdu = mesh_access_setup_segmented_message(&mesh_foundation_config_model_subscription_status,
                                                                                status, element_address, address, model_identifier);
    if (!transport_pdu) return;
    // send as segmented access pdu
    config_server_send_message(mesh_model, netkey_index, dest, (mesh_pdu_t *) transport_pdu);
}

static mesh_model_t * mesh_access_model_for_address_and_model_identifier(uint16_t element_address, uint32_t model_identifier, uint8_t * status){
    if (element_address != primary_element_address){
        *status = MESH_FOUNDATION_STATUS_INVALID_ADDRESS;
        return NULL;
    }
    mesh_element_t * element = mesh_primary_element();
    mesh_model_t * model = mesh_model_get_by_identifier(element, model_identifier);
    if (model == NULL) {
        *status = MESH_FOUNDATION_STATUS_INVALID_MODEL;
    } else {
        *status = MESH_FOUNDATION_STATUS_SUCCESS;
    }
    return model;
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
        status = mesh_model_add_subscription(target_model, address);
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, address, model_identifier);
    mesh_access_message_processed(pdu);
}

static void
config_model_subscription_virtual_address_add_handler(mesh_model_t *mesh_model, mesh_pdu_t * pdu) {
    // TODO: not implemented yet
    mesh_access_message_processed(pdu);
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
        mesh_model_delete_subscription(target_model, address);
    }   

    config_model_subscription_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, element_address, address, model_identifier);
    mesh_access_message_processed(pdu);
}

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
            &mesh_foundation_config_model_app_status, status, primary_element_address, publication_model->address,
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

    config_model_publication_status(mesh_model, mesh_pdu_netkey_index(pdu), mesh_pdu_src(pdu), status, model_identifier, &publication_model);
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
    if (mesh_heartbeat_publication.count == 0) return;

    uint32_t time_ms = heartbeat_pwr2(mesh_heartbeat_publication.period_log) * 1000;
    printf("CONFIG_SERVER_HEARTBEAT: Emit (dest %04x, count %u, period %u ms, seq %x)\n", mesh_heartbeat_publication.destination, mesh_heartbeat_publication.count, time_ms, mesh_lower_transport_peek_seq());
    mesh_heartbeat_publication.count--;

    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (network_pdu){
        uint8_t data[3];
        data[0] = mesh_heartbeat_publication.ttl;
        big_endian_store_16(data, 1, mesh_heartbeat_publication.features);
        mesh_upper_transport_setup_control_pdu((mesh_pdu_t *) network_pdu, mesh_heartbeat_publication.netkey_index,
                mesh_heartbeat_publication.ttl, primary_element_address, mesh_heartbeat_publication.destination,
                MESH_TRANSPORT_OPCODE_HEARTBEAT, data, sizeof(data));
        mesh_upper_transport_send_control_pdu((mesh_pdu_t *) network_pdu);
    }
    btstack_run_loop_set_timer(ts, time_ms);
    btstack_run_loop_add_timer(ts);
}

static void config_heartbeat_publication_status(mesh_model_t *mesh_model, uint16_t netkey_index, uint16_t dest){

    // setup message
    uint8_t status = 0;
    uint8_t count_log = heartbeat_count_log(mesh_heartbeat_publication.count);
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
    printf("MESH config_heartbeat_publication_status count = %u => count_log = %u\n", mesh_heartbeat_publication.count, count_log);

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
    mesh_heartbeat_publication.count = heartbeat_pwr2(mesh_access_parser_get_u8(&parser));
    //  Period for sending Heartbeat messages
    mesh_heartbeat_publication.period_log = mesh_access_parser_get_u8(&parser);
    //  TTL to be used when sending Heartbeat messages
    mesh_heartbeat_publication.ttl = mesh_access_parser_get_u8(&parser);
    // Bit field indicating features that trigger Heartbeat messages when changed
    mesh_heartbeat_publication.features = mesh_access_parser_get_u16(&parser) & MESH_HEARTBEAT_FEATURES_SUPPORTED_MASK;
    // NetKey Index
    mesh_heartbeat_publication.netkey_index = mesh_access_parser_get_u16(&parser);

    printf("MESH config_heartbeat_publication_set, destination %x, count = %x, period = %u s\n",
            mesh_heartbeat_publication.destination, mesh_heartbeat_publication.count, heartbeat_pwr2(mesh_heartbeat_publication.period_log));

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

typedef void (*mesh_operation_handler)(mesh_model_t * mesh_model, mesh_pdu_t * pdu);

typedef struct {
    uint32_t opcode;
    uint16_t minimum_length;
    mesh_operation_handler handler;
} mesh_operation_t;

static mesh_operation_t mesh_configuration_server_model_operations[] = {
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
//    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE,   20, config_model_subscription_virtual_address_delete_handler },
//    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE,                 6, config_model_subscription_overwrite_handler },
//    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE,20, config_model_subscription_virtual_address_overwrite_handler },
//    { MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL,                4, config_model_subscription_delete_all_handler },
//    { MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET,                   4, config_sig_model_subscription_get_handler },
//    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET,                6, config_vendor_model_subscription_get_handler },
    { MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_GET,                            4, config_sig_model_app_get_handler },
    { MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_GET,                         6, config_vendor_model_app_get_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET,                       11, config_model_publication_set_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET,       25, config_model_publication_virtual_address_set_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET,                        4, config_model_publication_get_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_APP_BIND,                               6, config_model_app_bind_handler },
    { MESH_FOUNDATION_OPERATION_MODEL_APP_UNBIND,                             6, config_model_app_unbind_handler },
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_GET,                    0, config_heartbeat_publication_get_handler },
    { MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_SET,                    9, config_heartbeat_publication_set_handler },
//    { MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_GET,                   0, config_heartbeat_subscription_get_handler},
//    { MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_SET,                   5, config_heartbeat_subscription_set_handler},
    { MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_GET,                        2, config_key_refresh_phase_get_handler },
    { MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_SET,                        3, config_key_refresh_phase_set_handler },
    { MESH_FOUNDATION_OPERATION_NODE_RESET,                                   0, config_node_reset_handler },
    { MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_GET,              2, config_low_power_node_poll_timeout_get_handler },
    { MESH_FOUNDATION_OPERATION_NODE_IDENTITY_GET,                            2, config_node_identity_get_handler },
    { MESH_FOUNDATION_OPERATION_NODE_IDENTITY_SET,                            3, config_node_identity_set_handler },
    { 0, 0, NULL }
};

static void mesh_access_message_process_handler(mesh_pdu_t * pdu){
    // get opcode and size
    uint32_t opcode = 0;
    uint16_t opcode_size = 0;

    int ok = mesh_access_pdu_get_opcode( pdu, &opcode, &opcode_size);
    if (!ok) {
        mesh_access_message_processed(pdu);
        return;
    }

    uint16_t len = mesh_pdu_len(pdu);
    printf("MESH Access Message, Opcode = %x: ", opcode);
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            printf_hexdump(&((mesh_network_pdu_t *) pdu)->data[10], len);
            break;
        case MESH_PDU_TYPE_TRANSPORT:
            printf_hexdump(((mesh_transport_pdu_t *) pdu)->data, len);
            break;
        default:
            break;
    }

    // TODO: get element for unicast address or 'find' element for group / virtual addresses

    // TODO: support other models
    
    // find opcode in table
    mesh_model_t * model = mesh_model_get_by_identifier(mesh_primary_element(), MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
    mesh_operation_t * operation;
    for (operation = mesh_configuration_server_model_operations; operation->handler != NULL ; operation++){
        if (operation->opcode != opcode) continue;
        if ((opcode_size + operation->minimum_length) > len) continue;
        operation->handler(model, pdu);
        return;
    }

    // operation not found -> done
    printf("Message not handled\n");
    mesh_access_message_processed(pdu);
}

static btstack_crypto_aes128_cmac_t salt_request;
static uint8_t label_uuid[16];
static uint8_t salt_hash[16];
static uint16_t virtual_address_hash;
static mesh_network_key_t test_network_key;

static void salt_complete(void * arg){
    int i;
    printf("uint8_t salt[16] = { ");
    for (i=0;i<16;i++){
        printf("0x%02x, ", salt_hash[i]);
    }
    printf("};\n");
}

static uint8_t mesh_salt_vtad[] = { 0xce, 0xf7, 0xfa, 0x9d, 0xc4, 0x7b, 0xaf, 0x5d, 0xaa, 0xee, 0xd1, 0x94, 0x06, 0x09, 0x4f, 0x37, };

static void virtual_address_complete(void * arg){
    printf("Label UUID: ");
    printf_hexdump(label_uuid, 16);
    printf("Virtual Address %04x\n", virtual_address_hash);
}

static void key_derived(void * arg){
}

static void mesh_proxy_packet_handler_beacon(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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

static void mesh_proxy_packet_handler_network_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            printf("mesh: Received network PDU (proxy)\n");
            printf_hexdump(packet, size);
            mesh_network_received_message(packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_CAN_SEND_NOW:  
                            mesh_gatt_handle_event(packet_type, channel, packet, size);
                            break;
                        case MESH_SUBEVENT_MESSAGE_SENT:
                            mesh_gatt_handle_event(packet_type, channel, packet, size);
                            break;
                        case MESH_SUBEVENT_PROXY_CONNECTED:
                            printf("mesh: MESH_PROXY_CONNECTED\n");
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

static mesh_network_pdu_t * encrypted_proxy_configuration_ready_to_send;

static void request_can_send_now_proxy_configuration_callback_handler(mesh_network_pdu_t * network_pdu){
    encrypted_proxy_configuration_ready_to_send = network_pdu;
    gatt_bearer_request_can_send_now_for_mesh_proxy_configuration();
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
                            printf("MESH_SUBEVENT_CAN_SEND_NOW packet_handler_for_mesh_proxy_configuration len %d\n", encrypted_proxy_configuration_ready_to_send->len);
                            printf_hexdump(encrypted_proxy_configuration_ready_to_send->data, encrypted_proxy_configuration_ready_to_send->len);
                            gatt_bearer_send_mesh_proxy_configuration(encrypted_proxy_configuration_ready_to_send->data, encrypted_proxy_configuration_ready_to_send->len); 
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

// Used to answer configutation request
static uint16_t proxy_configuration_filter_list_len;
static mesh_proxy_configuration_filter_type_t proxy_configuration_filter_type;
static uint16_t primary_element_address;

void proxy_configuration_message_handler(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * received_network_pdu){
    mesh_proxy_configuration_message_opcode_t opcode;
    uint8_t data[4];
    mesh_network_pdu_t * network_pdu;
    uint8_t * network_pdu_data;
    uint8_t   network_pdu_len;

    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("proxy_configuration_message_handler: MESH_PROXY_PDU_RECEIVED\n");
            network_pdu_len = mesh_network_pdu_len(received_network_pdu);
            network_pdu_data = mesh_network_pdu_data(received_network_pdu);
            // printf_hexdump(network_pdu_data, network_pdu_len);
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
                    uint32_t seq          = mesh_lower_transport_next_seq();
                    uint8_t  nid          = mesh_network_nid(received_network_pdu);
                    uint16_t netkey_index = received_network_pdu->netkey_index; 
                    printf("netkey index 0x%02x\n", netkey_index);

                    network_pdu = btstack_memory_mesh_network_pdu_get();
                    int pos = 0;
                    data[pos++] = MESH_PROXY_CONFIGURATION_MESSAGE_OPCODE_FILTER_STATUS;
                    data[pos++] = proxy_configuration_filter_type;
                    big_endian_store_16(data, pos, proxy_configuration_filter_list_len);

                    mesh_network_setup_pdu(network_pdu, netkey_index, nid, ctl, ttl, seq, src, dest, data, sizeof(data));
                    mesh_network_encrypt_proxy_message(network_pdu, &request_can_send_now_proxy_configuration_callback_handler);
                    
                    // received_network_pdu is processed
                    btstack_memory_mesh_network_pdu_free(received_network_pdu);
                    break;
                }
                default:
                    printf("proxy config not implemented, opcode %d\n", opcode);
                    break;
            }
            break;
        case MESH_NETWORK_PDU_SENT:
            // printf("test MESH_PROXY_PDU_SENT\n");
            // mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_SENT, network_pdu);
            break;
        default:
            break;
    }
}

// Test configuration

static mesh_model_t                 mesh_configuration_server_model;
static mesh_model_t                 mesh_health_server_model;
static mesh_model_t                 mesh_vendor_model;

int btstack_main(void);
int btstack_main(void)
{
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // console
    btstack_stdin_setup(stdin_process);

    // crypto
    btstack_crypto_init();

    // l2cap
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // 
    sm_init();

    // mesh
    adv_bearer_init();

    // setup connectable advertisments
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint8_t adv_type = 0;   // AFV_IND
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    adv_bearer_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    

    // Setup GATT bearer
    gatt_bearer_init();
    gatt_bearer_register_for_mesh_network_pdu(&mesh_proxy_packet_handler_network_pdu);
    gatt_bearer_register_for_mesh_beacon(&mesh_proxy_packet_handler_beacon);

    gatt_bearer_register_for_mesh_proxy_configuration(&packet_handler_for_mesh_proxy_configuration);
    mesh_network_set_proxy_message_handler(proxy_configuration_message_handler);

#ifdef ENABLE_MESH_ADV_BEARER
    // Setup Unprovisioned Device Beacon
    beacon_init();
    beacon_register_for_unprovisioned_device_beacons(&mesh_unprovisioned_beacon_handler);
#endif

    // Provisioning in device role
    provisioning_device_init(device_uuid);
    provisioning_device_register_packet_handler(&mesh_provisioning_message_handler);

    // Network layer
    mesh_network_init();

    // Transport layers (lower + upper))
    mesh_transport_init();
    mesh_upper_transport_register_access_message_handler(&mesh_access_message_process_handler);

    // PTS Virtual Address Label UUID - without Config Model, PTS uses our device uuid
    btstack_parse_hex("001BDC0810210B0E0A0C000B0E0A0C00", 16, label_uuid);
    pts_proxy_dst = mesh_virtual_address_register(label_uuid, 0x9779);
    
    // Access layer - add Primary Element to list of elements - should be hid in e.g mesh_access_init()
    mesh_element_add(mesh_primary_element());

    // Setup Primary Element
    mesh_element_t * primary_element = mesh_primary_element();

    // Loc - bottom - https://www.bluetooth.com/specifications/assigned-numbers/gatt-namespace-descriptors
    primary_element->loc = 0x0103;

    // Setup models
    mesh_configuration_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
    mesh_model_reset_appkeys(&mesh_configuration_server_model);
    mesh_element_add_model(primary_element, &mesh_configuration_server_model);

    mesh_health_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER);
    mesh_model_reset_appkeys(&mesh_health_server_model);
    mesh_element_add_model(primary_element, &mesh_health_server_model);

    mesh_vendor_model.model_identifier = mesh_model_get_model_identifier(BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER);
    mesh_model_reset_appkeys(&mesh_vendor_model);
    mesh_element_add_model(primary_element, &mesh_vendor_model);
    
    // calc s1('vtad')7
    // btstack_crypto_aes128_cmac_zero(&salt_request, 4, (const uint8_t *) "vtad", salt_hash, salt_complete, NULL);

    // calc virtual address hash
    // mesh_virtual_address(&salt_request, label_uuid, &virtual_address_hash, virtual_address_complete, NULL);

    // calc network key derivative
    // btstack_parse_hex("7dd7364cd842ad18c17c2b820c84c3d6", 16, test_network_key.net_key); // spec sample data
    // btstack_parse_hex("B72892443F28F28FD61F4B63FF86F695", 16, test_network_key.net_key);
    // printf("NetKey: ");
    // printf_hexdump(test_network_key.net_key, 16);
    // mesh_network_key_derive(&salt_request, &test_network_key, key_derived, NULL);

    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

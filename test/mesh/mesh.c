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
#include "mesh/adv_bearer.h"
#include "mesh/gatt_bearer.h"
#include "mesh/beacon.h"
#include "mesh/mesh_crypto.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/pb_adv.h"
#include "mesh/pb_gatt.h"
#include "ble/gatt-service/mesh_provisioning_service_server.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "mesh_foundation.h"
#include "mesh_configuration_server.h"
#include "mesh_generic_server.h"
#include "mesh_access.h"
#include "mesh_virtual_addresses.h"
#include "mesh_peer.h"
#include "mesh_proxy.h"
#include "mesh.h"
#include "btstack.h"
#include "btstack_tlv.h"

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
static uint8_t network_id[8];
static uint16_t primary_element_address;

static int provisioned;

// Test configuration

#define MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER   0x0000u

static mesh_configuration_server_model_context_t mesh_configuration_server_model_context;

static mesh_model_t                 mesh_configuration_server_model;
static mesh_model_t                 mesh_health_server_model;
static mesh_model_t                 mesh_vendor_model;

static mesh_model_t                 mesh_generic_on_off_server_model;
static mesh_generic_on_off_state_t  mesh_generic_on_off_state;

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
     printf("%-20s ", name);
     printf_hexdump(data, len);
}

// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }

static void mesh_provisioning_dump(const mesh_provisioning_data_t * data){
    printf("UnicastAddr:   0x%02x\n", data->unicast_address);
    printf("IV Index:      0x%08x\n", data->iv_index);
    printf("DevKey:        "); printf_hexdump(data->device_key, 16);
    printf("NetKey:        "); printf_hexdump(data->net_key, 16);
    printf("-- Derived from NetKey --\n");
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

    // internal index 0 for primary network key
    network_key->internal_index = 0;

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
    mesh_access_set_primary_element_address(provisioning_data->unicast_address);
    primary_element_address = provisioning_data->unicast_address;
    // set iv_index
    mesh_set_iv_index(provisioning_data->iv_index);
    // set device_key
    mesh_transport_set_device_key(provisioning_data->device_key);
    // copy beacon key and network id
    // memcpy(identity_key, provisioning_data->identity_key, 16);
    memcpy(beacon_key, provisioning_data->beacon_key, 16);
    memcpy(network_id, provisioning_data->network_id, 8);
    // for secure beacon
    mesh_flags = provisioning_data->flags;
    // dump data
    mesh_provisioning_dump(provisioning_data);
    
    // Mesh Proxy
#ifdef ENABLE_MESH_PROXY_SERVER
    // Setup Proxy
    mesh_proxy_init(provisioning_data->unicast_address);

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

static void printf_hex(const uint8_t * data, uint16_t len){
    while (len){
        printf("%02x", *data);
        data++;
        len--;
    }
    printf("\n");
}

static void mesh_pts_dump_mesh_options(void){
    printf("\nMeshOptions.ini\n");

    printf("[mesh]\n");

    printf("{IVindex}\n");
    printf("%08x\n", mesh_get_iv_index());

    mesh_network_key_t * network_key = mesh_network_key_list_get(0);
    if (network_key){
        printf("{NetKey}\n");
        printf_hex(network_key->net_key, 16);
    }

    mesh_transport_key_t * transport_key = mesh_transport_key_get(0);
    if (transport_key){
        printf("{AppKey}\n");
        printf_hex(transport_key->key, 16);
    }

    mesh_transport_key_t * device_key = mesh_transport_key_get(MESH_DEVICE_KEY_INDEX);
    if (device_key){
        printf("{DevKey}\n");
        printf_hex(device_key->key, 16);
    }
    printf("\n");
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

                    // dump PTS MeshOptions.ini
                    mesh_pts_dump_mesh_options();

#if defined(ENABLE_MESH_ADV_BEARER) || defined(ENABLE_MESH_PB_ADV)

                    // start sending Secure Network Beacon
                    mesh_network_key_t * network_key = mesh_network_key_list_get(0);
                    if (network_key){
                        beacon_secure_network_start(network_key);
                    }

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

                    // delete old app keys
                    mesh_delete_app_keys();
                    
                    // delete old model to appkey bindings
                    mesh_delete_appkey_lists();
                    
                    // store in TLV
                    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));

                    // setup after provisioned
                    mesh_setup_from_provisioning_data(&provisioning_data);

                    // start advertising with node id after provisioning
                    mesh_proxy_set_advertising_with_node_id(network_key->netkey_index, MESH_NODE_IDENTITY_STATE_ADVERTISING_RUNNING);

                    // start sending Secure Network Beacons
                    beacon_secure_network_start(network_key);

                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void mesh_state_update_message_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
   
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_STATE_UPDATE_BOOL:
                    printf("State update: model identifier 0x%08x, state identifier 0x%08x, reason %u, state %u\n",
                        mesh_subevent_state_update_bool_get_model_identifier(packet),
                        mesh_subevent_state_update_bool_get_state_identifier(packet),
                        mesh_subevent_state_update_bool_get_reason(packet),
                        mesh_subevent_state_update_bool_get_value(packet));
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

static mesh_transport_key_t pts_application_key;

static void load_pts_app_key(void){
    // PTS app key
    btstack_parse_hex("3216D1509884B533248541792B877F98", 16, pts_application_key.key);
    pts_application_key.aid = 0x38;
    pts_application_key.internal_index = mesh_transport_key_get_free_index();
    mesh_transport_key_add(&pts_application_key);
    printf("PTS Application Key (AID %02x): ", 0x38);
    printf_hexdump(pts_application_key.key, 16);
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

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Mesh Console at %s ---\n", bd_addr_to_str(iut_address));
    printf("0      - Send Network Message\n");
    printf("1      - Send Unsegmented Access Message\n");
    printf("2      - Send Segmented Access Message - Unicast\n");
    printf("3      - Send Segmented Access Message - Group   D000\n");
    printf("4      - Send Segmented Access Message - Virtual 9779\n");
    printf("6      - Clear Replay Protection List\n");
    printf("7      - Load PTS App key\n");
    printf("8      - Delete provisioning data\n");
    printf("p      - Enable Public Key OOB \n");
    printf("o      - Enable Output OOB \n");
    printf("i      - Input  Output OOB \n");
    printf("s      - Static Output OOB \n");
    printf("g      - Generic ON/OFF Server Toggle Value\n");
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
            mesh_node_reset();
            printf("Mesh Node Reset!\n");
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
        case 'g':
            printf("Generic ON/OFF Server Toggle Value\n");
            mesh_generic_on_off_server_set_value(&mesh_generic_on_off_server_model, 1-mesh_generic_on_off_server_get_value(&mesh_generic_on_off_server_model), 0, 0);
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

// Used to answer configuration request
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

    // Access layer
    mesh_access_init();
    // Loc - bottom - https://www.bluetooth.com/specifications/assigned-numbers/gatt-namespace-descriptors
    mesh_access_set_primary_element_location(0x103);

    // Setup models
    mesh_configuration_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_CONFIGURATION_SERVER);
    mesh_model_reset_appkeys(&mesh_configuration_server_model);
    mesh_configuration_server_model.model_data = &mesh_configuration_server_model_context;
    mesh_configuration_server_model.operations = mesh_configuration_server_get_operations();    
    mesh_element_add_model(mesh_primary_element(), &mesh_configuration_server_model);

    mesh_health_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_HEALTH_SERVER);
    mesh_model_reset_appkeys(&mesh_health_server_model);
    mesh_element_add_model(mesh_primary_element(), &mesh_health_server_model);

    mesh_generic_on_off_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_GENERIC_ON_OFF_SERVER);
    mesh_model_reset_appkeys(&mesh_generic_on_off_server_model);
    mesh_generic_on_off_server_model.operations = mesh_generic_on_off_server_get_operations();    
    mesh_element_add_model(mesh_primary_element(), &mesh_generic_on_off_server_model);
    mesh_generic_on_off_server_model.model_data = (void *) &mesh_generic_on_off_state;
    mesh_generic_on_off_server_register_packet_handler(&mesh_generic_on_off_server_model, &mesh_state_update_message_handler);

    mesh_vendor_model.model_identifier = mesh_model_get_model_identifier(BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER);
    mesh_model_reset_appkeys(&mesh_vendor_model);
    mesh_element_add_model(mesh_primary_element(), &mesh_vendor_model);
    
    // Enable PROXY
    mesh_foundation_gatt_proxy_set(1);
    
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

    // PTS Virtual Address Label UUID - without Config Model, PTS uses our device uuid
    btstack_parse_hex("001BDC0810210B0E0A0C000B0E0A0C00", 16, label_uuid);
    mesh_virtual_address_t * virtual_addresss = mesh_virtual_address_register(label_uuid, 0x9779);
    pts_proxy_dst = virtual_addresss->pseudo_dst;

    // PTS Device UUID
    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

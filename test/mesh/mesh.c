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
#include "ble/mesh/pb_adv.h"
#include "ble/mesh/beacon.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "btstack.h"
#include "btstack_tlv.h"

static void mesh_network_run(void);
static void mesh_transport_set_device_key(const uint8_t * device_key);

#define BEACON_TYPE_SECURE_NETWORK 1

const static uint8_t device_uuid[] = { 0x00, 0x1B, 0xDC, 0x08, 0x10, 0x21, 0x0B, 0x0E, 0x0A, 0x0C, 0x00, 0x0B, 0x0E, 0x0A, 0x0C, 0x00 };

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static int counter = 'a';

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

static void mesh_setup_from_provisioning_data(const mesh_provisioning_data_t * provisioning_data){
    // add to network key list
    mesh_network_key_list_add_from_provisioning_data(provisioning_data);
    // set unicast address
    mesh_network_set_primary_element_address(provisioning_data->unicast_address);
    primary_element_address = provisioning_data->unicast_address;
    // set iv_index
    mesh_set_iv_index(provisioning_data->iv_index);
    // set device_key
    mesh_transport_set_device_key(provisioning_data->device_key);
    // copy beacon key and network id
    memcpy(beacon_key, provisioning_data->beacon_key, 16);
    memcpy(network_id, provisioning_data->network_id, 8);
    // for secure beacon
    mesh_flags = provisioning_data->flags;
    // dump data
    mesh_provisioning_dump(provisioning_data);
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
                    }
                    // setup scanning
                    gap_set_scan_parameters(0, 0x300, 0x300);
                    gap_start_scan();
                    break;

                default:
                    break;
            }
            break;
    }
}

static void mesh_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    const uint8_t * adv_data;
    const uint8_t * pdu_data;
    uint8_t pdu_len;
    uint8_t adv_len;
    mesh_network_pdu_t * network_pdu;
    mesh_provisioning_data_t provisioning_data;
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_PB_TRANSPORT_LINK_OPEN:
                    printf("Provisioner link opened");
                    pb_transport_cid = mesh_pb_transport_link_open_event_get_pb_transport_cid(packet);
                    break;
                case MESH_PB_TRANSPORT_LINK_CLOSED:
                    pb_transport_cid = MESH_PB_TRANSPORT_INVALID_CID;
                    break;
                case MESH_PB_PROV_ATTENTION_TIMER:
                    printf("Attention Timer: %u\n", packet[3]);
                    break;
                case MESH_PB_PROV_INPUT_OOB_REQUEST:
                    printf("Enter passphrase: ");
                    fflush(stdout);
                    ui_chars_for_pin = 1;
                    ui_pin_offset = 0;
                    break;
                case MESH_PB_PROV_COMPLETE:
                    printf("Provisioning complete\n");
                    memcpy(provisioning_data.network_id, provisioning_device_data_get_network_id(), 8);
                    memcpy(provisioning_data.beacon_key, provisioning_device_data_get_beacon_key(), 16);
                    memcpy(provisioning_data.encryption_key, provisioning_device_data_get_encryption_key(), 16);
                    memcpy(provisioning_data.privacy_key, provisioning_device_data_get_privacy_key(), 16);
                    memcpy(provisioning_data.device_key, provisioning_device_data_get_device_key(), 16);
                    provisioning_data.iv_index = provisioning_device_data_get_iv_index();
                    provisioning_data.nid = provisioning_device_data_get_nid();
                    provisioning_data.flags = provisioning_device_data_get_flags();
                    provisioning_data.unicast_address = provisioning_device_data_get_unicast_address();
                    // store in TLV
                    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    mesh_setup_from_provisioning_data(&provisioning_data);
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
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t device_uuid[16];
    uint16_t oob;
    const uint8_t * data;
    switch(packet[0]){
        case GAP_EVENT_ADVERTISING_REPORT:
            data = gap_event_advertising_report_get_data(packet);
            memcpy(device_uuid, &packet[15], 16);
            oob = big_endian_read_16(data, 31);
            printf("received unprovisioned device beacon, oob data %x, device uuid: ", oob);
            printf_hexdump(device_uuid, 16);
            pb_adv_create_link(device_uuid);
            break;
        default:
            break;
    }
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

static uint8_t adv_prov_invite_pdu[] = { 0x00, 0x00 };
static uint8_t adv_prov_start_pdu[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00}; 
static uint8_t adv_prov_public_key_pdu[65];

static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

static uint8_t      prov_public_key_data[64];
static const char * prov_public_key_string = "F465E43FF23D3F1B9DC7DFC04DA8758184DBC966204796ECCF0D6CF5E16500CC0201D048BCBBD899EEEFC424164E33C201C2B010CA6B4D43A8A155CAD8ECB279";
static uint8_t      prov_private_key_data[32];
static const char * prov_private_key_string = "529AA0670D72CD6497502ED473502B037E8803B5C60829A5A3CAA219505530BA";

static btstack_crypto_aes128_cmac_t mesh_cmac_request;
static uint8_t mesh_secure_network_beacon[22];
static uint8_t mesh_secure_network_beacon_auth_value[16];

// application key list

typedef struct {
    uint8_t aid;
    uint8_t first;
} mesh_application_key_iterator_t;

typedef struct {
    btstack_linked_item_t item;

    // index into shared global key list
    uint16_t index;

    // app_key
    uint8_t key[16];

    uint8_t aid;
} mesh_application_key_t;

static mesh_application_key_t test_application_key;
static mesh_application_key_t mesh_transport_device_key;

void mesh_application_key_set(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key){
    test_application_key.index = appkey_index;
    test_application_key.aid   = aid;
    memcpy(test_application_key.key, application_key, 16);
}

static void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    memcpy(mesh_transport_device_key.key, device_key, 16);
}

static const mesh_application_key_t * mesh_application_key_list_get(uint16_t appkey_index){
    if (appkey_index != test_application_key.index) return NULL;
    return &test_application_key;
}


// mesh network key iterator
static void mesh_application_key_iterator_init(mesh_application_key_iterator_t * it, uint8_t aid){
    it->aid = aid;
    it->first = 1;
}

static int mesh_application_key_iterator_has_more(mesh_application_key_iterator_t * it){
    return it->first && it->aid == test_application_key.aid;
}

static const mesh_application_key_t * mesh_application_key_iterator_get_next(mesh_application_key_iterator_t * it){
    it->first = 0;
    return &test_application_key;
}


// stub lower transport

static void mesh_lower_transport_run(void);
static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu);
static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu);

static int mesh_transport_crypto_active;
static mesh_network_pdu_t   * network_pdu_in_validation;
static mesh_transport_pdu_t * transport_pdu_in_validation;
static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static mesh_application_key_iterator_t mesh_app_key_it;

// lower transport incoming
static btstack_linked_list_t lower_transport_incoming;
// upper transport segmented access messages (to validate)
static btstack_linked_list_t upper_transport_access;
// upper transport segmented control messages (to process)
static btstack_linked_list_t upper_transport_control;
// access segmented access messages (to process)
static btstack_linked_list_t access_incoming;


static void transport_unsegmented_setup_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[1] = (network_pdu->data[1] ^ 0x80) & network_pdu->data[10] & 0x80; // !CTL & ASZMIC
    memcpy(&nonce[2], &network_pdu->data[2], 7);
    big_endian_store_32(nonce, 9, mesh_get_iv_index());
}

static void transport_segmented_setup_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[1] = transport_pdu->transmic_len == 8 ? 0x80 : 0x00; // !CTL & ASZMIC
    memcpy(&nonce[2], &transport_pdu->network_header[2], 7);
    big_endian_store_32(nonce, 9, mesh_get_iv_index());
}

static void transport_unsegmented_setup_application_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[0] = 0x01;
    transport_unsegmented_setup_nonce(nonce, network_pdu);
    printf("AppNonce: ");
    printf_hexdump(nonce, 13);
}

static void transport_unsegmented_setup_device_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[0] = 0x02;
    transport_unsegmented_setup_nonce(nonce, network_pdu);
    printf("DeviceNonce: ");
    printf_hexdump(nonce, 13);
}

static void transport_segmented_setup_application_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[0] = 0x01;
    transport_segmented_setup_nonce(nonce, transport_pdu);
    printf("AppNonce: ");
    printf_hexdump(nonce, 13);
}

static void transport_segmented_setup_device_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[0] = 0x02;
    transport_segmented_setup_nonce(nonce, transport_pdu);
    printf("DeviceNonce: ");
    printf_hexdump(nonce, 13);
}

// Network PDU Getter
static uint16_t mesh_network_control(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[1] & 0x80;
}
static uint8_t mesh_network_ttl(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[1] & 0x7f;
}
static uint16_t mesh_network_src(mesh_network_pdu_t * network_pdu){
    return big_endian_read_16(network_pdu->data, 5);
}
static int mesh_network_segmented(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[9] & 0x80;
}

// Transport PDU Getter
static uint16_t mesh_transport_ctl(mesh_transport_pdu_t * transport_pdu){
    return transport_pdu->network_header[1] >> 7;
}
static uint16_t mesh_transport_ttl(mesh_transport_pdu_t * transport_pdu){
    return transport_pdu->network_header[1] & 0x7f;
}
static uint32_t mesh_transport_seq(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_24(transport_pdu->network_header, 2);
}
static uint16_t mesh_transport_src(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_16(transport_pdu->network_header, 5);
}

//


static void mesh_transport_process_unsegmented_transport_message(mesh_network_pdu_t * network_pdu){
    printf("Unsegmented transport message\n");
}

static void mesh_lower_transport_process_unsegmented_message_done(mesh_network_pdu_t * network_pdu){
    mesh_transport_crypto_active = 0;
    mesh_network_message_processed_by_higher_layer(network_pdu_in_validation);
    mesh_lower_transport_run();
}

static void mesh_lower_transport_process_segmented_message_done(mesh_transport_pdu_t * transport_pdu){
    printf("mesh_lower_transport_process_segmented_message_done\n");
    btstack_memory_mesh_transport_pdu_free(transport_pdu);
    mesh_transport_crypto_active = 0;
    mesh_lower_transport_run();
}

static void mesh_upper_transport_validate_unsegmented_message_ccm(void * arg){
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;

    uint8_t ctl_ttl     = network_pdu->data[1];
    uint8_t ctl         = ctl_ttl >> 7;

    uint8_t * lower_transport_pdu     = &network_pdu->data[9];
    int seg = lower_transport_pdu[0] >> 7;
    uint8_t trans_mic_len = 4;
    if (seg) {
        uint8_t szmic = lower_transport_pdu[1] >> 7;
        trans_mic_len = szmic ? 8 : 4;
    }

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypo_ccm_get_authentication_value(&ccm, trans_mic);
    printf("TransMIC: "); 
    printf_hexdump(trans_mic, trans_mic_len);

    uint8_t net_mic_len = ctl ? 8 : 4;

    uint8_t * upper_transport_pdu     = &network_pdu->data[10];
    uint8_t   upper_transport_pdu_len = network_pdu->len - 10 - net_mic_len;
    printf("Decryted Transport network_pdu: ");
    printf_hexdump(upper_transport_pdu, upper_transport_pdu_len - trans_mic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len - trans_mic_len], trans_mic_len) == 0){
        printf("TransMIC matches\n");

        // pass to upper layer
        printf("Pass unsegmented access message to upper transport\n");
        btstack_memory_mesh_network_pdu_free(network_pdu);
        
        printf("\n");

        // done
        mesh_lower_transport_process_unsegmented_message_done(network_pdu);
    } else {
        uint8_t afk = lower_transport_pdu[0] & 0x40;
        if (afk){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_unsegmented_message(network_pdu);
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_lower_transport_process_unsegmented_message_done(network_pdu);
        }
    }
}

static void mesh_upper_transport_validate_segmented_message_ccm(void * arg){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) arg;

    uint8_t * upper_transport_pdu     =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len =  transport_pdu->len - transport_pdu->transmic_len;
 
    printf("Decryted Transport network_pdu: ");
    printf_hexdump(upper_transport_pdu, upper_transport_pdu_len);

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypo_ccm_get_authentication_value(&ccm, trans_mic);
    printf("TransMIC: "); 
    printf_hexdump(trans_mic, transport_pdu->transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], transport_pdu->transmic_len) == 0){
        printf("TransMIC matches\n");

        // pass to upper layer
        printf("Pass segmented access message to upper transport\n");
        
        printf("\n");

        // done
        mesh_lower_transport_process_segmented_message_done(transport_pdu);
    } else {
        uint8_t akf = transport_pdu->akf_aid & 0x40;
        if (akf){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_segmented_message(transport_pdu);
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_lower_transport_process_segmented_message_done(transport_pdu);
        }
    }
}
static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu){
    uint8_t ctl_ttl     = network_pdu_in_validation->data[1];
    uint8_t ctl         = ctl_ttl >> 7;
    uint8_t net_mic_len = ctl ? 8 : 4;
    uint8_t * lower_transport_pdu     = &network_pdu_in_validation->data[9];
    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9 - net_mic_len;
    int seg = lower_transport_pdu[0] >> 7;

    const mesh_application_key_t * message_key;

    uint8_t afk = lower_transport_pdu[0] & 0x40;
    if (afk){
        // application key
        if (!mesh_application_key_iterator_has_more(&mesh_app_key_it)){
            printf("No valid application key found\n");
            mesh_lower_transport_process_unsegmented_message_done(network_pdu);
            return;
        }
        message_key = mesh_application_key_iterator_get_next(&mesh_app_key_it);
        transport_unsegmented_setup_application_nonce(application_nonce, network_pdu_in_validation);
    } else {
        // device key
        message_key = &mesh_transport_device_key;
        transport_unsegmented_setup_device_nonce(application_nonce, network_pdu_in_validation);
    }

    // store application / device key index
    printf("AppOrDevKey: ");
    printf_hexdump(message_key->key, 16);
    network_pdu->appkey_index = message_key->index; 

    // unsegmented message have TransMIC of 32 bit
    // segmenteed messages have SZMIC flag tthat indicates 64 bit TransMIC
    uint8_t trans_mic_len = 4;
    if (seg) {
        uint8_t szmic = lower_transport_pdu[1] >> 7;
        trans_mic_len = szmic ? 8 : 4;
    }

    printf("%s Access message with TransMIC len %u\n", seg ? "Segmented" : "Unsegmented", trans_mic_len);

    uint8_t * upper_transport_pdu_data = &network_pdu_in_validation->data[10];
    uint8_t   upper_transport_pdu_len  = lower_transport_pdu_len - 1 - trans_mic_len;

    // segemented messages have a 3 byte header
    if (seg){
        upper_transport_pdu_data += 3;
        upper_transport_pdu_len  -= 3;
    }

    printf("EncAccessPayload: ");
    printf_hexdump(upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    btstack_crypo_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, 0, trans_mic_len);
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data, upper_transport_pdu_data, &mesh_upper_transport_validate_unsegmented_message_ccm, network_pdu);
}

static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu){
    uint8_t * upper_transport_pdu_data =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len  =  transport_pdu->len - transport_pdu->transmic_len;

    const mesh_application_key_t * message_key;

    uint8_t akf = transport_pdu->akf_aid & 0x40;
    if (akf){
        // application key
        if (!mesh_application_key_iterator_has_more(&mesh_app_key_it)){
            printf("No valid application key found\n");
            // done
            mesh_lower_transport_process_segmented_message_done(transport_pdu);
            return;
        }
        message_key = mesh_application_key_iterator_get_next(&mesh_app_key_it);
        transport_segmented_setup_application_nonce(application_nonce, transport_pdu_in_validation);
    } else {
        // device key
        message_key = &mesh_transport_device_key;
        transport_segmented_setup_device_nonce(application_nonce, transport_pdu_in_validation);
    }

    // store application / device key index
    printf("AppOrDevKey: ");
    printf_hexdump(message_key->key, 16);
    transport_pdu->appkey_index = message_key->index; 

    printf("EncAccessPayload: ");
    printf_hexdump(upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    btstack_crypo_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, 0, transport_pdu->transmic_len);
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data, upper_transport_pdu_data, &mesh_upper_transport_validate_segmented_message_ccm, transport_pdu);
}


static void mesh_lower_transport_process_message(mesh_network_pdu_t * network_pdu){
    uint8_t ctl_ttl     = network_pdu_in_validation->data[1];
    uint8_t ctl         = ctl_ttl >> 7;

    uint8_t net_mic_len = ctl ? 8 : 4;

    // copy original pdu
    network_pdu->len = network_pdu_in_validation->len;
    memcpy(network_pdu->data, network_pdu_in_validation->data, network_pdu->len);

    // 
    uint8_t * lower_transport_pdu     = &network_pdu_in_validation->data[9];
    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9 - net_mic_len;
    printf("Lower Transport network pdu: ");
    printf_hexdump(&network_pdu_in_validation->data[9], lower_transport_pdu_len);

    uint8_t aid = lower_transport_pdu[0] & 0x3f;
    uint8_t afk = lower_transport_pdu[0] & 0x40;
    int seg = lower_transport_pdu[0] >> 7;
    printf("SEG: %u\n", seg);
    printf("AID: %02x\n", aid);

    if (afk){
        // init application key iterator if used
        mesh_application_key_iterator_init(&mesh_app_key_it, aid);
    }

    mesh_upper_transport_validate_unsegmented_message(network_pdu);
}


static void mesh_upper_transport_process_message(mesh_transport_pdu_t * transport_pdu){
    // copy original pdu
    memcpy(transport_pdu, transport_pdu_in_validation, sizeof(mesh_transport_pdu_t));

    // 
    uint8_t * upper_transport_pdu     =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len =  transport_pdu->len - transport_pdu->transmic_len;
    printf("Upper Transport pdu: ");
    printf_hexdump(upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid = upper_transport_pdu[0] & 0x3f;
    uint8_t afk = upper_transport_pdu[0] & 0x40;
    int seg = upper_transport_pdu[0] >> 7;
    printf("SEG: %u\n", seg);
    printf("AID: %02x\n", aid);

    if (afk){
        // init application key iterator if used
        mesh_application_key_iterator_init(&mesh_app_key_it, aid);
    }
    mesh_upper_transport_validate_segmented_message(transport_pdu);
}

// ack / incomplete message

static mesh_transport_pdu_t * test_transport_pdu;


static void mesh_lower_transport_setup_segemnted_acknowledge_message(uint8_t * data, uint8_t obo, uint16_t seq_zero, uint32_t block_ack){
    data[0] = 0;    // SEG = 0, Opcode = 0
    big_endian_store_16( data, 1, (obo << 15) | (seq_zero << 2) | 0);    // OBO, SeqZero, RFU
    big_endian_store_32( data, 3, block_ack);
} 

static void mesh_transport_send_ack(mesh_transport_pdu_t * transport_pdu){
    uint8_t ack_msg[7];
    uint16_t seq = mesh_transport_seq(transport_pdu);

    mesh_lower_transport_setup_segemnted_acknowledge_message(ack_msg, 0, seq & 0x1fff, transport_pdu->block_ack);

    printf("mesh_transport_send_ack with netkey_index %x: ", transport_pdu->netkey_index);
    printf_hexdump(ack_msg, sizeof(ack_msg));
    mesh_network_send(transport_pdu->netkey_index, mesh_transport_ctl(transport_pdu), mesh_transport_ttl(transport_pdu),
                      mesh_transport_seq(transport_pdu), primary_element_address, mesh_transport_src(transport_pdu), 
                      ack_msg, sizeof(ack_msg));
}

static void mesh_transport_rx_ack_timeout(btstack_timer_source_t * ts){
    printf("ACK: acknowledgement timer fired, send ACK\n");
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    transport_pdu->acknowledgement_timer_active = 0;
    mesh_transport_send_ack(transport_pdu);
}

static void mesh_transport_rx_incomplete_timeout(btstack_timer_source_t * ts){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    printf("mesh_transport_rx_incomplete_timeout - give up\n");
    // also stop ack timer
    btstack_run_loop_remove_timer(&transport_pdu->acknowledgement_timer);

    // free message
    btstack_memory_mesh_transport_pdu_free(transport_pdu);
    // 
    test_transport_pdu = NULL;
}

static void mesh_network_segmented_message_complete(mesh_transport_pdu_t * transport_pdu){
    // stop timers
    transport_pdu->acknowledgement_timer_active = 0;
    transport_pdu->incomplete_timer_active = 0;
    btstack_run_loop_remove_timer(&transport_pdu->acknowledgement_timer);
    btstack_run_loop_remove_timer(&transport_pdu->incomplete_timer);
    // send ack
    mesh_transport_send_ack(transport_pdu);
}

static void mesh_transport_start_acknowledgment_timer(mesh_transport_pdu_t * transport_pdu, uint32_t timeout, void (*callback)(btstack_timer_source_t * ts)){
    printf("ACK: start ack timer, timeout %u ms\n", (int) timeout);
    btstack_run_loop_set_timer(&transport_pdu->acknowledgement_timer, timeout);
    btstack_run_loop_set_timer_handler(&transport_pdu->acknowledgement_timer, callback);
    btstack_run_loop_set_timer_context(&transport_pdu->acknowledgement_timer, transport_pdu);
    btstack_run_loop_add_timer(&transport_pdu->acknowledgement_timer);
    transport_pdu->acknowledgement_timer_active = 1;
}

static void mesh_transport_restart_incomplete_timer(mesh_transport_pdu_t * transport_pdu, uint32_t timeout, void (*callback)(btstack_timer_source_t * ts)){
    if (transport_pdu->incomplete_timer_active){
        btstack_run_loop_remove_timer(&transport_pdu->incomplete_timer);
    }
    btstack_run_loop_set_timer(&transport_pdu->incomplete_timer, timeout);
    btstack_run_loop_set_timer_handler(&transport_pdu->incomplete_timer, callback);
    btstack_run_loop_set_timer_context(&transport_pdu->incomplete_timer, transport_pdu);
    btstack_run_loop_add_timer(&transport_pdu->incomplete_timer);
}

static mesh_transport_pdu_t * mesh_transport_pdu_for_segmented_message(mesh_network_pdu_t * network_pdu){
     // uint16_t src = mesh_network_src(next_pdu);
     if (test_transport_pdu == NULL){
        test_transport_pdu = btstack_memory_mesh_transport_pdu_get();
        // copy meta data
        memcpy(test_transport_pdu->network_header, network_pdu->data, 9);
        test_transport_pdu->netkey_index = network_pdu->netkey_index;
        test_transport_pdu->block_ack = 0;
        test_transport_pdu->acknowledgement_timer_active = 0;
    }
    // TODO validate SeqZero, reset buffer if needed
    return test_transport_pdu;
}

static void mesh_lower_transport_process_segment( mesh_transport_pdu_t * transport_pdu, mesh_network_pdu_t * network_pdu){

    // get 
    uint8_t ctl_ttl     = network_pdu->data[1];
    uint8_t ctl         = ctl_ttl >> 7;
    uint8_t net_mic_len = ctl ? 8 : 4;

    uint8_t * lower_transport_pdu =    &network_pdu->data[9];
    uint8_t   lower_transport_pdu_len = network_pdu->len - 9 - net_mic_len;

    // get akf_aid & transmic
    transport_pdu->akf_aid      = lower_transport_pdu[0];
    transport_pdu->transmic_len = lower_transport_pdu[1] & 0x80 ? 8 : 4;

    // get seg fields
    uint16_t seg_zero = ( big_endian_read_16(lower_transport_pdu, 1) >> 2) & 0x1fff;
    uint8_t  seg_o    =  ( big_endian_read_16(lower_transport_pdu, 2) >> 5) & 0x001f;
    uint8_t  seg_n    =  lower_transport_pdu[3] & 0x1f;
    uint8_t   segment_len  =  lower_transport_pdu_len - 4;
    uint8_t * segment_data = &lower_transport_pdu[4];

    printf("mesh_lower_transport_process_segment: seg zero %04x, seg_o %02x, seg_n %02x, transmic len: %u\n", seg_zero, seg_o, seg_n, transport_pdu->transmic_len * 8);
    printf("segment: ");
    printf_hexdump(segment_data, segment_len);

    // store segment
    memcpy(&transport_pdu->data[seg_o * 12], segment_data, 12);
    // mark as received
    transport_pdu->block_ack |= (1<<seg_o);
    // last segment -> store len
    if (seg_o == seg_n){
        transport_pdu->len = (seg_n * 12) + segment_len;
        printf("Assembled payload len %u\n", transport_pdu->len);
    }
    // check for complete
    int i;
    for (i=0;i<=seg_n;i++){
        if ( (transport_pdu->block_ack & (1<<i)) == 0) return;
    }

    printf("Assembled payload: ");
    printf_hexdump(transport_pdu->data, transport_pdu->len);

    // mark as done 
    mesh_network_segmented_message_complete(test_transport_pdu);

    // free
    test_transport_pdu = NULL;

    // forward to upper transport
    if (ctl){
        printf("Store Reassembled Control Message for processing\n");
        btstack_linked_list_add_tail(&upper_transport_control, (btstack_linked_item_t*) transport_pdu);
    } else {
        printf("Store Reassembled Access Message for decryption\n");
        btstack_linked_list_add_tail(&upper_transport_access, (btstack_linked_item_t*) transport_pdu);
    }
}

static void mesh_lower_transport_run(void){
    while(1){
        int done = 1;

        if (mesh_transport_crypto_active) return;

        if (!btstack_linked_list_empty(&lower_transport_incoming)){
            done = 0;
            // peek at next message
            mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_get_first_item(&lower_transport_incoming);
            // segmented?
            if (mesh_network_segmented(network_pdu)){
                mesh_transport_pdu_t * transport_pdu = mesh_transport_pdu_for_segmented_message(network_pdu);
                if (!transport_pdu) return;
                (void) btstack_linked_list_pop(&lower_transport_incoming);
                // start acknowledgment timer if inactive
                if (transport_pdu->acknowledgement_timer_active == 0){
                    // - "The acknowledgment timer shall be set to a minimum of 150 + 50 * TTL milliseconds"
                    uint32_t timeout = 150 + 50 * mesh_network_ttl(network_pdu);
                    mesh_transport_start_acknowledgment_timer(transport_pdu, timeout, &mesh_transport_rx_ack_timeout);
                }
                // restart incomplete timer
                mesh_transport_restart_incomplete_timer(transport_pdu, 10000, &mesh_transport_rx_incomplete_timeout);
                mesh_lower_transport_process_segment(transport_pdu, network_pdu);
                mesh_network_message_processed_by_higher_layer(network_pdu);
            } else {
                // control?
                if (mesh_network_control(network_pdu)){
                    // unsegmented transport message (not encrypted)
                    (void) btstack_linked_list_pop(&lower_transport_incoming);
                    mesh_transport_process_unsegmented_transport_message(network_pdu);
                    mesh_network_message_processed_by_higher_layer(network_pdu);
                } else {
                    // unsegmented access message
                    mesh_network_pdu_t * decode_pdu = btstack_memory_mesh_network_pdu_get();
                    if (!decode_pdu) return; 
                    // get encoded network pdu and start processing
                    network_pdu_in_validation = network_pdu;
                    (void) btstack_linked_list_pop(&lower_transport_incoming);
                    mesh_lower_transport_process_message(decode_pdu);
                }
            }
        }

        if (!btstack_linked_list_empty(&upper_transport_access)){
            // peek at next message
            mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_linked_list_get_first_item(&upper_transport_access);
            mesh_transport_pdu_t * decode_pdu = btstack_memory_mesh_transport_pdu_get();
            if (!decode_pdu) return; 
            // get encoded transport pdu and start processing
            transport_pdu_in_validation = transport_pdu;
            (void) btstack_linked_list_pop(&upper_transport_access);
            mesh_upper_transport_process_message(decode_pdu);
        }

        if (done) return;
    }
}

static void mesh_transport_received_mesage(mesh_network_pdu_t * network_pdu){
    // add to list and go
    btstack_linked_list_add_tail(&lower_transport_incoming, (btstack_linked_item_t *) network_pdu);
    mesh_lower_transport_run();
}


// #define TEST_MESSAGE_1
#define TEST_MESSAGE_6
// #define TEST_MESSAGE_24
// #define TEST_MESSAGE_20
// #define TEST_MESSAGE_23
// #define TEST_MESSAGE_18

static void load_provisioning_data_test_message(void){
    mesh_provisioning_data_t provisioning_data;
    provisioning_data.nid = 0x68;
    mesh_set_iv_index(0x12345678);
    btstack_parse_hex("0953fa93e7caac9638f58820220a398e", 16, provisioning_data.encryption_key);
    btstack_parse_hex("8b84eedec100067d670971dd2aa700cf", 16, provisioning_data.privacy_key);
    mesh_network_key_list_add_from_provisioning_data(&provisioning_data);

    uint8_t application_key[16];
    btstack_parse_hex("63964771734fbd76e3b40519d1d94a48", 16, application_key);
    mesh_application_key_set( 0, 0x26, application_key);

    uint8_t device_key[16];
    btstack_parse_hex("9d6dd0e96eb25dc19a40ed9914f8f03f", 16, device_key);
    mesh_transport_set_device_key(device_key);
}

static void receive_test_message(void){
    load_provisioning_data_test_message();
    uint8_t test_network_pdu_data[29];
    const char * test_network_pdu_string;
    uint8_t test_network_pdu_len;
#ifdef TEST_MESSAGE_1
    test_network_pdu_string = "68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df";
    uint8_t test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#ifdef TEST_MESSAGE_6
    test_network_pdu_string = "68cab5c5348a230afba8c63d4e686364979deaf4fd40961145939cda0e";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#if 1
    test_network_pdu_string = "681615b5dd4a846cae0c032bf0746f44f1b8cc8ce5edc57e55beed49c0";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#endif
#ifdef TEST_MESSAGE_18
    // test values - message #23
    test_network_pdu_string = "6848cba437860e5673728a627fb938535508e21a6baf57";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#ifdef TEST_MESSAGE_20
    // test values - message #20
    // correctly decoded incl transmic. it's an unsegmented access message (header 0x66)
    mesh_set_iv_index(0x12345677);
    test_network_pdu_string = "e85cca51e2e8998c3dc87344a16c787f6b08cc897c941a5368";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#ifdef TEST_MESSAGE_23
    // test values - message #23
    mesh_set_iv_index(0x12345677);
    test_network_pdu_string = "e877a48dd5fe2d7a9d696d3dd16a75489696f0b70c711b881385";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#ifdef TEST_MESSAGE_24
    // test values - message #24
    mesh_set_iv_index(0x12345677);
    test_network_pdu_string = "e834586babdef394e998b4081f5a7308ce3edbb3b06cdecd028e307f1c";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
#ifdef TEST_MESSAGE_X
    test_network_pdu_string = "6873F928228C0D4FBF888D73AAC1C3C417F3F85A76010893D1B6396B74";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
#endif
}

static void send_test_message(void){
    load_provisioning_data_test_message();
    uint8_t transport_pdu_data[16];
#ifdef TEST_MESSAGE_1
    // test values - message #1
    uint16_t src = 0x1201;
    uint16_t dst = 0xfffd;
    uint32_t seq = 0x0001;
    uint8_t  ttl = 0;
    uint8_t  ctl = 1;
    const char * message_1_transport_pdu = "034b50057e400000010000";
    uint8_t transport_pdu_len = strlen(message_1_transport_pdu) / 2;
    btstack_parse_hex(message_1_transport_pdu, transport_pdu_len, transport_pdu_data);
    mesh_network_send(0, ctl, ttl, seq, src, dst, transport_pdu_data, transport_pdu_len);
#endif
#ifdef TEST_MESSAGE_24
    // test values - message #24
    mesh_set_iv_index(0x12345677);
    uint16_t src = 0x1234;
    uint16_t dst = 0x9736;
    uint32_t seq = 0x07080d;
    uint8_t  ttl = 3;
    uint8_t  ctl = 0;
    const char * message_24_transport_pdu = "e6a03401de1547118463123e5f6a17b9";
    uint8_t transport_pdu_len = strlen(message_24_transport_pdu) / 2;
    btstack_parse_hex(message_24_transport_pdu, transport_pdu_len, transport_pdu_data);
    mesh_network_send(0, ctl, ttl, seq, src, dst, transport_pdu_data, transport_pdu_len);
#endif
#ifdef TEST_MESSAGE_X
    uint16_t src = 0x0025;
    uint16_t dst = 0x0001;
    uint32_t seq = 0x;
    uint8_t ttl = 3;
    uint8_t ctl = 0;
    memset(transport_pdu_data, 0x55, 16);
    transport_pdu_len = 16;
    mesh_network_send(0, ctl, ttl, seq, src, dst, transport_pdu_data, transport_pdu_len);
#endif
}

static void send_pts_messsage(int type){
    uint8_t transport_pdu_data[16];

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
            ttl = 10;
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
    int transport_pdu_len = 16;
    memset(transport_pdu_data, 0x55, transport_pdu_len);
    mesh_network_send(0, ctl, ttl, seq, src, dst, transport_pdu_data, transport_pdu_len);
}

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    UNUSED(arg);
    memcpy(&mesh_secure_network_beacon[14], mesh_secure_network_beacon_auth_value, 8);
    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
    adv_bearer_send_mesh_beacon(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
}

static int pts_type;

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
            send_pts_messsage(pts_type++);
            break;
        case '1':
            send_test_message();
            break;
        case '9':
            receive_test_message();
            break;
        case '2':
            printf("Creating link to device uuid: ");
            printf_hexdump(pts_device_uuid, 16);
            pb_adv_create_link(pts_device_uuid);
            break;
        case '3':
            printf("Close link\n");
            pb_adv_close_link(1, 0);
            break;
        case '4':
            printf("Send invite with attention timer = 0\n");
            pb_adv_send_pdu(pb_transport_cid, adv_prov_invite_pdu, sizeof(adv_prov_invite_pdu));
            break;
        case '5':
            printf("Send Start\n");
            pb_adv_send_pdu(pb_transport_cid, adv_prov_start_pdu, sizeof(adv_prov_start_pdu));
            break;
        case '6':
            printf("Send Public key\n");
            adv_prov_public_key_pdu[0] = 0x03;
            memset(&adv_prov_public_key_pdu[1], 0x5a, 64);
            pb_adv_send_pdu(pb_transport_cid, adv_prov_public_key_pdu, sizeof(adv_prov_public_key_pdu));
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
        default:
            printf("Command: '%c'\n", cmd);
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

    // 
    sm_init();

    // mesh
    adv_bearer_init();

    beacon_init(device_uuid, 0);
    beacon_register_for_unprovisioned_device_beacons(&mesh_unprovisioned_beacon_handler);
    
    // Provisioning in device role
    provisioning_device_init(device_uuid);
    provisioning_device_register_packet_handler(&mesh_message_handler);

    // Network layer
    mesh_network_init();
    mesh_network_set_higher_layer_handler(&mesh_transport_received_mesage);

    // PTS app key
    uint8_t application_key[16];
    const char * application_key_string = "3216D1509884B533248541792B877F98";
    btstack_parse_hex(application_key_string, 16, application_key);
    mesh_application_key_set(0, 0x38, application_key); 

    printf("Application Key: ");
    printf_hexdump(application_key, 16);

    //
    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

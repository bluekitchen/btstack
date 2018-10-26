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

#define BEACON_TYPE_SECURE_NETWORK 1

const static uint8_t device_uuid[] = { 0x00, 0x1B, 0xDC, 0x08, 0x10, 0x21, 0x0B, 0x0E, 0x0A, 0x0C, 0x00, 0x0B, 0x0E, 0x0A, 0x0C, 0x00 };

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static int counter = 'a';

static mesh_provisioning_data_t provisioning_data;
static uint8_t mesh_flags;

// pin entry
static int ui_chars_for_pin; 
static uint8_t ui_pin[17];
static int ui_pin_offset;

static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

static btstack_crypto_ccm_t    mesh_ccm_request;
static btstack_crypto_aes128_t mesh_aes128_request;

static void mesh_provisioning_dump( mesh_provisioning_data_t * data){
    printf("NID:           0x%02x\n", data->nid);
    printf("IV Index:      0x%08x\n", data->iv_index);
    printf("NetworkID:     "); printf_hexdump(data->network_id, 8);
    printf("BeaconKey:     "); printf_hexdump(data->beacon_key, 16);
    printf("EncryptionKey: "); printf_hexdump(data->encryption_key, 16);
    printf("PrivacyKey:    "); printf_hexdump(data->privacy_key, 16);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t addr;
    int i;
    int prov_len;

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
                        mesh_provisioning_dump(&provisioning_data);
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

static uint16_t network_pdu_src;
static uint16_t network_pdu_dst;
static uint32_t network_pdu_seq;
static uint8_t  network_pdu_ttl;
static uint8_t  network_pdu_ctl;

// transport pdu
static uint8_t transport_pdu_data[16];
static uint8_t transport_pdu_len;

// also used for PECB calculation 
static uint8_t encryption_block[18];
static uint8_t obfuscation_block[16];

// network pdu
static uint8_t network_pdu_data[31];  // get correct value
static uint8_t network_pdu_len;


static uint8_t network_nonce[13];

static void mesh_network_create_nonce(uint8_t * nonce, uint8_t ctl_ttl, uint32_t seq, uint16_t src, uint32_t iv_index){
    unsigned int pos = 0;
    nonce[pos++] = 0x0;      // Network Nonce
    nonce[pos++] = ctl_ttl;
    big_endian_store_24(nonce, pos, seq);
    pos += 3;
    big_endian_store_16(nonce, pos, src);
    pos += 2;
    big_endian_store_16(nonce, pos, 0);
    pos += 2;
    big_endian_store_32(nonce, pos, iv_index);
}

// NID/IVI | obfuscated (CTL/TTL, SEQ (24), SRC (16) ), encrypted ( DST(16), TransportPDU), MIC(32 or 64)

static void create_network_pdu_c(void *arg){
    UNUSED(arg);

    // obfuscate
    unsigned int i;
    for (i=0;i<6;i++){
        network_pdu_data[1+i] ^= obfuscation_block[i];
    }

    printf("NetworkPDU: ");
    printf_hexdump(network_pdu_data, network_pdu_len);

    // request to send
    adv_bearer_request_can_send_now_for_mesh_message();
}

// 
static void create_network_pdu_b(void *arg){
    UNUSED(arg);

    // store NetMIC
    uint8_t net_mic[8];
    btstack_crypo_ccm_get_authentication_value(&mesh_ccm_request, net_mic);

    // store MIC
    uint8_t net_mic_len = network_pdu_ctl ? 8 : 4;
    memcpy(&network_pdu_data[9+transport_pdu_len], net_mic, net_mic_len);

    // setup packet
    network_pdu_len = 0;
    
    // Plainn: NID / IVI
    uint32_t iv_index = provisioning_data.iv_index;
    uint8_t  nid      = provisioning_data.nid;
    network_pdu_data[network_pdu_len++] = (iv_index << 7) |  nid;
    
    // Obfuscated:
    uint8_t ctl_ttl = (network_pdu_ctl << 7) | (network_pdu_ttl & 0x7f);
    network_pdu_data[network_pdu_len++] = ctl_ttl;
    big_endian_store_24(network_pdu_data, network_pdu_len, network_pdu_seq);
    network_pdu_len += 3;
    big_endian_store_16(network_pdu_data, network_pdu_len, network_pdu_src);
    network_pdu_len += 2;

    // already encrypted
    network_pdu_len += 2 + transport_pdu_len + net_mic_len;

    // calc PECB
    memset(encryption_block, 0, 5);
    big_endian_store_32(encryption_block, 5, iv_index);
    memcpy(&encryption_block[9], &network_pdu_data[7], 7);
    btstack_crypto_aes128_encrypt(&mesh_aes128_request, provisioning_data.privacy_key, encryption_block, obfuscation_block, &create_network_pdu_c, NULL);
}

static void create_network_pdu_a(void *arg){
    btstack_crypto_ccm_encrypt_block(&mesh_ccm_request, 2 + transport_pdu_len - 16, &encryption_block[16], &network_pdu_data[7+16], &create_network_pdu_b, NULL);
}

static void create_network_pdu(void){

    // get network nonce
    uint32_t iv_index = provisioning_data.iv_index;
    uint8_t ctl_ttl = (network_pdu_ctl << 7) | (network_pdu_ttl & 0x7f);
    mesh_network_create_nonce(network_nonce, ctl_ttl, network_pdu_seq, network_pdu_src, iv_index); 

    // prepare 'DST | TransportPDU' for CCM
    big_endian_store_16(encryption_block, 0, network_pdu_dst);
    memcpy(&encryption_block[2], transport_pdu_data, transport_pdu_len);

    // start ccm
    uint8_t cypher_len = 2 + transport_pdu_len;
    btstack_crypo_ccm_init(&mesh_ccm_request, provisioning_data.encryption_key, network_nonce, cypher_len);
    if (cypher_len > 16){
        btstack_crypto_ccm_encrypt_block(&mesh_ccm_request, 16, encryption_block, &network_pdu_data[7], &create_network_pdu_a, NULL);
    } else {
        btstack_crypto_ccm_encrypt_block(&mesh_ccm_request, cypher_len, encryption_block, &network_pdu_data[7], &create_network_pdu_b, NULL);
    }
}

// provisioning data iterator

typedef struct {
    uint8_t nid;
    uint8_t first;
} provisioning_data_iterator_t;

static void provisioning_data_iterator_init(provisioning_data_iterator_t * it, uint8_t nid){
    it->nid = nid;
    it->first = 1;
}

static int provisioning_data_has_more(provisioning_data_iterator_t * it){
    return it->first && it->nid == provisioning_data.nid;
}

static const mesh_provisioning_data_t * provisioning_data_get_next(provisioning_data_iterator_t * it){
    it->first = 0;
    return &provisioning_data;
}

//

static provisioning_data_iterator_t     process_network_pdu_provisioning_data_it;
static const mesh_provisioning_data_t * process_network_pdu_prov_data;
static uint8_t                          process_network_pdu_decode_block;

static void process_network_pdu_validate(void);

static void process_network_pdu_done(void){
    printf("process_network_pdu_done\n");
}

static void process_network_pdu_validate_d(void * arg){

    uint8_t ctl_ttl  = network_pdu_data[1];
    uint8_t net_mic_len = (ctl_ttl & 0x80) ? 8 : 4;
    uint8_t cypher_len  = network_pdu_len - 9 - net_mic_len;

    printf("Transport PDU: "); 
    printf_hexdump(network_pdu_data, network_pdu_len);

    // store NetMIC
    uint8_t net_mic[8];
    btstack_crypo_ccm_get_authentication_value(&mesh_ccm_request, net_mic);
    printf("NetMIC: "); 
    printf_hexdump(net_mic, net_mic_len);

    printf("Decrypted: ");
    printf_hexdump(encryption_block, 2 + cypher_len);

    // compare nic to nic in data
    if (memcmp(net_mic, &network_pdu_data[network_pdu_len-net_mic_len], net_mic_len) == 0){
        // match
        printf("NetMIC matches\n");
        process_network_pdu_done();
    } else {
        // fail
        printf("NetMIC maismatch, try next key\n");
        process_network_pdu_validate();
    }

}

static void process_network_pdu_validate_c(void * arg){
    uint8_t ctl_ttl  = network_pdu_data[1];
    uint8_t net_mic_len = (ctl_ttl & 0x80) ? 8 : 4;
    uint8_t cypher_len  = network_pdu_len - 7 - net_mic_len;
    btstack_crypto_ccm_decrypt_block(&mesh_ccm_request, cypher_len - 16, &network_pdu_data[7+16], &encryption_block[16], &process_network_pdu_validate_d, NULL);
}

static void process_network_pdu_validate_b(void * arg){

    //
    printf("PECB: ");
    printf_hexdump(obfuscation_block, 6);

    // de-obfuscate
    unsigned int i;
    for (i=0;i<6;i++){
        network_pdu_data[1+i] ^= obfuscation_block[i];
    }

    //
    printf_hexdump(network_pdu_data, network_pdu_len);

    // parse header
    uint32_t iv_index = process_network_pdu_prov_data->iv_index;
    uint8_t  ctl_ttl  = network_pdu_data[1];
    uint32_t seq      = big_endian_read_24(network_pdu_data, 2);
    uint16_t src      = big_endian_read_16(network_pdu_data, 5);

    // get network nonce
    mesh_network_create_nonce(network_nonce, ctl_ttl, seq, src, iv_index); 
    printf("Network Nonce: ");
    printf_hexdump(network_nonce, 13);

    // 
    uint8_t net_mic_len = (ctl_ttl & 0x80) ? 8 : 4;
    uint8_t cypher_len  = network_pdu_len - 7 - net_mic_len;

    printf("Cyper len %u, mic len %u\n", cypher_len, net_mic_len);

    printf("Encryption Key: ");
    printf_hexdump(process_network_pdu_prov_data->encryption_key, 16);

    // 034b50057e400000010000

    btstack_crypo_ccm_init(&mesh_ccm_request, process_network_pdu_prov_data->encryption_key, network_nonce, cypher_len);
    if (cypher_len > 16){
        btstack_crypto_ccm_decrypt_block(&mesh_ccm_request, 16,         &network_pdu_data[7], encryption_block, &process_network_pdu_validate_c, NULL);
    } else {
        btstack_crypto_ccm_decrypt_block(&mesh_ccm_request, cypher_len, &network_pdu_data[7], encryption_block, &process_network_pdu_validate_d, NULL);
    }

}

static void process_network_pdu_validate(void){
    if (!provisioning_data_has_more(&process_network_pdu_provisioning_data_it)){
        process_network_pdu_done();
        return;
    }

    process_network_pdu_prov_data = provisioning_data_get_next(&process_network_pdu_provisioning_data_it);

    // calc PECB
    memset(encryption_block, 0, 5);
    big_endian_store_32(encryption_block, 5, process_network_pdu_prov_data->iv_index);
    memcpy(&encryption_block[9], &network_pdu_data[7], 7);
    btstack_crypto_aes128_encrypt(&mesh_aes128_request, process_network_pdu_prov_data->privacy_key, encryption_block, obfuscation_block, &process_network_pdu_validate_b, NULL);
}

static void process_network_pdu(void){
    //
    printf_hexdump(network_pdu_data, network_pdu_len);

    //
    uint8_t nid = network_pdu_data[0] & 0x7f;
    // uint8_t iv_index = network_pdu_data[0] >> 7;
    provisioning_data_iterator_init(&process_network_pdu_provisioning_data_it, nid);

    process_network_pdu_validate();
}

static void mesh_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    const uint8_t * adv_data;
    const uint8_t * pdu_data;
    uint8_t pdu_len;
    uint8_t adv_len;
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_CAN_SEND_NOW:
                    adv_bearer_send_mesh_message(&network_pdu_data[0], network_pdu_len);
                    break;
                case MESH_PB_ADV_LINK_OPEN:
                    printf("Provisioner link opened");
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
                    provisioning_data.iv_index = provisioning_device_data_get_iv_index();
                    provisioning_data.nid = provisioning_device_data_get_nid();
                    mesh_flags = provisioning_device_data_get_flags();
                    // store in TLV
                    btstack_tlv_singleton_impl->store_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    // dump
                    mesh_provisioning_dump(&provisioning_data);
                    break;
                default:
                    break;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            adv_len  = gap_event_advertising_report_get_data_length(packet);
            adv_data = gap_event_advertising_report_get_data(packet);
            // validate data item len
            pdu_len = adv_data[0] - 1;
            printf("adv len %u  pdu len %u\n", adv_len,  pdu_len);

            if ((pdu_len + 2) > adv_len) break;
            if (pdu_len < 13)            break;    // transport PDU len = 0, 32 bit NetMIC
            // get transport pdu
            pdu_data = &adv_data[2];
            printf("received mesh message: ");
            printf_hexdump(pdu_data, pdu_len);
            // temp.. 
            memcpy(network_pdu_data, pdu_data, pdu_len);
            network_pdu_len = pdu_len;
            // process
            process_network_pdu();
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

// #define TEST_MESSAGE_1
#define TEST_MESSAGE_24

static void load_provisioning_data_test_message(void){
    provisioning_data.nid = 0x68;
    provisioning_data.iv_index = 0x12345678;
    btstack_parse_hex("0953fa93e7caac9638f58820220a398e", 16, provisioning_data.encryption_key);
    btstack_parse_hex("8b84eedec100067d670971dd2aa700cf", 16, provisioning_data.privacy_key);
}

static void generate_transport_pdu(void){

    load_provisioning_data_test_message();

#ifdef TEST_MESSAGE_1
    // test values - message #1
    network_pdu_src = 0x1201;
    network_pdu_dst = 0xfffd;
    network_pdu_seq = 0x0001;
    network_pdu_ttl = 0;
    network_pdu_ctl = 1;

    const char * message_1_transport_pdu = "034b50057e400000010000";
    transport_pdu_len = strlen(message_1_transport_pdu) / 2;
    btstack_parse_hex(message_1_transport_pdu, transport_pdu_len, transport_pdu_data);
#endif

#ifdef TEST_MESSAGE_24
    // test values - message #24
    provisioning_data.iv_index = 0x12345677;

    network_pdu_src = 0x1234;
    network_pdu_dst = 0x9736;
    network_pdu_seq = 0x07080d;
    network_pdu_ttl = 3;
    network_pdu_ctl = 0;

    const char * message_24_transport_pdu = "e6a03401de1547118463123e5f6a17b9";
    transport_pdu_len = strlen(message_24_transport_pdu) / 2;
    btstack_parse_hex(message_24_transport_pdu, transport_pdu_len, transport_pdu_data);
#endif

#ifdef TEST_MESSAGE_X
    network_pdu_src = 0x0025;
    network_pdu_dst = 0x0001;
    network_pdu_seq = 0x;
    network_pdu_ttl = 3;
    network_pdu_ctl = 0;
    memset(transport_pdu_data, 0x55, 16);
    transport_pdu_len = 16;
#endif
}

static void generate_network_pdu(void){
    load_provisioning_data_test_message();
#ifdef TEST_MESSAGE_1
    const char * message_1_network_pdu = "68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df";
    network_pdu_len = strlen(message_1_network_pdu) / 2;
    btstack_parse_hex(message_1_network_pdu, network_pdu_len, network_pdu_data);
#endif
#ifdef TEST_MESSAGE_24
    // test values - message #24
    provisioning_data.iv_index = 0x12345677;
    const char * message_1_network_pdu = "E834586BABDEF394E998B4081F5A7308CE3EDBB3B06CDECD023C734EC9";
    network_pdu_len = strlen(message_1_network_pdu) / 2;
    btstack_parse_hex(message_1_network_pdu, network_pdu_len, network_pdu_data);
#endif
#ifdef TEST_MESSAGE_X
    const char * message_1_network_pdu = "6873F928228C0D4FBF888D73AAC1C3C417F3F85A76010893D1B6396B74";
    network_pdu_len = strlen(message_1_network_pdu) / 2;
    btstack_parse_hex(message_1_network_pdu, network_pdu_len, network_pdu_data);
    load_provisioning_data_test_message();
#endif
}

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    UNUSED(arg);
    memcpy(&mesh_secure_network_beacon[14], mesh_secure_network_beacon_auth_value, 8);
    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
    adv_bearer_send_mesh_beacon(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
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
        case '1':
            generate_transport_pdu();
            create_network_pdu();
            break;
        case '9':
            generate_network_pdu();
            process_network_pdu();
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
            pb_adv_send_pdu(adv_prov_invite_pdu, sizeof(adv_prov_invite_pdu));
            break;
        case '5':
            printf("Send Start\n");
            pb_adv_send_pdu(adv_prov_start_pdu, sizeof(adv_prov_start_pdu));
            break;
        case '6':
            printf("Send Public key\n");
            adv_prov_public_key_pdu[0] = 0x03;
            memset(&adv_prov_public_key_pdu[1], 0x5a, 64);
            pb_adv_send_pdu(adv_prov_public_key_pdu, sizeof(adv_prov_public_key_pdu));
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
            memcpy(&mesh_secure_network_beacon[2], provisioning_data.network_id, 8);
            big_endian_store_32(mesh_secure_network_beacon, 10, provisioning_data.iv_index);
            btstack_crypto_aes128_cmac_message(&mesh_cmac_request, provisioning_data.beacon_key, 13,
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
    adv_bearer_register_for_mesh_message(&mesh_message_handler);

    beacon_init(device_uuid, 0);
    beacon_register_for_unprovisioned_device_beacons(&mesh_unprovisioned_beacon_handler);
    
    // Provisioning in device role
    provisioning_device_init(device_uuid);
    provisioning_device_register_packet_handler(&mesh_message_handler);

    //
    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

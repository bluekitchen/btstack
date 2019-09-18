/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_network.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_util.h"

#include "mesh/beacon.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_node.h"
#include "mesh/provisioning.h"
#include "mesh/provisioning_device.h"

#ifdef ENABLE_MESH_ADV_BEARER
#include "mesh/adv_bearer.h"
#endif

#ifdef ENABLE_MESH_GATT_BEARER
#include "mesh/gatt_bearer.h"
#endif

// configuration
#define MESH_NETWORK_CACHE_SIZE 2

// debug config
// #define LOG_NETWORK

static void mesh_network_dump_network_pdus(const char * name, btstack_linked_list_t * list);

// structs

// globals

static void (*mesh_network_higher_layer_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu);
static void (*mesh_network_proxy_message_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu);

#ifdef ENABLE_MESH_GATT_BEARER
static hci_con_handle_t gatt_bearer_con_handle;
#endif

// shared send/receive crypto
static int mesh_crypto_active;

// crypto requests
static union {
    btstack_crypto_ccm_t         ccm;
    btstack_crypto_aes128_t      aes128;
} mesh_network_crypto_request;

static const mesh_network_key_t *  current_network_key;

// PECB calculation 
static uint8_t encryption_block[16];
static uint8_t obfuscation_block[16];

// Subnets
static btstack_linked_list_t subnets;

// Network Nonce
static uint8_t network_nonce[13];

// INCOMING //

// unprocessed network pdu - added by mesh_network_pdus_received_message
static btstack_linked_list_t        network_pdus_received;

// in validation
static mesh_network_pdu_t *         incoming_pdu_raw;
static mesh_network_pdu_t *         incoming_pdu_decoded;
static mesh_network_key_iterator_t  validation_network_key_it;

// OUTGOING //

// Network PDUs queued by mesh_network_send
static btstack_linked_list_t network_pdus_queued;

// Network PDU about to get send via all bearers when encrypted
static mesh_network_pdu_t * outgoing_pdu;

// Network PDUs ready to send via GATT Bearer
static btstack_linked_list_t network_pdus_outgoing_gatt;

#ifdef ENABLE_MESH_GATT_BEARER
static mesh_network_pdu_t * gatt_bearer_network_pdu;
#endif

// Network PDUs ready to send via ADV Bearer
static btstack_linked_list_t network_pdus_outgoing_adv;

#ifdef ENABLE_MESH_ADV_BEARER
static mesh_network_pdu_t * adv_bearer_network_pdu;
#endif


// mesh network cache - we use 32-bit 'hashes'
static uint32_t mesh_network_cache[MESH_NETWORK_CACHE_SIZE];
static int      mesh_network_cache_index;

// prototypes

static void mesh_network_run(void);
static void process_network_pdu_validate(void);

// network caching
static uint32_t mesh_network_cache_hash(mesh_network_pdu_t * network_pdu){
    // - The SEQ field is a 24-bit integer that when combined with the IV Index, 
    // shall be a unique value for each new Network PDU originated by this node (=> SRC)
    // - IV updates only rarely
    // => 16 bit SRC, 1 bit IVI, 15 bit SEQ
    uint8_t  ivi = network_pdu->data[0] >> 7;
    uint16_t seq = big_endian_read_16(network_pdu->data, 3);
    uint16_t src = big_endian_read_16(network_pdu->data, 5);
    return (src << 16) | (ivi << 15) | (seq & 0x7fff);
}

static int mesh_network_cache_find(uint32_t hash){
    int i;
    for (i = 0; i < MESH_NETWORK_CACHE_SIZE; i++) {
        if (mesh_network_cache[i] == hash) {
            return 1;
        }
    }
    return 0;
}

static void mesh_network_cache_add(uint32_t hash){
    mesh_network_cache[mesh_network_cache_index++] = hash;
    if (mesh_network_cache_index >= MESH_NETWORK_CACHE_SIZE){
        mesh_network_cache_index = 0;
    }
}

// common helper
int mesh_network_address_unicast(uint16_t addr){
    return addr != MESH_ADDRESS_UNSASSIGNED && (addr < 0x8000);
}

int mesh_network_address_virtual(uint16_t addr){
    return (addr & 0xC000) == 0x8000;   // 0b10xx xxxx xxxx xxxx
}

int mesh_network_address_group(uint16_t addr){
    return (addr & 0xC000) == 0xC000;   // 0b11xx xxxx xxxx xxxx
}

int mesh_network_address_all_proxies(uint16_t addr){
    return addr == MESH_ADDRESS_ALL_PROXIES;
}

int mesh_network_address_all_nodes(uint16_t addr){
    return addr == MESH_ADDRESS_ALL_NODES;
}

int mesh_network_address_all_friends(uint16_t addr){
    return addr == MESH_ADDRESS_ALL_FRIENDS;
}

int mesh_network_address_all_relays(uint16_t addr){
    return addr == MESH_ADDRESS_ALL_RELAYS;
}

int mesh_network_addresses_valid(uint8_t ctl, uint16_t src, uint16_t dst){
    // printf("CTL: %u\n", ctl);
    // printf("SRC: %04x\n", src);
    // printf("DST: %04x\n", dst);
    if (src == 0){
        // printf("SRC Unassigned Addr -> ignore\n");
        return 0;
    }
    if ((src & 0xC000) == 0x8000){
        // printf("SRC Virtual Addr -> ignore\n");
        return 0;
    }
    if ((src & 0xC000) == 0xC000){
        // printf("SRC Group Addr -> ignore\n");
        return 0;
    }
    if (dst == 0){
        // printf("DST Unassigned Addr -> ignore\n");
        return 0;
    }
    if ( ((dst & 0xC000) == 0x8000) && (ctl == 1)){
        // printf("DST Virtual Addr in CONTROL -> ignore\n");
        return 0;
    }
    if ( (0xFF00 <= dst) && (dst <= 0xfffb) && (ctl == 0) ){
        // printf("DST RFU Group Addr in MESSAGE -> ignore\n");
        return 0;
    }
    // printf("SRC + DST Addr valid\n");
    return 1;
}

static void mesh_network_create_nonce(uint8_t * nonce, const mesh_network_pdu_t * pdu, uint32_t iv_index){
    unsigned int pos = 0;
    nonce[pos++] = 0x0;      // Network Nonce
    memcpy(&nonce[pos], &pdu->data[1], 6);
    pos += 6;
    big_endian_store_16(nonce, pos, 0);
    pos += 2;
    big_endian_store_32(nonce, pos, iv_index);
}

static void mesh_proxy_create_nonce(uint8_t * nonce, const mesh_network_pdu_t * pdu, uint32_t iv_index){
    unsigned int pos = 0;
    nonce[pos++] = 0x3;      // Proxy Nonce
    nonce[pos++] = 0;
    memcpy(&nonce[pos], &pdu->data[2], 5);
    pos += 5;
    big_endian_store_16(nonce, pos, 0);
    pos += 2;
    big_endian_store_32(nonce, pos, iv_index);
}

// NID/IVI | obfuscated (CTL/TTL, SEQ (24), SRC (16) ), encrypted ( DST(16), TransportPDU), MIC(32 or 64)

static void mesh_network_send_complete(mesh_network_pdu_t * network_pdu){
    if (network_pdu->flags & MESH_NETWORK_PDU_FLAGS_RELAY){
#ifdef LOG_NETWORK
        printf("TX-F-NetworkPDU (%p): relay -> free packet\n", network_pdu);
#endif
        mesh_network_pdu_free(network_pdu);
    } else {
#ifdef LOG_NETWORK
        printf("TX-F-NetworkPDU (%p): notify lower transport\n", network_pdu);
#endif
        // notify higher layer
        (*mesh_network_higher_layer_handler)(MESH_NETWORK_PDU_SENT, network_pdu);
    }
}

static void mesh_network_send_d(mesh_network_pdu_t * network_pdu){

#ifdef LOG_NETWORK
    printf("TX-D-NetworkPDU (%p): ", network_pdu);
    printf_hexdump(network_pdu->data, network_pdu->len);
#endif

    // add to queue
    btstack_linked_list_add_tail(&network_pdus_outgoing_gatt, (btstack_linked_item_t *) network_pdu);

    // go
    mesh_network_run();
}

// new
static void mesh_network_send_c(void *arg){
    UNUSED(arg);

    // obfuscate
    unsigned int i;
    for (i=0;i<6;i++){
        outgoing_pdu->data[1+i] ^= obfuscation_block[i];
    }

#ifdef LOG_NETWORK
    printf("TX-C-NetworkPDU (%p): ", outgoing_pdu);
    printf_hexdump(outgoing_pdu->data, outgoing_pdu->len);
#endif

    // crypto done
    mesh_crypto_active = 0;

    // done
    mesh_network_pdu_t * network_pdu = outgoing_pdu;
    outgoing_pdu = NULL;
    (network_pdu->callback)(network_pdu);
}

static void mesh_network_send_b(void *arg){
    UNUSED(arg);

    uint32_t iv_index = mesh_get_iv_index_for_tx();

    // store NetMIC
    uint8_t net_mic[8];
    btstack_crypto_ccm_get_authentication_value(&mesh_network_crypto_request.ccm, net_mic);

    // store MIC
    uint8_t net_mic_len = outgoing_pdu->data[1] & 0x80 ? 8 : 4;
    memcpy(&outgoing_pdu->data[outgoing_pdu->len], net_mic, net_mic_len);
    outgoing_pdu->len += net_mic_len;

#ifdef LOG_NETWORK
    printf("TX-B-NetworkPDU (%p): ", outgoing_pdu);
    printf_hexdump(outgoing_pdu->data, outgoing_pdu->len);
#endif

    // calc PECB
    memset(encryption_block, 0, 5);
    big_endian_store_32(encryption_block, 5, iv_index);
    memcpy(&encryption_block[9], &outgoing_pdu->data[7], 7);
    btstack_crypto_aes128_encrypt(&mesh_network_crypto_request.aes128, current_network_key->privacy_key, encryption_block, obfuscation_block, &mesh_network_send_c, NULL);
}

static void mesh_network_send_a(void){

    mesh_crypto_active = 1;

    uint32_t iv_index = mesh_get_iv_index_for_tx();

    // lookup subnet by netkey_index
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(outgoing_pdu->netkey_index);
    if (!subnet) {
        mesh_crypto_active = 0;
        mesh_network_pdu_t * network_pdu = outgoing_pdu;
        outgoing_pdu = NULL;
        // notify upper layer
        mesh_network_send_complete(network_pdu);
        // run again
        mesh_network_run();
        return;
    }

    // get network key to use for sending
    current_network_key = mesh_subnet_get_outgoing_network_key(subnet);

#ifdef LOG_NETWORK
    printf("TX-A-NetworkPDU (%p): ", outgoing_pdu);
    printf_hexdump(outgoing_pdu->data, outgoing_pdu->len);
#endif

    // get network nonce
    if (outgoing_pdu->flags & MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION){
        mesh_proxy_create_nonce(network_nonce, outgoing_pdu, iv_index); 
#ifdef LOG_NETWORK
        printf("TX-ProxyNonce:  ");
        printf_hexdump(network_nonce, 13);
#endif
    } else {
        mesh_network_create_nonce(network_nonce, outgoing_pdu, iv_index); 
#ifdef LOG_NETWORK
        printf("TX-NetworkNonce:  ");
        printf_hexdump(network_nonce, 13);
#endif
    }

#ifdef LOG_NETWORK
   printf("TX-EncryptionKey: ");
    printf_hexdump(current_network_key->encryption_key, 16);
#endif

    // start ccm
    uint8_t cypher_len  = outgoing_pdu->len - 7;
    uint8_t net_mic_len = outgoing_pdu->data[1] & 0x80 ? 8 : 4;
    btstack_crypto_ccm_init(&mesh_network_crypto_request.ccm, current_network_key->encryption_key, network_nonce, cypher_len, 0, net_mic_len);
    btstack_crypto_ccm_encrypt_block(&mesh_network_crypto_request.ccm, cypher_len, &outgoing_pdu->data[7], &outgoing_pdu->data[7], &mesh_network_send_b, NULL);
}

#if defined(ENABLE_MESH_RELAY) || defined (ENABLE_MESH_PROXY_SERVER)
static void mesh_network_relay_message(mesh_network_pdu_t * network_pdu){

    uint8_t ctl_ttl     = network_pdu->data[1];
    uint8_t ctl         = ctl_ttl & 0x80;
    uint8_t ttl         = ctl_ttl & 0x7f;

#ifdef LOG_NETWORK
    printf("TX-Relay-NetworkPDU (%p): ", network_pdu);
    printf_hexdump(network_pdu->data, network_pdu->len);
    printf("^^ into network_pdus_queued\n");
#endif

    // prepare pdu for resending
    network_pdu->data[1] = (ctl << 7) | (ttl - 1);
    network_pdu->flags |= MESH_NETWORK_PDU_FLAGS_RELAY;

    // queue up
    network_pdu->callback = &mesh_network_send_d;
    btstack_linked_list_add_tail(&network_pdus_queued, (btstack_linked_item_t *) network_pdu);
}
#endif

void mesh_network_message_processed_by_higher_layer(mesh_network_pdu_t * network_pdu){

#if defined(ENABLE_MESH_RELAY) || defined (ENABLE_MESH_PROXY_SERVER)

    // check if address does not matches elements on our node and TTL >= 2
    uint16_t src     = mesh_network_src(network_pdu);
    uint8_t  ttl     = mesh_network_ttl(network_pdu);
    
    uint16_t mesh_network_primary_address = mesh_node_get_primary_element_address();

    if (((src < mesh_network_primary_address) || (src > (mesh_network_primary_address + mesh_node_element_count()))) && (ttl >= 2)){

        if ((network_pdu->flags & MESH_NETWORK_PDU_FLAGS_GATT_BEARER) == 0){

            // message received via ADV bearer are relayed:

#ifdef ENABLE_MESH_RELAY
            if (mesh_foundation_relay_get() != 0){
                // - to ADV bearer, if Relay supported and enabled
                mesh_network_relay_message(network_pdu);
                mesh_network_run();
                return;
            }
#endif

#ifdef ENABLE_MESH_PROXY_SERVER
            if (mesh_foundation_gatt_proxy_get() != 0){
                // - to GATT bearer, if Proxy supported and enabled
                mesh_network_relay_message(network_pdu);
                mesh_network_run();
                return;
            }
#endif

        } else {

            // messages received via GATT bearer are relayed:

#ifdef ENABLE_MESH_PROXY_SERVER
            if (mesh_foundation_gatt_proxy_get() != 0){
                // - to ADV bearer, if Proxy supported and enabled
                mesh_network_relay_message(network_pdu);
                mesh_network_run();
                return;
            }
#endif

        }
    }
#endif

    // otherwise, we're done
    btstack_memory_mesh_network_pdu_free(network_pdu);
}

static void process_network_pdu_done(void){
    btstack_memory_mesh_network_pdu_free(incoming_pdu_raw);
    incoming_pdu_raw = NULL;
    mesh_crypto_active = 0;

    mesh_network_run();
}

static void process_network_pdu_validate_d(void * arg){
    UNUSED(arg);
    // mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;

    uint8_t ctl_ttl     = incoming_pdu_decoded->data[1];
    uint8_t ctl         = ctl_ttl >> 7;
    uint8_t net_mic_len = (ctl_ttl & 0x80) ? 8 : 4;

    // store NetMIC
    uint8_t net_mic[8];
    btstack_crypto_ccm_get_authentication_value(&mesh_network_crypto_request.ccm, net_mic);
#ifdef LOG_NETWORK
    printf("RX-NetMIC (%p): ", incoming_pdu_decoded); 
    printf_hexdump(net_mic, net_mic_len);
#endif
    // store in decoded pdu
    memcpy(&incoming_pdu_decoded->data[incoming_pdu_decoded->len-net_mic_len], net_mic, net_mic_len);

#ifdef LOG_NETWORK
    uint8_t cypher_len  = incoming_pdu_decoded->len - 9 - net_mic_len;
    printf("RX-Decrypted DST/TransportPDU (%p): ", incoming_pdu_decoded);
    printf_hexdump(&incoming_pdu_decoded->data[7], 2 + cypher_len);

    printf("RX-Decrypted: ");
    printf_hexdump(incoming_pdu_decoded->data, incoming_pdu_decoded->len);
#endif

    // validate network mic
    if (memcmp(net_mic, &incoming_pdu_raw->data[incoming_pdu_decoded->len-net_mic_len], net_mic_len) != 0){
        // fail
        printf("RX-NetMIC mismatch, try next key (%p)\n", incoming_pdu_decoded);
        process_network_pdu_validate();
        return;
    }    

    // remove NetMIC from payload
    incoming_pdu_decoded->len -= net_mic_len;

#ifdef LOG_NETWORK
    // match
    printf("RX-NetMIC matches (%p)\n", incoming_pdu_decoded);
    printf("RX-TTL (%p): 0x%02x\n", incoming_pdu_decoded, incoming_pdu_decoded->data[1] & 0x7f);
#endif

    // set netkey_index
    incoming_pdu_decoded->netkey_index = current_network_key->netkey_index;

    if (incoming_pdu_decoded->flags & MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION){

        mesh_network_pdu_t * decoded_pdu = incoming_pdu_decoded;
        incoming_pdu_decoded = NULL;

        // no additional checks for proxy messages
        (*mesh_network_proxy_message_handler)(MESH_NETWORK_PDU_RECEIVED, decoded_pdu);
 
    } else {

        // validate src/dest addresses
        uint16_t src = big_endian_read_16(incoming_pdu_decoded->data, 5);
        uint16_t dst = big_endian_read_16(incoming_pdu_decoded->data, 7);
        int valid = mesh_network_addresses_valid(ctl, src, dst);
        if (!valid){
#ifdef LOG_NETWORK
            printf("RX Address invalid (%p)\n", incoming_pdu_decoded);
#endif
            btstack_memory_mesh_network_pdu_free(incoming_pdu_decoded);
            incoming_pdu_decoded = NULL;
            process_network_pdu_done();
            return;
        }

        // check cache
        uint32_t hash = mesh_network_cache_hash(incoming_pdu_decoded);
#ifdef LOG_NETWORK
        printf("RX-Hash (%p): %08x\n", incoming_pdu_decoded, hash);
#endif
        if (mesh_network_cache_find(hash)){
            // found in cache, drop
#ifdef LOG_NETWORK
            printf("Found in cache -> drop packet (%p)\n", incoming_pdu_decoded);
#endif
            btstack_memory_mesh_network_pdu_free(incoming_pdu_decoded);
            incoming_pdu_decoded = NULL;
            process_network_pdu_done();
            return;
        }

        // store in network cache
        mesh_network_cache_add(hash);

#ifdef LOG_NETWORK
            printf("RX-Validated (%p) - forward to lower transport\n", incoming_pdu_decoded);
#endif

        // forward to lower transport layer. message is freed by call to mesh_network_message_processed_by_upper_layer
        mesh_network_pdu_t * decoded_pdu = incoming_pdu_decoded;
        incoming_pdu_decoded = NULL;
        (*mesh_network_higher_layer_handler)(MESH_NETWORK_PDU_RECEIVED, decoded_pdu);
    }

    // done
    process_network_pdu_done();
}

static uint32_t iv_index_for_pdu(const mesh_network_pdu_t * network_pdu){
    // get IV Index and IVI
    uint32_t iv_index = mesh_get_iv_index();
    int ivi = network_pdu->data[0] >> 7;

    // if least significant bit differs, use previous IV Index
    if ((iv_index & 1 ) ^ ivi){
        iv_index--;
#ifdef LOG_NETWORK
        printf("RX-IV: IVI indicates previous IV index, using 0x%08x\n", iv_index);
#endif
    }
    return iv_index;
}

static void process_network_pdu_validate_b(void * arg){
    UNUSED(arg);

#ifdef LOG_NETWORK
    printf("RX-PECB: ");
    printf_hexdump(obfuscation_block, 6);
#endif

    // de-obfuscate
    unsigned int i;
    for (i=0;i<6;i++){
        incoming_pdu_decoded->data[1+i] = incoming_pdu_raw->data[1+i] ^ obfuscation_block[i];
    }

    uint32_t iv_index = iv_index_for_pdu(incoming_pdu_raw);

    if (incoming_pdu_decoded->flags & MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION){
        // create network nonce
        mesh_proxy_create_nonce(network_nonce, incoming_pdu_decoded, iv_index);
#ifdef LOG_NETWORK
        printf("RX-Proxy Nonce: ");
        printf_hexdump(network_nonce, 13);
#endif
    } else {
        // create network nonce
        mesh_network_create_nonce(network_nonce, incoming_pdu_decoded, iv_index);
#ifdef LOG_NETWORK
        printf("RX-Network Nonce: ");
        printf_hexdump(network_nonce, 13);
#endif
    }

    // 
    uint8_t ctl_ttl     = incoming_pdu_decoded->data[1];
    uint8_t net_mic_len = (ctl_ttl & 0x80) ? 8 : 4;
    uint8_t cypher_len  = incoming_pdu_decoded->len - 7 - net_mic_len;

#ifdef LOG_NETWORK
    printf("RX-Cyper len %u, mic len %u\n", cypher_len, net_mic_len);

    printf("RX-Encryption Key: ");
    printf_hexdump(current_network_key->encryption_key, 16);

#endif

    btstack_crypto_ccm_init(&mesh_network_crypto_request.ccm, current_network_key->encryption_key, network_nonce, cypher_len, 0, net_mic_len);
    btstack_crypto_ccm_decrypt_block(&mesh_network_crypto_request.ccm, cypher_len, &incoming_pdu_raw->data[7], &incoming_pdu_decoded->data[7], &process_network_pdu_validate_d, incoming_pdu_decoded);
}

static void process_network_pdu_validate(void){
    if (!mesh_network_key_nid_iterator_has_more(&validation_network_key_it)){
        printf("No valid network key found\n");
        btstack_memory_mesh_network_pdu_free(incoming_pdu_decoded);
        incoming_pdu_decoded = NULL;
        process_network_pdu_done();
        return;
    }

    current_network_key = mesh_network_key_nid_iterator_get_next(&validation_network_key_it);

    // calc PECB
    uint32_t iv_index = iv_index_for_pdu(incoming_pdu_raw);
    memset(encryption_block, 0, 5);
    big_endian_store_32(encryption_block, 5, iv_index);
    memcpy(&encryption_block[9], &incoming_pdu_raw->data[7], 7);
    btstack_crypto_aes128_encrypt(&mesh_network_crypto_request.aes128, current_network_key->privacy_key, encryption_block, obfuscation_block, &process_network_pdu_validate_b, NULL);
}


static void process_network_pdu(void){
    //
    uint8_t nid_ivi = incoming_pdu_raw->data[0];

    // setup pdu object
    incoming_pdu_decoded->data[0] = nid_ivi;
    incoming_pdu_decoded->len     = incoming_pdu_raw->len;
    incoming_pdu_decoded->flags   = incoming_pdu_raw->flags;

    // init provisioning data iterator
    uint8_t nid = nid_ivi & 0x7f;
    // uint8_t iv_index = network_pdu_data[0] >> 7;
    mesh_network_key_nid_iterator_init(&validation_network_key_it, nid);

    process_network_pdu_validate();
}

static void mesh_network_run(void){
    if (!btstack_linked_list_empty(&network_pdus_outgoing_gatt)){


#ifdef ENABLE_MESH_GATT_BEARER
        if (gatt_bearer_network_pdu == NULL){
            // move to 'gatt bearer queue'
            mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_outgoing_gatt);

#ifdef LOG_NETWORK
        printf("network run: pop %p from network_pdus_outgoing_gatt\n", network_pdu);
#endif
        // request to send via gatt if:
        // proxy active and connected
        // packet wasn't received via gatt bearer
            int send_via_gatt = ((mesh_foundation_gatt_proxy_get() != 0) &&
            (gatt_bearer_con_handle != HCI_CON_HANDLE_INVALID) &&
                                 ((network_pdu->flags & MESH_NETWORK_PDU_FLAGS_GATT_BEARER) == 0));
            if (send_via_gatt){
#ifdef LOG_NETWORK
        printf("network run: set %p as gatt_bearer_network_pdu\n", network_pdu);
#endif

               gatt_bearer_network_pdu = network_pdu;
                gatt_bearer_request_can_send_now_for_network_pdu();
            } else {
#ifdef LOG_NETWORK
        printf("network run: push %p to network_pdus_outgoing_adv\n", network_pdu);
#endif
                btstack_linked_list_add_tail(&network_pdus_outgoing_adv, (btstack_linked_item_t *) network_pdu);
            }
        }
#else
        // directly move to 'outgoing adv bearer queue'
        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_outgoing_gatt);
        btstack_linked_list_add_tail(&network_pdus_outgoing_adv, (btstack_linked_item_t *) network_pdu);
#endif
    }

    if (!btstack_linked_list_empty(&network_pdus_outgoing_adv)){
#ifdef ENABLE_MESH_ADV_BEARER        
        if (adv_bearer_network_pdu == NULL){
            // move to 'adv bearer queue'
#ifdef LOG_NETWORK
            mesh_network_dump_network_pdus("network_pdus_outgoing_adv", &network_pdus_outgoing_adv);
#endif
            mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_outgoing_adv);
#ifdef LOG_NETWORK
            printf("network run: pop %p from network_pdus_outgoing_adv\n", network_pdu);
            mesh_network_dump_network_pdus("network_pdus_outgoing_adv", &network_pdus_outgoing_adv);
#endif
            adv_bearer_network_pdu = network_pdu;
            adv_bearer_request_can_send_now_for_network_pdu();
        }
#else
        // done
        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_outgoing_adv);
        // directly notify upper layer
        mesh_network_send_complete(network_pdu);
#endif
    }

    if (mesh_crypto_active) return;

    if (!btstack_linked_list_empty(&network_pdus_received)){
        incoming_pdu_decoded = mesh_network_pdu_get();
        if (!incoming_pdu_decoded) return; 
        // get encoded network pdu and start processing
        mesh_crypto_active = 1;
        incoming_pdu_raw = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_received);
        process_network_pdu();
        return;
    }

    if (!btstack_linked_list_empty(&network_pdus_queued)){
        // get queued network pdu and start processing
#ifdef LOG_NETWORK
        mesh_network_dump_network_pdus("network_pdus_queued", &network_pdus_queued);
#endif
        outgoing_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&network_pdus_queued);
#ifdef LOG_NETWORK
        printf("network run: pop %p from network_pdus_queued\n", outgoing_pdu);
        mesh_network_dump_network_pdus("network_pdus_queued", &network_pdus_queued);
#endif
        mesh_network_send_a();
        return;
    }
}

#ifdef ENABLE_MESH_ADV_BEARER
static void mesh_adv_bearer_handle_network_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    mesh_network_pdu_t * network_pdu;
    uint8_t  transmission_count;
    uint16_t transmission_interval;
    uint8_t  transmit_config;
    
    switch (packet_type){
        case MESH_NETWORK_PACKET:
            // check len. minimal transport PDU len = 1, 32 bit NetMIC -> 13 bytes
            if (size < 13) break;

#ifdef LOG_NETWORK
            printf("received network pdu from adv (len %u): ", size);
            printf_hexdump(packet, size);
#endif
            mesh_network_received_message(packet, size, 0);
            break;

        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case HCI_EVENT_MESH_META:
                    switch(packet[2]){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            if (adv_bearer_network_pdu == NULL) break;

                            // Get Transmission config depending on relay flag
                            if (adv_bearer_network_pdu->flags & MESH_NETWORK_PDU_FLAGS_RELAY){
                                transmit_config = mesh_foundation_relay_get();
                            } else {
                                transmit_config = mesh_foundation_network_transmit_get();
                            }
                            transmission_count     = (transmit_config & 0x07) + 1;
                            transmission_interval = (transmit_config >> 3) * 10;

#ifdef LOG_NETWORK
                            printf("TX-E-NetworkPDU count %u, interval %u ms (%p): ", transmission_count, transmission_interval, adv_bearer_network_pdu);
                            printf_hexdump(adv_bearer_network_pdu->data, adv_bearer_network_pdu->len);
#endif

                            adv_bearer_send_network_pdu(adv_bearer_network_pdu->data, adv_bearer_network_pdu->len, transmission_count, transmission_interval);
                            network_pdu = adv_bearer_network_pdu;
                            adv_bearer_network_pdu = NULL;

                            // notify upper layer
                            mesh_network_send_complete(network_pdu);

                            // check if more to send
                            mesh_network_run();
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}
#endif

#ifdef ENABLE_MESH_GATT_BEARER
static void mesh_network_gatt_bearer_outgoing_complete(void){

    if (gatt_bearer_network_pdu == NULL) return;

    // forward to adv bearer
    btstack_linked_list_add_tail(&network_pdus_outgoing_adv, (btstack_linked_item_t*) gatt_bearer_network_pdu);
    gatt_bearer_network_pdu = NULL;

    mesh_network_run();
    return;
}

static void mesh_network_gatt_bearer_handle_network_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            if (mesh_foundation_gatt_proxy_get() == 0) break;
#ifdef LOG_NETWORK
            printf("received network pdu from gatt (len %u): ", size);
            printf_hexdump(packet, size);
#endif
            mesh_network_received_message(packet, size, MESH_NETWORK_PDU_FLAGS_GATT_BEARER);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_PROXY_CONNECTED:
                            gatt_bearer_con_handle = mesh_subevent_proxy_connected_get_con_handle(packet);
                            break;
                        case MESH_SUBEVENT_PROXY_DISCONNECTED:
                            gatt_bearer_con_handle = HCI_CON_HANDLE_INVALID;
                            mesh_network_gatt_bearer_outgoing_complete();
                            break;
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            if (gatt_bearer_network_pdu == NULL) break;
#ifdef LOG_NETWORK
                            printf("G-TX-E-NetworkPDU (%p): ", gatt_bearer_network_pdu);
                            printf_hexdump(gatt_bearer_network_pdu->data, gatt_bearer_network_pdu->len);
#endif
                            gatt_bearer_send_network_pdu(gatt_bearer_network_pdu->data, gatt_bearer_network_pdu->len);
                            break;

                        case MESH_SUBEVENT_MESSAGE_SENT:
                            mesh_network_gatt_bearer_outgoing_complete();
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
#endif

#ifdef ENABLE_MESH_GATT_BEARER
static void mesh_netework_gatt_bearer_handle_proxy_configuration(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    switch (packet_type){
        case MESH_PROXY_DATA_PACKET:
            mesh_network_process_proxy_configuration_message(packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_MESH_META:
                    switch (hci_event_mesh_meta_get_subevent_code(packet)){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            // forward to higher layer
                            (*mesh_network_proxy_message_handler)(MESH_NETWORK_CAN_SEND_NOW, NULL);
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
#endif

void mesh_network_init(void){
#ifdef ENABLE_MESH_ADV_BEARER
    adv_bearer_register_for_network_pdu(&mesh_adv_bearer_handle_network_event);
#endif
#ifdef ENABLE_MESH_GATT_BEARER
    gatt_bearer_con_handle = HCI_CON_HANDLE_INVALID;
    gatt_bearer_register_for_network_pdu(&mesh_network_gatt_bearer_handle_network_event);
    gatt_bearer_register_for_mesh_proxy_configuration(&mesh_netework_gatt_bearer_handle_proxy_configuration);
#endif
}

void mesh_network_set_higher_layer_handler(void (*packet_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu)){
    mesh_network_higher_layer_handler = packet_handler;
}

void mesh_network_set_proxy_message_handler(void (*packet_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu)){
    mesh_network_proxy_message_handler = packet_handler;
}

void mesh_network_received_message(const uint8_t * pdu_data, uint8_t pdu_len, uint8_t flags){
    // verify len
    if (pdu_len > 29) return;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (!network_pdu) return;

    // store data
    memcpy(network_pdu->data, pdu_data, pdu_len);
    network_pdu->len = pdu_len;
    network_pdu->flags = flags;

    // add to list and go
    btstack_linked_list_add_tail(&network_pdus_received, (btstack_linked_item_t *) network_pdu);
    mesh_network_run();

}

void mesh_network_process_proxy_configuration_message(const uint8_t * pdu_data, uint8_t pdu_len){
    // verify len
    if (pdu_len > 29) return;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (!network_pdu) return;

    // store data
    memcpy(network_pdu->data, pdu_data, pdu_len);
    network_pdu->len = pdu_len;
    network_pdu->flags = MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION; // Network PDU

    // add to list and go
    btstack_linked_list_add_tail(&network_pdus_received, (btstack_linked_item_t *) network_pdu);
    mesh_network_run();
}

void mesh_network_send_pdu(mesh_network_pdu_t * network_pdu){
#ifdef LOG_NETWORK
    printf("TX-NetworkPDU (%p):   ", network_pdu);
    printf_hexdump(network_pdu->data, network_pdu->len);
    printf("^^ into network_pdus_queued\n");
#endif

    if (network_pdu->len > 29){
        printf("too long, %u\n", network_pdu->len);
        while(1);
    }

    // network pdu without payload = 9 bytes
    if (network_pdu->len < 9){
        printf("too short, %u\n", network_pdu->len);
        while(1);
    }

    // setup callback
    network_pdu->callback = &mesh_network_send_d;
    network_pdu->flags    = 0;

    // queue up
    btstack_linked_list_add_tail(&network_pdus_queued, (btstack_linked_item_t *) network_pdu);
#ifdef LOG_NETWORK
    mesh_network_dump_network_pdus("network_pdus_queued", &network_pdus_queued);
#endif

    // go
    mesh_network_run();
}

void mesh_network_encrypt_proxy_configuration_message(mesh_network_pdu_t * network_pdu, void (* callback)(mesh_network_pdu_t * callback)){
    printf("ProxyPDU(unencrypted): ");
    printf_hexdump(network_pdu->data, network_pdu->len);

    // setup callback
    network_pdu->callback = callback;
    network_pdu->flags    = MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION;

    // queue up
    btstack_linked_list_add_tail(&network_pdus_queued, (btstack_linked_item_t *) network_pdu);

    // go
    mesh_network_run();
}

/*
 * @brief Setup network pdu header
 * @param netkey_index
 * @param ctl
 * @param ttl
 * @param seq
 * @param dest
 */
void mesh_network_setup_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t nid, uint8_t ctl, uint8_t ttl, uint32_t seq, uint16_t src, uint16_t dest, const uint8_t * transport_pdu_data, uint8_t transport_pdu_len){
    memset(network_pdu, 0, sizeof(mesh_network_pdu_t));
    // set netkey_index
    network_pdu->netkey_index = netkey_index;
    // setup header
    network_pdu->data[network_pdu->len++] = (mesh_get_iv_index_for_tx() << 7) |  nid;
    uint8_t ctl_ttl = (ctl << 7) | (ttl & 0x7f);
    network_pdu->data[network_pdu->len++] = ctl_ttl;
    big_endian_store_24(network_pdu->data, 2, seq);
    network_pdu->len += 3;
    big_endian_store_16(network_pdu->data, network_pdu->len, src);
    network_pdu->len += 2;
    big_endian_store_16(network_pdu->data, network_pdu->len, dest);
    network_pdu->len += 2;
    memcpy(&network_pdu->data[network_pdu->len], transport_pdu_data, transport_pdu_len);
    network_pdu->len += transport_pdu_len;
}

/*
 * @brief Setup network pdu header
 * @param netkey_index
 * @param ctl
 * @param ttl
 * @param seq
 * @param dest
 */
void mesh_network_setup_pdu_header(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t nid, uint8_t ctl, uint8_t ttl, uint32_t seq, uint16_t src, uint16_t dest){
    // set netkey_index
    network_pdu->netkey_index = netkey_index;
    // setup header
    network_pdu->data[0] = (mesh_get_iv_index_for_tx() << 7) |  nid;
    uint8_t ctl_ttl = (ctl << 7) | (ttl & 0x7f);
    network_pdu->data[1] = ctl_ttl;
    big_endian_store_24(network_pdu->data, 2, seq);
    big_endian_store_16(network_pdu->data, 5, src);
    big_endian_store_16(network_pdu->data, 7, dest);
}

// Network PDU Getter
uint8_t  mesh_network_nid(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[0] & 0x7f;
}
uint16_t mesh_network_control(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[1] & 0x80;
}
uint8_t mesh_network_ttl(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[1] & 0x7f;
}
uint32_t mesh_network_seq(mesh_network_pdu_t * network_pdu){
    return big_endian_read_24(network_pdu->data, 2);
}
uint16_t mesh_network_src(mesh_network_pdu_t * network_pdu){
    return big_endian_read_16(network_pdu->data, 5);
}
uint16_t mesh_network_dst(mesh_network_pdu_t * network_pdu){
    return big_endian_read_16(network_pdu->data, 7);
}
int mesh_network_segmented(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[9] & 0x80;
}
uint8_t mesh_network_control_opcode(mesh_network_pdu_t * network_pdu){
    return network_pdu->data[9] & 0x7f;
}
uint8_t * mesh_network_pdu_data(mesh_network_pdu_t * network_pdu){
    return &network_pdu->data[9];
}
uint8_t   mesh_network_pdu_len(mesh_network_pdu_t * network_pdu){
    return network_pdu->len - 9;
}

static void mesh_network_dump_network_pdu(mesh_network_pdu_t * network_pdu){
    if (network_pdu){
        printf("- %p: ", network_pdu); printf_hexdump(network_pdu->data, network_pdu->len);
    }
}
static void mesh_network_dump_network_pdus(const char * name, btstack_linked_list_t * list){
    printf("List: %s:\n", name);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t*) btstack_linked_list_iterator_next(&it);
        mesh_network_dump_network_pdu(network_pdu);
    }
}
static void mesh_network_reset_network_pdus(btstack_linked_list_t * list){
    while (!btstack_linked_list_empty(list)){
        mesh_network_pdu_t * pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(list);
        btstack_memory_mesh_network_pdu_free(pdu);
    }
}
void mesh_network_dump(void){
    mesh_network_dump_network_pdus("network_pdus_received", &network_pdus_received);
    mesh_network_dump_network_pdus("network_pdus_queued", &network_pdus_queued);
    mesh_network_dump_network_pdus("network_pdus_outgoing_gatt", &network_pdus_outgoing_gatt);
    mesh_network_dump_network_pdus("network_pdus_outgoing_adv", &network_pdus_outgoing_adv);
    printf("outgoing_pdu: \n");
    mesh_network_dump_network_pdu(outgoing_pdu);
    printf("incoming_pdu_raw: \n");
    mesh_network_dump_network_pdu(incoming_pdu_raw);
#ifdef ENABLE_MESH_GATT_BEARER
    printf("gatt_bearer_network_pdu: \n");
    mesh_network_dump_network_pdu(gatt_bearer_network_pdu);
#endif
#ifdef ENABLE_MESH_ADV_BEARER
    printf("adv_bearer_network_pdu: \n");
    mesh_network_dump_network_pdu(adv_bearer_network_pdu);
#endif

}
void mesh_network_reset(void){
    mesh_network_reset_network_pdus(&network_pdus_received);
    mesh_network_reset_network_pdus(&network_pdus_queued);
    mesh_network_reset_network_pdus(&network_pdus_outgoing_gatt);
    mesh_network_reset_network_pdus(&network_pdus_outgoing_adv);
    
    // outgoing network pdus are owned by higher layer, so we don't free:
    // - adv_bearer_network_pdu
    // - gatt_bearer_network_pdu
    // - outoing_pdu
#ifdef ENABLE_MESH_ADV_BEARER
    adv_bearer_network_pdu = NULL;
#endif
#ifdef ENABLE_MESH_GATT_BEARER
    gatt_bearer_network_pdu = NULL;
#endif
    outgoing_pdu = NULL;
    
    if (incoming_pdu_raw){
        mesh_network_pdu_free(incoming_pdu_raw);
        incoming_pdu_raw = NULL;
    }
    if (incoming_pdu_decoded){
        mesh_network_pdu_free(incoming_pdu_decoded);
        incoming_pdu_decoded = NULL;
    }
    mesh_crypto_active = 0;
}

// buffer pool
mesh_network_pdu_t * mesh_network_pdu_get(void){
    mesh_network_pdu_t * network_pdu = btstack_memory_mesh_network_pdu_get();
    if (network_pdu) {
        memset(network_pdu, 0, sizeof(mesh_network_pdu_t));
        network_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_NETWORK;
    }
    return network_pdu;
}

void mesh_network_pdu_free(mesh_network_pdu_t * network_pdu){
    btstack_memory_mesh_network_pdu_free(network_pdu);
}

// Mesh Subnet Management

void mesh_subnet_add(mesh_subnet_t * subnet){
    btstack_linked_list_add_tail(&subnets, (btstack_linked_item_t *) subnet);
}

void mesh_subnet_remove(mesh_subnet_t * subnet){
    btstack_linked_list_remove(&subnets, (btstack_linked_item_t *) subnet);
}

mesh_subnet_t * mesh_subnet_get_by_netkey_index(uint16_t netkey_index){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &subnets);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_subnet_t * item = (mesh_subnet_t *) btstack_linked_list_iterator_next(&it);
        if (item->netkey_index == netkey_index) return item;
    }
    return NULL;
}

int mesh_subnet_list_count(void){
    return btstack_linked_list_count(&subnets);
}

// mesh network key iterator over all keys
void mesh_subnet_iterator_init(mesh_subnet_iterator_t *it){
    btstack_linked_list_iterator_init(&it->it, &subnets);
}

int mesh_subnet_iterator_has_more(mesh_subnet_iterator_t *it){
    return btstack_linked_list_iterator_has_next(&it->it);
}

mesh_subnet_t * mesh_subnet_iterator_get_next(mesh_subnet_iterator_t *it){
    return (mesh_subnet_t *) btstack_linked_list_iterator_next(&it->it);
}

mesh_network_key_t * mesh_subnet_get_outgoing_network_key(mesh_subnet_t * subnet){
    switch (subnet->key_refresh){
        case MESH_KEY_REFRESH_SECOND_PHASE:
            return subnet->new_key;
        case MESH_KEY_REFRESH_NOT_ACTIVE:
        case MESH_KEY_REFRESH_FIRST_PHASE:
        default:
            return subnet->old_key;
    }
}

/**
 * @brief Setup subnet for given netkey index
 */
void mesh_subnet_setup_for_netkey_index(uint16_t netkey_index){
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (subnet != NULL) return;

    // find old / new keys
    mesh_network_key_t * old_key = NULL;
    mesh_network_key_t * new_key = NULL;
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * network_key = mesh_network_key_iterator_get_next(&it);
        if (network_key->netkey_index != netkey_index) continue;
        if (old_key == NULL){
            old_key = network_key;
            continue;
        }
        // assign current key depending on key version
        if (((int8_t) (network_key->version - new_key->version)) > 0) {
            new_key = network_key;
        } else {
            new_key = old_key;
            old_key = network_key;
        }
    }

    // create subnet for netkey index
    subnet = btstack_memory_mesh_subnet_get();
    if (subnet == NULL) return;
    subnet->netkey_index = netkey_index;
    mesh_subnet_add(subnet);

    // set keys
    subnet->old_key = old_key;
    subnet->new_key = new_key;

    // key refresh
    if (new_key == NULL){
        // single key -> key refresh not active
        subnet->key_refresh = MESH_KEY_REFRESH_NOT_ACTIVE;
    }
    else {
        // two keys -> at least phase 1
        subnet->key_refresh = MESH_KEY_REFRESH_FIRST_PHASE;
    }
}

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

#define __BTSTACK_FILE__ "mesh_transport.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ble/mesh/beacon.h"
#include "mesh_transport.h"
#include "btstack_util.h"
#include "btstack_memory.h"

static uint16_t primary_element_address;

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}
// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }

// application key list

typedef struct {
    uint8_t  label_uuid[16];
    uint16_t pseudo_dst;
    uint16_t dst;
    uint8_t  akf;
    uint8_t  aid;
    uint8_t  first;
} mesh_transport_key_iterator_t;

typedef struct {
    btstack_linked_item_t item;

    // index into shared global key list
    uint16_t index;

    // app_key
    uint8_t key[16];
 
    // application key flag, 0 for device key
    uint8_t akf;

    // application key hash id
    uint8_t aid;
} mesh_transport_key_t;

typedef struct {
    uint16_t pseudo_dst;
    uint16_t hash;
    uint8_t  label_uuid[16];
} mesh_virtual_address_t;

static mesh_transport_key_t   test_application_key;
static mesh_transport_key_t   mesh_transport_device_key;
static mesh_virtual_address_t test_virtual_address;

void mesh_application_key_set(uint16_t appkey_index, uint8_t aid, const uint8_t * application_key){
    test_application_key.index = appkey_index;
    test_application_key.aid   = aid;
    test_application_key.akf   = 1;
    memcpy(test_application_key.key, application_key, 16);
}

void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    mesh_transport_device_key.akf   = 0;
    memcpy(mesh_transport_device_key.key, device_key, 16);
}

static void mesh_virtual_address_run(void){
}

uint16_t mesh_virtual_address_register(uint8_t * label_uuid, uint16_t hash){
    // TODO:: check if already exists
    // TODO: calc hash
    test_virtual_address.hash   = hash;
    memcpy(test_virtual_address.label_uuid, label_uuid, 16);
    test_virtual_address.pseudo_dst = 0x8000;    
    mesh_virtual_address_run();
    return test_virtual_address.pseudo_dst;
}

void mesh_virtual_address_unregister(uint16_t pseudo_dst){
}

static mesh_virtual_address_t * mesh_virtual_address_for_pseudo_dst(uint16_t pseudo_dst){
    if (test_virtual_address.pseudo_dst == pseudo_dst){
        return &test_virtual_address;
    }
    return NULL;
}

static const mesh_transport_key_t * mesh_transport_key_get(uint16_t appkey_index){
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        return &mesh_transport_device_key;
    }
    if (appkey_index != test_application_key.index) return NULL;
    return &test_application_key;
}

// mesh network key iterator
static void mesh_transport_key_iterator_init(mesh_transport_key_iterator_t * it, uint16_t dst, uint8_t akf, uint8_t aid){
    it->dst = dst; 
    it->aid      = aid;
    it->akf      = akf;
    it->first    = 1;
}

static int mesh_transport_key_iterator_has_more(mesh_transport_key_iterator_t * it){
    if (!it->first) return 0;
    if (mesh_network_address_virtual(it->dst) && it->dst != test_virtual_address.hash) return 0;
    if (it->akf){
        return it->aid == test_application_key.aid;
    } else {
        return 1;
    }
}

static const mesh_transport_key_t * mesh_transport_key_iterator_get_next(mesh_transport_key_iterator_t * it){
    it->first = 0;
    if (mesh_network_address_virtual(it->dst)){
        memcpy(it->label_uuid, test_virtual_address.label_uuid, 16);
        it->pseudo_dst = test_virtual_address.pseudo_dst;
    }
    if (it->akf){
        return &test_application_key;
    } else {
        return &mesh_transport_device_key;
    }
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
    mesh_network_pdu_t * network_pdu = btstack_memory_mesh_network_pdu_get();
    if (!network_pdu) return 0;

    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, ctl, ttl, seq, src, dest, transport_pdu_data, transport_pdu_len);

    // send network_pdu
    mesh_network_send_pdu(network_pdu);
    return 0;
}

// mesh seq auth validation
// TODO: support multiple devices
#define MESH_ADDRESS_UNSASSIGNED 0xfffb
static uint16_t mesh_seq_auth_single_src = MESH_ADDRESS_UNSASSIGNED;
static uint32_t mesh_seq_auth_single_seq;
static uint16_t mesh_seq_auth_single_zero;

void mesh_seq_auth_reset(void){
    mesh_seq_auth_single_src = MESH_ADDRESS_UNSASSIGNED;
}

static int mesh_seq_auth_validate(uint16_t src, uint32_t seq){
    if (mesh_seq_auth_single_src == MESH_ADDRESS_UNSASSIGNED){
        mesh_seq_auth_single_src = src;
        mesh_seq_auth_single_seq = seq;
        return 0;
    }
    if (mesh_seq_auth_single_src != src){
        return 1;
    }
    if (mesh_seq_auth_single_seq >= seq){
        return 2;
    }
    mesh_seq_auth_single_seq = seq;
    return 0;
}

static int mesh_seq_zero_validate(uint16_t src, uint16_t seq_zero){
    // assume mesh_seq_auth_validate was already called
    if (mesh_seq_auth_single_src == MESH_ADDRESS_UNSASSIGNED) return 0;
    if (mesh_seq_auth_single_src != src){
        return 1;
    }
    if (mesh_seq_auth_single_zero >= seq_zero){
        return 2;
    }
    mesh_seq_auth_single_zero = seq_zero;
    return 0;
}


// stub lower transport

static void mesh_lower_transport_run(void);
static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu);
static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu);
static void mesh_transport_abort_transmission(void);

static int mesh_transport_crypto_active;
static mesh_network_pdu_t   * network_pdu_in_validation;
static mesh_transport_pdu_t * transport_pdu_in_validation;
static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static mesh_transport_key_iterator_t mesh_transport_key_it;

// upper transport callbacks - in access layer
static void (*mesh_access_unsegmented_handler)(mesh_network_pdu_t * network_pdu);
static void (*mesh_access_segmented_handler)(mesh_transport_pdu_t * transport_pdu);

// lower transport incoming
static btstack_linked_list_t lower_transport_incoming;
// upper transport segmented access messages (to validate)
static btstack_linked_list_t upper_transport_access;
// upper transport segmented control messages (to process)
static btstack_linked_list_t upper_transport_control;
// access segmented access messages (to process)
// static btstack_linked_list_t access_incoming;

static mesh_transport_pdu_t * upper_transport_outgoing_pdu;
static mesh_network_pdu_t   * upper_transport_outgoing_segment;
static uint16_t               upper_transport_outgoing_seg_o;

static uint32_t               upper_transport_seq;


static void transport_unsegmented_setup_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[1] = 0x00;    // SZMIC if a Segmented Access message or 0 for all other message formats
    memcpy(&nonce[2], &network_pdu->data[2], 7);
    big_endian_store_32(nonce, 9, mesh_get_iv_index());
}

static void transport_segmented_setup_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[1] = transport_pdu->transmic_len == 8 ? 0x80 : 0x00;
    memcpy(&nonce[2], &transport_pdu->network_header[2], 7);
    big_endian_store_32(nonce, 9, mesh_get_iv_index());
}

static void transport_unsegmented_setup_application_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[0] = 0x01;
    transport_unsegmented_setup_nonce(nonce, network_pdu);
    mesh_print_hex("AppNonce", nonce, 13);
}

static void transport_unsegmented_setup_device_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[0] = 0x02;
    transport_unsegmented_setup_nonce(nonce, network_pdu);
    mesh_print_hex("DeviceNonce", nonce, 13);
}

static void transport_segmented_setup_application_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[0] = 0x01;
    transport_segmented_setup_nonce(nonce, transport_pdu);
    mesh_print_hex("AppNonce", nonce, 13);
}

static void transport_segmented_setup_device_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[0] = 0x02;
    transport_segmented_setup_nonce(nonce, transport_pdu);
    mesh_print_hex("DeviceNonce", nonce, 13);
}

// Transport PDU Getter
static uint16_t mesh_transport_nid(mesh_transport_pdu_t * transport_pdu){
    return transport_pdu->network_header[0] & 0x7f;
}
static uint16_t mesh_transport_ctl(mesh_transport_pdu_t * transport_pdu){
    return transport_pdu->network_header[1] >> 7;
}
static uint16_t mesh_transport_ttl(mesh_transport_pdu_t * transport_pdu){
    return transport_pdu->network_header[1] & 0x7f;
}
static uint32_t mesh_transport_seq(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_24(transport_pdu->network_header, 2);
}
static uint32_t mesh_transport_seq_zero(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_24(transport_pdu->network_header, 2) & 0x1fff;
}
static uint16_t mesh_transport_src(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_16(transport_pdu->network_header, 5);
}
static uint16_t mesh_transport_dst(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_16(transport_pdu->network_header, 7);
}
static void mesh_transport_set_nid_ivi(mesh_transport_pdu_t * transport_pdu, uint8_t nid_ivi){
    transport_pdu->network_header[0] = nid_ivi;
}
static void mesh_transport_set_ctl_ttl(mesh_transport_pdu_t * transport_pdu, uint8_t ctl_ttl){
    transport_pdu->network_header[1] = ctl_ttl;
}
static void mesh_transport_set_seq(mesh_transport_pdu_t * transport_pdu, uint32_t seq){
    big_endian_store_24(transport_pdu->network_header, 2, seq);
}
static void mesh_transport_set_src(mesh_transport_pdu_t * transport_pdu, uint16_t src){
    big_endian_store_16(transport_pdu->network_header, 5, src);
}
static void mesh_transport_set_dest(mesh_transport_pdu_t * transport_pdu, uint16_t dest){
    big_endian_store_16(transport_pdu->network_header, 7, dest);
}

static void mesh_transport_process_unsegmented_control_message(mesh_network_pdu_t * network_pdu){
    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t  opcode = lower_transport_pdu[0];
    printf("Unsegmented Control message, outgoing message %p, opcode %x\n", upper_transport_outgoing_pdu, opcode);
    uint16_t seq_zero_pdu;
    uint16_t seq_zero_out;
    uint32_t block_ack;
    switch (opcode){
        case 0:
            if (upper_transport_outgoing_pdu == NULL) break;
            seq_zero_pdu = big_endian_read_16(lower_transport_pdu, 1) >> 2;
            seq_zero_out = mesh_transport_seq(upper_transport_outgoing_pdu) & 0x1fff;
            block_ack = big_endian_read_32(lower_transport_pdu, 3); 
            printf("[+] Segment Acknowledgment message with seq_zero %06x, block_ack %08x - outgoing seq %06x, block_ack %08x\n",
                seq_zero_pdu, block_ack, seq_zero_out, upper_transport_outgoing_pdu->block_ack);
            if (block_ack == 0){
                // If a Segment Acknowledgment message with the BlockAck field set to 0x00000000 is received,
                // then the Upper Transport PDU shall be immediately cancelled and the higher layers shall be notified that
                // the Upper Transport PDU has been cancelled.
                printf("[+] Block Ack == 0 => Abort\n");
                mesh_transport_abort_transmission();
                break;                
            }
            if (seq_zero_pdu != seq_zero_out){
                printf("[!] Seq Zero doesn't match\n");
                break;
            }
            upper_transport_outgoing_pdu->block_ack &= ~block_ack;
            printf("[+] Updated block_ack %08x\n", upper_transport_outgoing_pdu->block_ack);
            if (upper_transport_outgoing_pdu->block_ack == 0){
                printf("[+] Sent complete\n");
                mesh_transport_abort_transmission();
            }
            break;
        default:
            if (mesh_access_unsegmented_handler){
                mesh_access_unsegmented_handler(network_pdu);
            } else {
                printf("[!] Unhandled Control message with opcode %02x\n", opcode);
            }
            break;
    }    
}

static void mesh_lower_transport_process_unsegmented_message_done(mesh_network_pdu_t * network_pdu){
    mesh_transport_crypto_active = 0;
    mesh_network_message_processed_by_higher_layer(network_pdu_in_validation);
    mesh_lower_transport_run();
}

static void mesh_lower_transport_process_segmented_message_done(mesh_transport_pdu_t * transport_pdu){
    mesh_transport_crypto_active = 0;
    btstack_memory_mesh_transport_pdu_free(transport_pdu);
    mesh_lower_transport_run();
}

static void mesh_upper_transport_validate_unsegmented_message_ccm(void * arg){
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t trans_mic_len = 4;

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, trans_mic_len);

    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;

    mesh_print_hex("Decryted PDU", upper_transport_pdu, upper_transport_pdu_len - trans_mic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len - trans_mic_len], trans_mic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        network_pdu->len -= trans_mic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(mesh_network_dst(network_pdu))){
            big_endian_store_16(network_pdu->data, 7, mesh_transport_key_it.pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_unsegmented_handler){
            mesh_access_unsegmented_handler(network_pdu);
        } else {
            printf("[!] Unhandled Unsegmented Access message\n");
        }
        
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
 
    mesh_print_hex("Decrypted PDU", upper_transport_pdu, upper_transport_pdu_len);

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, transport_pdu->transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], transport_pdu->transmic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        transport_pdu->len -= transport_pdu->transmic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(mesh_transport_dst(transport_pdu))){
            big_endian_store_16(transport_pdu->network_header, 7, mesh_transport_key_it.pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_segmented_handler){
            mesh_access_segmented_handler(transport_pdu);
        } else {
            printf("[!] Unhandled Segmented Access/Control message\n");
        }
        
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

static void mesh_upper_transport_validate_segmented_message_digest(void * arg){
    mesh_transport_pdu_t * transport_pdu   = (mesh_transport_pdu_t*) arg;
    uint8_t   upper_transport_pdu_len      = transport_pdu_in_validation->len - transport_pdu_in_validation->transmic_len;
    uint8_t * upper_transport_pdu_data_in  = transport_pdu_in_validation->data;
    uint8_t * upper_transport_pdu_data_out = transport_pdu->data;
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_in, upper_transport_pdu_data_out, &mesh_upper_transport_validate_segmented_message_ccm, transport_pdu);
}

static void mesh_upper_transport_validate_unsegmented_message_digest(void * arg){
    mesh_network_pdu_t * network_pdu       = (mesh_network_pdu_t *) arg;
    uint8_t   trans_mic_len = 4;
    uint8_t   lower_transport_pdu_len      = network_pdu_in_validation->len - 9;
    uint8_t * upper_transport_pdu_data_in  = &network_pdu_in_validation->data[10];
    uint8_t * upper_transport_pdu_data_out = &network_pdu->data[10];
    uint8_t   upper_transport_pdu_len      = lower_transport_pdu_len - 1 - trans_mic_len;
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_in, upper_transport_pdu_data_out, &mesh_upper_transport_validate_unsegmented_message_ccm, network_pdu);
}

static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu){

    if (!mesh_transport_key_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_lower_transport_process_unsegmented_message_done(network_pdu);
        return;
    }
    const mesh_transport_key_t * message_key = mesh_transport_key_iterator_get_next(&mesh_transport_key_it);

    if (message_key->akf){
        transport_unsegmented_setup_application_nonce(application_nonce, network_pdu_in_validation);
    } else {
        transport_unsegmented_setup_device_nonce(application_nonce, network_pdu_in_validation);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    network_pdu->appkey_index = message_key->index; 

    // unsegmented message have TransMIC of 32 bit
    uint8_t trans_mic_len = 4;
    printf("Unsegmented Access message with TransMIC len 4\n");

    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9;
    uint8_t * upper_transport_pdu_data = &network_pdu_in_validation->data[10];
    uint8_t   upper_transport_pdu_len  = lower_transport_pdu_len - 1 - trans_mic_len;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_network_dst(network_pdu))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, trans_mic_len);
    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, mesh_transport_key_it.label_uuid, aad_len, &mesh_upper_transport_validate_unsegmented_message_digest, network_pdu);
    } else {
        mesh_upper_transport_validate_unsegmented_message_digest(network_pdu);
    }
}

static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu){
    uint8_t * upper_transport_pdu_data =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len  =  transport_pdu->len - transport_pdu->transmic_len;

    if (!mesh_transport_key_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_lower_transport_process_segmented_message_done(transport_pdu);
        return;
    }
    const mesh_transport_key_t * message_key = mesh_transport_key_iterator_get_next(&mesh_transport_key_it);

    if (message_key->akf){
        transport_segmented_setup_application_nonce(application_nonce, transport_pdu_in_validation);
    } else {
        transport_segmented_setup_device_nonce(application_nonce, transport_pdu_in_validation);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    transport_pdu->appkey_index = message_key->index; 

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_transport_dst(transport_pdu))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, transport_pdu->transmic_len);

    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, mesh_transport_key_it.label_uuid, aad_len, &mesh_upper_transport_validate_segmented_message_digest, transport_pdu);
    } else {
        mesh_upper_transport_validate_segmented_message_digest(transport_pdu);
    }
}


static void mesh_lower_transport_process_unsegmented_access_message(mesh_network_pdu_t * network_pdu){
    // copy original pdu
    network_pdu->len = network_pdu_in_validation->len;
    memcpy(network_pdu->data, network_pdu_in_validation->data, network_pdu->len);

    // 
    uint8_t * lower_transport_pdu     = &network_pdu_in_validation->data[9];
    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9;

    mesh_print_hex("Lower Transport network pdu", &network_pdu_in_validation->data[9], lower_transport_pdu_len);

    uint8_t aid =  lower_transport_pdu[0] & 0x3f;
    uint8_t akf = (lower_transport_pdu[0] & 0x40) >> 6;
    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_iterator_init(&mesh_transport_key_it, mesh_network_dst(network_pdu), akf, aid);
    mesh_upper_transport_validate_unsegmented_message(network_pdu);
}


static void mesh_upper_transport_process_message(mesh_transport_pdu_t * transport_pdu){
    // copy original pdu
    transport_pdu->len = transport_pdu_in_validation->len;
    memcpy(transport_pdu, transport_pdu_in_validation, sizeof(mesh_transport_pdu_t));

    // 
    uint8_t * upper_transport_pdu     =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len =  transport_pdu->len - transport_pdu->transmic_len;
    mesh_print_hex("Upper Transport pdu", upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid =  transport_pdu->akf_aid & 0x3f;
    uint8_t akf = (transport_pdu->akf_aid & 0x40) >> 6;

    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_iterator_init(&mesh_transport_key_it, mesh_transport_dst(transport_pdu), akf, aid);
    mesh_upper_transport_validate_segmented_message(transport_pdu);
}

// ack / incomplete message

static mesh_transport_pdu_t * test_transport_pdu;


static void mesh_lower_transport_setup_segemented_acknowledge_message(uint8_t * data, uint8_t obo, uint16_t seq_zero, uint32_t block_ack){
    data[0] = 0;    // SEG = 0, Opcode = 0
    big_endian_store_16( data, 1, (obo << 15) | (seq_zero << 2) | 0);    // OBO, SeqZero, RFU
    big_endian_store_32( data, 3, block_ack);
    mesh_print_hex("ACK Upper Transport", data, 7);
} 

static void mesh_transport_send_ack(mesh_transport_pdu_t * transport_pdu){
    // setup ack message
    uint8_t ack_msg[7];
    uint16_t seq_zero = mesh_transport_seq(transport_pdu) & 0x1fff;
    mesh_lower_transport_setup_segemented_acknowledge_message(ack_msg, 0, seq_zero, transport_pdu->block_ack);

    printf("mesh_transport_send_ack for pdu %p with netkey_index %x, TTL = %u, SeqZero = %x, SRC = %x, DST = %x\n",
        transport_pdu, transport_pdu->netkey_index, mesh_transport_ttl(transport_pdu), seq_zero, primary_element_address, mesh_transport_src(transport_pdu));

    // send ack 
    int i;
    for (i=0;i<1;i++){
        mesh_network_send(transport_pdu->netkey_index, 1, mesh_transport_ttl(transport_pdu), mesh_upper_transport_next_seq(),
            primary_element_address, mesh_transport_src(transport_pdu), ack_msg, sizeof(ack_msg));
    }
}

static void mesh_transport_stop_acknowledgment_timer(mesh_transport_pdu_t * transport_pdu){
    if (!transport_pdu->acknowledgement_timer_active) return;
    transport_pdu->acknowledgement_timer_active = 0;
    btstack_run_loop_remove_timer(&transport_pdu->acknowledgement_timer);
}

static void mesh_transport_stop_incomplete_timer(mesh_transport_pdu_t * transport_pdu){
    if (!transport_pdu->incomplete_timer_active) return;
    transport_pdu->incomplete_timer_active = 0;
    btstack_run_loop_remove_timer(&transport_pdu->incomplete_timer);
}

// does not free packet, just stops timers and updates reassembly engine
static void mesh_transport_segmented_message_complete(mesh_transport_pdu_t * transport_pdu){
    // stop timers
    mesh_transport_stop_acknowledgment_timer(transport_pdu);
    mesh_transport_stop_incomplete_timer(transport_pdu);
    // reset reassembly
    if (transport_pdu == test_transport_pdu){
        printf("Reset Reassembly Engine\n");
        test_transport_pdu = NULL;
    }
}

static void mesh_transport_rx_ack_timeout(btstack_timer_source_t * ts){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    printf("ACK: acknowledgement timer fired for %p, send ACK\n", transport_pdu);
    transport_pdu->acknowledgement_timer_active = 0;
    mesh_transport_send_ack(transport_pdu);
}

static void mesh_transport_rx_incomplete_timeout(btstack_timer_source_t * ts){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    printf("mesh_transport_rx_incomplete_timeout - give up\n");
    mesh_transport_segmented_message_complete(transport_pdu);

    // free message
    btstack_memory_mesh_transport_pdu_free(transport_pdu);
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

// abort outgoing transmission
static void mesh_transport_abort_transmission(void){
    // stop ack timers
    mesh_transport_stop_acknowledgment_timer(upper_transport_outgoing_pdu);
    // free pdus
    btstack_memory_mesh_transport_pdu_free(upper_transport_outgoing_pdu);        
    upper_transport_outgoing_pdu     = NULL;
    btstack_memory_mesh_network_pdu_free(upper_transport_outgoing_segment);        
    upper_transport_outgoing_segment = NULL;
}

static mesh_transport_pdu_t * mesh_transport_pdu_for_segmented_message(mesh_network_pdu_t * network_pdu){
    // tracks last received seq auth and indirectly drops segments for older transport pdus
    uint16_t seq_zero = ( big_endian_read_16(mesh_network_pdu_data(network_pdu), 1) >> 2) & 0x1fff;
    // printf("mesh_transport_pdu_for_segmented_message: seq_zero %x\n", seq_zero);
    // printf("mesh_transport_pdu_for_segmented_message: test transport pdu %p\n", test_transport_pdu);
    if (test_transport_pdu){
        // check if segment for same seq zero
        uint16_t active_seq_zero = mesh_transport_seq(test_transport_pdu) & 0x1fff;
        if (active_seq_zero == seq_zero) {
            printf("mesh_transport_pdu_for_segmented_message: segment for current transport pdu with SeqZero %x\n", active_seq_zero);
            return test_transport_pdu;
        } else {
            // seq zero differs from current transport pdu
            printf("mesh_transport_pdu_for_segmented_message: drop segment. current transport pdu SeqZero %x, now %x\n", active_seq_zero, seq_zero);
            return NULL;
        }
    } else {
        // no transport pdu acive, check if seq zero is new
        uint16_t src = mesh_network_src(network_pdu);
        if (mesh_seq_zero_validate(src, seq_zero) == 0){
            test_transport_pdu = btstack_memory_mesh_transport_pdu_get();
            // copy meta data
            memcpy(test_transport_pdu->network_header, network_pdu->data, 9);
            test_transport_pdu->netkey_index = network_pdu->netkey_index;
            test_transport_pdu->block_ack = 0;
            test_transport_pdu->acknowledgement_timer_active = 0;
            printf("mesh_transport_pdu_for_segmented_message: setup transport pdu %p for src %x, seq %06x\n", test_transport_pdu, src, mesh_transport_seq(test_transport_pdu));
            return test_transport_pdu;
        }  else {
            // seq zero differs from current transport pdu
            printf("mesh_transport_pdu_for_segmented_message: drop segment for old seq %x\n", seq_zero);
            return NULL;
        }     
    }
}

static void mesh_lower_transport_process_segment( mesh_transport_pdu_t * transport_pdu, mesh_network_pdu_t * network_pdu){

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t   lower_transport_pdu_len = mesh_network_pdu_len(network_pdu);

    // get akf_aid & transmic
    transport_pdu->akf_aid      = lower_transport_pdu[0];
    transport_pdu->transmic_len = lower_transport_pdu[1] & 0x80 ? 8 : 4;

    // get seq_zero
    uint16_t seq_zero =  ( big_endian_read_16(lower_transport_pdu, 1) >> 2) & 0x1fff;

    // get seg fields
    uint8_t  seg_o    =  ( big_endian_read_16(lower_transport_pdu, 2) >> 5) & 0x001f;
    uint8_t  seg_n    =  lower_transport_pdu[3] & 0x1f;
    uint8_t   segment_len  =  lower_transport_pdu_len - 4;
    uint8_t * segment_data = &lower_transport_pdu[4];

    printf("mesh_lower_transport_process_segment: seq zero %04x, seg_o %02x, seg_n %02x, transmic len: %u\n", seq_zero, seg_o, seg_n, transport_pdu->transmic_len * 8);
    mesh_print_hex("Segment", segment_data, segment_len);

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

    mesh_print_hex("Assembled payload", transport_pdu->data, transport_pdu->len);

    // mark as done
    mesh_transport_segmented_message_complete(transport_pdu);

    // send ack
    mesh_transport_send_ack(transport_pdu);

    // forward to upper transport
    uint8_t ctl = mesh_network_control(network_pdu);
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
                    // unsegmented control message (not encrypted)
                    (void) btstack_linked_list_pop(&lower_transport_incoming);
                    mesh_transport_process_unsegmented_control_message(network_pdu);
                    mesh_network_message_processed_by_higher_layer(network_pdu);
                } else {
                    // unsegmented access message (encrypted)
                    mesh_network_pdu_t * decode_pdu = btstack_memory_mesh_network_pdu_get();
                    if (!decode_pdu) return; 
                    // get encoded network pdu and start processing
                    network_pdu_in_validation = network_pdu;
                    (void) btstack_linked_list_pop(&lower_transport_incoming);
                    mesh_lower_transport_process_unsegmented_access_message(decode_pdu);
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

static void mesh_upper_transport_network_pdu_sent(mesh_network_pdu_t * network_pdu);

void mesh_lower_transport_received_mesage(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu){
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("Tranport: received message. SRC %x, SEQ %x\n", mesh_network_src(network_pdu), mesh_network_seq(network_pdu));
            // validate seq
            if (mesh_seq_auth_validate(mesh_network_src(network_pdu), mesh_network_seq(network_pdu))) {
                printf("Transport: drop packet - src/seq auth failed\n");
                mesh_network_message_processed_by_higher_layer(network_pdu);
                break;
            }
            // add to list and go
            btstack_linked_list_add_tail(&lower_transport_incoming, (btstack_linked_item_t *) network_pdu);
            mesh_lower_transport_run();
            break;
        case MESH_NETWORK_PDU_SENT:
            mesh_upper_transport_network_pdu_sent(network_pdu);
            break;
        default:
            break;
    }
}

// UPPER TRANSPORT

static void mesh_transport_tx_ack_timeout(btstack_timer_source_t * ts);

static int mesh_upper_transport_retry_count;

uint32_t mesh_upper_transport_next_seq(void){
    return upper_transport_seq++;
}

static uint32_t mesh_upper_transport_peek_seq(void){
    return upper_transport_seq;
}

static void mesh_upper_transport_setup_segment(mesh_transport_pdu_t * transport_pdu, uint8_t seg_o, mesh_network_pdu_t * network_pdu){

    int ctl = mesh_transport_ctl(transport_pdu);
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)

    uint32_t seq      = mesh_upper_transport_next_seq();
    uint16_t seq_zero = mesh_transport_seq(transport_pdu) & 0x01fff;
    uint8_t  seg_n    = (transport_pdu->len - 1) / max_segment_len;
    uint8_t  szmic    = ((!ctl) && (transport_pdu->transmic_len == 8)) ? 1 : 0; // only 1 for access messages with 64 bit TransMIC
    uint8_t  nid      = mesh_transport_nid(transport_pdu);
    uint8_t  ttl      = mesh_transport_ttl(transport_pdu);
    uint16_t src      = mesh_transport_src(transport_pdu);
    uint16_t dest     = mesh_transport_dst(transport_pdu);    

    // current segment.
    uint16_t seg_offset = seg_o * max_segment_len;

    uint8_t lower_transport_pdu_data[16];
    lower_transport_pdu_data[0] = 0x80 |  transport_pdu->akf_aid;
    big_endian_store_24(lower_transport_pdu_data, 1, (szmic << 23) | (seq_zero << 10) | (seg_o << 5) | seg_n);
    uint16_t segment_len = btstack_min(transport_pdu->len - seg_offset, max_segment_len);
    memcpy(&lower_transport_pdu_data[4], &transport_pdu->data[seg_offset], segment_len);
    uint16_t lower_transport_pdu_len = 4 + segment_len;

    mesh_network_setup_pdu(network_pdu, transport_pdu->netkey_index, nid, 0, ttl, seq, src, dest, lower_transport_pdu_data, lower_transport_pdu_len);
}

static void mesh_upper_transport_send_next_segment(void){
    if (!upper_transport_outgoing_pdu) return;

    int ctl = mesh_transport_ctl(upper_transport_outgoing_pdu);
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)
    uint8_t  seg_n = (upper_transport_outgoing_pdu->len - 1) / max_segment_len;

    // find next unacknowledged segement
    while ((upper_transport_outgoing_seg_o <= seg_n) && ((upper_transport_outgoing_pdu->block_ack & (1 << upper_transport_outgoing_seg_o)) == 0)){
        upper_transport_outgoing_seg_o++;
    }

    if (upper_transport_outgoing_seg_o > seg_n){
        printf("[+] Upper transport, send segmented pdu complete (dst %x)\n", mesh_transport_dst(upper_transport_outgoing_pdu));
        upper_transport_outgoing_seg_o   = 0;

        // done for unicast, ack timer already set, too
        if (mesh_network_address_unicast(mesh_transport_dst(upper_transport_outgoing_pdu))) return;

        // done, more?
        if (mesh_upper_transport_retry_count == 0){
            printf("[+] Upper transport, message unacknowledged -> free\n");
            // note: same as in seg ack handling code
            btstack_memory_mesh_transport_pdu_free(upper_transport_outgoing_pdu);        
            upper_transport_outgoing_pdu     = NULL;
            btstack_memory_mesh_network_pdu_free(upper_transport_outgoing_segment);        
            upper_transport_outgoing_segment = NULL;
            return;
        }

        // start retry
        printf("[+] Upper transport, message unacknowledged retry count %u\n", mesh_upper_transport_retry_count);
        mesh_upper_transport_retry_count--;
    }

    if (mesh_network_address_unicast(mesh_transport_dst(upper_transport_outgoing_pdu))){
        // restart acknowledgment timer for unicast dst
        // - "This timer shall be set to a minimum of 200 + 50 * TTL milliseconds."
        if (upper_transport_outgoing_pdu->acknowledgement_timer_active){
            btstack_run_loop_remove_timer(&upper_transport_outgoing_pdu->incomplete_timer);
            upper_transport_outgoing_pdu->acknowledgement_timer_active = 0;
        }
        uint32_t timeout = 200 + 50 * mesh_transport_ttl(upper_transport_outgoing_pdu);
        mesh_transport_start_acknowledgment_timer(upper_transport_outgoing_pdu, timeout, &mesh_transport_tx_ack_timeout);
    }

    mesh_upper_transport_setup_segment(upper_transport_outgoing_pdu, upper_transport_outgoing_seg_o, upper_transport_outgoing_segment);

    printf("[+] Upper transport, send segmented pdu: seg_o %x, seg_n %x\n", upper_transport_outgoing_seg_o, seg_n);
    mesh_print_hex("LowerTransportPDU", upper_transport_outgoing_segment->data, upper_transport_outgoing_segment->len);

    // next segment
    upper_transport_outgoing_seg_o++;

    // send network pdu
    mesh_network_send_pdu(upper_transport_outgoing_segment);
}

static void mesh_upper_transport_network_pdu_sent(mesh_network_pdu_t * network_pdu){
    if (upper_transport_outgoing_segment == network_pdu){
        mesh_upper_transport_send_next_segment();
    } else {
         btstack_memory_mesh_network_pdu_free(network_pdu);        
    }
}

static void mesh_upper_transport_send_segmented_pdu_once(mesh_transport_pdu_t * transport_pdu){

    if (mesh_upper_transport_retry_count == 0){
        printf("[!] Upper transport, send segmented pdu failed, retries exhausted\n");
        return;
    }

    // chop into chunks
    printf("[+] Upper transport, send segmented pdu (retry count %u)\n", mesh_upper_transport_retry_count);
    mesh_upper_transport_retry_count--;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = btstack_memory_mesh_network_pdu_get();
    if (!network_pdu) return;

    // setup
    upper_transport_outgoing_pdu     = transport_pdu;
    upper_transport_outgoing_segment = network_pdu;
    upper_transport_outgoing_seg_o   = 0;

    // setup block ack - set bit for segment to send, clear on ack
    int      ctl = mesh_transport_ctl(upper_transport_outgoing_pdu);
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)
    uint8_t  seg_n = (upper_transport_outgoing_pdu->len - 1) / max_segment_len;
    if (seg_n == 31){
        transport_pdu->block_ack = -1;
    } else {
        transport_pdu->block_ack = (1 << (seg_n+1)) - 1;
    }

    // start sending
    mesh_upper_transport_send_next_segment();
}

void mesh_upper_transport_send_segmented_pdu(mesh_transport_pdu_t * transport_pdu){
    mesh_upper_transport_retry_count = 2;
    mesh_upper_transport_send_segmented_pdu_once(transport_pdu);
}

static void mesh_transport_tx_ack_timeout(btstack_timer_source_t * ts){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    printf("[+] Upper transport, acknowledgement timer fired for %p\n", transport_pdu);
    transport_pdu->acknowledgement_timer_active = 0;
    upper_transport_outgoing_seg_o = 0;
    mesh_upper_transport_send_next_segment();
}

static void mesh_upper_transport_send_unsegmented_access_pdu_ccm(void * arg){
    mesh_transport_crypto_active = 0;

    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;
    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;
    mesh_print_hex("EncAccessPayload", upper_transport_pdu, upper_transport_pdu_len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &upper_transport_pdu[upper_transport_pdu_len]);
    mesh_print_hex("TransMIC", &upper_transport_pdu[upper_transport_pdu_len], 4);
    network_pdu->len += 4;
    // send network pdu
    mesh_network_send_pdu(network_pdu);
}

static void mesh_upper_transport_send_segmented_access_pdu_ccm(void * arg){
    mesh_transport_crypto_active = 0;

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) arg;
    mesh_print_hex("EncAccessPayload", transport_pdu->data, transport_pdu->len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &transport_pdu->data[transport_pdu->len]);
    mesh_print_hex("TransMIC", &transport_pdu->data[transport_pdu->len], transport_pdu->transmic_len);
    transport_pdu->len += transport_pdu->transmic_len;
    mesh_upper_transport_send_segmented_pdu(transport_pdu);
}

uint8_t mesh_upper_transport_setup_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    printf("[+] Upper transport, setup unsegmented Control PDU (opcode %02x): \n", opcode);
    printf_hexdump(control_pdu_data, control_pdu_len);

    if (control_pdu_len > 11) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint8_t transport_pdu_data[12];
    transport_pdu_data[0] = opcode;
    memcpy(&transport_pdu_data[1], control_pdu_data, control_pdu_len);
    uint16_t transport_pdu_len = control_pdu_len + 1;

    mesh_print_hex("LowerTransportPDU", transport_pdu_data, transport_pdu_len);
    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, mesh_upper_transport_next_seq(), src, dest, transport_pdu_data, transport_pdu_len);

    return 0;
}

uint8_t mesh_upper_transport_setup_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    printf("[+] Upper transport, setup segmented Control PDU (opcode %02x): \n", opcode);
    printf_hexdump(control_pdu_data, control_pdu_len);

    if (control_pdu_len > 256) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint32_t seq = mesh_upper_transport_peek_seq();

    memcpy(transport_pdu->data, control_pdu_data, control_pdu_len);
    transport_pdu->len = control_pdu_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->akf_aid = opcode;
    transport_pdu->transmic_len = 0;    // no TransMIC for control
    mesh_transport_set_nid_ivi(transport_pdu, network_key->nid);
    mesh_transport_set_seq(transport_pdu, seq);
    mesh_transport_set_src(transport_pdu, src);
    mesh_transport_set_dest(transport_pdu, dest);
    mesh_transport_set_ctl_ttl(transport_pdu, 0x80 | ttl);

    return 0;
}

uint8_t mesh_upper_transport_setup_unsegmented_access_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                          const uint8_t * access_pdu_data, uint8_t access_pdu_len){

    uint32_t seq = mesh_upper_transport_peek_seq();

    printf("[+] Upper transport, setup unsegmented Access PDU (seq %06x): ", seq);
    printf_hexdump(access_pdu_data, access_pdu_len);

    // get app or device key
    const mesh_transport_key_t * appkey;
    appkey = mesh_transport_key_get(appkey_index);
    if (appkey == NULL){
        printf("appkey_index %x unknown\n", appkey_index);
        return 1;
    }
    uint8_t akf_aid = (appkey->akf << 6) | appkey->aid;

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint8_t transport_pdu_data[16];
    transport_pdu_data[0] = akf_aid;
    memcpy(&transport_pdu_data[1], access_pdu_data, access_pdu_len);
    uint16_t transport_pdu_len = access_pdu_len + 1;
    mesh_print_hex("Access Payload", access_pdu_data, access_pdu_len);

    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 0, ttl, mesh_upper_transport_next_seq(), src, dest, transport_pdu_data, transport_pdu_len);
    network_pdu->appkey_index = appkey_index;
    return 0;
}

uint8_t mesh_upper_transport_setup_segmented_access_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                          uint8_t szmic, const uint8_t * access_pdu_data, uint8_t access_pdu_len){

    uint32_t seq = mesh_upper_transport_peek_seq();

    printf("[+] Upper transport, setup segmented Access PDU (seq %06x, szmic %u): ", seq, szmic);
    printf_hexdump(access_pdu_data, access_pdu_len);

    // get app or device key
    const mesh_transport_key_t * appkey;
    appkey = mesh_transport_key_get(appkey_index);
    if (appkey == NULL){
        printf("appkey_index %x unknown\n", appkey_index);
        return 1;
    }
    uint8_t akf_aid = (appkey->akf << 6) | appkey->aid;

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    const uint8_t trans_mic_len = szmic ? 8 : 4;


    // store in transport pdu
    memcpy(transport_pdu->data, access_pdu_data, access_pdu_len);
    transport_pdu->len = access_pdu_len;
    transport_pdu->transmic_len = trans_mic_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->appkey_index = appkey_index;
    transport_pdu->akf_aid      = akf_aid;
    mesh_transport_set_nid_ivi(transport_pdu, network_key->nid);
    mesh_transport_set_seq(transport_pdu, seq);
    mesh_transport_set_src(transport_pdu, src);
    mesh_transport_set_dest(transport_pdu, dest);
    mesh_transport_set_ctl_ttl(transport_pdu, ttl);

    return 0;
}

void mesh_upper_transport_send_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu){
    mesh_network_send_pdu(network_pdu);
}

void mesh_upper_transport_send_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu){
    mesh_upper_transport_send_segmented_pdu(transport_pdu);
}

void mesh_upper_transport_send_unsegmented_access_pdu_digest(void * arg){
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;
    uint8_t * access_pdu_data = mesh_network_pdu_data(network_pdu) + 1;
    uint16_t  access_pdu_len  = mesh_network_pdu_len(network_pdu)  - 1;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, access_pdu_data, access_pdu_data, &mesh_upper_transport_send_unsegmented_access_pdu_ccm, network_pdu);
}

void mesh_upper_transport_send_unsegmented_access_pdu(mesh_network_pdu_t * network_pdu){

    // if dst is virtual address, lookup label uuid and hash
    uint16_t aad_len = 0;
    mesh_virtual_address_t * virtual_address = NULL;
    uint16_t dst = mesh_network_dst(network_pdu);
    if (mesh_network_address_virtual(dst)){
        virtual_address = mesh_virtual_address_for_pseudo_dst(dst);
        if (!virtual_address){
            printf("No virtual address register for pseudo dst %4x\n", dst);
            btstack_memory_mesh_network_pdu_free(network_pdu);
            return;
        }
        aad_len = 16;
        big_endian_store_16(network_pdu->data, 7, virtual_address->hash);
    }

    // setup nonce
    uint16_t appkey_index = network_pdu->appkey_index;
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        transport_unsegmented_setup_device_nonce(application_nonce, network_pdu);
    } else {
        transport_unsegmented_setup_application_nonce(application_nonce, network_pdu);
    }

    // get app or device key
    const mesh_transport_key_t * appkey = mesh_transport_key_get(appkey_index);
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   trans_mic_len = 4;
    uint16_t  access_pdu_len  = mesh_network_pdu_len(network_pdu)  - 1;
    mesh_transport_crypto_active = 1;

    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, trans_mic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16, &mesh_upper_transport_send_unsegmented_access_pdu_digest, network_pdu);
    } else {
        mesh_upper_transport_send_unsegmented_access_pdu_digest(network_pdu);    
    }
}

void mesh_upper_transport_send_segmented_access_pdu_digest(void *arg){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) arg;
    uint16_t  access_pdu_len  = transport_pdu->len;
    uint8_t * access_pdu_data = transport_pdu->data;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len,access_pdu_data, access_pdu_data, &mesh_upper_transport_send_segmented_access_pdu_ccm, transport_pdu);
}

void mesh_upper_transport_send_segmented_access_pdu(mesh_transport_pdu_t * transport_pdu){

    // if dst is virtual address, lookup label uuid and hash
    uint16_t aad_len = 0;
    mesh_virtual_address_t * virtual_address = NULL;
    uint16_t dst = mesh_transport_dst(transport_pdu);
    if (mesh_network_address_virtual(dst)){
        virtual_address = mesh_virtual_address_for_pseudo_dst(dst);
        if (!virtual_address){
            printf("No virtual address register for pseudo dst %4x\n", dst);
            btstack_memory_mesh_transport_pdu_free(transport_pdu);
            return;
        }
        // printf("Using hash %4x with LabelUUID: ", virtual_address->hash);
        // printf_hexdump(virtual_address->label_uuid, 16);
        aad_len = 16;
        big_endian_store_16(transport_pdu->network_header, 7, virtual_address->hash);
    }

    // setup nonce - uses dst, so after pseudo address translation
    uint16_t appkey_index = transport_pdu->appkey_index;
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        transport_segmented_setup_device_nonce(application_nonce, transport_pdu);
    } else {
        transport_segmented_setup_application_nonce(application_nonce, transport_pdu);
    }

    // get app or device key
    const mesh_transport_key_t * appkey = mesh_transport_key_get(appkey_index);
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   transmic_len    = transport_pdu->transmic_len;
    uint16_t  access_pdu_len  = transport_pdu->len;
    mesh_transport_crypto_active = 1;
    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, transmic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16, &mesh_upper_transport_send_segmented_access_pdu_digest, transport_pdu);
    } else {
        mesh_upper_transport_send_segmented_access_pdu_digest(transport_pdu);
    }
}

void mesh_upper_transport_set_primary_element_address(uint16_t unicast_address){
    primary_element_address = unicast_address;
}

void mesh_upper_transport_set_seq(uint32_t seq){
    upper_transport_seq = seq;
}

void mesh_upper_transport_register_unsegemented_message_handler(void (*callback)(mesh_network_pdu_t * network_pdu)){
    mesh_access_unsegmented_handler = callback;
}
void mesh_upper_transport_register_segemented_message_handler(void (*callback)(mesh_transport_pdu_t * transport_pdu)){
    mesh_access_segmented_handler = callback;
}

static void mesh_transport_dump_network_pdus(const char * name, btstack_linked_list_t * list){
    printf("List: %s:\n", name);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t*) btstack_linked_list_iterator_next(&it);
        printf("- %p: ", network_pdu); printf_hexdump(network_pdu->data, network_pdu->len);
    }
}
static void mesh_transport_reset_network_pdus(btstack_linked_list_t * list){
    while (!btstack_linked_list_empty(list)){
        mesh_network_pdu_t * pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(list);
        btstack_memory_mesh_network_pdu_free(pdu);
    }
}
void mesh_transport_dump(void){
    // static btstack_linked_list_t upper_transport_control; 
    // static btstack_linked_list_t upper_transport_access;
    mesh_transport_dump_network_pdus("lower_transport_incoming", &lower_transport_incoming);
}
void mesh_transport_reset(void){
    // static btstack_linked_list_t upper_transport_control; 
    // static btstack_linked_list_t upper_transport_access;
    mesh_transport_reset_network_pdus(&lower_transport_incoming);
}

void mesh_transport_init(){
    mesh_network_set_higher_layer_handler(&mesh_lower_transport_received_mesage);
}

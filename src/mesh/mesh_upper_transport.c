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
#include "mesh/beacon.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_upper_transport.h"
#include "btstack_util.h"
#include "btstack_memory.h"
#include "mesh_peer.h"
#include "mesh_keys.h"
#include "mesh_virtual_addresses.h"
#include "mesh_iv_index_seq_number.h"

static uint16_t primary_element_address;

static void (*higher_layer_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}
// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }


// combined key x address iterator for upper transport decryption

typedef struct {
    // state
    mesh_transport_key_iterator_t  key_it;
    mesh_virtual_address_iterator_t address_it;
    // elements
    const mesh_transport_key_t *   key;
    const mesh_virtual_address_t * address;
    // address - might be virtual
    uint16_t dst;
    // key info
} mesh_transport_key_and_virtual_address_iterator_t;

static void mesh_transport_key_and_virtual_address_iterator_init(mesh_transport_key_and_virtual_address_iterator_t *it,
                                                                 uint16_t dst, uint16_t netkey_index, uint8_t akf,
                                                                 uint8_t aid) {
    printf("KEY_INIT: dst %04x, akf %x, aid %x\n", dst, akf, aid);
    // config
    it->dst   = dst;
    // init elements
    it->key     = NULL;
    it->address = NULL;
    // init element iterators
    mesh_transport_key_aid_iterator_init(&it->key_it, netkey_index, akf, aid);
    // init address iterator
    if (mesh_network_address_virtual(it->dst)){
        mesh_virtual_address_iterator_init(&it->address_it, dst);
        // get first key
        if (mesh_transport_key_aid_iterator_has_more(&it->key_it)) {
            it->key = mesh_transport_key_aid_iterator_get_next(&it->key_it);
        }
    }
}

// cartesian product: keys x addressses
static int mesh_transport_key_and_virtual_address_iterator_has_more(mesh_transport_key_and_virtual_address_iterator_t * it){
    if (mesh_network_address_virtual(it->dst)) {
        // find next valid entry
        while (1){
            if (mesh_virtual_address_iterator_has_more(&it->address_it)) return 1;
            if (!mesh_transport_key_aid_iterator_has_more(&it->key_it)) return 0;
            // get next key
            it->key = mesh_transport_key_aid_iterator_get_next(&it->key_it);
            mesh_virtual_address_iterator_init(&it->address_it, it->dst);
        }
    } else {
        return mesh_transport_key_aid_iterator_has_more(&it->key_it);
    }
}

static void mesh_transport_key_and_virtual_address_iterator_next(mesh_transport_key_and_virtual_address_iterator_t * it){
    if (mesh_network_address_virtual(it->dst)) {
        it->address = mesh_virtual_address_iterator_get_next(&it->address_it);
    } else {
        it->key = mesh_transport_key_aid_iterator_get_next(&it->key_it);
    }
}

// UPPER TRANSPORT

// stub lower transport

static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu);
static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu);

static void mesh_transport_run(void);

static int crypto_active;
static mesh_network_pdu_t   * network_pdu_in_validation;
static mesh_transport_pdu_t * transport_pdu_in_validation;
static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static mesh_transport_key_and_virtual_address_iterator_t mesh_transport_key_it;

// upper transport callbacks - in access layer
static void (*mesh_access_message_handler)(mesh_pdu_t * pdu);
static void (*mesh_control_message_handler)(mesh_pdu_t * pdu);

// unsegmented (network) and segmented (transport) control and access messages
static btstack_linked_list_t upper_transport_incoming;


void mesh_upper_unsegmented_control_message_received(mesh_network_pdu_t * network_pdu){
    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t  opcode = lower_transport_pdu[0];
    if (mesh_control_message_handler){
        mesh_control_message_handler((mesh_pdu_t*) network_pdu);
    } else {
        printf("[!] Unhandled Control message with opcode %02x\n", opcode);
        // done
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *) network_pdu);
    }
}

static void mesh_upper_transport_process_unsegmented_message_done(mesh_network_pdu_t *network_pdu){
    crypto_active = 0;
    if (mesh_network_control(network_pdu)) {
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *) network_pdu);
    } else {
        mesh_network_pdu_free(network_pdu);
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *) network_pdu_in_validation);
        network_pdu_in_validation = NULL;
    }
    mesh_transport_run();
}

static void mesh_upper_transport_process_segmented_message_done(mesh_transport_pdu_t *transport_pdu){
    crypto_active = 0;
    if (mesh_transport_ctl(transport_pdu)) {
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)transport_pdu);
    } else {
        mesh_transport_pdu_free(transport_pdu);
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)transport_pdu_in_validation);
        transport_pdu_in_validation = NULL;
    }
    mesh_transport_run();
}

static uint32_t iv_index_for_ivi_nid(uint8_t ivi_nid){
    // get IV Index and IVI
    uint32_t iv_index = mesh_get_iv_index();
    int ivi = ivi_nid >> 7;

    // if least significant bit differs, use previous IV Index
    if ((iv_index & 1 ) ^ ivi){
        iv_index--;
    }
    return iv_index;
}

static void transport_unsegmented_setup_nonce(uint8_t * nonce, const mesh_network_pdu_t * network_pdu){
    nonce[1] = 0x00;    // SZMIC if a Segmented Access message or 0 for all other message formats
    memcpy(&nonce[2], &network_pdu->data[2], 7);
    big_endian_store_32(nonce, 9, iv_index_for_ivi_nid(network_pdu->data[0]));
}

static void transport_segmented_setup_nonce(uint8_t * nonce, const mesh_transport_pdu_t * transport_pdu){
    nonce[1] = transport_pdu->transmic_len == 8 ? 0x80 : 0x00;
    memcpy(&nonce[2], &transport_pdu->network_header[2], 7);
    big_endian_store_32(nonce, 9, iv_index_for_ivi_nid(transport_pdu->network_header[0]));
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
            big_endian_store_16(network_pdu->data, 7, mesh_transport_key_it.address->pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_message_handler){
            mesh_access_message_handler((mesh_pdu_t*) network_pdu);
        } else {
            printf("[!] Unhandled Unsegmented Access message\n");
            // done
            mesh_upper_transport_process_unsegmented_message_done(network_pdu);
        }
        
        printf("\n");
    } else {
        uint8_t afk = lower_transport_pdu[0] & 0x40;
        if (afk){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_unsegmented_message(network_pdu);
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_unsegmented_message_done(network_pdu);
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
            big_endian_store_16(transport_pdu->network_header, 7, mesh_transport_key_it.address->pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_message_handler){
            mesh_access_message_handler((mesh_pdu_t*) transport_pdu);
        } else {
            printf("[!] Unhandled Segmented Access/Control message\n");
            // done
            mesh_upper_transport_process_segmented_message_done(transport_pdu);
        }
        
        printf("\n");

    } else {
        uint8_t akf = transport_pdu->akf_aid & 0x40;
        if (akf){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_segmented_message(transport_pdu);
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_segmented_message_done(transport_pdu);
        }
    }
}

void mesh_upper_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu){
    crypto_active = 0;
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            mesh_upper_transport_process_unsegmented_message_done((mesh_network_pdu_t *) pdu);
            break;
        case MESH_PDU_TYPE_TRANSPORT:
            mesh_upper_transport_process_segmented_message_done((mesh_transport_pdu_t *) pdu);
            break;
        default:
            break;
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

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_unsegmented_message_done(network_pdu);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_unsegmented_setup_application_nonce(application_nonce, network_pdu_in_validation);
    } else {
        transport_unsegmented_setup_device_nonce(application_nonce, network_pdu_in_validation);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    network_pdu->appkey_index = message_key->appkey_index;

    // unsegmented message have TransMIC of 32 bit
    uint8_t trans_mic_len = 4;
    printf("Unsegmented Access message with TransMIC len 4\n");

    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9;
    uint8_t * upper_transport_pdu_data = &network_pdu_in_validation->data[10];
    uint8_t   upper_transport_pdu_len  = lower_transport_pdu_len - 1 - trans_mic_len;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_network_dst(network_pdu))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, trans_mic_len);
    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t*) mesh_transport_key_it.address->label_uuid, aad_len, &mesh_upper_transport_validate_unsegmented_message_digest, network_pdu);
    } else {
        mesh_upper_transport_validate_unsegmented_message_digest(network_pdu);
    }
}

static void mesh_upper_transport_validate_segmented_message(mesh_transport_pdu_t * transport_pdu){
    uint8_t * upper_transport_pdu_data =  transport_pdu->data;
    uint8_t   upper_transport_pdu_len  =  transport_pdu->len - transport_pdu->transmic_len;

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_segmented_message_done(transport_pdu);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_segmented_setup_application_nonce(application_nonce, transport_pdu_in_validation);
    } else {
        transport_segmented_setup_device_nonce(application_nonce, transport_pdu_in_validation);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    transport_pdu->appkey_index = message_key->appkey_index;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_transport_dst(transport_pdu))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, transport_pdu->transmic_len);

    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t *) mesh_transport_key_it.address->label_uuid, aad_len, &mesh_upper_transport_validate_segmented_message_digest, transport_pdu);
    } else {
        mesh_upper_transport_validate_segmented_message_digest(transport_pdu);
    }
}

static void mesh_upper_transport_process_unsegmented_access_message(mesh_network_pdu_t *network_pdu){
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

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, mesh_network_dst(network_pdu),
            network_pdu->netkey_index, akf, aid);
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

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, mesh_transport_dst(transport_pdu),
            transport_pdu->netkey_index, akf, aid);
    mesh_upper_transport_validate_segmented_message(transport_pdu);
}

void mesh_upper_transport_message_received(mesh_pdu_t * pdu){
    btstack_linked_list_add_tail(&upper_transport_incoming, (btstack_linked_item_t*) pdu);
    mesh_transport_run();
}

void mesh_upper_transport_pdu_free(mesh_pdu_t * pdu){
    mesh_network_pdu_t * network_pdu;
    mesh_transport_pdu_t * transport_pdu;
    switch (pdu->pdu_type) {
        case MESH_PDU_TYPE_NETWORK:
            network_pdu = (mesh_network_pdu_t *) pdu;
            mesh_network_pdu_free(network_pdu);
            break;
        case MESH_PDU_TYPE_TRANSPORT:
            transport_pdu = (mesh_transport_pdu_t *) pdu;
            mesh_transport_pdu_free(transport_pdu);
            break;
        default:
            break;
    }
}

void mesh_upper_transport_pdu_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    switch (callback_type){
        case MESH_TRANSPORT_PDU_RECEIVED:
            mesh_upper_transport_message_received(pdu);
            break;
        case MESH_TRANSPORT_PDU_SENT:
            // notify upper layer (or just free pdu)
            if (higher_layer_handler){
                higher_layer_handler(callback_type, status, pdu);
            } else {
                mesh_upper_transport_pdu_free(pdu);
            }
            break;
        default:
            break;
    }
}
static void mesh_upper_transport_send_unsegmented_access_pdu_ccm(void * arg){
    crypto_active = 0;

    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;
    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;
    mesh_print_hex("EncAccessPayload", upper_transport_pdu, upper_transport_pdu_len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &upper_transport_pdu[upper_transport_pdu_len]);
    mesh_print_hex("TransMIC", &upper_transport_pdu[upper_transport_pdu_len], 4);
    network_pdu->len += 4;
    // send network pdu
    mesh_lower_transport_send_pdu((mesh_pdu_t*) network_pdu);
}

static void mesh_upper_transport_send_segmented_access_pdu_ccm(void * arg){
    crypto_active = 0;

    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) arg;
    mesh_print_hex("EncAccessPayload", transport_pdu->data, transport_pdu->len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &transport_pdu->data[transport_pdu->len]);
    mesh_print_hex("TransMIC", &transport_pdu->data[transport_pdu->len], transport_pdu->transmic_len);
    transport_pdu->len += transport_pdu->transmic_len;
    mesh_lower_transport_send_pdu((mesh_pdu_t*) transport_pdu);
}

static uint8_t mesh_upper_transport_setup_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
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
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, mesh_lower_transport_next_seq(), src, dest, transport_pdu_data, transport_pdu_len);

    return 0;
}

static uint8_t mesh_upper_transport_setup_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    printf("[+] Upper transport, setup segmented Control PDU (opcode %02x): \n", opcode);
    printf_hexdump(control_pdu_data, control_pdu_len);

    if (control_pdu_len > 256) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint32_t seq = mesh_lower_transport_peek_seq();

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

uint8_t mesh_upper_transport_setup_control_pdu(mesh_pdu_t * pdu, uint16_t netkey_index,
                                               uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode, const uint8_t * control_pdu_data, uint16_t control_pdu_len){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            return mesh_upper_transport_setup_unsegmented_control_pdu((mesh_network_pdu_t *) pdu, netkey_index, ttl, src, dest, opcode, control_pdu_data, control_pdu_len);
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_upper_transport_setup_segmented_control_pdu((mesh_transport_pdu_t *) pdu, netkey_index, ttl, src, dest, opcode, control_pdu_data, control_pdu_len);
        default:
            return 1;
    }
}

static uint8_t mesh_upper_transport_setup_unsegmented_access_pdu_header(mesh_network_pdu_t * network_pdu, uint16_t netkey_index,
        uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest){

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

    network_pdu->data[9] = akf_aid;
    // setup network_pdu
    mesh_network_setup_pdu_header(network_pdu, netkey_index, network_key->nid, 0, ttl, mesh_lower_transport_next_seq(), src, dest);
    network_pdu->appkey_index = appkey_index;
    return 0;
}

static uint8_t mesh_upper_transport_setup_unsegmented_access_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                                                       const uint8_t * access_pdu_data, uint8_t access_pdu_len){

    int status = mesh_upper_transport_setup_unsegmented_access_pdu_header(network_pdu, netkey_index, appkey_index, ttl, src, dest);
    if (status) return status;

    printf("[+] Upper transport, setup unsegmented Access PDU - seq %06x\n", mesh_network_seq(network_pdu));
    mesh_print_hex("Access Payload", access_pdu_data, access_pdu_len);

    // store in transport pdu
    memcpy(&network_pdu->data[10], access_pdu_data, access_pdu_len);
    network_pdu->len = 10 + access_pdu_len;
    return 0;
}

static uint8_t mesh_upper_transport_setup_segmented_access_pdu_header(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                                                        uint8_t szmic){
    uint32_t seq = mesh_lower_transport_peek_seq();

    printf("[+] Upper transport, setup segmented Access PDU - seq %06x, szmic %u, iv_index %08x\n", seq, szmic,
           mesh_get_iv_index_for_tx());
    mesh_print_hex("Access Payload", transport_pdu->data, transport_pdu->len);

    // get app or device key
    const mesh_transport_key_t *appkey;
    appkey = mesh_transport_key_get(appkey_index);
    if (appkey == NULL) {
        printf("appkey_index %x unknown\n", appkey_index);
        return 1;
    }
    uint8_t akf_aid = (appkey->akf << 6) | appkey->aid;

    // lookup network by netkey_index
    const mesh_network_key_t *network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    const uint8_t trans_mic_len = szmic ? 8 : 4;

    // store in transport pdu
    transport_pdu->transmic_len = trans_mic_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->appkey_index = appkey_index;
    transport_pdu->akf_aid = akf_aid;
    mesh_transport_set_nid_ivi(transport_pdu, network_key->nid | ((mesh_get_iv_index_for_tx() & 1) << 7));
    mesh_transport_set_seq(transport_pdu, seq);
    mesh_transport_set_src(transport_pdu, src);
    mesh_transport_set_dest(transport_pdu, dest);
    mesh_transport_set_ctl_ttl(transport_pdu, ttl);
    return 0;
}


static uint8_t mesh_upper_transport_setup_segmented_access_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                          uint8_t szmic, const uint8_t * access_pdu_data, uint8_t access_pdu_len){
    int status = mesh_upper_transport_setup_segmented_access_pdu_header(transport_pdu, netkey_index, appkey_index, ttl, src, dest, szmic);
    if (status) return status;

    // store in transport pdu
    memcpy(transport_pdu->data, access_pdu_data, access_pdu_len);
    transport_pdu->len = access_pdu_len;
    return 0;
}
uint8_t mesh_upper_transport_setup_access_pdu_header(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                              uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            return mesh_upper_transport_setup_unsegmented_access_pdu_header((mesh_network_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest);
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_upper_transport_setup_segmented_access_pdu_header((mesh_transport_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest, szmic);
        default:
            return 1;
    }
}

uint8_t mesh_upper_transport_setup_access_pdu(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                              uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic,
                                              const uint8_t * access_pdu_data, uint8_t access_pdu_len){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            return mesh_upper_transport_setup_unsegmented_access_pdu((mesh_network_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest, access_pdu_data, access_pdu_len);
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_upper_transport_setup_segmented_access_pdu((mesh_transport_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest, szmic, access_pdu_data, access_pdu_len);
        default:
            return 1;
    }
}

void mesh_upper_transport_send_control_pdu(mesh_pdu_t * pdu){
    mesh_lower_transport_send_pdu((mesh_pdu_t*) pdu);
}

static void mesh_upper_transport_send_unsegmented_access_pdu_digest(void * arg){
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;
    uint8_t * access_pdu_data = mesh_network_pdu_data(network_pdu) + 1;
    uint16_t  access_pdu_len  = mesh_network_pdu_len(network_pdu)  - 1;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, access_pdu_data, access_pdu_data, &mesh_upper_transport_send_unsegmented_access_pdu_ccm, network_pdu);
}

static mesh_transport_key_t * mesh_upper_transport_get_outgoing_appkey(uint16_t netkey_index, uint16_t appkey_index){
    // Device Key is fixed
    if (appkey_index == MESH_DEVICE_KEY_INDEX) {
        return mesh_transport_key_get(appkey_index);
    }

    // Get key refresh state from subnet
    mesh_subnet_t * subnet = mesh_subnet_get_by_netkey_index(netkey_index);
    if (subnet == NULL) return NULL;

    // identify old and new app keys for given appkey_index
    mesh_transport_key_t * old_key = NULL;
    mesh_transport_key_t * new_key = NULL;
    mesh_transport_key_iterator_t it;
    mesh_transport_key_iterator_init(&it, netkey_index);
    while (mesh_transport_key_iterator_has_more(&it)){
        mesh_transport_key_t * transport_key = mesh_transport_key_iterator_get_next(&it);
        if (transport_key->appkey_index != appkey_index) continue;
        if (transport_key->old_key == 0) {
            new_key = transport_key;
        } else {
            old_key = transport_key;
        }
    }

    // if no key is marked as old, just use the current one
    if (old_key == NULL) return new_key;

    // use new key if it exists in phase two
    if ((subnet->key_refresh == MESH_KEY_REFRESH_SECOND_PHASE) && (new_key != NULL)){
        return new_key;
    } else {
        return old_key;
    }
}

static void mesh_upper_transport_send_unsegmented_access_pdu(mesh_network_pdu_t * network_pdu){

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
    const mesh_transport_key_t * appkey = mesh_upper_transport_get_outgoing_appkey(network_pdu->netkey_index, appkey_index);
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   trans_mic_len = 4;
    uint16_t  access_pdu_len  = mesh_network_pdu_len(network_pdu)  - 1;
    crypto_active = 1;

    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, trans_mic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16, &mesh_upper_transport_send_unsegmented_access_pdu_digest, network_pdu);
    } else {
        mesh_upper_transport_send_unsegmented_access_pdu_digest(network_pdu);    
    }
}

static void mesh_upper_transport_send_segmented_access_pdu_digest(void *arg){
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) arg;
    uint16_t  access_pdu_len  = transport_pdu->len;
    uint8_t * access_pdu_data = transport_pdu->data;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len,access_pdu_data, access_pdu_data, &mesh_upper_transport_send_segmented_access_pdu_ccm, transport_pdu);
}

static void mesh_upper_transport_send_segmented_access_pdu(mesh_transport_pdu_t * transport_pdu){

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
    const mesh_transport_key_t * appkey = mesh_upper_transport_get_outgoing_appkey(transport_pdu->netkey_index, appkey_index);
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   transmic_len    = transport_pdu->transmic_len;
    uint16_t  access_pdu_len  = transport_pdu->len;
    crypto_active = 1;
    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, transmic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16, &mesh_upper_transport_send_segmented_access_pdu_digest, transport_pdu);
    } else {
        mesh_upper_transport_send_segmented_access_pdu_digest(transport_pdu);
    }
}

void mesh_upper_transport_send_access_pdu(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
            mesh_upper_transport_send_unsegmented_access_pdu((mesh_network_pdu_t *) pdu);
            break;
        case MESH_PDU_TYPE_TRANSPORT:
            mesh_upper_transport_send_segmented_access_pdu((mesh_transport_pdu_t *) pdu);
            break;
        default:
            break;
    }
}

void mesh_upper_transport_set_primary_element_address(uint16_t unicast_address){
    primary_element_address = unicast_address;
}

void mesh_upper_transport_register_access_message_handler(void (*callback)(mesh_pdu_t *pdu)){
    mesh_access_message_handler = callback;
}

void mesh_upper_transport_register_control_message_handler(void (*callback)(mesh_pdu_t *pdu)){
    mesh_control_message_handler = callback;
}

void mesh_upper_transport_set_higher_layer_handler(void (*pdu_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu)){
    higher_layer_handler = pdu_handler;
}

void mesh_upper_transport_init(){
    mesh_lower_transport_init();
    mesh_lower_transport_set_higher_layer_handler(&mesh_upper_transport_pdu_handler);
}

static void mesh_transport_run(void){
    while(!btstack_linked_list_empty(&upper_transport_incoming)){

        if (crypto_active) return;

        // peek at next message
        mesh_pdu_t * pdu =  (mesh_pdu_t *) btstack_linked_list_get_first_item(&upper_transport_incoming);
        mesh_transport_pdu_t * transport_pdu;
        mesh_network_pdu_t   * network_pdu;
        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_NETWORK:
                network_pdu = (mesh_network_pdu_t *) pdu;
                // control?
                if (mesh_network_control(network_pdu)) {
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_upper_unsegmented_control_message_received(network_pdu);
                } else {
                    mesh_network_pdu_t * decode_pdu = mesh_network_pdu_get();
                    if (!decode_pdu) return;
                    // get encoded network pdu and start processing
                    network_pdu_in_validation = network_pdu;
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_upper_transport_process_unsegmented_access_message(decode_pdu);
                }
                break;
            case MESH_PDU_TYPE_TRANSPORT:
                transport_pdu = (mesh_transport_pdu_t *) pdu;
                uint8_t ctl = mesh_transport_ctl(transport_pdu);
                if (ctl){
                    printf("Ignoring Segmented Control Message\n");
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *) transport_pdu);
                } else {
                    mesh_transport_pdu_t * decode_pdu = mesh_transport_pdu_get();
                    if (!decode_pdu) return;
                    // get encoded transport pdu and start processing
                    transport_pdu_in_validation = transport_pdu;
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_upper_transport_process_message(decode_pdu);
                }
                break;
            default:
                break;
        }
    }
}

// buffer pool
mesh_transport_pdu_t * mesh_transport_pdu_get(void){
    mesh_transport_pdu_t * transport_pdu = btstack_memory_mesh_transport_pdu_get();
    if (transport_pdu) {
        memset(transport_pdu, 0, sizeof(mesh_transport_pdu_t));
        transport_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_TRANSPORT;
    }
    return transport_pdu;
}

void mesh_transport_pdu_free(mesh_transport_pdu_t * transport_pdu){
    btstack_memory_mesh_transport_pdu_free(transport_pdu);
}

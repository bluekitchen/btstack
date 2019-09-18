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

#define __BTSTACK_FILE__ "mesh_upper_transport.c"

#include "mesh/mesh_upper_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_util.h"
#include "btstack_memory.h"

#include "mesh/beacon.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_peer.h"
#include "mesh/mesh_virtual_addresses.h"

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

static void mesh_upper_transport_validate_unsegmented_message(void);
static void mesh_upper_transport_validate_segmented_message(void);

static void mesh_transport_run(void);

static int crypto_active;
static mesh_network_pdu_t   * incoming_network_pdu_raw;
static mesh_network_pdu_t   * incoming_network_pdu_decoded;
static mesh_transport_pdu_t * incoming_transport_pdu_raw;
static mesh_transport_pdu_t * incoming_transport_pdu_decoded;
static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static mesh_transport_key_and_virtual_address_iterator_t mesh_transport_key_it;

// upper transport callbacks - in access layer
static void (*mesh_access_message_handler)(mesh_pdu_t * pdu);
static void (*mesh_control_message_handler)(mesh_pdu_t * pdu);

// unsegmented (network) and segmented (transport) control and access messages
static btstack_linked_list_t upper_transport_incoming;

static void mesh_upper_transport_dump_pdus(const char *name, btstack_linked_list_t *list){
    printf("List: %s:\n", name);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_pdu_t * pdu = (mesh_pdu_t*) btstack_linked_list_iterator_next(&it);
        printf("- %p\n", pdu);
        // printf_hexdump( mesh_pdu_data(pdu), mesh_pdu_len(pdu));
    }
}

static void mesh_upper_transport_reset_pdus(btstack_linked_list_t *list){
    while (!btstack_linked_list_empty(list)){
        mesh_pdu_t * pdu = (mesh_pdu_t *) btstack_linked_list_pop(list);
        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_NETWORK:
                btstack_memory_mesh_network_pdu_free((mesh_network_pdu_t *) pdu);
                break;
            case MESH_PDU_TYPE_TRANSPORT:
                btstack_memory_mesh_transport_pdu_free((mesh_transport_pdu_t *) pdu);
                break;
            default:
                break;
        }
    }
}

void mesh_upper_transport_dump(void){
    printf("incoming_network_pdu_raw: %p\n", incoming_network_pdu_raw);
    printf("incoming_network_pdu_decoded: %p\n", incoming_network_pdu_decoded);
    mesh_upper_transport_dump_pdus("upper_transport_incoming", &upper_transport_incoming);
}

void mesh_upper_transport_reset(void){
    crypto_active = 0;
    if (incoming_network_pdu_raw){
        mesh_network_pdu_free(incoming_network_pdu_raw);
        incoming_network_pdu_raw = NULL;
    }
    // if (incoming_network_pdu_decoded){
    //     mesh_network_pdu_free(incoming_network_pdu_decoded);
    //     incoming_network_pdu_decoded = NULL;
    // }
    mesh_upper_transport_reset_pdus(&upper_transport_incoming);
}

static void mesh_upper_unsegmented_control_message_received(mesh_network_pdu_t * network_pdu){
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
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *) incoming_network_pdu_raw);
        incoming_network_pdu_raw = NULL;
    }
    mesh_transport_run();
}

static void mesh_upper_transport_process_segmented_message_done(mesh_transport_pdu_t *transport_pdu){
    crypto_active = 0;
    if (mesh_transport_ctl(transport_pdu)) {
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)transport_pdu);
    } else {
        mesh_transport_pdu_free(transport_pdu);
        mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)incoming_transport_pdu_raw);
        incoming_transport_pdu_raw = NULL;
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
    UNUSED(arg);

    uint8_t * lower_transport_pdu = mesh_network_pdu_data(incoming_network_pdu_decoded);
    uint8_t trans_mic_len = 4;

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, trans_mic_len);

    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(incoming_network_pdu_decoded) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(incoming_network_pdu_decoded)  - 1;

    mesh_print_hex("Decryted PDU", upper_transport_pdu, upper_transport_pdu_len - trans_mic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len - trans_mic_len], trans_mic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        incoming_network_pdu_decoded->len -= trans_mic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(mesh_network_dst(incoming_network_pdu_decoded))){
            big_endian_store_16(incoming_network_pdu_decoded->data, 7, mesh_transport_key_it.address->pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_message_handler){
            mesh_pdu_t * pdu = (mesh_pdu_t*) incoming_network_pdu_decoded;
            incoming_network_pdu_decoded = NULL;
            mesh_access_message_handler(pdu);
        } else {
            printf("[!] Unhandled Unsegmented Access message\n");
            // done
            mesh_upper_transport_process_unsegmented_message_done(incoming_network_pdu_decoded);
        }
        
        printf("\n");
    } else {
        uint8_t afk = lower_transport_pdu[0] & 0x40;
        if (afk){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_unsegmented_message();
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_unsegmented_message_done(incoming_network_pdu_decoded);
        }
    }
}

static void mesh_upper_transport_validate_segmented_message_ccm(void * arg){
    UNUSED(arg);

    uint8_t * upper_transport_pdu     =  incoming_transport_pdu_decoded->data;
    uint8_t   upper_transport_pdu_len =  incoming_transport_pdu_decoded->len - incoming_transport_pdu_decoded->transmic_len;
 
    mesh_print_hex("Decrypted PDU", upper_transport_pdu, upper_transport_pdu_len);

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, incoming_transport_pdu_decoded->transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], incoming_transport_pdu_decoded->transmic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        incoming_transport_pdu_decoded->len -= incoming_transport_pdu_decoded->transmic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(mesh_transport_dst(incoming_transport_pdu_decoded))){
            big_endian_store_16(incoming_transport_pdu_decoded->network_header, 7, mesh_transport_key_it.address->pseudo_dst);
        }

        // pass to upper layer
        if (mesh_access_message_handler){
            mesh_pdu_t * pdu = (mesh_pdu_t*) incoming_transport_pdu_decoded;
            incoming_network_pdu_decoded = NULL;
            mesh_access_message_handler(pdu);
        } else {
            printf("[!] Unhandled Segmented Access/Control message\n");
            // done
            mesh_upper_transport_process_segmented_message_done(incoming_transport_pdu_decoded);
        }
        
        printf("\n");

    } else {
        uint8_t akf = incoming_transport_pdu_decoded->akf_aid_control & 0x40;
        if (akf){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_segmented_message();
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_segmented_message_done(incoming_transport_pdu_decoded);
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
    UNUSED(arg);
    uint8_t   upper_transport_pdu_len      = incoming_transport_pdu_raw->len - incoming_transport_pdu_raw->transmic_len;
    uint8_t * upper_transport_pdu_data_in  = incoming_transport_pdu_raw->data;
    uint8_t * upper_transport_pdu_data_out = incoming_transport_pdu_decoded->data;
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_in, upper_transport_pdu_data_out, &mesh_upper_transport_validate_segmented_message_ccm, NULL);
}

static void mesh_upper_transport_validate_unsegmented_message_digest(void * arg){
    UNUSED(arg);
    uint8_t   trans_mic_len = 4;
    uint8_t   lower_transport_pdu_len      = incoming_network_pdu_raw->len - 9;
    uint8_t * upper_transport_pdu_data_in  = &incoming_network_pdu_raw->data[10];
    uint8_t * upper_transport_pdu_data_out = &incoming_network_pdu_decoded->data[10];
    uint8_t   upper_transport_pdu_len      = lower_transport_pdu_len - 1 - trans_mic_len;
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_in, upper_transport_pdu_data_out, &mesh_upper_transport_validate_unsegmented_message_ccm, NULL);
}

static void mesh_upper_transport_validate_unsegmented_message(void){

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_unsegmented_message_done(incoming_network_pdu_decoded);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_unsegmented_setup_application_nonce(application_nonce, incoming_network_pdu_raw);
    } else {
        transport_unsegmented_setup_device_nonce(application_nonce, incoming_network_pdu_raw);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    incoming_network_pdu_decoded->appkey_index = message_key->appkey_index;

    // unsegmented message have TransMIC of 32 bit
    uint8_t trans_mic_len = 4;
    printf("Unsegmented Access message with TransMIC len 4\n");

    uint8_t   lower_transport_pdu_len = incoming_network_pdu_raw->len - 9;
    uint8_t * upper_transport_pdu_data = &incoming_network_pdu_raw->data[10];
    uint8_t   upper_transport_pdu_len  = lower_transport_pdu_len - 1 - trans_mic_len;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_network_dst(incoming_network_pdu_decoded))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, trans_mic_len);
    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t*) mesh_transport_key_it.address->label_uuid, aad_len, &mesh_upper_transport_validate_unsegmented_message_digest, NULL);
    } else {
        mesh_upper_transport_validate_unsegmented_message_digest(incoming_network_pdu_decoded);
    }
}

static void mesh_upper_transport_validate_segmented_message(void){
    uint8_t * upper_transport_pdu_data =  incoming_transport_pdu_decoded->data;
    uint8_t   upper_transport_pdu_len  =  incoming_transport_pdu_decoded->len - incoming_transport_pdu_decoded->transmic_len;

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_segmented_message_done(incoming_transport_pdu_decoded);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_segmented_setup_application_nonce(application_nonce, incoming_transport_pdu_raw);
    } else {
        transport_segmented_setup_device_nonce(application_nonce, incoming_transport_pdu_raw);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    incoming_transport_pdu_decoded->appkey_index = message_key->appkey_index;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_transport_dst(incoming_transport_pdu_decoded))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, incoming_transport_pdu_decoded->transmic_len);

    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t *) mesh_transport_key_it.address->label_uuid, aad_len, &mesh_upper_transport_validate_segmented_message_digest, NULL);
    } else {
        mesh_upper_transport_validate_segmented_message_digest(NULL);
    }
}

static void mesh_upper_transport_process_unsegmented_access_message(void){
    // copy original pdu
    incoming_network_pdu_decoded->len = incoming_network_pdu_raw->len;
    memcpy(incoming_network_pdu_decoded->data, incoming_network_pdu_raw->data, incoming_network_pdu_decoded->len);

    // 
    uint8_t * lower_transport_pdu     = &incoming_network_pdu_raw->data[9];
    uint8_t   lower_transport_pdu_len = incoming_network_pdu_raw->len - 9;

    mesh_print_hex("Lower Transport network pdu", &incoming_network_pdu_raw->data[9], lower_transport_pdu_len);

    uint8_t aid =  lower_transport_pdu[0] & 0x3f;
    uint8_t akf = (lower_transport_pdu[0] & 0x40) >> 6;
    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, mesh_network_dst(incoming_network_pdu_decoded),
            incoming_network_pdu_decoded->netkey_index, akf, aid);
    mesh_upper_transport_validate_unsegmented_message();
}

static void mesh_upper_transport_process_message(void){
    // copy original pdu
    memcpy(incoming_transport_pdu_decoded, incoming_transport_pdu_raw, sizeof(mesh_transport_pdu_t));

    // 
    uint8_t * upper_transport_pdu     =  incoming_transport_pdu_decoded->data;
    uint8_t   upper_transport_pdu_len =  incoming_transport_pdu_decoded->len - incoming_transport_pdu_decoded->transmic_len;
    mesh_print_hex("Upper Transport pdu", upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid =  incoming_transport_pdu_decoded->akf_aid_control & 0x3f;
    uint8_t akf = (incoming_transport_pdu_decoded->akf_aid_control & 0x40) >> 6;

    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, mesh_transport_dst(incoming_transport_pdu_decoded),
            incoming_transport_pdu_decoded->netkey_index, akf, aid);
    mesh_upper_transport_validate_segmented_message();
}

static void mesh_upper_transport_message_received(mesh_pdu_t * pdu){
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

static void mesh_upper_transport_pdu_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
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
    mesh_print_hex("UpperTransportPDU", upper_transport_pdu, network_pdu->len);
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
    mesh_print_hex("UpperTransportPDU", transport_pdu->data, transport_pdu->len);
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
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, mesh_sequence_number_next(), src, dest, transport_pdu_data, transport_pdu_len);

    return 0;
}

static uint8_t mesh_upper_transport_setup_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    printf("[+] Upper transport, setup segmented Control PDU (opcode %02x): \n", opcode);
    printf_hexdump(control_pdu_data, control_pdu_len);

    if (control_pdu_len > 256) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint32_t seq = mesh_sequence_number_next();

    memcpy(transport_pdu->data, control_pdu_data, control_pdu_len);
    transport_pdu->len = control_pdu_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->akf_aid_control = opcode;
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
    mesh_network_setup_pdu_header(network_pdu, netkey_index, network_key->nid, 0, ttl, mesh_sequence_number_next(), src, dest);
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

static uint8_t mesh_upper_transport_setup_segmented_access_pdu_header(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index,
    uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic){

    // get app or device key
    const mesh_transport_key_t *appkey;
    appkey = mesh_transport_key_get(appkey_index);
    if (appkey == NULL) {
        printf("[!] Upper transport, setup segmented Access PDU - appkey_index %x unknown\n", appkey_index);
        return 1;
    }
    uint8_t akf_aid = (appkey->akf << 6) | appkey->aid;

    // lookup network by netkey_index
    const mesh_network_key_t *network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;
    if (network_key == NULL) {
        printf("[!] Upper transport, setup segmented Access PDU - netkey_index %x unknown\n", appkey_index);
        return 1;
    }

    const uint8_t trans_mic_len = szmic ? 8 : 4;

    // lower transport will call next sequence number. Only peek here to use same seq for access payload encryption as well as for first network pdu (unit test)
    uint32_t seq = mesh_sequence_number_peek();

    // store in transport pdu
    transport_pdu->transmic_len = trans_mic_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->appkey_index = appkey_index;
    transport_pdu->akf_aid_control = akf_aid;
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

    printf("[+] Upper transport, setup segmented Access PDU - seq %06x, szmic %u, iv_index %08x\n", mesh_transport_seq(transport_pdu), szmic, mesh_get_iv_index_for_tx());
    mesh_print_hex("Access Payload", transport_pdu->data, transport_pdu->len);
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
                    incoming_network_pdu_decoded = mesh_network_pdu_get();
                    if (!incoming_network_pdu_decoded) return;
                    // get encoded network pdu and start processing
                    incoming_network_pdu_raw = network_pdu;
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_upper_transport_process_unsegmented_access_message();
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
                    incoming_transport_pdu_decoded = mesh_transport_pdu_get();
                    if (!incoming_transport_pdu_decoded) return;
                    // get encoded transport pdu and start processing
                    incoming_transport_pdu_raw = transport_pdu;
                    (void) btstack_linked_list_pop(&upper_transport_incoming);
                    mesh_upper_transport_process_message();
                }
                break;
            default:
                break;
        }
    }
}

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

#define BTSTACK_FILE__ "mesh_upper_transport.c"

#include "mesh/mesh_upper_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_util.h"
#include "btstack_memory.h"
#include "btstack_debug.h"

#include "mesh/beacon.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_peer.h"
#include "mesh/mesh_virtual_addresses.h"

// TODO: extract mesh_pdu functions into lower transport or network
#include "mesh/mesh_access.h"

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

static void mesh_upper_transport_validate_segmented_message(void);
static void mesh_upper_transport_run(void);

static int crypto_active;

static mesh_unsegmented_pdu_t * incoming_unsegmented_pdu_raw;

static mesh_segmented_pdu_t     incoming_message_pdu_singleton;

static mesh_access_pdu_t *      incoming_access_pdu_encrypted;
static mesh_access_pdu_t *      incoming_access_pdu_decrypted;

static mesh_access_pdu_t        incoming_access_pdu_encrypted_singleton;
static mesh_access_pdu_t        incoming_access_pdu_decrypted_singleton;

static mesh_control_pdu_t       incoming_control_pdu_singleton;
static mesh_control_pdu_t *     incoming_control_pdu;

static mesh_segmented_pdu_t     outgoing_segmented_message_singleton;
static mesh_access_pdu_t *      outgoing_segmented_access_pdu;

static mesh_unsegmented_pdu_t   outgoing_unsegmented_pdu_singleton;
static mesh_upper_transport_pdu_t  * outgoing_upper_transport_pdu;

static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static uint8_t crypto_buffer[MESH_ACCESS_PAYLOAD_MAX];
static mesh_transport_key_and_virtual_address_iterator_t mesh_transport_key_it;

// upper transport callbacks - in access layer
static void (*mesh_access_message_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);
static void (*mesh_control_message_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);

// incoming unsegmented (network) and segmented (transport) control and access messages
static btstack_linked_list_t upper_transport_incoming;

// outgoing unsegmented (network) and segmented (uppert_transport_outgoing) control and access messages
static btstack_linked_list_t upper_transport_outgoing;


// TODO: higher layer define used for assert
#define MESH_ACCESS_OPCODE_NOT_SET 0xFFFFFFFEu

void mesh_upper_transport_send_access_pdu(mesh_pdu_t *pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            break;
        default:
            btstack_assert(false);
            break;
    }

    btstack_linked_list_add_tail(&upper_transport_outgoing, (btstack_linked_item_t*) pdu);
    mesh_upper_transport_run();
}

void mesh_upper_transport_send_control_pdu(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL:
            break;
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
            btstack_assert( ((mesh_network_pdu_t *) pdu)->len >= 9);
            break;
        default:
            btstack_assert(false);
            break;
    }

    btstack_linked_list_add_tail(&upper_transport_outgoing, (btstack_linked_item_t*) pdu);
    mesh_upper_transport_run();
}

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}
// static void mesh_print_x(const char * name, uint32_t value){
//     printf("%20s: 0x%x", name, (int) value);
// }

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
        while (true){
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

uint16_t mesh_access_dst(mesh_access_pdu_t * access_pdu){
    return big_endian_read_16(access_pdu->network_header, 7);
}

uint16_t mesh_access_ctl(mesh_access_pdu_t * access_pdu){
    return access_pdu->network_header[1] >> 7;
}

uint32_t mesh_access_seq(mesh_access_pdu_t * access_pdu){
    return big_endian_read_24(access_pdu->network_header, 2);
}

void mesh_access_set_nid_ivi(mesh_access_pdu_t * access_pdu, uint8_t nid_ivi){
    access_pdu->network_header[0] = nid_ivi;
}
void mesh_access_set_ctl_ttl(mesh_access_pdu_t * access_pdu, uint8_t ctl_ttl){
    access_pdu->network_header[1] = ctl_ttl;
}
void mesh_access_set_seq(mesh_access_pdu_t * access_pdu, uint32_t seq){
    big_endian_store_24(access_pdu->network_header, 2, seq);
}
void mesh_access_set_src(mesh_access_pdu_t * access_pdu, uint16_t src){
    big_endian_store_16(access_pdu->network_header, 5, src);
}
void mesh_access_set_dest(mesh_access_pdu_t * access_pdu, uint16_t dest){
    big_endian_store_16(access_pdu->network_header, 7, dest);
}

static void mesh_segmented_pdu_flatten(btstack_linked_list_t * segments, uint8_t segment_len, uint8_t * buffer) {
    // assemble payload
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, segments);
    while (btstack_linked_list_iterator_has_next(&it)) {
        mesh_network_pdu_t *segment = (mesh_network_pdu_t *) btstack_linked_list_iterator_next(&it);
        btstack_assert(segment->pdu_header.pdu_type == MESH_PDU_TYPE_NETWORK);
        // get segment n
        uint8_t *lower_transport_pdu = mesh_network_pdu_data(segment);
        uint8_t seg_o = (big_endian_read_16(lower_transport_pdu, 2) >> 5) & 0x001f;
        uint8_t *segment_data = &lower_transport_pdu[4];
        (void) memcpy(&buffer[seg_o * segment_len], segment_data, segment_len);
    }
}

static uint16_t mesh_upper_pdu_flatten(mesh_upper_transport_pdu_t * upper_pdu, uint8_t * buffer, uint16_t buffer_len) {
    // assemble payload
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &upper_pdu->segments);
    uint16_t offset = 0;
    while (btstack_linked_list_iterator_has_next(&it)) {
        mesh_network_pdu_t *segment = (mesh_network_pdu_t *) btstack_linked_list_iterator_next(&it);
        btstack_assert(segment->pdu_header.pdu_type == MESH_PDU_TYPE_NETWORK);
        btstack_assert((offset + segment->len) <= buffer_len);
        (void) memcpy(&buffer[offset], segment->data, segment->len);
        offset += segment->len;
    }
    return offset;
}

static void mesh_segmented_append_payload(const uint8_t * payload, uint16_t payload_len, btstack_linked_list_t * segments){
    uint16_t payload_offset = 0;
    uint16_t bytes_current_segment = 0;
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_get_last_item(segments);
    if (network_pdu){
        bytes_current_segment = MESH_NETWORK_PAYLOAD_MAX - network_pdu->len;
    }
    while (payload_offset < payload_len){
        if (bytes_current_segment == 0){
            network_pdu = mesh_network_pdu_get();
            btstack_assert(network_pdu != NULL);
            btstack_linked_list_add_tail(segments, (btstack_linked_item_t *) network_pdu);
            bytes_current_segment = MESH_NETWORK_PAYLOAD_MAX;
        }
        uint16_t bytes_to_copy = btstack_min(bytes_current_segment, payload_len - payload_offset);
        (void) memcpy(&network_pdu->data[network_pdu->len], &payload[payload_offset], bytes_to_copy);
        bytes_current_segment -= bytes_to_copy;
        network_pdu->len += bytes_to_copy;
        payload_offset += bytes_to_copy;
    }
}

// stub lower transport

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
        mesh_upper_transport_pdu_free((mesh_pdu_t *) btstack_linked_list_pop(list));
    }
}

void mesh_upper_transport_dump(void){
    printf("incoming_unsegmented_pdu_raw: %p\n", incoming_unsegmented_pdu_raw);
    mesh_upper_transport_dump_pdus("upper_transport_incoming", &upper_transport_incoming);
}

void mesh_upper_transport_reset(void){
    crypto_active = 0;
    if (incoming_unsegmented_pdu_raw){
        mesh_network_pdu_t * network_pdu = incoming_unsegmented_pdu_raw->segment;
        btstack_assert(network_pdu != NULL);
        incoming_unsegmented_pdu_raw->segment = NULL;
        mesh_network_pdu_free(network_pdu);
        incoming_unsegmented_pdu_raw = NULL;
    }
    outgoing_segmented_access_pdu = NULL;
    mesh_upper_transport_reset_pdus(&upper_transport_incoming);
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

static void transport_segmented_setup_nonce(uint8_t * nonce, const mesh_pdu_t * pdu){
    mesh_access_pdu_t * access_pdu;
    mesh_upper_transport_pdu_t * upper_pdu;
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_ACCESS:
            access_pdu = (mesh_access_pdu_t *) pdu;
            nonce[1] = access_pdu->transmic_len == 8 ? 0x80 : 0x00;
            (void)memcpy(&nonce[2], &access_pdu->network_header[2], 7);
            big_endian_store_32(nonce, 9, iv_index_for_ivi_nid(access_pdu->network_header[0]));
            break;
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            upper_pdu = (mesh_upper_transport_pdu_t *) pdu;
            nonce[1] = upper_pdu->transmic_len == 8 ? 0x80 : 0x00;
            // 'network header'
            big_endian_store_24(nonce, 2, upper_pdu->seq);
            big_endian_store_16(nonce, 5, upper_pdu->src);
            big_endian_store_16(nonce, 7, upper_pdu->dst);
            big_endian_store_32(nonce, 9, iv_index_for_ivi_nid(upper_pdu->ivi_nid));
            break;
        default:
            btstack_assert(0);
            break;
    }
}

static void transport_segmented_setup_application_nonce(uint8_t * nonce, const mesh_pdu_t * pdu){
    nonce[0] = 0x01;
    transport_segmented_setup_nonce(nonce, pdu);
    mesh_print_hex("AppNonce", nonce, 13);
}

static void transport_segmented_setup_device_nonce(uint8_t * nonce, const mesh_pdu_t * pdu){
    nonce[0] = 0x02;
    transport_segmented_setup_nonce(nonce, pdu);
    mesh_print_hex("DeviceNonce", nonce, 13);
}

static void mesh_upper_transport_process_message_done(mesh_segmented_pdu_t *message_pdu){
    crypto_active = 0;
    btstack_assert(message_pdu == &incoming_message_pdu_singleton);
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&incoming_message_pdu_singleton.segments);
    if (mesh_network_control(network_pdu)) {
        btstack_assert(0);
    } else {
        btstack_assert(network_pdu != NULL);
        mesh_network_pdu_free(network_pdu);
        mesh_pdu_t * pdu = (mesh_pdu_t *) incoming_unsegmented_pdu_raw;
        incoming_unsegmented_pdu_raw = NULL;
        mesh_lower_transport_message_processed_by_higher_layer(pdu);
    }
    mesh_upper_transport_run();
}

static void mesh_upper_transport_process_unsegmented_message_done(mesh_pdu_t * pdu){
    btstack_assert(pdu != NULL);
    btstack_assert(pdu->pdu_type == MESH_PDU_TYPE_UNSEGMENTED);

    mesh_unsegmented_pdu_t * unsegmented_incoming_pdu = (mesh_unsegmented_pdu_t *) pdu;
    btstack_assert(unsegmented_incoming_pdu == incoming_unsegmented_pdu_raw);

    crypto_active = 0;
    incoming_unsegmented_pdu_raw = NULL;
    mesh_network_pdu_t * network_pdu = unsegmented_incoming_pdu->segment;
    if (!mesh_network_control(network_pdu)) {
        mesh_network_pdu_free(network_pdu);
    }

    mesh_lower_transport_message_processed_by_higher_layer(pdu);
    mesh_upper_transport_run();
}

static void mesh_upper_transport_process_access_message_done(mesh_access_pdu_t *access_pdu){
    crypto_active = 0;
    btstack_assert(mesh_access_ctl(access_pdu) == 0);
    incoming_access_pdu_encrypted = NULL;
    mesh_upper_transport_run();
}

static void mesh_upper_transport_process_control_message_done(mesh_control_pdu_t * control_pdu){
    crypto_active = 0;
    incoming_control_pdu = NULL;
    mesh_upper_transport_run();
}

static void mesh_upper_transport_validate_segmented_message_ccm(void * arg){
    UNUSED(arg);

    uint8_t * upper_transport_pdu     = incoming_access_pdu_decrypted->data;
    uint8_t   upper_transport_pdu_len = incoming_access_pdu_decrypted->len - incoming_access_pdu_decrypted->transmic_len;
 
    mesh_print_hex("Decrypted PDU", upper_transport_pdu, upper_transport_pdu_len);

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, incoming_access_pdu_decrypted->transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], incoming_access_pdu_decrypted->transmic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        incoming_access_pdu_decrypted->len -= incoming_access_pdu_decrypted->transmic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(mesh_access_dst(incoming_access_pdu_decrypted))){
            big_endian_store_16(incoming_access_pdu_decrypted->network_header, 7, mesh_transport_key_it.address->pseudo_dst);
        }

        // pass to upper layer
        btstack_assert(mesh_access_message_handler != NULL);
        mesh_pdu_t * pdu = (mesh_pdu_t*) incoming_access_pdu_decrypted;
        mesh_access_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, pdu);

        printf("\n");

    } else {
        uint8_t akf = incoming_access_pdu_decrypted->akf_aid_control & 0x40;
        if (akf){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_segmented_message();
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_access_message_done(incoming_access_pdu_decrypted);
        }
    }
}

static void mesh_upper_transport_validate_segmented_message_digest(void * arg){
    UNUSED(arg);
    uint8_t   upper_transport_pdu_len      = incoming_access_pdu_encrypted->len - incoming_access_pdu_encrypted->transmic_len;
    uint8_t * upper_transport_pdu_data_in  = incoming_access_pdu_encrypted->data;
    uint8_t * upper_transport_pdu_data_out = incoming_access_pdu_decrypted->data;
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_in, upper_transport_pdu_data_out, &mesh_upper_transport_validate_segmented_message_ccm, NULL);
}

static void mesh_upper_transport_validate_segmented_message(void){
    uint8_t * upper_transport_pdu_data =  incoming_access_pdu_decrypted->data;
    uint8_t   upper_transport_pdu_len  = incoming_access_pdu_decrypted->len - incoming_access_pdu_decrypted->transmic_len;

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_access_message_done(incoming_access_pdu_decrypted);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_segmented_setup_application_nonce(application_nonce, (mesh_pdu_t *) incoming_access_pdu_encrypted);
    } else {
        transport_segmented_setup_device_nonce(application_nonce, (mesh_pdu_t *) incoming_access_pdu_encrypted);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    incoming_access_pdu_decrypted->appkey_index = message_key->appkey_index;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(mesh_access_dst(incoming_access_pdu_decrypted))){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, incoming_access_pdu_decrypted->transmic_len);

    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t *) mesh_transport_key_it.address->label_uuid, aad_len, &mesh_upper_transport_validate_segmented_message_digest, NULL);
    } else {
        mesh_upper_transport_validate_segmented_message_digest(NULL);
    }
}

static void mesh_upper_transport_process_segmented_message(void){
    // copy original pdu
    (void)memcpy(incoming_access_pdu_decrypted, incoming_access_pdu_encrypted,
                 sizeof(mesh_access_pdu_t));

    // 
    uint8_t * upper_transport_pdu     =  incoming_access_pdu_decrypted->data;
    uint8_t   upper_transport_pdu_len = incoming_access_pdu_decrypted->len - incoming_access_pdu_decrypted->transmic_len;
    mesh_print_hex("Upper Transport pdu", upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid = incoming_access_pdu_decrypted->akf_aid_control & 0x3f;
    uint8_t akf = (incoming_access_pdu_decrypted->akf_aid_control & 0x40) >> 6;

    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, mesh_access_dst(incoming_access_pdu_decrypted),
                                                         incoming_access_pdu_decrypted->netkey_index, akf, aid);
    mesh_upper_transport_validate_segmented_message();
}

static void mesh_upper_transport_message_received(mesh_pdu_t * pdu){
    btstack_linked_list_add_tail(&upper_transport_incoming, (btstack_linked_item_t*) pdu);
    mesh_upper_transport_run();
}

static void mesh_upper_transport_send_segmented_pdu(mesh_access_pdu_t * access_pdu){
    outgoing_segmented_access_pdu = access_pdu;
    mesh_segmented_pdu_t * message_pdu   = &outgoing_segmented_message_singleton;
    message_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;

    // convert mesh_access_pdu_t into mesh_segmented_pdu_t
    mesh_segmented_append_payload(access_pdu->data, access_pdu->len, &message_pdu->segments);

    // copy meta
    message_pdu->len = access_pdu->len;
    message_pdu->netkey_index = access_pdu->netkey_index;
    message_pdu->transmic_len = access_pdu->transmic_len;
    message_pdu->akf_aid_control = access_pdu->akf_aid_control;
    message_pdu->flags = access_pdu->flags;
    (void)memcpy(message_pdu->network_header, access_pdu->network_header, 9);

    mesh_lower_transport_send_pdu((mesh_pdu_t*) message_pdu);
}

static uint8_t mesh_upper_transport_setup_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    if (control_pdu_len > 11) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint8_t transport_pdu_data[12];
    transport_pdu_data[0] = opcode;
    (void)memcpy(&transport_pdu_data[1], control_pdu_data, control_pdu_len);
    uint16_t transport_pdu_len = control_pdu_len + 1;

    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, 0, src, dest, transport_pdu_data, transport_pdu_len);

    return 0;
}

#if 0
static uint8_t mesh_upper_transport_setup_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    if (control_pdu_len > 256) return 1;

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    (void)memcpy(transport_pdu->data, control_pdu_data, control_pdu_len);
    transport_pdu->len = control_pdu_len;
    transport_pdu->netkey_index = netkey_index;
    transport_pdu->akf_aid_control = opcode;
    transport_pdu->transmic_len = 0;    // no TransMIC for control
    mesh_transport_set_nid_ivi(transport_pdu, network_key->nid);
    mesh_transport_set_src(transport_pdu, src);
    mesh_transport_set_dest(transport_pdu, dest);
    mesh_transport_set_ctl_ttl(transport_pdu, 0x80 | ttl);

    return 0;
}
#endif

uint8_t mesh_upper_transport_setup_control_pdu(mesh_pdu_t * pdu, uint16_t netkey_index,
                                               uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode, const uint8_t * control_pdu_data, uint16_t control_pdu_len){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
            return mesh_upper_transport_setup_unsegmented_control_pdu((mesh_network_pdu_t *) pdu, netkey_index, ttl, src, dest, opcode, control_pdu_data, control_pdu_len);
        default:
            btstack_assert(0);
            return 1;
    }
}

static uint8_t mesh_upper_transport_setup_unsegmented_access_pdu_header(mesh_unsegmented_pdu_t * unsegmented_pdu, uint16_t netkey_index,
                                                                        uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest){

    mesh_network_pdu_t * network_pdu = unsegmented_pdu->segment;

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

    unsegmented_pdu->appkey_index = appkey_index;

    network_pdu->data[9] = akf_aid;
    // setup network_pdu
    mesh_network_setup_pdu_header(network_pdu, netkey_index, network_key->nid, 0, ttl, 0, src, dest);
    return 0;
}

static uint8_t mesh_upper_transport_setup_segmented_access_pdu_header(mesh_access_pdu_t * access_pdu, uint16_t netkey_index,
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

    // store in transport pdu
    access_pdu->transmic_len = trans_mic_len;
    access_pdu->netkey_index = netkey_index;
    access_pdu->appkey_index = appkey_index;
    access_pdu->akf_aid_control = akf_aid;
    mesh_access_set_nid_ivi(access_pdu, network_key->nid | ((mesh_get_iv_index_for_tx() & 1) << 7));
    mesh_access_set_src(access_pdu, src);
    mesh_access_set_dest(access_pdu, dest);
    mesh_access_set_ctl_ttl(access_pdu, ttl);
    return 0;
}

static uint8_t mesh_upper_transport_setup_upper_access_pdu_header(mesh_upper_transport_pdu_t * upper_pdu, uint16_t netkey_index,
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

    // store in transport pdu
    upper_pdu->ivi_nid = network_key->nid | ((mesh_get_iv_index_for_tx() & 1) << 7);
    upper_pdu->ctl_ttl = ttl;
    upper_pdu->src = src;
    upper_pdu->dst = dest;
    upper_pdu->transmic_len = trans_mic_len;
    upper_pdu->netkey_index = netkey_index;
    upper_pdu->appkey_index = appkey_index;
    upper_pdu->akf_aid_control = akf_aid;
    return 0;
}

static uint8_t mesh_upper_transport_setup_upper_access_pdu(mesh_upper_transport_pdu_t * upper_pdu, uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                                                           uint8_t szmic, const uint8_t * access_pdu_data, uint8_t access_pdu_len){
    int status = mesh_upper_transport_setup_upper_access_pdu_header(upper_pdu, netkey_index, appkey_index, ttl, src,
                                                                    dest, szmic);
    if (status) return status;

    // store in transport pdu
    uint16_t offset = 0;
    mesh_segmented_append_payload(access_pdu_data, access_pdu_len, &upper_pdu->segments);
    upper_pdu->len = access_pdu_len;
    return 0;
}


uint8_t mesh_upper_transport_setup_access_pdu_header(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                              uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_ACCESS:
            return mesh_upper_transport_setup_segmented_access_pdu_header((mesh_access_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest, szmic);
        case MESH_PDU_TYPE_UNSEGMENTED:
            return mesh_upper_transport_setup_unsegmented_access_pdu_header((mesh_unsegmented_pdu_t *) pdu, netkey_index, appkey_index, ttl, src, dest);
        default:
            btstack_assert(false);
            return 1;
    }
}

uint8_t mesh_upper_transport_setup_access_pdu(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                              uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic,
                                              const uint8_t * access_pdu_data, uint8_t access_pdu_len){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            return mesh_upper_transport_setup_upper_access_pdu((mesh_upper_transport_pdu_t *) pdu, netkey_index,
                                                               appkey_index, ttl, src, dest, szmic, access_pdu_data,
                                                               access_pdu_len);
        default:
            btstack_assert(false);
            return 1;
    }
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

static void mesh_upper_transport_send_upper_segmented_pdu(mesh_upper_transport_pdu_t * upper_pdu){

    // TODO: store upper pdu in outgoing pdus active or similar
    outgoing_upper_transport_pdu = upper_pdu;

    mesh_segmented_pdu_t * message_pdu   = &outgoing_segmented_message_singleton;
    message_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;

    // convert mesh_access_pdu_t into mesh_segmented_pdu_t
    mesh_segmented_append_payload(crypto_buffer, upper_pdu->len, &message_pdu->segments);

    // copy meta
    message_pdu->len = upper_pdu->len;
    message_pdu->netkey_index = upper_pdu->netkey_index;
    message_pdu->transmic_len = upper_pdu->transmic_len;
    message_pdu->akf_aid_control = upper_pdu->akf_aid_control;
    message_pdu->flags = upper_pdu->flags;

    // setup message_pdu header
    // (void)memcpy(message_pdu->network_header, upper_pdu->network_header, 9);
    // TODO: use fields in mesh_segmented_pdu_t and setup network header in lower transport
    message_pdu->network_header[0] = upper_pdu->ivi_nid;
    message_pdu->network_header[1] = upper_pdu->ctl_ttl;
    big_endian_store_24(message_pdu->network_header, 2, upper_pdu->seq);
    big_endian_store_16(message_pdu->network_header, 5, upper_pdu->src);
    big_endian_store_16(message_pdu->network_header, 7, upper_pdu->dst);

    mesh_lower_transport_send_pdu((mesh_pdu_t*) message_pdu);
}

static void mesh_upper_transport_send_upper_unsegmented_pdu(mesh_upper_transport_pdu_t * upper_pdu){

    // TODO: store upper pdu in outgoing pdus active or similar
    outgoing_upper_transport_pdu = upper_pdu;

    mesh_unsegmented_pdu_t * unsegmented_pdu   = &outgoing_unsegmented_pdu_singleton;
    unsegmented_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_UNSEGMENTED;

    // provide segment
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    btstack_assert(network_pdu);
    unsegmented_pdu->segment = network_pdu;

    // setup network pdu
    network_pdu->data[0] = upper_pdu->ivi_nid;
    network_pdu->data[1] = upper_pdu->ctl_ttl;
    big_endian_store_24(network_pdu->data, 2, upper_pdu->seq);
    big_endian_store_16(network_pdu->data, 5, upper_pdu->src);
    big_endian_store_16(network_pdu->data, 7, upper_pdu->dst);
    network_pdu->netkey_index = upper_pdu->netkey_index;

    // setup acess message
    network_pdu->data[9] = upper_pdu->akf_aid_control;
    btstack_assert(upper_pdu->len < 15);
    (void)memcpy(&network_pdu->data[10], crypto_buffer, upper_pdu->len);
    network_pdu->len = 10 + upper_pdu->len;
    network_pdu->flags = 0;

    mesh_lower_transport_send_pdu((mesh_pdu_t*) unsegmented_pdu);
}

static void mesh_upper_transport_send_upper_access_pdu_ccm(void * arg){
    crypto_active = 0;

    mesh_upper_transport_pdu_t * upper_pdu = (mesh_upper_transport_pdu_t *) arg;
    mesh_print_hex("EncAccessPayload", crypto_buffer, upper_pdu->len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &crypto_buffer[upper_pdu->len]);
    mesh_print_hex("TransMIC", &crypto_buffer[upper_pdu->len], upper_pdu->transmic_len);
    upper_pdu->len += upper_pdu->transmic_len;
    mesh_print_hex("UpperTransportPDU", crypto_buffer, upper_pdu->len);
    switch (upper_pdu->pdu_header.pdu_type){
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            mesh_upper_transport_send_upper_unsegmented_pdu(upper_pdu);
            break;
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
            mesh_upper_transport_send_upper_segmented_pdu(upper_pdu);
            break;
        default:
            btstack_assert(false);
    }
}

static void mesh_upper_transport_send_upper_access_pdu_digest(void *arg){
    mesh_upper_transport_pdu_t * upper_pdu = (mesh_upper_transport_pdu_t *) arg;
    uint16_t  access_pdu_len  = upper_pdu->len;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, crypto_buffer, crypto_buffer,
                                     &mesh_upper_transport_send_upper_access_pdu_ccm, upper_pdu);
}

static void mesh_upper_transport_send_upper_access_pdu(mesh_upper_transport_pdu_t * upper_pdu){

    // if dst is virtual address, lookup label uuid and hash
    uint16_t aad_len = 0;
    mesh_virtual_address_t * virtual_address = NULL;
    if (mesh_network_address_virtual(upper_pdu->dst)){
        virtual_address = mesh_virtual_address_for_pseudo_dst(upper_pdu->dst);
        if (!virtual_address){
            printf("No virtual address register for pseudo dst %4x\n", upper_pdu->dst);
            mesh_access_message_handler(MESH_TRANSPORT_PDU_SENT, MESH_TRANSPORT_STATUS_SEND_FAILED, (mesh_pdu_t *) upper_pdu);
            return;
        }
        // printf("Using hash %4x with LabelUUID: ", virtual_address->hash);
        // printf_hexdump(virtual_address->label_uuid, 16);
        aad_len = 16;
        upper_pdu->dst = virtual_address->hash;
    }

    // get app or device key
    uint16_t appkey_index = upper_pdu->appkey_index;
    const mesh_transport_key_t * appkey = mesh_upper_transport_get_outgoing_appkey(upper_pdu->netkey_index, appkey_index);
    if (appkey == NULL){
        printf("AppKey %04x not found, drop message\n", appkey_index);
        mesh_access_message_handler(MESH_TRANSPORT_PDU_SENT, MESH_TRANSPORT_STATUS_SEND_FAILED, (mesh_pdu_t *) upper_pdu);
        return;
    }

    // reserve slot
    mesh_lower_transport_reserve_slot();

    // reserve one sequence number, which is also used to encrypt access payload
    uint32_t seq = mesh_sequence_number_next();
    upper_pdu->flags |= MESH_TRANSPORT_FLAG_SEQ_RESERVED;
    upper_pdu->seq = seq;

    // also reserves crypto_buffer
    crypto_active = 1;

    // flatten segmented pdu into crypto buffer
    uint16_t payload_len = mesh_upper_pdu_flatten(upper_pdu, crypto_buffer, sizeof(crypto_buffer));
    btstack_assert(payload_len == upper_pdu->len);

    // Dump PDU
    printf("[+] Upper transport, send upper (un)segmented Access PDU - dest %04x, seq %06x\n", upper_pdu->dst, upper_pdu->seq);
    mesh_print_hex("Access Payload", crypto_buffer, upper_pdu->len);

    // setup nonce - uses dst, so after pseudo address translation
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        transport_segmented_setup_device_nonce(application_nonce, (mesh_pdu_t *) upper_pdu);
    } else {
        transport_segmented_setup_application_nonce(application_nonce, (mesh_pdu_t *) upper_pdu);
    }

    // Dump key
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   transmic_len    = upper_pdu->transmic_len;
    uint16_t  access_pdu_len  = upper_pdu->len;
    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, transmic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16,
                                  &mesh_upper_transport_send_upper_access_pdu_digest, upper_pdu);
    } else {
        mesh_upper_transport_send_upper_access_pdu_digest(upper_pdu);
    }
}

static void mesh_upper_transport_send_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu){
    // reserve slot
    mesh_lower_transport_reserve_slot();
    // reserve sequence number
    uint32_t seq = mesh_sequence_number_next();
    mesh_network_pdu_set_seq(network_pdu, seq);
    // Dump PDU
    uint8_t opcode = network_pdu->data[9];
    printf("[+] Upper transport, send unsegmented Control PDU %p - seq %06x opcode %02x\n", network_pdu, seq, opcode);
    mesh_print_hex("Access Payload", &network_pdu->data[10], network_pdu->len - 10);
    // wrap into mesh-unsegmented-pdu
    outgoing_unsegmented_pdu_singleton.pdu_header.pdu_type = MESH_PDU_TYPE_UNSEGMENTED;
    outgoing_unsegmented_pdu_singleton.segment = network_pdu;
    outgoing_unsegmented_pdu_singleton.flags = MESH_TRANSPORT_FLAG_CONTROL;

    // send
     mesh_lower_transport_send_pdu((mesh_pdu_t *) &outgoing_unsegmented_pdu_singleton);
}

#if 0
static void mesh_upper_transport_send_segmented_control_pdu(mesh_transport_pdu_t * transport_pdu){
    // reserve slot
    mesh_lower_transport_reserve_slot();
    // reserve sequence number
    uint32_t seq = mesh_sequence_number_next();
    transport_pdu->flags |= MESH_TRANSPORT_FLAG_SEQ_RESERVED;
    mesh_transport_set_seq(transport_pdu, seq);
    // Dump PDU
    uint8_t opcode = transport_pdu->data[0];
    printf("[+] Upper transport, send segmented Control PDU %p - seq %06x opcode %02x\n", transport_pdu, seq, opcode);
    mesh_print_hex("Access Payload", &transport_pdu->data[1], transport_pdu->len - 1);
    // send
    btstack_assert(false);
    // mesh_upper_transport_send_segmented_pdu(transport_pdu);
}
#endif

static void mesh_upper_transport_run(void){

    while(!btstack_linked_list_empty(&upper_transport_incoming)){

        if (crypto_active) return;

        // get next message
        mesh_pdu_t * pdu =  (mesh_pdu_t *) btstack_linked_list_pop(&upper_transport_incoming);
        mesh_network_pdu_t   * network_pdu;
        mesh_segmented_pdu_t   * message_pdu;
        mesh_unsegmented_pdu_t * unsegmented_pdu;
        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_UNSEGMENTED:
                unsegmented_pdu = (mesh_unsegmented_pdu_t *) pdu;
                network_pdu = unsegmented_pdu->segment;
                btstack_assert(network_pdu != NULL);
                // control?
                if (mesh_network_control(network_pdu)) {

                    incoming_control_pdu =  &incoming_control_pdu_singleton;
                    incoming_control_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_CONTROL;
                    incoming_control_pdu->len =  network_pdu->len;
                    incoming_control_pdu->netkey_index =  network_pdu->netkey_index;

                    uint8_t * lower_transport_pdu = mesh_network_pdu_data(network_pdu);

                    incoming_control_pdu->akf_aid_control = lower_transport_pdu[0];
                    incoming_control_pdu->len = network_pdu->len - 10; // 9 header + 1 opcode
                    (void)memcpy(incoming_control_pdu->data, &lower_transport_pdu[1], incoming_control_pdu->len);

                    // copy meta data into encrypted pdu buffer
                    (void)memcpy(incoming_control_pdu->network_header, network_pdu->data, 9);

                    mesh_print_hex("Assembled payload", incoming_control_pdu->data, incoming_control_pdu->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer(pdu);

                    btstack_assert(mesh_control_message_handler != NULL);
                    mesh_pdu_t * pdu = (mesh_pdu_t*) incoming_control_pdu;
                    mesh_control_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, pdu);

                } else {

                    incoming_access_pdu_encrypted = &incoming_access_pdu_encrypted_singleton;
                    incoming_access_pdu_encrypted->pdu_header.pdu_type = MESH_PDU_TYPE_ACCESS;
                    incoming_access_pdu_decrypted = &incoming_access_pdu_decrypted_singleton;

                    incoming_access_pdu_encrypted->netkey_index = network_pdu->netkey_index;
                    incoming_access_pdu_encrypted->transmic_len = 4;

                    uint8_t * lower_transport_pdu = mesh_network_pdu_data(network_pdu);

                    incoming_access_pdu_encrypted->akf_aid_control = lower_transport_pdu[0];
                    incoming_access_pdu_encrypted->len = network_pdu->len - 10; // 9 header + 1 AID
                    (void)memcpy(incoming_access_pdu_encrypted->data, &lower_transport_pdu[1], incoming_access_pdu_encrypted->len);

                    // copy meta data into encrypted pdu buffer
                    (void)memcpy(incoming_access_pdu_encrypted->network_header, network_pdu->data, 9);

                    mesh_print_hex("Assembled payload", incoming_access_pdu_encrypted->data, incoming_access_pdu_encrypted->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer(pdu);

                    // get encoded transport pdu and start processing
                    mesh_upper_transport_process_segmented_message();
                }
                break;
            case MESH_PDU_TYPE_SEGMENTED:
                message_pdu = (mesh_segmented_pdu_t *) pdu;
                uint8_t ctl = mesh_message_ctl(message_pdu);
                if (ctl){
                    incoming_control_pdu=  &incoming_control_pdu_singleton;
                    incoming_control_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_CONTROL;

                    // flatten
                    mesh_segmented_pdu_flatten(&message_pdu->segments, 8, incoming_control_pdu->data);

                    // copy meta data into encrypted pdu buffer
                    incoming_control_pdu->len =  message_pdu->len;
                    incoming_control_pdu->netkey_index =  message_pdu->netkey_index;
                    incoming_control_pdu->akf_aid_control =  message_pdu->akf_aid_control;
                    incoming_control_pdu->flags = 0;
                    (void)memcpy(incoming_control_pdu->network_header, message_pdu->network_header, 9);

                    mesh_print_hex("Assembled payload", incoming_control_pdu->data, incoming_control_pdu->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)message_pdu);

                    btstack_assert(mesh_control_message_handler != NULL);
                    mesh_pdu_t * pdu = (mesh_pdu_t*) incoming_control_pdu;
                    mesh_access_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, pdu);

                } else {

                    incoming_access_pdu_encrypted = &incoming_access_pdu_encrypted_singleton;
                    incoming_access_pdu_encrypted->pdu_header.pdu_type = MESH_PDU_TYPE_ACCESS;
                    incoming_access_pdu_decrypted = &incoming_access_pdu_decrypted_singleton;

                    // flatten
                    mesh_segmented_pdu_flatten(&message_pdu->segments, 12, incoming_access_pdu_encrypted->data);

                    // copy meta data into encrypted pdu buffer
                    incoming_access_pdu_encrypted->len =  message_pdu->len;
                    incoming_access_pdu_encrypted->netkey_index =  message_pdu->netkey_index;
                    incoming_access_pdu_encrypted->transmic_len =  message_pdu->transmic_len;
                    incoming_access_pdu_encrypted->akf_aid_control =  message_pdu->akf_aid_control;
                    (void)memcpy(incoming_access_pdu_encrypted->network_header, message_pdu->network_header, 9);

                    mesh_print_hex("Assembled payload", incoming_access_pdu_encrypted->data, incoming_access_pdu_encrypted->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)message_pdu);

                    // get encoded transport pdu and start processing
                    mesh_upper_transport_process_segmented_message();
                }
                break;
            default:
                btstack_assert(0);
                break;
        }
    }

    while (!btstack_linked_list_empty(&upper_transport_outgoing)){

        if (crypto_active) break;

        if (outgoing_segmented_access_pdu != NULL) break;

        mesh_pdu_t * pdu =  (mesh_pdu_t *) btstack_linked_list_get_first_item(&upper_transport_outgoing);
        if (mesh_lower_transport_can_send_to_dest(mesh_pdu_dst(pdu)) == 0) break;

        (void) btstack_linked_list_pop(&upper_transport_outgoing);

        mesh_unsegmented_pdu_t * unsegmented_pdu;

        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
                btstack_assert(mesh_pdu_ctl(pdu) != 0);
                mesh_upper_transport_send_unsegmented_control_pdu((mesh_network_pdu_t *) pdu);
                break;
            case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
                mesh_upper_transport_send_upper_access_pdu((mesh_upper_transport_pdu_t *) pdu);
                break;
            default:
                btstack_assert(false);
                break;
        }
    }
}



static void mesh_upper_transport_pdu_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    mesh_pdu_t * pdu_to_report;
    mesh_unsegmented_pdu_t * unsegmented_pdu;
    switch (callback_type){
        case MESH_TRANSPORT_PDU_RECEIVED:
            mesh_upper_transport_message_received(pdu);
            break;
        case MESH_TRANSPORT_PDU_SENT:
            switch (pdu->pdu_type){
                case MESH_PDU_TYPE_SEGMENTED:
                    // free chunks
                    while (!btstack_linked_list_empty(&outgoing_segmented_message_singleton.segments)){
                        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&outgoing_segmented_message_singleton.segments);
                        mesh_network_pdu_free(network_pdu);
                    }
                    // notify upper layer but use transport pdu
                    pdu_to_report = (mesh_pdu_t *) outgoing_segmented_access_pdu;
                    outgoing_segmented_access_pdu = NULL;
                    if (mesh_pdu_ctl(pdu_to_report)){
                        mesh_control_message_handler(callback_type, status, pdu_to_report);
                    } else {
                        mesh_access_message_handler(callback_type, status, pdu_to_report);
                    }
                    break;
                case MESH_PDU_TYPE_UNSEGMENTED:
                    unsegmented_pdu = (mesh_unsegmented_pdu_t *) pdu;
                    if (unsegmented_pdu == &outgoing_unsegmented_pdu_singleton){
                        if ((unsegmented_pdu->flags & MESH_TRANSPORT_FLAG_CONTROL) == 0){
                            // notify upper layer but use network pdu (control pdu)
                            mesh_network_pdu_t * network_pdu = outgoing_unsegmented_pdu_singleton.segment;
                            outgoing_unsegmented_pdu_singleton.segment = NULL;
                            mesh_control_message_handler(callback_type, status, (mesh_pdu_t *) network_pdu);
                        } else {
                            // notify upper layer but use upper access pdu
                            mesh_network_pdu_t * network_pdu = outgoing_unsegmented_pdu_singleton.segment;
                            outgoing_unsegmented_pdu_singleton.segment = NULL;
                            mesh_network_pdu_free(network_pdu);
                            pdu_to_report = (mesh_pdu_t *) outgoing_upper_transport_pdu;
                            mesh_access_message_handler(callback_type, status, pdu_to_report);
                        }
                    } else {
                        btstack_assert((unsegmented_pdu->flags & MESH_TRANSPORT_FLAG_CONTROL) == 0);
                        mesh_access_message_handler(callback_type, status, pdu);
                    }
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            mesh_upper_transport_run();
            break;
        default:
            break;
    }
}

void mesh_upper_transport_pdu_free(mesh_pdu_t * pdu){
    mesh_network_pdu_t   * network_pdu;
    mesh_segmented_pdu_t   * message_pdu;
    switch (pdu->pdu_type) {
        case MESH_PDU_TYPE_NETWORK:
            network_pdu = (mesh_network_pdu_t *) pdu;
            mesh_network_pdu_free(network_pdu);
            break;
        case MESH_PDU_TYPE_SEGMENTED:
            message_pdu = (mesh_segmented_pdu_t *) pdu;
            mesh_message_pdu_free(message_pdu);
        default:
            btstack_assert(false);
            break;
    }
}

void mesh_upper_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu){
    crypto_active = 0;
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_ACCESS:
            mesh_upper_transport_process_access_message_done((mesh_access_pdu_t *) pdu);
        case MESH_PDU_TYPE_CONTROL:
            mesh_upper_transport_process_control_message_done((mesh_control_pdu_t *) pdu);
            break;
        default:
            btstack_assert(0);
            break;
    }
}

void mesh_upper_transport_register_access_message_handler(void (*callback)(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu)) {
    mesh_access_message_handler = callback;
}

void mesh_upper_transport_register_control_message_handler(void (*callback)(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu)){
    mesh_control_message_handler = callback;
}

void mesh_upper_transport_init(){
    mesh_lower_transport_set_higher_layer_handler(&mesh_upper_transport_pdu_handler);
}

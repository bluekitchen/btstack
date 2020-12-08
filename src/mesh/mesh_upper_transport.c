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

#include <stdarg.h>
#include "btstack_tlv.h"
#include "mesh/mesh_foundation.h"
#include "mesh_upper_transport.h"
#include "mesh/mesh.h"
#include "mesh/mesh_proxy.h"
#include "mesh/mesh_node.h"

#define BTSTACK_FILE__ "mesh_upper_transport.c"

#include "mesh/mesh_upper_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_util.h"
#include "btstack_memory.h"
#include "btstack_debug.h"
#include "btstack_bool.h"

#include "mesh/beacon.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_peer.h"
#include "mesh/mesh_virtual_addresses.h"

// TODO: extract mesh_pdu functions into lower transport or network
#include "mesh/mesh_access.h"

// MESH_ACCESS_MESH_NETWORK_PAYLOAD_MAX (384) / MESH_NETWORK_PAYLOAD_MAX (29) = 13.24.. < 14
#define MESSAGE_BUILDER_MAX_NUM_NETWORK_PDUS (14)

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

static void mesh_upper_transport_run(void);
static void mesh_upper_transport_schedule_send_requests(void);
static void mesh_upper_transport_validate_access_message(void);

// upper transport callbacks - in access layer
static void (*mesh_access_message_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);
static void (*mesh_control_message_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);

//
static int crypto_active;
static uint8_t application_nonce[13];
static btstack_crypto_ccm_t ccm;
static mesh_transport_key_and_virtual_address_iterator_t mesh_transport_key_it;

// incoming segmented (mesh_segmented_pdu_t) or unsegmented (network_pdu_t)
static mesh_pdu_t *          incoming_access_encrypted;

// multi-purpose union: segmented control reassembly, decrypted access pdu
static union {
    mesh_control_pdu_t    control;
    mesh_access_pdu_t     access;
} incoming_pdu_singleton;

// pointer to incoming_pdu_singleton.access
static mesh_access_pdu_t *   incoming_access_decrypted;

// pointer to incoming_pdu_singleton.access
static mesh_control_pdu_t *  incoming_control_pdu;

// incoming incoming_access_decrypted ready to be deliverd
static bool incoming_access_pdu_ready;

// incoming unsegmented (network) and segmented (transport) control and access messages
static btstack_linked_list_t upper_transport_incoming;


// outgoing unsegmented and segmented control and access messages
static btstack_linked_list_t upper_transport_outgoing;

// outgoing upper transport messages that have been sent to lower transport and wait for sent event
static btstack_linked_list_t upper_transport_outgoing_active;

// outgoing send requests
static btstack_linked_list_t upper_transport_send_requests;

// message builder buffers
static mesh_upper_transport_pdu_t * message_builder_reserved_upper_pdu;
static uint8_t message_builder_num_network_pdus_reserved;
static btstack_linked_list_t message_builder_reserved_network_pdus;

// requets network pdus for outgoing send requests and outgoing run
static bool upper_transport_need_pdu_for_send_requests;
static bool upper_transport_need_pdu_for_run_outgoing;

// TODO: higher layer define used for assert
#define MESH_ACCESS_OPCODE_NOT_SET 0xFFFFFFFEu

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

static void mesh_segmented_pdu_flatten(btstack_linked_list_t * segments, uint8_t segment_len, uint8_t * buffer) {
    // assemble payload
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, segments);
    while (btstack_linked_list_iterator_has_next(&it)) {
        mesh_network_pdu_t *segment = (mesh_network_pdu_t *) btstack_linked_list_iterator_next(&it);
        btstack_assert(segment->pdu_header.pdu_type == MESH_PDU_TYPE_NETWORK);
        uint8_t offset = 0;
        while (offset < segment->len){
            uint8_t seg_o = segment->data[offset++];
            (void) memcpy(&buffer[seg_o * segment_len], &segment->data[offset], segment_len);
            offset += segment_len;
        }
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

// store payload in provided list of network pdus
static void mesh_segmented_store_payload(const uint8_t * payload, uint16_t payload_len, btstack_linked_list_t * in_segments, btstack_linked_list_t * out_segments){
    uint16_t payload_offset = 0;
    uint16_t bytes_current_segment = 0;
    mesh_network_pdu_t * network_pdu = NULL;
    while (payload_offset < payload_len){
        if (bytes_current_segment == 0){
            network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(in_segments);
            btstack_assert(network_pdu != NULL);
            btstack_linked_list_add_tail(out_segments, (btstack_linked_item_t *) network_pdu);
            bytes_current_segment = MESH_NETWORK_PAYLOAD_MAX;
        }
        uint16_t bytes_to_copy = btstack_min(bytes_current_segment, payload_len - payload_offset);
        (void) memcpy(&network_pdu->data[network_pdu->len], &payload[payload_offset], bytes_to_copy);
        bytes_current_segment -= bytes_to_copy;

        // on enter, bytes_current_segment = 0 => network_pdu = pop (in segements) + assert (network != NULL)
        // cppcheck-suppress nullPointer
        network_pdu->len += bytes_to_copy;
        payload_offset += bytes_to_copy;
    }
}

// tries allocate and add enough segments to store payload of given size
static bool mesh_segmented_allocate_segments(btstack_linked_list_t * segments, uint16_t payload_len){
    uint16_t storage_size = btstack_linked_list_count(segments) * MESH_NETWORK_PAYLOAD_MAX;
    while (storage_size < payload_len){
        mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
        if (network_pdu == NULL) break;
        storage_size += MESH_NETWORK_PAYLOAD_MAX;
        btstack_linked_list_add(segments, (btstack_linked_item_t *) network_pdu);
    }
    return (storage_size >= payload_len);
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
    mesh_upper_transport_dump_pdus("upper_transport_incoming", &upper_transport_incoming);
}

void mesh_upper_transport_reset(void){
    crypto_active = 0;
    mesh_upper_transport_reset_pdus(&upper_transport_incoming);
    mesh_upper_transport_reset_pdus(&upper_transport_outgoing);
    message_builder_num_network_pdus_reserved = 0;
    mesh_upper_transport_reset_pdus(&message_builder_reserved_network_pdus);
    if (message_builder_reserved_upper_pdu != NULL){
        btstack_memory_mesh_upper_transport_pdu_free(message_builder_reserved_upper_pdu);
        message_builder_reserved_upper_pdu = NULL;
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
            nonce[1] = ((access_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 0x80 : 0x00;
            big_endian_store_24(nonce, 2, access_pdu->seq);
            big_endian_store_16(nonce, 5, access_pdu->src);
            big_endian_store_16(nonce, 7, access_pdu->dst);
            big_endian_store_32(nonce, 9, iv_index_for_ivi_nid(access_pdu->ivi_nid));
            break;
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            upper_pdu = (mesh_upper_transport_pdu_t *) pdu;
            nonce[1] = ((upper_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 0x80 : 0x00;
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

static void mesh_upper_transport_process_access_message_done(mesh_access_pdu_t *access_pdu){
    crypto_active = 0;
    btstack_assert((access_pdu->ctl_ttl & 0x80) == 0);
    mesh_lower_transport_message_processed_by_higher_layer(incoming_access_encrypted);
    incoming_access_encrypted = NULL;
    incoming_access_decrypted = NULL;
    mesh_upper_transport_run();
}

static void mesh_upper_transport_process_control_message_done(mesh_control_pdu_t * control_pdu){
    UNUSED(control_pdu);
    crypto_active = 0;
    incoming_control_pdu = NULL;
    mesh_upper_transport_run();
}

static void mesh_upper_transport_network_pdu_freed(void){
    // call both while prioritizing run outgoing
    // both functions will trigger request for network pdu if needed
    if (upper_transport_need_pdu_for_run_outgoing){
        upper_transport_need_pdu_for_run_outgoing = false;
        mesh_upper_transport_run();
    }
    if (upper_transport_need_pdu_for_send_requests){
        upper_transport_need_pdu_for_send_requests = false;
        mesh_upper_transport_schedule_send_requests();
    }
}

static void mesh_upper_transport_need_pdu_for_send_requests(void) {
    bool waiting = upper_transport_need_pdu_for_send_requests || upper_transport_need_pdu_for_run_outgoing;
    upper_transport_need_pdu_for_send_requests = true;
    if (waiting == false) {
        mesh_network_notify_on_freed_pdu(&mesh_upper_transport_network_pdu_freed);
    }
}
static void mesh_upper_transport_need_pdu_for_run_outgoing(void) {
    bool waiting = upper_transport_need_pdu_for_send_requests || upper_transport_need_pdu_for_run_outgoing;
    upper_transport_need_pdu_for_run_outgoing = true;
    if (waiting == false) {
        mesh_network_notify_on_freed_pdu(&mesh_upper_transport_network_pdu_freed);
    }
}

static void mesh_upper_transport_deliver_access_message(void) {
    incoming_access_pdu_ready = false;
    mesh_access_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, (mesh_pdu_t *) incoming_access_decrypted);
}

static bool mesh_upper_transport_send_requests_pending(void){
    if (incoming_access_pdu_ready) {
        return true;
    }
    return btstack_linked_list_empty(&upper_transport_send_requests) == false;
}

static void mesh_upper_transport_schedule_send_requests(void){

    while (mesh_upper_transport_send_requests_pending()){

        // get ready
        bool message_builder_ready = mesh_upper_transport_message_reserve();

        if (message_builder_ready == false){
            // waiting for free upper pdu, we will get called again on pdu free
            if (message_builder_reserved_upper_pdu == NULL){
                return;
            }
            // request callback on network pdu free
            mesh_upper_transport_need_pdu_for_send_requests();
            return;
        }

        // process send requests

        // incoming access pdu
        if (incoming_access_pdu_ready){
            // message builder ready = one outgoing pdu is guaranteed, deliver access pdu
            mesh_upper_transport_deliver_access_message();
            continue;
        }

        // regular send request
        btstack_context_callback_registration_t * send_request = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&upper_transport_send_requests);
        btstack_assert(send_request != NULL);
        (*send_request->callback)(send_request->context);
    }
}

void mesh_upper_transport_request_to_send(btstack_context_callback_registration_t * request){
    btstack_linked_list_add_tail(&upper_transport_send_requests, (btstack_linked_item_t *) request);
    mesh_upper_transport_schedule_send_requests();
}

static void mesh_upper_transport_validate_access_message_ccm(void * arg){
    UNUSED(arg);

    uint8_t transmic_len = ((incoming_access_decrypted->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    uint8_t * upper_transport_pdu     = incoming_access_decrypted->data;
    uint8_t   upper_transport_pdu_len = incoming_access_decrypted->len - transmic_len;
 
    mesh_print_hex("Decrypted PDU", upper_transport_pdu, upper_transport_pdu_len);

    // store TransMIC
    uint8_t trans_mic[8];
    btstack_crypto_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], transmic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        incoming_access_decrypted->len -= transmic_len;

        // if virtual address, update dst to pseudo_dst
        if (mesh_network_address_virtual(incoming_access_decrypted->dst)){
            incoming_access_decrypted->dst = mesh_transport_key_it.address->pseudo_dst;
        }

        // pass to upper layer
        incoming_access_pdu_ready = true;
        mesh_upper_transport_schedule_send_requests();

    } else {
        uint8_t akf = incoming_access_decrypted->akf_aid_control & 0x40;
        if (akf){
            printf("TransMIC does not match, try next key\n");
            mesh_upper_transport_validate_access_message();
        } else {
            printf("TransMIC does not match device key, done\n");
            // done
            mesh_upper_transport_process_access_message_done(incoming_access_decrypted);
        }
    }
}

static void mesh_upper_transport_validate_access_message_digest(void * arg){
    UNUSED(arg);
    uint8_t   transmic_len = ((incoming_access_decrypted->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    uint8_t   upper_transport_pdu_len      = incoming_access_decrypted->len - transmic_len;
    uint8_t * upper_transport_pdu_data_out = incoming_access_decrypted->data;

    mesh_network_pdu_t * unsegmented_pdu = NULL;
    mesh_segmented_pdu_t * segmented_pdu = NULL;
    switch (incoming_access_encrypted->pdu_type){
        case MESH_PDU_TYPE_SEGMENTED:
            segmented_pdu = (mesh_segmented_pdu_t *) incoming_access_encrypted;
            mesh_segmented_pdu_flatten(&segmented_pdu->segments, 12, upper_transport_pdu_data_out);
            mesh_print_hex("Encrypted Payload:", upper_transport_pdu_data_out, upper_transport_pdu_len);
            btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_out, upper_transport_pdu_data_out,
                                             &mesh_upper_transport_validate_access_message_ccm, NULL);
            break;
        case MESH_PDU_TYPE_UNSEGMENTED:
            unsegmented_pdu = (mesh_network_pdu_t *) incoming_access_encrypted;
            (void)memcpy(upper_transport_pdu_data_out, &unsegmented_pdu->data[10], incoming_access_decrypted->len);
            btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data_out, upper_transport_pdu_data_out,
                                             &mesh_upper_transport_validate_access_message_ccm, NULL);
            break;
        default:
            btstack_assert(false);
            break;
    }

}

static void mesh_upper_transport_validate_access_message(void){
    uint8_t   transmic_len = ((incoming_access_decrypted->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    uint8_t * upper_transport_pdu_data =  incoming_access_decrypted->data;
    uint8_t   upper_transport_pdu_len  = incoming_access_decrypted->len - transmic_len;

    if (!mesh_transport_key_and_virtual_address_iterator_has_more(&mesh_transport_key_it)){
        printf("No valid transport key found\n");
        mesh_upper_transport_process_access_message_done(incoming_access_decrypted);
        return;
    }
    mesh_transport_key_and_virtual_address_iterator_next(&mesh_transport_key_it);
    const mesh_transport_key_t * message_key = mesh_transport_key_it.key;

    if (message_key->akf){
        transport_segmented_setup_application_nonce(application_nonce, (mesh_pdu_t *) incoming_access_decrypted);
    } else {
        transport_segmented_setup_device_nonce(application_nonce, (mesh_pdu_t *) incoming_access_decrypted);
    }

    // store application / device key index
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    incoming_access_decrypted->appkey_index = message_key->appkey_index;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    crypto_active = 1;
    uint16_t aad_len  = 0;
    if (mesh_network_address_virtual(incoming_access_decrypted->dst)){
        aad_len  = 16;
    }
    btstack_crypto_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, aad_len, transmic_len);

    if (aad_len){
        btstack_crypto_ccm_digest(&ccm, (uint8_t *) mesh_transport_key_it.address->label_uuid, aad_len,
                                  &mesh_upper_transport_validate_access_message_digest, NULL);
    } else {
        mesh_upper_transport_validate_access_message_digest(NULL);
    }
}

static void mesh_upper_transport_process_access_message(void){
    uint8_t   transmic_len = ((incoming_access_decrypted->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    uint8_t * upper_transport_pdu     =  incoming_access_decrypted->data;
    uint8_t   upper_transport_pdu_len = incoming_access_decrypted->len - transmic_len;
    mesh_print_hex("Upper Transport pdu", upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid = incoming_access_decrypted->akf_aid_control & 0x3f;
    uint8_t akf = (incoming_access_decrypted->akf_aid_control & 0x40) >> 6;

    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    mesh_transport_key_and_virtual_address_iterator_init(&mesh_transport_key_it, incoming_access_decrypted->dst,
                                                         incoming_access_decrypted->netkey_index, akf, aid);
    mesh_upper_transport_validate_access_message();
}

static void mesh_upper_transport_message_received(mesh_pdu_t * pdu){
    btstack_linked_list_add_tail(&upper_transport_incoming, (btstack_linked_item_t*) pdu);
    mesh_upper_transport_run();
}

static void mesh_upper_transport_send_access_segmented(mesh_upper_transport_pdu_t * upper_pdu){

    mesh_segmented_pdu_t * segmented_pdu   = (mesh_segmented_pdu_t *) upper_pdu->lower_pdu;
    segmented_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;

    // convert mesh_access_pdu_t into mesh_segmented_pdu_t
    btstack_linked_list_t free_segments = segmented_pdu->segments;
    segmented_pdu->segments = NULL;
    mesh_segmented_store_payload(incoming_pdu_singleton.access.data, upper_pdu->len, &free_segments, &segmented_pdu->segments);

    // copy meta
    segmented_pdu->len = upper_pdu->len;
    segmented_pdu->netkey_index = upper_pdu->netkey_index;
    segmented_pdu->akf_aid_control = upper_pdu->akf_aid_control;
    segmented_pdu->flags = upper_pdu->flags;

    // setup segmented_pdu header
    // (void)memcpy(segmented_pdu->network_header, upper_pdu->network_header, 9);
    // TODO: use fields in mesh_segmented_pdu_t and setup network header in lower transport
    segmented_pdu->ivi_nid = upper_pdu->ivi_nid;
    segmented_pdu->ctl_ttl = upper_pdu->ctl_ttl;
    segmented_pdu->seq = upper_pdu->seq;
    segmented_pdu->src = upper_pdu->src;
    segmented_pdu->dst = upper_pdu->dst;

    // queue up
    upper_pdu->lower_pdu = (mesh_pdu_t *) segmented_pdu;
    btstack_linked_list_add(&upper_transport_outgoing_active, (btstack_linked_item_t *) upper_pdu);

    mesh_lower_transport_send_pdu((mesh_pdu_t*) segmented_pdu);
}

static void mesh_upper_transport_send_access_unsegmented(mesh_upper_transport_pdu_t * upper_pdu){

    // provide segment
    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) upper_pdu->lower_pdu;

    // setup network pdu
    network_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS;
    network_pdu->data[0] = upper_pdu->ivi_nid;
    network_pdu->data[1] = upper_pdu->ctl_ttl;
    big_endian_store_24(network_pdu->data, 2, upper_pdu->seq);
    big_endian_store_16(network_pdu->data, 5, upper_pdu->src);
    big_endian_store_16(network_pdu->data, 7, upper_pdu->dst);
    network_pdu->netkey_index = upper_pdu->netkey_index;

    // setup access message
    network_pdu->data[9] = upper_pdu->akf_aid_control;
    btstack_assert(upper_pdu->len < 15);
    (void)memcpy(&network_pdu->data[10], &incoming_pdu_singleton.access.data, upper_pdu->len);
    network_pdu->len = 10 + upper_pdu->len;
    network_pdu->flags = 0;

    // queue up
    btstack_linked_list_add(&upper_transport_outgoing_active, (btstack_linked_item_t *) upper_pdu);

    mesh_lower_transport_send_pdu((mesh_pdu_t*) network_pdu);
}

static void mesh_upper_transport_send_access_ccm(void * arg){
    crypto_active = 0;

    mesh_upper_transport_pdu_t * upper_pdu = (mesh_upper_transport_pdu_t *) arg;
    mesh_print_hex("EncAccessPayload", incoming_pdu_singleton.access.data, upper_pdu->len);
    // store TransMIC
    btstack_crypto_ccm_get_authentication_value(&ccm, &incoming_pdu_singleton.access.data[upper_pdu->len]);
    uint8_t transmic_len = ((upper_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    mesh_print_hex("TransMIC", &incoming_pdu_singleton.access.data[upper_pdu->len], transmic_len);
    upper_pdu->len += transmic_len;
    mesh_print_hex("UpperTransportPDU", incoming_pdu_singleton.access.data, upper_pdu->len);
    switch (upper_pdu->pdu_header.pdu_type){
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            mesh_upper_transport_send_access_unsegmented(upper_pdu);
            break;
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
            mesh_upper_transport_send_access_segmented(upper_pdu);
            break;
        default:
            btstack_assert(false);
    }
}

static void mesh_upper_transport_send_access_digest(void *arg){
    mesh_upper_transport_pdu_t * upper_pdu = (mesh_upper_transport_pdu_t *) arg;
    uint16_t  access_pdu_len  = upper_pdu->len;
    btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, incoming_pdu_singleton.access.data, incoming_pdu_singleton.access.data,
                                     &mesh_upper_transport_send_access_ccm, upper_pdu);
}

static void mesh_upper_transport_send_access(mesh_upper_transport_pdu_t * upper_pdu){

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
    uint16_t payload_len = mesh_upper_pdu_flatten(upper_pdu, incoming_pdu_singleton.access.data, sizeof(incoming_pdu_singleton.access.data));
    btstack_assert(payload_len == upper_pdu->len);
    UNUSED(payload_len);
    
    // Dump PDU
    printf("[+] Upper transport, send upper (un)segmented Access PDU - dest %04x, seq %06x\n", upper_pdu->dst, upper_pdu->seq);
    mesh_print_hex("Access Payload", incoming_pdu_singleton.access.data, upper_pdu->len);

    // setup nonce - uses dst, so after pseudo address translation
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        transport_segmented_setup_device_nonce(application_nonce, (mesh_pdu_t *) upper_pdu);
    } else {
        transport_segmented_setup_application_nonce(application_nonce, (mesh_pdu_t *) upper_pdu);
    }

    // Dump key
    mesh_print_hex("AppOrDevKey", appkey->key, 16);

    // encrypt ccm
    uint8_t   transmic_len = ((upper_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
    uint16_t  access_pdu_len  = upper_pdu->len;
    btstack_crypto_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, aad_len, transmic_len);
    if (virtual_address){
        mesh_print_hex("LabelUUID", virtual_address->label_uuid, 16);
        btstack_crypto_ccm_digest(&ccm, virtual_address->label_uuid, 16,
                                  &mesh_upper_transport_send_access_digest, upper_pdu);
    } else {
        mesh_upper_transport_send_access_digest(upper_pdu);
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

    // send
     mesh_lower_transport_send_pdu((mesh_pdu_t *) network_pdu);
}

static void mesh_upper_transport_send_segmented_control_pdu(mesh_upper_transport_pdu_t * upper_pdu){
    // reserve slot
    mesh_lower_transport_reserve_slot();
    // reserve sequence number
    uint32_t seq = mesh_sequence_number_next();
    upper_pdu->flags |= MESH_TRANSPORT_FLAG_SEQ_RESERVED;
    upper_pdu->seq = seq;
    // Dump PDU
    // uint8_t opcode = upper_pdu->data[0];
    // printf("[+] Upper transport, send segmented Control PDU %p - seq %06x opcode %02x\n", upper_pdu, seq, opcode);
    // mesh_print_hex("Access Payload", &upper_pdu->data[1], upper_pdu->len - 1);
    // send
    mesh_segmented_pdu_t * segmented_pdu   = (mesh_segmented_pdu_t *) upper_pdu->lower_pdu;
    segmented_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;

    // lend segments to lower transport pdu
    segmented_pdu->segments = upper_pdu->segments;
    upper_pdu->segments = NULL;

    // copy meta
    segmented_pdu->len = upper_pdu->len;
    segmented_pdu->netkey_index = upper_pdu->netkey_index;
    segmented_pdu->akf_aid_control = upper_pdu->akf_aid_control;
    segmented_pdu->flags = upper_pdu->flags;

    btstack_assert((upper_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) == 0);

    // setup segmented_pdu header
    // TODO: use fields in mesh_segmented_pdu_t and setup network header in lower transport
    segmented_pdu->ivi_nid = upper_pdu->ivi_nid;
    segmented_pdu->ctl_ttl = upper_pdu->ctl_ttl;
    segmented_pdu->seq = upper_pdu->seq;
    segmented_pdu->src = upper_pdu->src;
    segmented_pdu->dst = upper_pdu->dst;

    // queue up
    upper_pdu->lower_pdu = (mesh_pdu_t *) segmented_pdu;
    btstack_linked_list_add(&upper_transport_outgoing_active, (btstack_linked_item_t *) upper_pdu);

    mesh_lower_transport_send_pdu((mesh_pdu_t *) segmented_pdu);
}

static void mesh_upper_transport_run(void){

    while(!btstack_linked_list_empty(&upper_transport_incoming)){

        if (crypto_active) return;

        // get next message
        mesh_pdu_t * pdu =  (mesh_pdu_t *) btstack_linked_list_pop(&upper_transport_incoming);
        mesh_network_pdu_t   * network_pdu;
        mesh_segmented_pdu_t   * segmented_pdu;
        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_UNSEGMENTED:
                network_pdu = (mesh_network_pdu_t *) pdu;
                // control?
                if (mesh_network_control(network_pdu)) {

                    incoming_control_pdu =  &incoming_pdu_singleton.control;
                    incoming_control_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_CONTROL;
                    incoming_control_pdu->len =  network_pdu->len;
                    incoming_control_pdu->netkey_index =  network_pdu->netkey_index;

                    uint8_t * lower_transport_pdu = mesh_network_pdu_data(network_pdu);

                    incoming_control_pdu->akf_aid_control = lower_transport_pdu[0];
                    incoming_control_pdu->len = network_pdu->len - 10; // 9 header + 1 opcode
                    (void)memcpy(incoming_control_pdu->data, &lower_transport_pdu[1], incoming_control_pdu->len);

                    // copy meta data into encrypted pdu buffer
                    incoming_control_pdu->ivi_nid = network_pdu->data[0];
                    incoming_control_pdu->ctl_ttl = network_pdu->data[1];
                    incoming_control_pdu->seq = big_endian_read_24(network_pdu->data, 2);
                    incoming_control_pdu->src = big_endian_read_16(network_pdu->data, 5);
                    incoming_control_pdu->dst = big_endian_read_16(network_pdu->data, 7);

                    mesh_print_hex("Assembled payload", incoming_control_pdu->data, incoming_control_pdu->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer(pdu);

                    btstack_assert(mesh_control_message_handler != NULL);
                    mesh_control_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, (mesh_pdu_t*) incoming_control_pdu);

                } else {

                    incoming_access_encrypted = (mesh_pdu_t *) network_pdu;

                    incoming_access_decrypted = &incoming_pdu_singleton.access;
                    incoming_access_decrypted->pdu_header.pdu_type = MESH_PDU_TYPE_ACCESS;
                    incoming_access_decrypted->flags = 0;
                    incoming_access_decrypted->netkey_index = network_pdu->netkey_index;
                    incoming_access_decrypted->akf_aid_control = network_pdu->data[9];
                    incoming_access_decrypted->len = network_pdu->len - 10; // 9 header + 1 AID
                    incoming_access_decrypted->ivi_nid = network_pdu->data[0];
                    incoming_access_decrypted->ctl_ttl = network_pdu->data[1];
                    incoming_access_decrypted->seq = big_endian_read_24(network_pdu->data, 2);
                    incoming_access_decrypted->src = big_endian_read_16(network_pdu->data, 5);
                    incoming_access_decrypted->dst = big_endian_read_16(network_pdu->data, 7);

                    mesh_upper_transport_process_access_message();
                }
                break;
            case MESH_PDU_TYPE_SEGMENTED:
                segmented_pdu = (mesh_segmented_pdu_t *) pdu;
                uint8_t ctl = segmented_pdu->ctl_ttl >> 7;
                if (ctl){
                    incoming_control_pdu=  &incoming_pdu_singleton.control;
                    incoming_control_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_CONTROL;

                    // flatten
                    mesh_segmented_pdu_flatten(&segmented_pdu->segments, 8, incoming_control_pdu->data);

                    // copy meta data into encrypted pdu buffer
                    incoming_control_pdu->flags = 0;
                    incoming_control_pdu->len =  segmented_pdu->len;
                    incoming_control_pdu->netkey_index =  segmented_pdu->netkey_index;
                    incoming_control_pdu->akf_aid_control = segmented_pdu->akf_aid_control;
                    incoming_control_pdu->ivi_nid = segmented_pdu->ivi_nid;
                    incoming_control_pdu->ctl_ttl = segmented_pdu->ctl_ttl;
                    incoming_control_pdu->seq = segmented_pdu->seq;
                    incoming_control_pdu->src = segmented_pdu->src;
                    incoming_control_pdu->dst = segmented_pdu->dst;

                    mesh_print_hex("Assembled payload", incoming_control_pdu->data, incoming_control_pdu->len);

                    // free mesh message
                    mesh_lower_transport_message_processed_by_higher_layer((mesh_pdu_t *)segmented_pdu);

                    btstack_assert(mesh_control_message_handler != NULL);
                    mesh_control_message_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, (mesh_pdu_t*) incoming_control_pdu);

                } else {

                    incoming_access_encrypted = (mesh_pdu_t *) segmented_pdu;

                    incoming_access_decrypted = &incoming_pdu_singleton.access;
                    incoming_access_decrypted->pdu_header.pdu_type = MESH_PDU_TYPE_ACCESS;
                    incoming_access_decrypted->flags = segmented_pdu->flags;
                    incoming_access_decrypted->len =  segmented_pdu->len;
                    incoming_access_decrypted->netkey_index = segmented_pdu->netkey_index;
                    incoming_access_decrypted->akf_aid_control =  segmented_pdu->akf_aid_control;
                    incoming_access_decrypted->ivi_nid = segmented_pdu->ivi_nid;
                    incoming_access_decrypted->ctl_ttl = segmented_pdu->ctl_ttl;
                    incoming_access_decrypted->seq = segmented_pdu->seq;
                    incoming_access_decrypted->src = segmented_pdu->src;
                    incoming_access_decrypted->dst = segmented_pdu->dst;

                    mesh_upper_transport_process_access_message();
                }
                break;
            default:
                btstack_assert(0);
                break;
        }
    }

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &upper_transport_outgoing);
    while (btstack_linked_list_iterator_has_next(&it)){

        if (crypto_active) break;

        mesh_pdu_t * pdu =  (mesh_pdu_t *) btstack_linked_list_iterator_next(&it);
        if (mesh_lower_transport_can_send_to_dest(mesh_pdu_dst(pdu)) == false) {
            // skip pdu for now
            continue;
        }

        mesh_upper_transport_pdu_t * upper_pdu;
        mesh_segmented_pdu_t * segmented_pdu;
        uint8_t transmic_len;
        bool ok;
        bool abort_outgoing_loop = false;

        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
                // control pdus can go through directly
                btstack_assert(mesh_pdu_ctl(pdu) != 0);
                btstack_linked_list_iterator_remove(&it);
                mesh_upper_transport_send_unsegmented_control_pdu((mesh_network_pdu_t *) pdu);
                break;
            case MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL:
                // control pdus can go through directly
                btstack_assert(mesh_pdu_ctl(pdu) != 0);
                btstack_linked_list_iterator_remove(&it);
                mesh_upper_transport_send_segmented_control_pdu((mesh_upper_transport_pdu_t *) pdu);
                break;
            case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
                // segmented access pdus required a mesh-segmented-pdu
                upper_pdu = (mesh_upper_transport_pdu_t *) pdu;
                if (upper_pdu->lower_pdu == NULL){
                    upper_pdu->lower_pdu  = (mesh_pdu_t *) btstack_memory_mesh_segmented_pdu_get();
                }
                if (upper_pdu->lower_pdu == NULL){
                    mesh_upper_transport_need_pdu_for_run_outgoing();
                    abort_outgoing_loop = true;
                    break;
                }
                segmented_pdu = (mesh_segmented_pdu_t *) upper_pdu->lower_pdu;
                segmented_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;
                // and a mesh-network-pdu for each segment in upper pdu
                transmic_len = ((upper_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 8 : 4;
                ok = mesh_segmented_allocate_segments(&segmented_pdu->segments, upper_pdu->len + transmic_len);
                if (!ok) {
                    abort_outgoing_loop = true;
                    break;
                }
                // all buffers available, get started
                btstack_linked_list_iterator_remove(&it);
                mesh_upper_transport_send_access(upper_pdu);
                break;
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
                // unsegmented access pdus require a single mesh-network-dpu
                upper_pdu = (mesh_upper_transport_pdu_t *) pdu;
                if (upper_pdu->lower_pdu == NULL){
                    upper_pdu->lower_pdu = (mesh_pdu_t *) mesh_network_pdu_get();
                }
                if (upper_pdu->lower_pdu == NULL) {
                    mesh_upper_transport_need_pdu_for_run_outgoing();
                    abort_outgoing_loop = true;
                    break;
                }
                btstack_linked_list_iterator_remove(&it);
                mesh_upper_transport_send_access((mesh_upper_transport_pdu_t *) pdu);
                break;
            default:
                btstack_assert(false);
                break;
        }
        if (abort_outgoing_loop) {
            break;
        }
    }
}

static mesh_upper_transport_pdu_t * mesh_upper_transport_find_and_remove_pdu_for_lower(mesh_pdu_t * pdu_to_find){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &upper_transport_outgoing_active);
    mesh_upper_transport_pdu_t * upper_pdu;
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_pdu_t * mesh_pdu = (mesh_pdu_t *) btstack_linked_list_iterator_next(&it);
        switch (mesh_pdu->pdu_type){
            case MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL:
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
                upper_pdu = (mesh_upper_transport_pdu_t *) mesh_pdu;
                if (upper_pdu->lower_pdu == pdu_to_find){
                    btstack_linked_list_iterator_remove(&it);
                    return upper_pdu;
                }
                break;
            default:
                break;
        }
    }
    return NULL;
}

static void mesh_upper_transport_pdu_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    mesh_upper_transport_pdu_t * upper_pdu;
    mesh_segmented_pdu_t * segmented_pdu;
    switch (callback_type){
        case MESH_TRANSPORT_PDU_RECEIVED:
            mesh_upper_transport_message_received(pdu);
            break;
        case MESH_TRANSPORT_PDU_SENT:
            switch (pdu->pdu_type){
                case MESH_PDU_TYPE_SEGMENTED:
                    // try to find in outgoing active
                    upper_pdu = mesh_upper_transport_find_and_remove_pdu_for_lower(pdu);
                    btstack_assert(upper_pdu != NULL);
                    segmented_pdu = (mesh_segmented_pdu_t *) pdu;
                    // free chunks
                    while (!btstack_linked_list_empty(&segmented_pdu->segments)){
                        mesh_network_pdu_t * chunk_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&segmented_pdu->segments);
                        mesh_network_pdu_free(chunk_pdu);
                    }
                    // free segmented pdu
                    btstack_memory_mesh_segmented_pdu_free(segmented_pdu);
                    upper_pdu->lower_pdu = NULL;
                    switch (upper_pdu->pdu_header.pdu_type){
                        case MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL:
                            mesh_control_message_handler(callback_type, status, (mesh_pdu_t *) upper_pdu);
                            break;
                        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
                            mesh_access_message_handler(callback_type, status, (mesh_pdu_t *) upper_pdu);
                            break;
                        default:
                            btstack_assert(false);
                            break;
                    }
                    break;
                case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
                    // find corresponding upper transport pdu and free single segment
                    upper_pdu = mesh_upper_transport_find_and_remove_pdu_for_lower(pdu);
                    btstack_assert(upper_pdu != NULL);
                    btstack_assert(upper_pdu->lower_pdu == (mesh_pdu_t *) pdu);
                    mesh_network_pdu_free((mesh_network_pdu_t *) pdu);
                    upper_pdu->lower_pdu = NULL;
                    mesh_access_message_handler(callback_type, status, (mesh_pdu_t*) upper_pdu);
                    break;
                case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
                    mesh_access_message_handler(callback_type, status, pdu);
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
    btstack_assert(pdu != NULL);
    mesh_network_pdu_t   * network_pdu;
    mesh_segmented_pdu_t   * message_pdu;
    mesh_upper_transport_pdu_t * upper_pdu;
    switch (pdu->pdu_type) {
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
        case MESH_PDU_TYPE_NETWORK:
            network_pdu = (mesh_network_pdu_t *) pdu;
            mesh_network_pdu_free(network_pdu);
            break;
        case MESH_PDU_TYPE_SEGMENTED:
            message_pdu = (mesh_segmented_pdu_t *) pdu;
            mesh_segmented_pdu_free(message_pdu);
            break;
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL:
            upper_pdu = (mesh_upper_transport_pdu_t *) pdu;
            while (upper_pdu->segments) {
                mesh_network_pdu_t *segment = (mesh_network_pdu_t *) btstack_linked_list_pop(&upper_pdu->segments);
                mesh_network_pdu_free(segment);
            }
            btstack_memory_mesh_upper_transport_pdu_free(upper_pdu);
            // check if send request can be handled now
            mesh_upper_transport_schedule_send_requests();
            break;
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
            break;
        case MESH_PDU_TYPE_CONTROL:
            mesh_upper_transport_process_control_message_done((mesh_control_pdu_t *) pdu);
            break;
        default:
            btstack_assert(0);
            break;
    }
}

void mesh_upper_transport_send_access_pdu(mesh_pdu_t *pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            break;
        default:
            btstack_assert(false);
            break;
    }

    btstack_assert(((mesh_upper_transport_pdu_t *) pdu)->lower_pdu == NULL);

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

uint8_t mesh_upper_transport_setup_unsegmented_control_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                                                                  const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    btstack_assert(network_pdu != NULL);
    btstack_assert(control_pdu_len <= 11);

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    uint8_t transport_pdu_data[12];
    transport_pdu_data[0] = opcode;
    (void)memcpy(&transport_pdu_data[1], control_pdu_data, control_pdu_len);
    uint16_t transport_pdu_len = control_pdu_len + 1;

    // setup network_pdu
    network_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL;
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, 0, src, dest, transport_pdu_data, transport_pdu_len);

    return 0;
}

uint8_t mesh_upper_transport_setup_segmented_control_pdu_header(mesh_upper_transport_pdu_t * upper_pdu, uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode){

    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 1;

    upper_pdu->ivi_nid = network_key->nid | ((mesh_get_iv_index_for_tx() & 1) << 7);
    upper_pdu->ctl_ttl = ttl;
    upper_pdu->src = src;
    upper_pdu->dst = dest;
    upper_pdu->netkey_index = netkey_index;
    upper_pdu->akf_aid_control = opcode;
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

    // store in transport pdu
    upper_pdu->ivi_nid = network_key->nid | ((mesh_get_iv_index_for_tx() & 1) << 7);
    upper_pdu->ctl_ttl = ttl;
    upper_pdu->src = src;
    upper_pdu->dst = dest;
    upper_pdu->netkey_index = netkey_index;
    upper_pdu->appkey_index = appkey_index;
    upper_pdu->akf_aid_control = akf_aid;
    if (szmic) {
        upper_pdu->flags |= MESH_TRANSPORT_FLAG_TRANSMIC_64;
    }
    return 0;
}

uint8_t mesh_upper_transport_setup_access_pdu_header(mesh_pdu_t * pdu, uint16_t netkey_index, uint16_t appkey_index,
                                                     uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            return mesh_upper_transport_setup_upper_access_pdu_header((mesh_upper_transport_pdu_t *) pdu, netkey_index,
                                                               appkey_index, ttl, src, dest, szmic);
        default:
            btstack_assert(false);
            return 1;
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

bool mesh_upper_transport_message_reserve(void){
    if (message_builder_reserved_upper_pdu == NULL){
        message_builder_reserved_upper_pdu = btstack_memory_mesh_upper_transport_pdu_get();
    }
    if (message_builder_reserved_upper_pdu == NULL){
        return false;
    }
    while (message_builder_num_network_pdus_reserved < MESSAGE_BUILDER_MAX_NUM_NETWORK_PDUS){
        mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
        if (network_pdu == NULL){
            return false;
        }
        btstack_linked_list_add(&message_builder_reserved_network_pdus, (btstack_linked_item_t *) network_pdu);
        message_builder_num_network_pdus_reserved++;
    }
    return true;
}

void mesh_upper_transport_message_init(mesh_upper_transport_builder_t * builder, mesh_pdu_type_t pdu_type) {
    btstack_assert(builder != NULL);

    // use reserved buffer if available
    if (message_builder_reserved_upper_pdu != NULL){
        builder->pdu = message_builder_reserved_upper_pdu;
        message_builder_reserved_upper_pdu = NULL;
    } else {
        builder->pdu = btstack_memory_mesh_upper_transport_pdu_get();
    }
    if (!builder->pdu) return;

    builder->segment = NULL;
    builder->pdu->pdu_header.pdu_type = pdu_type;
    builder->pdu->ack_opcode = MESH_ACCESS_OPCODE_NOT_SET;
}


void mesh_upper_transport_message_add_data(mesh_upper_transport_builder_t * builder, const uint8_t * data, uint16_t data_len){
    btstack_assert(builder != NULL);

    if (builder->pdu == NULL) return;

    builder->pdu->len += data_len;

    uint16_t bytes_current_segment = 0;
    if (builder->segment){
        bytes_current_segment = MESH_NETWORK_PAYLOAD_MAX - builder->segment->len;
    }
    while (data_len > 0){
        if (bytes_current_segment == 0){
            // use reserved buffer if available
            if (message_builder_num_network_pdus_reserved > 0){
                message_builder_num_network_pdus_reserved--;
                builder->segment = (mesh_network_pdu_t *) btstack_linked_list_pop(&message_builder_reserved_network_pdus);
            } else {
                builder->segment = (mesh_network_pdu_t *) mesh_network_pdu_get();
            }
            if (builder->segment == NULL) {
                mesh_upper_transport_pdu_free((mesh_pdu_t *) builder->pdu);
                builder->pdu = NULL;
                return;
            }
            btstack_linked_list_add_tail(&builder->pdu->segments, (btstack_linked_item_t *) builder->segment);
            bytes_current_segment = MESH_NETWORK_PAYLOAD_MAX;
        }
        uint16_t bytes_to_copy = btstack_min(bytes_current_segment, data_len);
        (void) memcpy(&builder->segment->data[builder->segment->len], data, bytes_to_copy);
        builder->segment->len += bytes_to_copy;
        bytes_current_segment -= bytes_to_copy;
        data                  += bytes_to_copy;
        data_len              -= bytes_to_copy;
    }
}

void mesh_upper_transport_message_add_uint8(mesh_upper_transport_builder_t * builder, uint8_t value){
    mesh_upper_transport_message_add_data(builder, &value, 1);
}

void mesh_upper_transport_message_add_uint16(mesh_upper_transport_builder_t * builder, uint16_t value){
    uint8_t buffer[2];
    little_endian_store_16(buffer, 0, value);
    mesh_upper_transport_message_add_data(builder, buffer, sizeof(buffer));
}

void mesh_upper_transport_message_add_uint24(mesh_upper_transport_builder_t * builder, uint32_t value){
    uint8_t buffer[3];
    little_endian_store_24(buffer, 0, value);
    mesh_upper_transport_message_add_data(builder, buffer, sizeof(buffer));
}

void mesh_upper_transport_message_add_uint32(mesh_upper_transport_builder_t * builder, uint32_t value){
    uint8_t buffer[4];
    little_endian_store_32(buffer, 0, value);
    mesh_upper_transport_message_add_data(builder, buffer, sizeof(buffer));
}

mesh_upper_transport_pdu_t * mesh_upper_transport_message_finalize(mesh_upper_transport_builder_t * builder){
    return builder->pdu;
}

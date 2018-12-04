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

void mesh_transport_set_device_key(const uint8_t * device_key){
    mesh_transport_device_key.index = MESH_DEVICE_KEY_INDEX;
    mesh_transport_device_key.aid   = 0;
    memcpy(mesh_transport_device_key.key, device_key, 16);
}

static const mesh_application_key_t * mesh_application_key_list_get(uint16_t appkey_index){
    if (appkey_index != test_application_key.index) return NULL;
    return &test_application_key;
}

static const mesh_application_key_t * mesh_device_key_get(void){
    return &mesh_transport_device_key;
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
static uint32_t mesh_transport_outgoing_seq = 0;

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
static uint16_t mesh_transport_src(mesh_transport_pdu_t * transport_pdu){
    return big_endian_read_16(transport_pdu->network_header, 5);
}
static uint16_t mesh_transport_dest(mesh_transport_pdu_t * transport_pdu){
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

//


static void mesh_transport_process_unsegmented_control_message(mesh_network_pdu_t * network_pdu){
    printf("Unsegmented Control message\n");
    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t  opcode = lower_transport_pdu[0];
    uint16_t seq_zero_pdu;
    uint16_t seq_zero_out;
    uint32_t block_ack;
    switch (opcode){
        case 0:
            if (upper_transport_outgoing_pdu == NULL) break;
            seq_zero_pdu = big_endian_read_16(lower_transport_pdu, 1) >> 2;
            seq_zero_out = mesh_transport_seq(upper_transport_outgoing_pdu) & 0x1fff;
            block_ack = big_endian_read_32(lower_transport_pdu, 3); 
            printf("[+] Segment Acknowledgment message with seq_zero %06x, block_ack %08x - outgoing seq %06x, block_ack %08x",
                seq_zero_pdu, block_ack, seq_zero_out, upper_transport_outgoing_pdu->block_ack);
            if (seq_zero_pdu == seq_zero_out){
                upper_transport_outgoing_pdu->block_ack &= ~block_ack;
                printf("[+] Updated block_ack %08x", upper_transport_outgoing_pdu->block_ack);
                if (upper_transport_outgoing_pdu->block_ack == 0){
                    printf("[+] Sent complete\n");

                    btstack_memory_mesh_transport_pdu_free(upper_transport_outgoing_pdu);        
                    upper_transport_outgoing_pdu     = NULL;
                    btstack_memory_mesh_network_pdu_free(upper_transport_outgoing_segment);        
                    upper_transport_outgoing_segment = NULL;
                }
            } else {
                printf("[!] Seq Zero doesn't match\n");
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
    btstack_crypo_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, trans_mic_len);

    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;

    mesh_print_hex("Decryted PDU", upper_transport_pdu, upper_transport_pdu_len - trans_mic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len - trans_mic_len], trans_mic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        network_pdu->len -= trans_mic_len;

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
    btstack_crypo_ccm_get_authentication_value(&ccm, trans_mic);
    mesh_print_hex("TransMIC", trans_mic, transport_pdu->transmic_len);

    if (memcmp(trans_mic, &upper_transport_pdu[upper_transport_pdu_len], transport_pdu->transmic_len) == 0){
        printf("TransMIC matches\n");

        // remove TransMIC from payload
        transport_pdu->len -= transport_pdu->transmic_len;

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
static void mesh_upper_transport_validate_unsegmented_message(mesh_network_pdu_t * network_pdu){
    uint8_t * lower_transport_pdu     = &network_pdu_in_validation->data[9];
    uint8_t   lower_transport_pdu_len = network_pdu_in_validation->len - 9;

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
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    network_pdu->appkey_index = message_key->index; 

    // unsegmented message have TransMIC of 32 bit
    uint8_t trans_mic_len = 4;
    printf("Unsegmented Access message with TransMIC len 4\n");

    uint8_t * upper_transport_pdu_data = &network_pdu_in_validation->data[10];
    uint8_t   upper_transport_pdu_len  = lower_transport_pdu_len - 1 - trans_mic_len;

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    btstack_crypo_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, 0, trans_mic_len);
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data, upper_transport_pdu_data, &mesh_upper_transport_validate_unsegmented_message_ccm, network_pdu_in_validation);
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
    mesh_print_hex("AppOrDevKey", message_key->key, 16);
    transport_pdu->appkey_index = message_key->index; 

    mesh_print_hex("EncAccessPayload", upper_transport_pdu_data, upper_transport_pdu_len);

    // decrypt ccm
    mesh_transport_crypto_active = 1;
    btstack_crypo_ccm_init(&ccm, message_key->key, application_nonce, upper_transport_pdu_len, 0, transport_pdu->transmic_len);
    btstack_crypto_ccm_decrypt_block(&ccm, upper_transport_pdu_len, upper_transport_pdu_data, upper_transport_pdu_data, &mesh_upper_transport_validate_segmented_message_ccm, transport_pdu);
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

    if (akf){
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
    mesh_print_hex("Upper Transport pdu", upper_transport_pdu, upper_transport_pdu_len);

    uint8_t aid =  transport_pdu->akf_aid & 0x3f;
    uint8_t akf = (transport_pdu->akf_aid & 0x40) >> 6;

    printf("AKF: %u\n",   akf);
    printf("AID: %02x\n", aid);

    if (akf){
        // init application key iterator if used
        mesh_application_key_iterator_init(&mesh_app_key_it, aid);
    }
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
    uint16_t seq = mesh_transport_seq(transport_pdu);
    mesh_lower_transport_setup_segemented_acknowledge_message(ack_msg, 0, seq & 0x1fff, transport_pdu->block_ack);

    printf("mesh_transport_send_ack with netkey_index %x, CTL=1, ttl = %u, seq = %x, src = %x, dst = %x\n", transport_pdu->netkey_index, mesh_transport_ttl(transport_pdu),
        mesh_transport_seq(transport_pdu), primary_element_address, mesh_transport_src(transport_pdu));

    mesh_network_send(transport_pdu->netkey_index, 1, mesh_transport_ttl(transport_pdu),
                      mesh_transport_outgoing_seq++, primary_element_address, mesh_transport_src(transport_pdu), 
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

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t   lower_transport_pdu_len = mesh_network_pdu_len(network_pdu);

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
    mesh_network_segmented_message_complete(test_transport_pdu);

    // free
    test_transport_pdu = NULL;

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

static uint32_t mesh_upper_transport_next_seq(void){
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
    uint16_t dest     = mesh_transport_dest(transport_pdu);    

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
    int ctl = mesh_transport_ctl(upper_transport_outgoing_pdu);
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)
    uint8_t  seg_n = (upper_transport_outgoing_pdu->len - 1) / max_segment_len;

    // find next unacknowledged segement
    while ((upper_transport_outgoing_seg_o <= seg_n) && ((upper_transport_outgoing_pdu->block_ack & (1 << upper_transport_outgoing_seg_o)) == 0)){
        upper_transport_outgoing_seg_o++;
    }

    if (upper_transport_outgoing_seg_o > seg_n){
        printf("[+] Upper transport, send segmented pdu complete\n");
        return;
    }

    // restart acknowledgment timer
    // - "This timer shall be set to a minimum of 200 + 50 * TTL milliseconds."
    if (upper_transport_outgoing_pdu->acknowledgement_timer_active){
        btstack_run_loop_remove_timer(&upper_transport_outgoing_pdu->incomplete_timer);
        upper_transport_outgoing_pdu->acknowledgement_timer_active = 0;
    }
    uint32_t timeout = 200 + 50 * mesh_transport_ttl(upper_transport_outgoing_pdu);
    mesh_transport_start_acknowledgment_timer(upper_transport_outgoing_pdu, timeout, &mesh_transport_tx_ack_timeout);

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

static void mesh_upper_transport_send_segmented_pdu(mesh_transport_pdu_t * transport_pdu){
    mesh_upper_transport_retry_count = 2;
    mesh_upper_transport_send_segmented_pdu_once(transport_pdu);
}

static void mesh_transport_tx_ack_timeout(btstack_timer_source_t * ts){
    printf("[+] Upper transport, acknowledgement timer fired\n");
    mesh_transport_pdu_t * transport_pdu = (mesh_transport_pdu_t *) btstack_run_loop_get_timer_context(ts);
    transport_pdu->acknowledgement_timer_active = 0;
    mesh_upper_transport_send_segmented_pdu_once(transport_pdu);
}

static void mesh_upper_transport_send_unsegmented_access_pdu_ccm(void * arg){
    mesh_transport_crypto_active = 0;

    mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) arg;
    uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
    uint8_t   upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;
    mesh_print_hex("EncAccessPayload", upper_transport_pdu, upper_transport_pdu_len);
    // store TransMIC
    btstack_crypo_ccm_get_authentication_value(&ccm, &upper_transport_pdu[upper_transport_pdu_len]);
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
    btstack_crypo_ccm_get_authentication_value(&ccm, &transport_pdu->data[transport_pdu->len]);
    mesh_print_hex("TransMIC", &transport_pdu->data[transport_pdu->len], transport_pdu->transmic_len);
    transport_pdu->len += transport_pdu->transmic_len;
    mesh_upper_transport_send_segmented_pdu(transport_pdu);
}

uint8_t mesh_upper_transport_access_send(uint16_t netkey_index, uint16_t appkey_index, uint8_t ttl, uint16_t src, uint16_t dest,
                          const uint8_t * access_pdu_data, uint8_t access_pdu_len, uint8_t szmic){

    uint32_t seq = mesh_upper_transport_peek_seq();

    printf("[+] Upper transport, send Access PDU (seq %06x, szmic %u): ", seq, szmic);
    printf_hexdump(access_pdu_data, access_pdu_len);


    // get app or device key
    uint8_t akf;
    const mesh_application_key_t * appkey;
    if (appkey_index == MESH_DEVICE_KEY_INDEX){
        appkey = mesh_device_key_get();
        akf = 0;
    } else {
        appkey = mesh_application_key_list_get(appkey_index);
        if (appkey == NULL){
            printf("appkey_index %x unknown\n", appkey_index);
            return 1;
        }
        akf = 1;
    }
    uint8_t akf_aid = (akf << 6) | appkey->aid;

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 0;

    const uint8_t trans_mic_len = szmic ? 8 : 4;

    if (access_pdu_len + trans_mic_len <= 15){
        // unsegmented access message

        // allocate network_pdu
        mesh_network_pdu_t * network_pdu = btstack_memory_mesh_network_pdu_get();
        if (!network_pdu) return 0;

        // setup access pdu
        uint8_t transport_pdu_data[16];
        transport_pdu_data[0] = akf_aid;
        memcpy(&transport_pdu_data[1], access_pdu_data, access_pdu_len);
        uint16_t transport_pdu_len = access_pdu_len + 1;
        mesh_print_hex("Access Payload", access_pdu_data, access_pdu_len);

        // setup network_pdu
        mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 0, ttl, mesh_upper_transport_next_seq(), src, dest, transport_pdu_data, transport_pdu_len);

        // setup nonce
        mesh_print_hex("AppOrDevKey", appkey->key, 16);
        if (akf){
            transport_unsegmented_setup_application_nonce(application_nonce, network_pdu);
        } else {
            transport_unsegmented_setup_device_nonce(application_nonce, network_pdu);
        }

        // encrypt ccm
        mesh_transport_crypto_active = 1;
        const uint8_t trans_mic_len = 4;
        uint8_t * upper_transport_pdu     = mesh_network_pdu_data(network_pdu) + 1;
        btstack_crypo_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, 0, trans_mic_len);
        btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, upper_transport_pdu, upper_transport_pdu, &mesh_upper_transport_send_unsegmented_access_pdu_ccm, network_pdu);
        return 0;
    }
    if (access_pdu_len + trans_mic_len <= 384){
        // temp store in transport pdu
        mesh_transport_pdu_t * transport_pdu = btstack_memory_mesh_transport_pdu_get();
        if (!transport_pdu) return 0;
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

        // setup nonce
        mesh_print_hex("AppOrDevKey", appkey->key, 16);
        if (akf){
            transport_segmented_setup_application_nonce(application_nonce, transport_pdu);
        } else {
            transport_segmented_setup_device_nonce(application_nonce, transport_pdu);
        }

        // encrypt ccm
        btstack_crypo_ccm_init(&ccm, appkey->key, application_nonce, access_pdu_len, 0, trans_mic_len);
        btstack_crypto_ccm_encrypt_block(&ccm, access_pdu_len, transport_pdu->data, transport_pdu->data, &mesh_upper_transport_send_segmented_access_pdu_ccm, transport_pdu);
    }
    return 0;
}

uint8_t mesh_upper_transport_send_control_pdu(uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, uint8_t opcode,
                          const uint8_t * control_pdu_data, uint16_t control_pdu_len){

    printf("[+] Upper transport, send Control PDU (opcode %02x): \n", opcode);
    printf_hexdump(control_pdu_data, control_pdu_len);

    uint32_t seq = mesh_upper_transport_peek_seq();

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 0;

    if (control_pdu_len <= 11){
        // unsegmented control message

        // allocate network_pdu
        mesh_network_pdu_t * network_pdu = btstack_memory_mesh_network_pdu_get();
        if (!network_pdu) return 0;

        // setup access pdu
        uint8_t transport_pdu_data[12];
        transport_pdu_data[0] = opcode;
        memcpy(&transport_pdu_data[1], control_pdu_data, control_pdu_len);
        uint16_t transport_pdu_len = control_pdu_len + 1;

        mesh_print_hex("LowerTransportPDU", transport_pdu_data, transport_pdu_len);
        // setup network_pdu
        mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, mesh_upper_transport_next_seq(), src, dest, transport_pdu_data, transport_pdu_len);
        // send network pdu
        mesh_network_send_pdu(network_pdu);

        return 0;
    }
    if (control_pdu_len <= 256) {

        // store in transport pdu
        mesh_transport_pdu_t * transport_pdu = btstack_memory_mesh_transport_pdu_get();
        if (!transport_pdu) return 0;
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

        //
        mesh_upper_transport_send_segmented_pdu(transport_pdu);
    }
    // Message too long
    return 0;
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

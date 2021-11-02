/*
 * Copyright (C) 2017 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "provisioning_provisioner.c"

#include "mesh/provisioning_provisioner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"

#include "mesh/mesh_crypto.h"
#include "mesh/pb_adv.h"
#include "mesh/provisioning.h"

static void provisioning_public_key_ready(void);

// global
static uint8_t         prov_ec_q[64];
static const uint8_t * prov_public_key_oob_q;
static const uint8_t * prov_public_key_oob_d;
static uint8_t  prov_public_key_oob_available;

static btstack_packet_handler_t prov_packet_handler;

// NetKey
static uint8_t  net_key[16];
// NetKeyIndex
static uint16_t net_key_index;
// Flags
static uint8_t  flags;
// IV Index
static uint32_t iv_index;

// either used once or per session
static btstack_crypto_aes128_cmac_t prov_cmac_request;
static btstack_crypto_random_t      prov_random_request;
static btstack_crypto_ecc_p256_t    prov_ecc_p256_request;
static btstack_crypto_ccm_t         prov_ccm_request;

// data per provisioning session
static btstack_timer_source_t       prov_protocol_timer;

static uint16_t pb_adv_cid;
static uint8_t  prov_attention_timer;
static uint8_t  prov_buffer_out[100];   // TODO: how large are prov messages?
static uint8_t  prov_waiting_for_outgoing_complete;
static uint8_t  prov_error_code;
static uint8_t  prov_start_algorithm;
static uint8_t  prov_start_public_key_used;
static uint8_t  prov_start_authentication_method;
static uint8_t  prov_start_authentication_action;
static uint8_t  prov_start_authentication_size;
static uint8_t  prov_authentication_string;
// ConfirmationInputs = ProvisioningInvitePDUValue || ProvisioningCapabilitiesPDUValue || ProvisioningStartPDUValue || PublicKeyProvisioner || PublicKeyDevice
static uint8_t  prov_confirmation_inputs[1 + 11 + 5 + 64 + 64];
static uint8_t  confirmation_provisioner[16];
static uint8_t  random_provisioner[16];
static uint8_t  auth_value[16];
static uint8_t  remote_ec_q[64];
static uint8_t  dhkey[32];
static uint8_t  confirmation_salt[16];
static uint8_t  confirmation_key[16];
static uint8_t provisioning_salt[16];
static uint8_t session_key[16];
static uint8_t session_nonce[16];
static uint16_t unicast_address;
static uint8_t provisioning_data[25];
static uint8_t enc_provisioning_data[25];
static uint8_t provisioning_data_mic[8];
static uint8_t  prov_emit_output_oob_active;

static const uint8_t * prov_static_oob_data;
static uint16_t  prov_static_oob_len;

#if 0
static uint8_t  prov_public_key_oob_used;
static uint8_t  prov_emit_public_key_oob_active;

// capabilites

static uint16_t  prov_output_oob_actions;
static uint16_t  prov_input_oob_actions;
static uint8_t   prov_output_oob_size;
static uint8_t   prov_input_oob_size;

// derived
static uint8_t network_id[8];
static uint8_t beacon_key[16];

static void provisioning_attention_timer_timeout(btstack_timer_source_t * ts){
    UNUSED(ts);
    if (prov_attention_timer_timeout == 0) return;
    prov_attention_timer_timeout--;
    provisioning_attention_timer_set();
}

static void provisioning_attention_timer_set(void){
    provisioning_emit_attention_timer_event(1, prov_attention_timer_timeout);
    if (prov_attention_timer_timeout){
        btstack_run_loop_set_timer_handler(&prov_attention_timer, &provisioning_attention_timer_timeout);
        btstack_run_loop_set_timer(&prov_attention_timer, 1000);
        btstack_run_loop_add_timer(&prov_attention_timer);
    }
}
#endif 

static void provisioning_emit_output_oob_event(uint16_t the_pb_adv_cid, uint32_t number){
    if (!prov_packet_handler) return;
    uint8_t event[9] = { HCI_EVENT_MESH_META, 7, MESH_SUBEVENT_PB_PROV_START_EMIT_OUTPUT_OOB};
    little_endian_store_16(event, 3, the_pb_adv_cid);
    little_endian_store_16(event, 5, number);
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisioning_emit_event(uint8_t mesh_subevent, uint16_t the_pb_adv_cid){
    if (!prov_packet_handler) return;
    uint8_t event[5] = { HCI_EVENT_MESH_META, 3, mesh_subevent};
    little_endian_store_16(event, 3, the_pb_adv_cid);
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisiong_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("Provisioning Protocol Timeout -> Close Link!\n");
    // TODO: use actual pb_adv_cid
    pb_adv_close_link(1, 1);
}

// The provisioning protocol shall have a minimum timeout of 60 seconds that is reset
// each time a provisioning protocol PDU is sent or received
static void provisioning_timer_start(void){
    btstack_run_loop_remove_timer(&prov_protocol_timer);
    btstack_run_loop_set_timer_handler(&prov_protocol_timer, &provisiong_timer_handler);
    btstack_run_loop_set_timer(&prov_protocol_timer, PROVISIONING_PROTOCOL_TIMEOUT_MS);
    btstack_run_loop_add_timer(&prov_protocol_timer);
}

static void provisioning_timer_stop(void){
    btstack_run_loop_remove_timer(&prov_protocol_timer);
}

// Outgoing Provisioning PDUs

static void provisioning_send_invite(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_INVITE;
    prov_buffer_out[1] = prov_attention_timer;
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 2);
    // collect confirmation_inputs
    (void)memcpy(&prov_confirmation_inputs[0], &prov_buffer_out[1], 1);
}

static void provisioning_send_start(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_START;
    prov_buffer_out[1] = prov_start_algorithm;
    prov_buffer_out[2] = prov_start_public_key_used;
    prov_buffer_out[3] = prov_start_authentication_method;
    prov_buffer_out[4] = prov_start_authentication_action;
    prov_buffer_out[5] = prov_start_authentication_size;
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 6);
    // store for confirmation inputs: len 5
    (void)memcpy(&prov_confirmation_inputs[12], &prov_buffer_out[1], 5);
}

static void provisioning_send_provisioning_error(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_FAILED;
    prov_buffer_out[1] = prov_error_code;
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 2);
}

static void provisioning_send_public_key(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_PUB_KEY;
    (void)memcpy(&prov_buffer_out[1], prov_ec_q, 64);
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 65);
    // store for confirmation inputs: len 64
    (void)memcpy(&prov_confirmation_inputs[17], &prov_buffer_out[1], 64);
}

static void provisioning_send_confirm(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_CONFIRM;
    (void)memcpy(&prov_buffer_out[1], confirmation_provisioner, 16);
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 17);
}

static void provisioning_send_random(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_RANDOM;
    (void)memcpy(&prov_buffer_out[1], random_provisioner, 16);
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 17);
}

static void provisioning_send_data(uint16_t the_pb_adv_cid){
    prov_buffer_out[0] = MESH_PROV_DATA;
    (void)memcpy(&prov_buffer_out[1], enc_provisioning_data, 25);
    (void)memcpy(&prov_buffer_out[26], provisioning_data_mic, 8);
    pb_adv_send_pdu(the_pb_adv_cid, prov_buffer_out, 34);
}

typedef enum {
    PROVISIONER_IDLE,
    PROVISIONER_W4_LINK_OPENED,
    PROVISIONER_SEND_INVITE,
    PROVISIONER_W4_CAPABILITIES,
    PROVISIONER_W4_AUTH_CONFIGURATION,
    PROVISIONER_SEND_START,
    PROVISIONED_W2_EMIT_READ_PUB_KEY_OOB,
    PROVISIONER_SEND_PUB_KEY,
    PROVISIONER_W4_PUB_KEY,
    PROVISIONER_W4_PUB_KEY_OOB,
    PROVISIONER_W4_INPUT_OOK,
    PROVISIONER_W4_INPUT_COMPLETE,
    PROVISIONER_SEND_CONFIRM,
    PROVISIONER_W4_CONFIRM,
    PROVISIONER_SEND_RANDOM,
    PROVISIONER_W4_RANDOM,
    PROVISIONER_SEND_DATA,
    PROVISIONER_W4_COMPLETE,
    PROVISIONER_SEND_ERROR,
} provisioner_state_t;

static provisioner_state_t provisioner_state;

static void provisioning_run(void){
    if (prov_waiting_for_outgoing_complete) return;
    int start_timer = 1;
    switch (provisioner_state){
        case PROVISIONER_SEND_ERROR:
            start_timer = 0;    // game over
            provisioning_send_provisioning_error(pb_adv_cid);
            break;
        case PROVISIONER_SEND_INVITE:
            provisioning_send_invite(pb_adv_cid);
            provisioner_state = PROVISIONER_W4_CAPABILITIES;
            break;
        case PROVISIONER_SEND_START:
            provisioning_send_start(pb_adv_cid);
            if (prov_start_public_key_used){
                provisioner_state = PROVISIONED_W2_EMIT_READ_PUB_KEY_OOB;
            } else {
                provisioner_state = PROVISIONER_SEND_PUB_KEY;
            }
            break;
        case PROVISIONED_W2_EMIT_READ_PUB_KEY_OOB:
            printf("Public OOB: please read OOB from remote device\n");
            provisioner_state = PROVISIONER_W4_PUB_KEY_OOB;
            provisioning_emit_event(MESH_SUBEVENT_PB_PROV_START_RECEIVE_PUBLIC_KEY_OOB, 1);
            break;
        case PROVISIONER_SEND_PUB_KEY:
            provisioning_send_public_key(pb_adv_cid);
            if (prov_start_public_key_used){
                provisioning_public_key_ready();
            } else {
                provisioner_state = PROVISIONER_W4_PUB_KEY;
            }
            break;
        case PROVISIONER_SEND_CONFIRM:
            provisioning_send_confirm(pb_adv_cid);
            provisioner_state = PROVISIONER_W4_CONFIRM;
            break;
        case PROVISIONER_SEND_RANDOM:
            provisioning_send_random(pb_adv_cid);
            provisioner_state = PROVISIONER_W4_RANDOM;
            break;
        case PROVISIONER_SEND_DATA:
            provisioning_send_data(pb_adv_cid);
            provisioner_state = PROVISIONER_W4_COMPLETE;
            break;
        default:
            return;
    }
    if (start_timer){
        provisioning_timer_start();
    }
    prov_waiting_for_outgoing_complete = 1;
}

// End of outgoing PDUs

static void provisioning_done(void){
    // if (prov_emit_public_key_oob_active){
    //     prov_emit_public_key_oob_active = 0;
    //     provisioning_emit_event(MESH_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB, 1);
    // }
    if (prov_emit_output_oob_active){
        prov_emit_output_oob_active = 0;
        provisioning_emit_event(MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB, 1);
    }
    provisioner_state = PROVISIONER_IDLE;
}


static void provisioning_handle_provisioning_error(uint8_t error_code){
    provisioning_timer_stop();
    prov_error_code = error_code;
    provisioner_state = PROVISIONER_SEND_ERROR;
    provisioning_run();
}

static void provisioning_handle_link_opened(uint16_t the_pb_adv_cid){
    UNUSED(the_pb_adv_cid);
    provisioner_state = PROVISIONER_SEND_INVITE;
}

static void provisioning_handle_capabilities(uint16_t the_pb_adv_cid, const uint8_t * packet_data, uint16_t packet_len){
    
    if (packet_len != 11) return;

    // collect confirmation_inputs
    (void)memcpy(&prov_confirmation_inputs[1], packet_data, packet_len);

    provisioner_state = PROVISIONER_W4_AUTH_CONFIGURATION; 

    // notify client and wait for auth method selection
    uint8_t event[16] = { HCI_EVENT_MESH_META, 3, MESH_SUBEVENT_PB_PROV_CAPABILITIES};
    little_endian_store_16(event, 3, the_pb_adv_cid);
    event[5] = packet_data[0];
    little_endian_store_16(event, 6, big_endian_read_16(packet_data, 1));
    event[8] = packet_data[3];
    event[9] = packet_data[4];
    event[10] = packet_data[5];
    little_endian_store_16(event, 11, big_endian_read_16(packet_data, 6));
    event[13] = packet_data[8];
    little_endian_store_16(event, 14, big_endian_read_16(packet_data, 9));
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisioning_handle_confirmation_provisioner_calculated(void * arg){
    UNUSED(arg);

    printf("ConfirmationProvisioner: ");
    printf_hexdump(confirmation_provisioner, sizeof(confirmation_provisioner));

    provisioner_state = PROVISIONER_SEND_CONFIRM;
    provisioning_run();
}

static void provisioning_handle_random_provisioner(void * arg){
    UNUSED(arg);

    printf("RandomProvisioner:   ");
    printf_hexdump(random_provisioner, sizeof(random_provisioner));

    // re-use prov_confirmation_inputs buffer
    (void)memcpy(&prov_confirmation_inputs[0], random_provisioner, 16);
    (void)memcpy(&prov_confirmation_inputs[16], auth_value, 16);

    // calc confirmation device
    btstack_crypto_aes128_cmac_message(&prov_cmac_request, confirmation_key, 32, prov_confirmation_inputs, confirmation_provisioner, &provisioning_handle_confirmation_provisioner_calculated, NULL);
}

static void provisioning_handle_confirmation_k1_calculated(void * arg){
    UNUSED(arg);

    printf("ConfirmationKey:   ");
    printf_hexdump(confirmation_key, sizeof(confirmation_key));

    // generate random_device
    btstack_crypto_random_generate(&prov_random_request,random_provisioner, 16, &provisioning_handle_random_provisioner, NULL);
}

static void provisioning_handle_confirmation_salt(void * arg){
    UNUSED(arg);

    // dump
    printf("ConfirmationSalt:   ");
    printf_hexdump(confirmation_salt, sizeof(confirmation_salt));

    // ConfirmationKey
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), confirmation_salt, (const uint8_t*) "prck", 4, confirmation_key, &provisioning_handle_confirmation_k1_calculated, NULL);
}

static void provisioning_handle_auth_value_ready(void){
    // CalculationInputs
    printf("ConfirmationInputs: ");
    printf_hexdump(prov_confirmation_inputs, sizeof(prov_confirmation_inputs));

    // calculate s1
    btstack_crypto_aes128_cmac_zero(&prov_cmac_request, sizeof(prov_confirmation_inputs), prov_confirmation_inputs, confirmation_salt, &provisioning_handle_confirmation_salt, NULL);
}

static void provisioning_handle_auth_value_input_oob(void * arg){
    UNUSED(arg);

    // limit auth value to single digit
    auth_value[15] = auth_value[15] % 9 + 1;
    printf("Input OOB: %u\n", auth_value[15]);

    if (prov_authentication_string){
        // strings start at 0 while numbers are stored as 16-byte big endian
        auth_value[0] = auth_value[15] + '0';
        auth_value[15] = 0;
    }

    printf("AuthValue: ");
    printf_hexdump(auth_value, sizeof(auth_value));

    // emit output oob value
    provisioning_emit_output_oob_event(1, auth_value[15]);
    prov_emit_output_oob_active = 1;

    provisioner_state = PROVISIONER_W4_INPUT_COMPLETE;
}

static void provisioning_handle_input_complete(uint16_t the_pb_adv_cid){
    UNUSED(the_pb_adv_cid);
    provisioning_handle_auth_value_ready();
}

static void provisioning_public_key_exchange_complete(void){
    // reset auth_value
    memset(auth_value, 0, sizeof(auth_value));

    // handle authentication method
    switch (prov_start_authentication_method){
        case 0x00:
            provisioning_handle_auth_value_ready();
            break;        
        case 0x01:
            (void)memcpy(&auth_value[16 - prov_static_oob_len],
                         prov_static_oob_data, prov_static_oob_len);
            provisioning_handle_auth_value_ready();
            break;
        case 0x02:
            // Output OOB
            prov_authentication_string = prov_start_authentication_action == 0x04;
            printf("Output OOB requested (and we're in Provisioniner role), string %u\n", prov_authentication_string);
            provisioner_state = PROVISIONER_W4_INPUT_OOK;
            provisioning_emit_event(MESH_SUBEVENT_PB_PROV_OUTPUT_OOB_REQUEST, 1);
            break;
        case 0x03:
            // Input OOB
            prov_authentication_string = prov_start_authentication_action == 0x03;
            printf("Input OOB requested, string %u\n", prov_authentication_string);
            printf("Generate random for auth_value\n");
            // generate single byte of random data to use for authentication
            btstack_crypto_random_generate(&prov_random_request, &auth_value[15], 1, &provisioning_handle_auth_value_input_oob, NULL);
            provisioning_emit_event(MESH_SUBEVENT_PB_PROV_START_EMIT_INPUT_OOB, 1);
            break;
        default:
            break;
    }
}

static void provisioning_handle_public_key_dhkey(void * arg){
    UNUSED(arg);

    printf("DHKEY: ");
    printf_hexdump(dhkey, sizeof(dhkey));

#if 0
    // skip sending own public key when public key oob is used
    if (prov_public_key_oob_available && prov_public_key_oob_used){
        // just copy key for confirmation inputs
        memcpy(&prov_confirmation_inputs[81], prov_ec_q, 64);
    } else {
        // queue public key pdu
        provisioning_queue_pdu(MESH_PROV_PUB_KEY);
    }
#endif

    provisioning_public_key_exchange_complete();
}

static void provisioning_public_key_ready(void){
    // calculate DHKey
    btstack_crypto_ecc_p256_calculate_dhkey(&prov_ecc_p256_request, remote_ec_q, dhkey, provisioning_handle_public_key_dhkey, NULL);
}

static void provisioning_handle_public_key(uint16_t the_pb_adv_cid, const uint8_t *packet_data, uint16_t packet_len){
    // validate public key
    if (packet_len != sizeof(remote_ec_q) || btstack_crypto_ecc_p256_validate_public_key(packet_data) != 0){
        printf("Public Key invalid, abort provisioning\n");
        
        // disconnect provisioning link 
        pb_adv_close_link(the_pb_adv_cid, 0x02);    // reason: fail
        provisioning_timer_stop();
        return;
    }

#if 0
    // stop emit public OOK if specified and send to crypto module
    if (prov_public_key_oob_available && prov_public_key_oob_used){
        provisioning_emit_event(MESH_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB, 1);

        printf("Replace generated ECC with Public Key OOB:");
        memcpy(prov_ec_q, prov_public_key_oob_q, 64);
        printf_hexdump(prov_ec_q, sizeof(prov_ec_q));
        btstack_crypto_ecc_p256_set_key(prov_public_key_oob_q, prov_public_key_oob_d);
    }
#endif

    // store for confirmation inputs: len 64
    (void)memcpy(&prov_confirmation_inputs[81], packet_data, 64);

    // store remote q
    (void)memcpy(remote_ec_q, packet_data, sizeof(remote_ec_q));

    provisioning_public_key_ready();
}

static void provisioning_handle_confirmation(uint16_t the_pb_adv_cid, const uint8_t *packet_data, uint16_t packet_len){

    UNUSED(the_pb_adv_cid);
    UNUSED(packet_data);
    UNUSED(packet_len);

    // 
    if (prov_emit_output_oob_active){
        prov_emit_output_oob_active = 0;
        provisioning_emit_event(MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB, 1);
    }

#if 0
    // CalculationInputs
    printf("ConfirmationInputs: ");
    printf_hexdump(prov_confirmation_inputs, sizeof(prov_confirmation_inputs));

    // calculate s1
    btstack_crypto_aes128_cmac_zero(&prov_cmac_request, sizeof(prov_confirmation_inputs), prov_confirmation_inputs, confirmation_salt, &provisioning_handle_confirmation_s1_calculated, NULL);
#endif
    provisioner_state = PROVISIONER_SEND_RANDOM;
}

static void provisioning_handle_data_encrypted(void * arg){
    UNUSED(arg);

    // enc_provisioning_data
    printf("EncProvisioningData:   ");
    printf_hexdump(enc_provisioning_data, sizeof(enc_provisioning_data));

    btstack_crypto_ccm_get_authentication_value(&prov_ccm_request, provisioning_data_mic);
    printf("MIC:   ");
    printf_hexdump(provisioning_data_mic, sizeof(provisioning_data_mic));

    // send
    provisioner_state = PROVISIONER_SEND_DATA;
    provisioning_run();
}

static void provisioning_handle_session_nonce_calculated(void * arg){
    UNUSED(arg);

    // The nonce shall be the 13 least significant octets == zero most significant octets
    uint8_t temp[13];
    (void)memcpy(temp, &session_nonce[3], 13);
    (void)memcpy(session_nonce, temp, 13);

    // SessionNonce
    printf("SessionNonce:   ");
    printf_hexdump(session_nonce, 13);

    // setup provisioning data
    (void)memcpy(&provisioning_data[0], net_key, 16);
    big_endian_store_16(provisioning_data, 16, net_key_index);
    provisioning_data[18] = flags;
    big_endian_store_32(provisioning_data, 19, iv_index);
    big_endian_store_16(provisioning_data, 23, unicast_address);

    btstack_crypto_ccm_init(&prov_ccm_request, session_key, session_nonce, 25, 0, 8);
    btstack_crypto_ccm_encrypt_block(&prov_ccm_request, 25, provisioning_data, enc_provisioning_data, &provisioning_handle_data_encrypted, NULL);
}

static void provisioning_handle_session_key_calculated(void * arg){
    UNUSED(arg);

    // SessionKey
    printf("SessionKey:   ");
    printf_hexdump(session_key, sizeof(session_key));

    // SessionNonce
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), provisioning_salt, (const uint8_t*) "prsn", 4, session_nonce, &provisioning_handle_session_nonce_calculated, NULL);
}


static void provisioning_handle_provisioning_salt_calculated(void * arg){
    UNUSED(arg);
    
    // ProvisioningSalt
    printf("ProvisioningSalt:   ");
    printf_hexdump(provisioning_salt, sizeof(provisioning_salt));

    // SessionKey
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), provisioning_salt, (const uint8_t*) "prsk", 4, session_key, &provisioning_handle_session_key_calculated, NULL);
}

static void provisioning_handle_random(uint16_t the_pb_adv_cid, const uint8_t *packet_data, uint16_t packet_len){

    UNUSED(the_pb_adv_cid);
    UNUSED(packet_len);

    // TODO: validate Confirmation

    // calc ProvisioningSalt = s1(ConfirmationSalt || RandomProvisioner || RandomDevice)
    (void)memcpy(&prov_confirmation_inputs[0], confirmation_salt, 16);
    (void)memcpy(&prov_confirmation_inputs[16], random_provisioner, 16);
    (void)memcpy(&prov_confirmation_inputs[32], packet_data, 16);
    btstack_crypto_aes128_cmac_zero(&prov_cmac_request, 48, prov_confirmation_inputs, provisioning_salt, &provisioning_handle_provisioning_salt_calculated, NULL);
}

static void provisioning_handle_complete(uint16_t the_pb_adv_cid){
    UNUSED(the_pb_adv_cid);
}

static void provisioning_handle_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    if (size < 1) return;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) != HCI_EVENT_MESH_META)  break;
            
            switch (hci_event_mesh_meta_get_subevent_code(packet)){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                    if (provisioner_state != PROVISIONER_W4_LINK_OPENED) break;
                    switch (mesh_subevent_pb_transport_link_open_get_status(packet)) {
                        case ERROR_CODE_SUCCESS:
                            printf("Link opened, sending Invite\n");
                            provisioning_handle_link_opened(pb_adv_cid);
                            break;
                        default:
                            printf("Link open failed, abort\n");
                            provisioning_done();
                            break;
                    }
                    break;
                case MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT:
                    printf("Outgoing packet acked\n");
                    prov_waiting_for_outgoing_complete = 0;
                    break;                    
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED:
                    printf("Link close, reset state\n");
                    provisioning_done();
                    break;
                default:
                    break;
            }
            break;
        case PROVISIONING_DATA_PACKET:
            // check state
            switch (provisioner_state){
                case PROVISIONER_W4_CAPABILITIES:
                    if (packet[0] != MESH_PROV_CAPABILITIES) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_CAPABILITIES: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_capabilities(pb_adv_cid, &packet[1], size-1);
                    break;
                case PROVISIONER_W4_PUB_KEY:
                    if (packet[0] != MESH_PROV_PUB_KEY) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_PUB_KEY: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_public_key(pb_adv_cid, &packet[1], size-1);
                    break;
                case PROVISIONER_W4_INPUT_COMPLETE:
                    if (packet[0] != MESH_PROV_INPUT_COMPLETE) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_INPUT_COMPLETE: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_input_complete(pb_adv_cid);
                    break;
                case PROVISIONER_W4_CONFIRM:
                    if (packet[0] != MESH_PROV_CONFIRM) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_CONFIRM: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_confirmation(pb_adv_cid, &packet[1], size-1);
                    break;
                case PROVISIONER_W4_RANDOM:
                    if (packet[0] != MESH_PROV_RANDOM) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_RANDOM:  ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_random(pb_adv_cid, &packet[1], size-1);
                    break;
                case PROVISIONER_W4_COMPLETE:
                    if (packet[0] != MESH_PROV_COMPLETE) provisioning_handle_provisioning_error(0x03);
                    printf("MESH_PROV_COMPLETE:  ");
                    provisioning_handle_complete(pb_adv_cid);
                    break;
                default:
                    printf("TODO: handle provisioning state %x\n", provisioner_state);
                    break;
            }            
            break;
        default:
            break;
    }
    provisioning_run();
}

static void prov_key_generated(void * arg){
    UNUSED(arg);
    printf("ECC-P256: ");
    printf_hexdump(prov_ec_q, sizeof(prov_ec_q));
    // allow override
    if (prov_public_key_oob_available){
        printf("Replace generated ECC with Public Key OOB:");
        (void)memcpy(prov_ec_q, prov_public_key_oob_q, 64);
        printf_hexdump(prov_ec_q, sizeof(prov_ec_q));
        btstack_crypto_ecc_p256_set_key(prov_public_key_oob_q, prov_public_key_oob_d);
    }
}

void provisioning_provisioner_init(void){
    pb_adv_cid = MESH_PB_TRANSPORT_INVALID_CID;
    pb_adv_init();
    pb_adv_register_provisioner_packet_handler(&provisioning_handle_pdu);
}

void provisioning_provisioner_register_packet_handler(btstack_packet_handler_t packet_handler){
    prov_packet_handler = packet_handler;
}

uint16_t provisioning_provisioner_start_provisioning(const uint8_t * device_uuid){
    // generate new public key
    btstack_crypto_ecc_p256_generate_key(&prov_ecc_p256_request, prov_ec_q, &prov_key_generated, NULL);

    if (pb_adv_cid == MESH_PB_TRANSPORT_INVALID_CID) {
        provisioner_state = PROVISIONER_W4_LINK_OPENED;
        pb_adv_cid = pb_adv_create_link(device_uuid);
    }
    return pb_adv_cid;
}

void provisioning_provisioner_set_static_oob(uint16_t the_pb_adv_cid, uint16_t static_oob_len, const uint8_t * static_oob_data){
    UNUSED(the_pb_adv_cid);
    prov_static_oob_data = static_oob_data;
    prov_static_oob_len  = btstack_min(static_oob_len, 16);
}

uint8_t provisioning_provisioner_select_authentication_method(uint16_t the_pb_adv_cid, uint8_t algorithm, uint8_t public_key_used, uint8_t authentication_method, uint8_t authentication_action, uint8_t authentication_size){
    UNUSED(the_pb_adv_cid);

    if (provisioner_state != PROVISIONER_W4_AUTH_CONFIGURATION) return ERROR_CODE_COMMAND_DISALLOWED;

    prov_start_algorithm = algorithm;
    prov_start_public_key_used = public_key_used;
    prov_start_authentication_method = authentication_method;
    prov_start_authentication_action = authentication_action;
    prov_start_authentication_size   = authentication_size;
    provisioner_state = PROVISIONER_SEND_START;

    return ERROR_CODE_SUCCESS;
}

uint8_t provisioning_provisioner_public_key_oob_received(uint16_t the_pb_adv_cid, const uint8_t * public_key){
    UNUSED(the_pb_adv_cid);

    if (provisioner_state != PROVISIONER_W4_PUB_KEY_OOB) return ERROR_CODE_COMMAND_DISALLOWED;

    // store for confirmation inputs: len 64
    (void)memcpy(&prov_confirmation_inputs[81], public_key, 64);

    // store remote q
    (void)memcpy(remote_ec_q, public_key, sizeof(remote_ec_q));

    // continue procedure
    provisioner_state = PROVISIONER_SEND_PUB_KEY;
    provisioning_run();

    return ERROR_CODE_SUCCESS;
}

void provisioning_provisioner_input_oob_complete_numeric(uint16_t the_pb_adv_cid, uint32_t input_oob){
    UNUSED(the_pb_adv_cid);
    if (provisioner_state != PROVISIONER_W4_INPUT_OOK) return;

    // store input_oob as auth value
    big_endian_store_32(auth_value, 12, input_oob);
    provisioning_handle_auth_value_ready();
}

void provisioning_provisioner_input_oob_complete_alphanumeric(uint16_t the_pb_adv_cid, const uint8_t * input_oob_data, uint16_t input_oob_len){
    UNUSED(the_pb_adv_cid);
    if (provisioner_state != PROVISIONER_W4_INPUT_OOK) return;

    // store input_oob and fillup with zeros
    input_oob_len = btstack_min(input_oob_len, 16);
    memset(auth_value, 0, 16);
    (void)memcpy(auth_value, input_oob_data, input_oob_len);
    provisioning_handle_auth_value_ready();
}


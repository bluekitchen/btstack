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

#define BTSTACK_FILE__ "provisioning_device.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_memory.h"
#include "btstack_event.h"

#include "mesh/provisioning_device.h"
#include "mesh/mesh_crypto.h"
#ifdef ENABLE_MESH_ADV_BEARER
#include "mesh/pb_adv.h"
#endif
#ifdef ENABLE_MESH_GATT_BEARER
#include "mesh/pb_gatt.h"
#endif
#include "mesh/provisioning.h"

static void prov_key_generated(void * arg);

// remote ecc
static uint8_t remote_ec_q[64];
static uint8_t dhkey[32];

static btstack_packet_handler_t prov_packet_handler;

static uint8_t  prov_buffer_out[MESH_PROV_MAX_PROXY_PDU];
// ConfirmationInputs = ProvisioningInvitePDUValue || ProvisioningCapabilitiesPDUValue || ProvisioningStartPDUValue || PublicKeyProvisioner || PublicKeyDevice
static uint8_t  prov_confirmation_inputs[1 + 11 + 5 + 64 + 64];
static uint8_t  prov_authentication_method;
static uint8_t  prov_public_key_oob_used;
static uint8_t  prov_emit_public_key_oob_active;
static uint8_t  prov_emit_output_oob_active;
static uint8_t  prov_ec_q[64];

static const uint8_t * prov_public_key_oob_q;
static const uint8_t * prov_public_key_oob_d;

// num elements
static uint8_t  prov_num_elements = 1;

// capabilites
static const uint8_t * prov_static_oob_data;

static uint16_t  prov_static_oob_len;
static uint16_t  prov_output_oob_actions;
static uint16_t  prov_input_oob_actions;
static uint8_t   prov_public_key_oob_available;
static uint8_t   prov_static_oob_available;
static uint8_t   prov_output_oob_size;
static uint8_t   prov_input_oob_size;
static uint8_t   prov_error_code;
static uint8_t   prov_waiting_for_outgoing_complete;

static uint8_t                      prov_attention_timer_timeout;

static btstack_timer_source_t       prov_protocol_timer;

static btstack_crypto_aes128_cmac_t prov_cmac_request;
static btstack_crypto_random_t      prov_random_request;
static btstack_crypto_ecc_p256_t    prov_ecc_p256_request;
static btstack_crypto_ccm_t         prov_ccm_request;

// ConfirmationDevice
static uint8_t confirmation_device[16];
// ConfirmationSalt
static uint8_t confirmation_salt[16];
// ConfirmationKey
static uint8_t confirmation_key[16];
// RandomDevice
static uint8_t random_device[16];
// ProvisioningSalt
static uint8_t provisioning_salt[16];
// AuthValue
static uint8_t auth_value[16];
// SessionKey
static uint8_t session_key[16];
// SessionNonce
static uint8_t session_nonce[16];
// EncProvisioningData
static uint8_t enc_provisioning_data[25];
// ProvisioningData
static uint8_t provisioning_data[25];

// received network_key
static mesh_network_key_t * network_key;

// DeviceKey
static uint8_t device_key[16];

static uint8_t  flags;

static uint32_t iv_index;
static uint16_t unicast_address;

typedef enum {
    DEVICE_W4_INVITE,
    DEVICE_SEND_CAPABILITIES,
    DEVICE_W4_START,
    DEVICE_W4_INPUT_OOK,
    DEVICE_SEND_INPUT_COMPLETE,
    DEVICE_W4_PUB_KEY,
    DEVICE_SEND_PUB_KEY,
    DEVICE_PUB_KEY_SENT,
    DEVICE_W4_CONFIRM,
    DEVICE_SEND_CONFIRM,
    DEVICE_W4_RANDOM,
    DEVICE_SEND_RANDOM,
    DEVICE_W4_DATA,
    DEVICE_SEND_COMPLETE,
    DEVICE_SEND_ERROR,
    DEVICE_W4_DONE,
} device_state_t;

static device_state_t device_state;
static uint16_t pb_transport_cid;
static mesh_pb_type_t pb_type;

static void pb_send_pdu(uint16_t transport_cid, const uint8_t * buffer, uint16_t buffer_size){
    switch (pb_type){
#ifdef ENABLE_MESH_ADV_BEARER
        case MESH_PB_TYPE_ADV:
            pb_adv_send_pdu(transport_cid, buffer, buffer_size);    
            break;
#endif
#ifdef ENABLE_MESH_GATT_BEARER
        case MESH_PB_TYPE_GATT:
            pb_gatt_send_pdu(transport_cid, buffer, buffer_size);    
            break;
#endif
        default:
            break;
     }
}

static void pb_close_link(uint16_t transport_cid, uint8_t reason){
    switch (pb_type){
#ifdef ENABLE_MESH_ADV_BEARER
        case MESH_PB_TYPE_ADV:
            pb_adv_close_link(transport_cid, reason);    
            break;
#endif
#ifdef ENABLE_MESH_GATT_BEARER
        case MESH_PB_TYPE_GATT:
            pb_gatt_close_link(transport_cid, reason);    
            break;
#endif
        default:
            break;
    }
}

static void provisioning_emit_event(uint16_t pb_adv_cid, uint8_t mesh_subevent){
    if (!prov_packet_handler) return;
    uint8_t event[5] = { HCI_EVENT_MESH_META, 3, mesh_subevent};
    little_endian_store_16(event, 3, pb_adv_cid);
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisioning_emit_output_oob_event(uint16_t pb_adv_cid, uint32_t number){
    if (!prov_packet_handler) return;
    uint8_t event[9] = { HCI_EVENT_MESH_META, 7, MESH_SUBEVENT_PB_PROV_START_EMIT_OUTPUT_OOB};
    little_endian_store_16(event, 3, pb_adv_cid);
    little_endian_store_32(event, 5, number);
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisioning_emit_attention_timer_event(uint16_t pb_adv_cid, uint8_t timer_s){
    if (!prov_packet_handler) return;
    uint8_t event[6] = { HCI_EVENT_MESH_META, 4, MESH_SUBEVENT_PB_PROV_ATTENTION_TIMER};
    little_endian_store_16(event, 3, pb_adv_cid);
    event[5] = timer_s;
    prov_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void provisiong_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("Provisioning Protocol Timeout -> Close Link!\n");
    pb_close_link(1, 1);
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
static void provisioning_send_provisioning_error(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_FAILED;
    prov_buffer_out[1] = prov_error_code;
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 2);
}

static void provisioning_send_capabilites(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_CAPABILITIES;

    /* Number of Elements supported */
    prov_buffer_out[1] = prov_num_elements;

    /* Supported algorithms - FIPS P-256 Eliptic Curve */
    big_endian_store_16(prov_buffer_out, 2, 1);

    /* Public Key Type - Public Key OOB information available */
    prov_buffer_out[4] = prov_public_key_oob_available;

    /* Static OOB Type - Static OOB information available */
    prov_buffer_out[5] = prov_static_oob_available; 

    /* Output OOB Size - max of 8 */
    prov_buffer_out[6] = prov_output_oob_size; 

    /* Output OOB Action */
    big_endian_store_16(prov_buffer_out, 7, prov_output_oob_actions);

    /* Input OOB Size - max of 8*/
    prov_buffer_out[9] = prov_input_oob_size; 

    /* Input OOB Action */
    big_endian_store_16(prov_buffer_out, 10, prov_input_oob_actions);

    // store for confirmation inputs: len 11
    (void)memcpy(&prov_confirmation_inputs[1], &prov_buffer_out[1], 11);

    // send

    pb_send_pdu(pb_transport_cid, prov_buffer_out, 12);    
}

static void provisioning_send_public_key(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_PUB_KEY;
    (void)memcpy(&prov_buffer_out[1], prov_ec_q, 64);

    // store for confirmation inputs: len 64
    (void)memcpy(&prov_confirmation_inputs[81], &prov_buffer_out[1], 64);

    // send
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 65);
}

static void provisioning_send_input_complete(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_INPUT_COMPLETE;

    // send
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 17);
}
static void provisioning_send_confirm(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_CONFIRM;
    (void)memcpy(&prov_buffer_out[1], confirmation_device, 16);

    // send
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 17);
}

static void provisioning_send_random(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_RANDOM;
    (void)memcpy(&prov_buffer_out[1], random_device, 16);

    // send pdu
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 17);
}

static void provisioning_send_complete(void){
    // setup response 
    prov_buffer_out[0] = MESH_PROV_COMPLETE;

    // send pdu
    pb_send_pdu(pb_transport_cid, prov_buffer_out, 1);
}

static void provisioning_done(void){
    if (prov_emit_public_key_oob_active){
        prov_emit_public_key_oob_active = 0;
        provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB);
    }
    if (prov_emit_output_oob_active){
        prov_emit_output_oob_active = 0;
        provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB);
    }
    if (prov_attention_timer_timeout){
        prov_attention_timer_timeout = 0;
        provisioning_emit_attention_timer_event(1, 0);        
    }
    device_state = DEVICE_W4_INVITE;

    // generate new public key
    printf("Generate new public key\n");
    btstack_crypto_ecc_p256_generate_key(&prov_ecc_p256_request, prov_ec_q, &prov_key_generated, NULL);
}

static void provisioning_handle_auth_value_output_oob(void * arg){
    UNUSED(arg);
    // limit auth value to single digit
    auth_value[15] = auth_value[15] % 9 + 1;

    printf("Output OOB: %u\n", auth_value[15]);

    // emit output oob value
    provisioning_emit_output_oob_event(1, auth_value[15]);
    prov_emit_output_oob_active = 1;
}

static void provisioning_public_key_exchange_complete(void){

    // reset auth_value
    memset(auth_value, 0, sizeof(auth_value));

    // handle authentication method
    switch (prov_authentication_method){
        case 0x00:
            device_state = DEVICE_W4_CONFIRM;
            break;        
        case 0x01:
            (void)memcpy(&auth_value[16 - prov_static_oob_len],
                         prov_static_oob_data, prov_static_oob_len);
            device_state = DEVICE_W4_CONFIRM;
            break;
        case 0x02:
            device_state = DEVICE_W4_CONFIRM;
            printf("Generate random for auth_value\n");
            // generate single byte of random data to use for authentication
            btstack_crypto_random_generate(&prov_random_request, &auth_value[15], 1, &provisioning_handle_auth_value_output_oob, NULL);
            break;
        case 0x03:
            // Input OOB
            printf("Input OOB requested\n");
            provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_INPUT_OOB_REQUEST);
            device_state = DEVICE_W4_INPUT_OOK;
            break;
        default:
            break;
    }
}

static void provisioning_run(void){
    printf("provisioning_run: state %x, wait for outgoing complete %u\n", device_state, prov_waiting_for_outgoing_complete);
    if (prov_waiting_for_outgoing_complete) return;
    int start_timer = 1;
    switch (device_state){
        case DEVICE_SEND_ERROR:
            start_timer = 0;    // game over
            device_state = DEVICE_W4_DONE;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_provisioning_error();
            break;
        case DEVICE_SEND_CAPABILITIES:
            device_state = DEVICE_W4_START;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_capabilites();
            break;
        case DEVICE_SEND_INPUT_COMPLETE:
            device_state = DEVICE_W4_CONFIRM;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_input_complete();
            break;
        case DEVICE_SEND_PUB_KEY:
            device_state = DEVICE_PUB_KEY_SENT;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_public_key();
            provisioning_public_key_exchange_complete();
            break;
        case DEVICE_SEND_CONFIRM:
            device_state = DEVICE_W4_RANDOM;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_confirm();
            break;
        case DEVICE_SEND_RANDOM:
            device_state = DEVICE_W4_DATA;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_random();
            break;
        case DEVICE_SEND_COMPLETE:
            start_timer = 0;    // last message
            device_state = DEVICE_W4_DONE;
            prov_waiting_for_outgoing_complete = 1;
            provisioning_send_complete();
            break;
        default:
            return;
    }
    if (start_timer){
        provisioning_timer_start();
    }
}

static void provisioning_handle_provisioning_error(uint8_t error_code){
    printf("PROVISIONING ERROR\n");
    provisioning_timer_stop();
    prov_error_code = error_code;
    device_state = DEVICE_SEND_ERROR;
    provisioning_run();
}

static void provisioning_handle_invite(uint8_t *packet, uint16_t size){

    if (size != 1) return;

    // store for confirmation inputs: len 1
    (void)memcpy(&prov_confirmation_inputs[0], packet, 1);

    // handle invite message
    prov_attention_timer_timeout = packet[0];
    if (prov_attention_timer_timeout){
        provisioning_emit_attention_timer_event(pb_transport_cid, prov_attention_timer_timeout);
    }

    device_state = DEVICE_SEND_CAPABILITIES;
    provisioning_run();
}

static void provisioning_handle_start(uint8_t * packet, uint16_t size){

    if (size != 5) return;

    // validate Algorithm
    int ok = 1;
    if (packet[0] > 0x00){
        ok = 0;
    }
    // validate Publik Key
    if (packet[1] > 0x01){
        ok = 0;
    }
    // validate Authentication Method
    switch (packet[2]){
        case 0:
        case 1:
            if (packet[3] != 0 || packet[4] != 0){
                ok = 0;
                break;
            }
            break;
        case 2:
            if (packet[3] > 0x04 || packet[4] == 0 || packet[4] > 0x08){
                ok = 0;
                break;
            }
            break;
        case 3:
            if (packet[3] > 0x03 || packet[4] == 0 || packet[4] > 0x08){
                ok = 0;
                break;
            }
            break;
        default:
            // TODO check
            break;
    }
    if (!ok){
        printf("PROV_START arguments incorrect\n");
        provisioning_handle_provisioning_error(0x02);
        return;
    }

    // store for confirmation inputs: len 5
    (void)memcpy(&prov_confirmation_inputs[12], packet, 5);

    // public key oob
    prov_public_key_oob_used = packet[1];

    // authentication method
    prov_authentication_method = packet[2];

    // start emit public OOK if specified
    if (prov_public_key_oob_available && prov_public_key_oob_used){
        provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_START_EMIT_PUBLIC_KEY_OOB);
    }

    printf("PublicKey:  %02x\n", prov_public_key_oob_used);
    printf("AuthMethod: %02x\n", prov_authentication_method);

    device_state = DEVICE_W4_PUB_KEY;
    provisioning_run();
}

static void provisioning_handle_public_key_dhkey(void * arg){
    UNUSED(arg);

    printf("DHKEY: ");
    printf_hexdump(dhkey, sizeof(dhkey));

    // skip sending own public key when public key oob is used
    if (prov_public_key_oob_available && prov_public_key_oob_used){
        // just copy key for confirmation inputs
        (void)memcpy(&prov_confirmation_inputs[81], prov_ec_q, 64);
        provisioning_public_key_exchange_complete();
    } else {
        // queue public key pdu
        printf("DEVICE_SEND_PUB_KEY\n");
        device_state = DEVICE_SEND_PUB_KEY;
    }
    provisioning_run();
}

static void provisioning_handle_public_key(uint8_t *packet, uint16_t size){

    // validate public key
    if (size != sizeof(remote_ec_q) || btstack_crypto_ecc_p256_validate_public_key(packet) != 0){
        printf("Public Key invalid, abort provisioning\n");
        provisioning_handle_provisioning_error(0x07);   // Unexpected Error
        return;
    }

    // stop emit public OOK if specified and send to crypto module
    if (prov_public_key_oob_available && prov_public_key_oob_used){
        provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_STOP_EMIT_PUBLIC_KEY_OOB);

        printf("Replace generated ECC with Public Key OOB:");
        (void)memcpy(prov_ec_q, prov_public_key_oob_q, 64);
        printf_hexdump(prov_ec_q, sizeof(prov_ec_q));
        btstack_crypto_ecc_p256_set_key(prov_public_key_oob_q, prov_public_key_oob_d);
    }

    // store for confirmation inputs: len 64
    (void)memcpy(&prov_confirmation_inputs[17], packet, 64);

    // store remote q
    (void)memcpy(remote_ec_q, packet, sizeof(remote_ec_q));

    // calculate DHKey
    btstack_crypto_ecc_p256_calculate_dhkey(&prov_ecc_p256_request, remote_ec_q, dhkey, provisioning_handle_public_key_dhkey, NULL);
}

static void provisioning_handle_confirmation_device_calculated(void * arg){
    UNUSED(arg);

    printf("ConfirmationDevice: ");
    printf_hexdump(confirmation_device, sizeof(confirmation_device));

    device_state = DEVICE_SEND_CONFIRM;
    provisioning_run();
}

static void provisioning_handle_confirmation_random_device(void * arg){
    UNUSED(arg);

    // re-use prov_confirmation_inputs buffer
    (void)memcpy(&prov_confirmation_inputs[0], random_device, 16);
    (void)memcpy(&prov_confirmation_inputs[16], auth_value, 16);

    // calc confirmation device
    btstack_crypto_aes128_cmac_message(&prov_cmac_request, confirmation_key, 32, prov_confirmation_inputs, confirmation_device, &provisioning_handle_confirmation_device_calculated, NULL);
}

static void provisioning_handle_confirmation_k1_calculated(void * arg){
    UNUSED(arg);

    printf("ConfirmationKey:   ");
    printf_hexdump(confirmation_key, sizeof(confirmation_key));

    printf("AuthValue: ");
    printf_hexdump(auth_value, 16);

    // generate random_device
    btstack_crypto_random_generate(&prov_random_request,random_device, 16, &provisioning_handle_confirmation_random_device, NULL);
}

static void provisioning_handle_confirmation_s1_calculated(void * arg){
    UNUSED(arg);

    // ConfirmationSalt
    printf("ConfirmationSalt:   ");
    printf_hexdump(confirmation_salt, sizeof(confirmation_salt));

    // ConfirmationKey
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), confirmation_salt, (const uint8_t*) "prck", 4, confirmation_key, &provisioning_handle_confirmation_k1_calculated, NULL);
}

static void provisioning_handle_confirmation(uint8_t *packet, uint16_t size){
    UNUSED(size);
    UNUSED(packet);

    // 
    if (prov_emit_output_oob_active){
        prov_emit_output_oob_active = 0;
        provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_STOP_EMIT_OUTPUT_OOB);
    }

    // CalculationInputs
    printf("ConfirmationInputs: ");
    printf_hexdump(prov_confirmation_inputs, sizeof(prov_confirmation_inputs));

    // calculate s1
    btstack_crypto_aes128_cmac_zero(&prov_cmac_request, sizeof(prov_confirmation_inputs), prov_confirmation_inputs, confirmation_salt, &provisioning_handle_confirmation_s1_calculated, NULL);
}

// PROV_RANDOM
static void provisioning_handle_random_session_nonce_calculated(void * arg){
    UNUSED(arg);

    // The nonce shall be the 13 least significant octets == zero most significant octets
    uint8_t temp[13];
    (void)memcpy(temp, &session_nonce[3], 13);
    (void)memcpy(session_nonce, temp, 13);

    // SessionNonce
    printf("SessionNonce:   ");
    printf_hexdump(session_nonce, 13);

    device_state = DEVICE_SEND_RANDOM;
    provisioning_run();
}

static void provisioning_handle_random_session_key_calculated(void * arg){
    UNUSED(arg);

    // SessionKey
    printf("SessionKey:   ");
    printf_hexdump(session_key, sizeof(session_key));

    // SessionNonce
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), provisioning_salt, (const uint8_t*) "prsn", 4, session_nonce, &provisioning_handle_random_session_nonce_calculated, NULL);
}

static void provisioning_handle_random_s1_calculated(void * arg){

    UNUSED(arg);
    
    // ProvisioningSalt
    printf("ProvisioningSalt:   ");
    printf_hexdump(provisioning_salt, sizeof(provisioning_salt));

    // SessionKey
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), provisioning_salt, (const uint8_t*) "prsk", 4, session_key, &provisioning_handle_random_session_key_calculated, NULL);
}

static void provisioning_handle_random(uint8_t *packet, uint16_t size){

    UNUSED(size);
    UNUSED(packet);

    // TODO: validate Confirmation

    // calc ProvisioningSalt = s1(ConfirmationSalt || RandomProvisioner || RandomDevice)
    (void)memcpy(&prov_confirmation_inputs[0], confirmation_salt, 16);
    (void)memcpy(&prov_confirmation_inputs[16], packet, 16);
    (void)memcpy(&prov_confirmation_inputs[32], random_device, 16);
    btstack_crypto_aes128_cmac_zero(&prov_cmac_request, 48, prov_confirmation_inputs, provisioning_salt, &provisioning_handle_random_s1_calculated, NULL);
}

// PROV_DATA
static void provisioning_handle_network_dervived(void * arg){
    UNUSED(arg);

    provisioning_timer_stop();

    // notify client
    provisioning_emit_event(1, MESH_SUBEVENT_PB_PROV_COMPLETE);

    device_state = DEVICE_SEND_COMPLETE;
    provisioning_run();

}

static void provisioning_handle_data_device_key(void * arg){
    UNUSED(arg);

    // derive full network key
    mesh_network_key_derive(&prov_cmac_request, network_key, &provisioning_handle_network_dervived, NULL);
}

static void provisioning_handle_data_ccm(void * arg){

    UNUSED(arg);

    // TODO: validate MIC?
    uint8_t mic[8];
    btstack_crypto_ccm_get_authentication_value(&prov_ccm_request, mic);
    printf("MIC: ");
    printf_hexdump(mic, 8);

    // allocate network key
    network_key = btstack_memory_mesh_network_key_get();

    // sort provisoning data
    (void)memcpy(network_key->net_key, provisioning_data, 16);
    network_key->netkey_index = big_endian_read_16(provisioning_data, 16);
    // assume free index available for very first network key
    network_key->internal_index = mesh_network_key_get_free_index();
    flags = provisioning_data[18];
    iv_index = big_endian_read_32(provisioning_data, 19);
    unicast_address = big_endian_read_16(provisioning_data, 23);

    // DeviceKey
    mesh_k1(&prov_cmac_request, dhkey, sizeof(dhkey), provisioning_salt, (const uint8_t*) "prdk", 4, device_key, &provisioning_handle_data_device_key, NULL);
}

static void provisioning_handle_data(uint8_t *packet, uint16_t size){

    UNUSED(size);

    (void)memcpy(enc_provisioning_data, packet, 25);

    // decode response
    btstack_crypto_ccm_init(&prov_ccm_request, session_key, session_nonce, 25, 0, 8);
    btstack_crypto_ccm_decrypt_block(&prov_ccm_request, 25, enc_provisioning_data, provisioning_data, &provisioning_handle_data_ccm, NULL);
}

static void provisioning_handle_unexpected_pdu(uint8_t *packet, uint16_t size){
    UNUSED(size);
    printf("Unexpected PDU #%u in state #%u\n", packet[0], (int) device_state);
    provisioning_handle_provisioning_error(0x03);    
}

static void provisioning_handle_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    if (size < 1) return;

    switch (packet_type){
        case HCI_EVENT_PACKET:
        
            if (hci_event_packet_get_type(packet) != HCI_EVENT_MESH_META)  break;

            switch (hci_event_mesh_meta_get_subevent_code(packet)){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                    pb_transport_cid = mesh_subevent_pb_transport_link_open_get_pb_transport_cid(packet);
                    pb_type = mesh_subevent_pb_transport_link_open_get_pb_type(packet);
                    printf("Link opened, reset state, transport cid 0x%02x, PB type %d\n", pb_transport_cid, pb_type);
                    provisioning_done();
                    break;
                case MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT:
                    printf("Outgoing packet acked\n");
                    prov_waiting_for_outgoing_complete = 0;
                    if (device_state == DEVICE_W4_DONE){
                        pb_close_link(1, 0);
                        provisioning_done();
                    }
                    break;                    
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED:
                    printf("Link close, reset state\n");
                    pb_transport_cid = MESH_PB_TRANSPORT_INVALID_CID;
                    provisioning_done();
                    break;
                default:
                    break;
            }
            break;
        case PROVISIONING_DATA_PACKET:
            // check state
            switch (device_state){
                case DEVICE_W4_INVITE:
                    if (packet[0] != MESH_PROV_INVITE) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_INVITE: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_invite(&packet[1], size-1);
                    break;
                case DEVICE_W4_START:
                    if (packet[0] != MESH_PROV_START) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_START:  ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_start(&packet[1], size-1);
                    break;
                case DEVICE_W4_PUB_KEY:
                    if (packet[0] != MESH_PROV_PUB_KEY) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_PUB_KEY: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_public_key(&packet[1], size-1);
                    break;
                case DEVICE_W4_CONFIRM:
                    if (packet[0] != MESH_PROV_CONFIRM) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_CONFIRM: ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_confirmation(&packet[1], size-1);
                    break;
                case DEVICE_W4_RANDOM:
                    if (packet[0] != MESH_PROV_RANDOM) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_RANDOM:  ");
                    printf_hexdump(&packet[1], size-1);
                    provisioning_handle_random(&packet[1], size-1);
                    break;
                case DEVICE_W4_DATA:
                    if (packet[0] != MESH_PROV_DATA) provisioning_handle_unexpected_pdu(packet, size);
                    printf("MESH_PROV_DATA:  ");
                    provisioning_handle_data(&packet[1], size-1);
                    break;
                default:
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

void provisioning_device_init(void){
#ifdef ENABLE_MESH_ADV_BEARER
    // setup PB ADV
    pb_adv_init();
    pb_adv_register_device_packet_handler(&provisioning_handle_pdu);
#endif
#ifdef ENABLE_MESH_GATT_BEARER
    // setup PB GATT
    pb_gatt_init();
    pb_gatt_register_packet_handler(&provisioning_handle_pdu);
#endif
    
    pb_transport_cid = MESH_PB_TRANSPORT_INVALID_CID;
    
    // init provisioning state and generate ecc key
    provisioning_done();
}

void provisioning_device_register_packet_handler(btstack_packet_handler_t packet_handler){
    prov_packet_handler = packet_handler;
}

void provisioning_device_set_public_key_oob(const uint8_t * public_key, const uint8_t * private_key){
    prov_public_key_oob_q = public_key;
    prov_public_key_oob_d = private_key;
    prov_public_key_oob_available = 1;
    btstack_crypto_ecc_p256_set_key(prov_public_key_oob_q, prov_public_key_oob_d);
}

void provisioning_device_set_static_oob(uint16_t static_oob_len, const uint8_t * static_oob_data){
    prov_static_oob_available = 1;
    prov_static_oob_data = static_oob_data;
    prov_static_oob_len  = btstack_min(static_oob_len, 16);
}

void provisioning_device_set_output_oob_actions(uint16_t supported_output_oob_action_types, uint8_t max_oob_output_size){
    prov_output_oob_actions = supported_output_oob_action_types;
    prov_output_oob_size    = max_oob_output_size;
}

void provisioning_device_set_input_oob_actions(uint16_t supported_input_oob_action_types, uint8_t max_oob_input_size){
    prov_input_oob_actions = supported_input_oob_action_types;
    prov_input_oob_size    = max_oob_input_size;
}

void provisioning_device_input_oob_complete_numeric(uint16_t pb_adv_cid, uint32_t input_oob){
    UNUSED(pb_adv_cid);
    if (device_state != DEVICE_W4_INPUT_OOK) return;

    // store input_oob as auth value
    big_endian_store_32(auth_value, 12, input_oob);
    device_state = DEVICE_SEND_INPUT_COMPLETE;
    provisioning_run();
}

void provisioning_device_input_oob_complete_alphanumeric(uint16_t pb_adv_cid, const uint8_t * input_oob_data, uint16_t input_oob_len){
    UNUSED(pb_adv_cid);
    if (device_state != DEVICE_W4_INPUT_OOK) return;

    // store input_oob and fillup with zeros
    input_oob_len = btstack_min(input_oob_len, 16);
    memset(auth_value, 0, 16);
    (void)memcpy(auth_value, input_oob_data, input_oob_len);
    device_state = DEVICE_SEND_INPUT_COMPLETE;
    provisioning_run();
}

void provisioning_device_data_get(mesh_provisioning_data_t * the_provisioning_data){
    the_provisioning_data->unicast_address = unicast_address;
    the_provisioning_data->iv_index = iv_index;
    the_provisioning_data->flags = flags;
    (void)memcpy(the_provisioning_data->device_key, device_key, 16);
    the_provisioning_data->network_key = network_key;
}

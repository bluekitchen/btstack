/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// att device demo
//
//*****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// ..

// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "att.h"

#include "rijndael.h"

// Bluetooth Spec definitions
typedef enum {
    SM_CODE_PAIRING_REQUEST = 0X01,
    SM_CODE_PAIRING_RESPONSE,
    SM_CODE_PAIRING_CONFIRM,
    SM_CODE_PAIRING_RANDOM,
    SM_CODE_PAIRING_FAILED,
    SM_CODE_ENCRYPTION_INFORMATION,
    SM_CODE_MASTER_IDENTIFICATION,
    SM_CODE_IDENTITY_INFORMATION,
    SM_CODE_IDENTITY_ADDRESS_INFORMATION,
    SM_CODE_SIGNING_INFORMATION,
    SM_CODE_SECURITY_REQUEST
} SECURITY_MANAGER_COMMANDS;

// Authentication requirement flags
#define SM_AUTHREQ_NO_BONDING 0x00
#define SM_AUTHREQ_BONDING 0x01
#define SM_AUTHREQ_MITM_PROTECTION 0x02

// Key distribution flags used by spec
#define SM_KEYDIST_ENC_KEY 0X01
#define SM_KEYDIST_ID_KEY  0x02
#define SM_KEYDIST_SIGN    0x04

// Key distribution flags used internally
#define SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION       0x01
#define SM_KEYDIST_FLAG_MASTER_IDENTIFICATION        0x02
#define SM_KEYDIST_FLAG_IDENTITY_INFORMATION         0x04
#define SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION 0x08
#define SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION       0x10

// STK Generation Methods
#define SM_STK_GENERATION_METHOD_JUST_WORKS 0x01
#define SM_STK_GENERATION_METHOD_OOB        0x02
#define SM_STK_GENERATION_METHOD_PASSKEY    0x04

// Pairing Failed Reasons
#define SM_REASON_RESERVED                     0x00
#define SM_REASON_PASSKEYT_ENTRY_FAILED        0x01
#define SM_REASON_OOB_NOT_AVAILABLE            0x02
#define SM_REASON_AUTHENTHICATION_REQUIREMENTS 0x03
#define SM_REASON_CONFIRM_VALUE_FAILED         0x04
#define SM_REASON_PAIRING_NOT_SUPPORTED        0x05
#define SM_REASON_ENCRYPTION_KEY_SIZE          0x06
#define SM_REASON_COMMAND_NOT_SUPPORTED        0x07
#define SM_REASON_UNSPECIFIED_REASON           0x08
#define SM_REASON_REPEATED_ATTEMPTS            0x09
// also, invalid parameters
// and reserved

// IO Capability Values
typedef enum {
    IO_CAPABILITY_DISPLAY_ONLY = 0,
    IO_CAPABILITY_DISPLAY_YES_NO,
    IO_CAPABILITY_KEYBOARD_ONLY,
    IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
    IO_CAPABILITY_KEYBOARD_DISPLAY, // not used by secure simple pairing
    IO_CAPABILITY_UNKNOWN = 0xff
} io_capability_t;



//
// types used by client
//

typedef struct sm_event {
    uint8_t   type;   // see <btstack/hci_cmds.h> SM_...
    uint8_t   addr_type;
    bd_addr_t address;
    uint32_t  passkey;  // only used for SM_PASSKEY_DISPLAY_NUMBER 
} sm_event_t;

//
// internal types and globals
//

typedef uint8_t key_t[16];

typedef enum {

    SM_STATE_IDLE,

    SM_STATE_SEND_SECURITY_REQUEST,
    SM_STATE_SEND_LTK_REQUESTED_NEGATIVE_REPLY,

    // Phase 1: Pairing Feature Exchange

    SM_STATE_PH1_SEND_PAIRING_RESPONSE,
    SM_STATE_PH1_W4_PAIRING_CONFIRM,
    SM_STATE_PH1_W4_USER_RESPONSE,

    SM_STATE_SEND_PAIRING_FAILED,
    SM_STATE_SEND_PAIRING_RANDOM,

    // Phase 2: Authenticating and Encrypting

    // get random number for TK if we show it 
    SM_STATE_PH2_GET_RANDOM_TK,
    SM_STATE_PH2_W4_RANDOM_TK,

    // calculate confirm values for local and remote connection
    SM_STATE_PH2_C1_GET_RANDOM_A,
    SM_STATE_PH2_C1_W4_RANDOM_A,
    SM_STATE_PH2_C1_GET_RANDOM_B,
    SM_STATE_PH2_C1_W4_RANDOM_B,
    SM_STATE_PH2_C1_GET_ENC_A,
    SM_STATE_PH2_C1_W4_ENC_A,
    SM_STATE_PH2_C1_GET_ENC_B,
    SM_STATE_PH2_C1_W4_ENC_B,
    SM_STATE_PH2_C1_SEND_PAIRING_CONFIRM,
    SM_STATE_PH2_W4_PAIRING_RANDOM,
    SM_STATE_PH2_C1_GET_ENC_C,
    SM_STATE_PH2_C1_W4_ENC_C,
    SM_STATE_PH2_C1_GET_ENC_D,
    SM_STATE_PH2_C1_W4_ENC_D,

    // calc STK
    SM_STATE_PH2_CALC_STK,
    SM_STATE_PH2_W4_STK,
    SM_STATE_PH2_SEND_STK,
    SM_STATE_PH2_W4_LTK_REQUEST,
    SM_STATE_PH2_W4_CONNECTION_ENCRYPTED,

    // Phase 3: Transport Specific Key Distribution
    
    // calculate DHK, Y, EDIV, and LTK
    SM_STATE_PH3_GET_RANDOM,
    SM_STATE_PH3_W4_RANDOM,
    SM_STATE_PH3_GET_DIV,
    SM_STATE_PH3_W4_DIV,
    SM_STATE_PH3_DHK_GET_ENC,
    SM_STATE_PH3_DHK_W4_ENC,
    SM_STATE_PH3_Y_GET_ENC,
    SM_STATE_PH3_Y_W4_ENC,
    SM_STATE_PH3_LTK_GET_ENC,
    SM_STATE_PH3_LTK_W4_ENC,
    SM_STATE_PH3_IRK_GET_ENC,
    SM_STATE_PH3_IRK_W4_ENC,

    //
    SM_STATE_DISTRIBUTE_KEYS,

    // re establish previously distribued LTK
    SM_STATE_PH4_DHK_GET_ENC,
    SM_STATE_PH4_DHK_W4_ENC,
    SM_STATE_PH4_Y_GET_ENC,
    SM_STATE_PH4_Y_W4_ENC,
    SM_STATE_PH4_LTK_GET_ENC,
    SM_STATE_PH4_LTK_W4_ENC,
    SM_STATE_PH4_SEND_LTK,

    SM_STATE_TIMEOUT, // no other security messages are exchanged

} security_manager_state_t;

typedef enum {
    JUST_WORKS,
    PK_RESP_INPUT,  // Initiator displays PK, initiator inputs PK 
    PK_INIT_INPUT,  // Responder displays PK, responder inputs PK
    OK_BOTH_INPUT,  // Only input on both, both input PK
    OOB             // OOB available on both sides
} stk_generation_method_t;

typedef enum {
    SM_USER_RESPONSE_IDLE,
    SM_USER_RESPONSE_PENDING,
    SM_USER_RESPONSE_CONFIRM,
    SM_USER_RESPONSE_PASSKEY,
    SM_USER_RESPONSE_DECLINE
} sm_user_response_t;

//
// GLOBAL DATA
//

// Security Manager Master Keys, please use sm_set_er(er) and sm_set_ir(ir) with your own 128 bit random values
static key_t sm_persistent_er;
static key_t sm_persistent_ir;

// derived from sm_persistent_ir
static key_t sm_persistent_dhk;
static key_t sm_persistent_irk;

// derived from sm_persistent_er
// ..

static uint8_t sm_accepted_stk_generation_methods;
static uint8_t sm_max_encrypted_key_size;
static uint8_t sm_min_encrypted_key_size;
static uint8_t sm_s_auth_req = 0;
static uint8_t sm_s_io_capabilities = IO_CAPABILITY_UNKNOWN;
static uint8_t sm_s_request_security = 0;

static uint8_t   sm_s_addr_type;
static bd_addr_t sm_s_address;

// PER INSTANCE DATA

static security_manager_state_t sm_state_responding = SM_STATE_IDLE;
static uint16_t sm_response_handle = 0;
static uint8_t  sm_pairing_failed_reason = 0;

// SM timeout
static timer_source_t sm_timeout;

// data to send to aes128 crypto engine, see sm_aes128_set_key and sm_aes128_set_plaintext
static key_t sm_aes128_key;
static key_t sm_aes128_plaintext;

// generation method and temporary key for STK - STK is stored in sm_s_ltk
static stk_generation_method_t sm_stk_generation_method;
static key_t sm_tk;

// user response
static uint8_t   sm_user_response;

// defines which keys will be send  after connection is encrypted
static int sm_key_distribution_send_set;
static int sm_key_distribution_received_set;

//
// Volume 3, Part H, Chapter 24
// "Security shall be initiated by the Security Manager in the device in the master role.
// The device in the slave role shall be the responding device."
// -> master := initiator, slave := responder
//

static uint8_t   sm_m_io_capabilities;
static uint8_t   sm_m_have_oob_data;
static uint8_t   sm_m_auth_req;
static uint8_t   sm_m_max_encryption_key_size;
static uint8_t   sm_m_key_distribution;
static uint8_t   sm_m_preq[7];
static key_t     sm_m_random;
static key_t     sm_m_confirm;

static key_t     sm_s_random;
static key_t     sm_s_confirm;
static uint8_t   sm_s_pres[7];

// key distribution, slave sends
static key_t     sm_s_ltk;
static uint16_t  sm_s_y;
static uint16_t  sm_s_div;
static uint16_t  sm_s_ediv;
static uint8_t   sm_s_rand[8];
static key_t     sm_s_csrk;

// key distribution, received from master
static key_t     sm_m_ltk;
static uint16_t  sm_m_ediv;
static uint8_t   sm_m_rand[8];
static uint8_t   sm_m_addr_type;
static bd_addr_t sm_m_address;
static key_t     sm_m_csrk;
static key_t     sm_m_irk;

// @returns 1 if oob data is available
// stores oob data in provided 16 byte buffer if not null
static int (*sm_get_oob_data)(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data) = NULL;

// horizontal: initiator capabilities
// vertial:    responder capabilities
static const stk_generation_method_t stk_generation_method[5][5] = {
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    OK_BOTH_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
    { JUST_WORKS,      JUST_WORKS,       JUST_WORKS,      JUST_WORKS,    JUST_WORKS    },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    PK_INIT_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
};

// ATT Server

static att_connection_t att_connection;
static uint16_t         att_addr_type;
static bd_addr_t        att_address;
static uint16_t         att_response_handle = 0;
static uint16_t         att_response_size   = 0;
static uint8_t          att_response_buffer[28];

// SECURITY MANAGER (SM) MATERIALIZES HERE
static inline void swapX(uint8_t *src, uint8_t *dst, int len){
    int i;
    for (i = 0; i < len; i++)
        dst[len - 1 - i] = src[i];
}
static inline void swap56(uint8_t src[7], uint8_t dst[7]){
    swapX(src, dst, 7);
}
static inline void swap64(uint8_t src[8], uint8_t dst[8]){
    swapX(src, dst, 8);
}
static inline void swap128(uint8_t src[16], uint8_t dst[16]){
    swapX(src, dst, 16);
}

// @returns 1 if all bytes are 0
static int sm_is_null_random(uint8_t random[8]){
    int i;
    for (i=0; i < 8 ; i++){
        if (random[i]) return 0;
    }
    return 1;
}

static void print_key(const char * name, key_t key){
    printf("%-6s ", name);
    hexdump(key, 16);
}

static void print_hex16(const char * name, uint16_t value){
    printf("%-6s 0x%04x\n", name, value);
}

// SMP Timeout implementation

// Upon transmission of the Pairing Request command or reception of the Pairing Request command,
// the Security Manager Timer shall be reset and started.
//
// The Security Manager Timer shall be reset when an L2CAP SMP command is queued for transmission.
//
// If the Security Manager Timer reaches 30 seconds, the procedure shall be considered to have failed,
// and the local higher layer shall be notified. No further SMP commands shall be sent over the L2CAP
// Security Manager Channel. A new SM procedure shall only be performed when a new physical link has been
// established.

static void sm_timeout_handler(timer_source_t * timer){
    printf("SM timeout");
    sm_state_responding = SM_STATE_TIMEOUT;
}
static void sm_timeout_start(){
    run_loop_set_timer_handler(&sm_timeout, sm_timeout_handler);
    run_loop_set_timer(&sm_timeout, 30000); // 30 seconds sm timeout
    run_loop_add_timer(&sm_timeout);
}
static void sm_timeout_stop(){
    run_loop_remove_timer(&sm_timeout);
}
static void sm_timeout_reset(){
    sm_timeout_stop();
    sm_timeout_start();    
}

// end of sm timeout

static inline void sm_aes128_set_key(key_t key){
    memcpy(sm_aes128_key, key, 16);
} 

static inline void sm_aes128_set_plaintext(key_t plaintext){
    memcpy(sm_aes128_plaintext, plaintext, 16);
} 

static void sm_d1_d_prime(uint16_t d, uint16_t r, key_t d1_prime){
    // d'= padding || r || d
    memset(d1_prime, 0, 16);
    net_store_16(d1_prime, 12, r);
    net_store_16(d1_prime, 14, d);
}

static void sm_d1(key_t k, uint16_t d, uint16_t r, key_t d1){
    key_t d1_prime;
    sm_d1_d_prime(d, r, d1_prime);
    // d1(k,d,r) = e(k, d'),
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, d1_prime, d1);
}

static void sm_dm_r_prime(uint8_t r[8], key_t r_prime){
    // r’ = padding || r
    memset(r_prime, 0, 16);
    memcpy(&r_prime[8], r, 8);
}


// calculate arguments for first AES128 operation in C1 function
static void sm_c1_t1(key_t r, uint8_t preq[7], uint8_t pres[7], uint8_t iat, uint8_t rat, key_t t1){

    // p1 = pres || preq || rat’ || iat’
    // "The octet of iat’ becomes the least significant octet of p1 and the most signifi-
    // cant octet of pres becomes the most significant octet of p1.
    // For example, if the 8-bit iat’ is 0x01, the 8-bit rat’ is 0x00, the 56-bit preq
    // is 0x07071000000101 and the 56 bit pres is 0x05000800000302 then
    // p1 is 0x05000800000302070710000001010001."
    
    key_t p1;
    swap56(pres, &p1[0]);
    swap56(preq, &p1[7]);
    p1[14] = rat;
    p1[15] = iat;
    print_key("p1", p1);
    print_key("r", r);
    
    // t1 = r xor p1
    int i;
    for (i=0;i<16;i++){
        t1[i] = r[i] ^ p1[i];
    }
    print_key("t1", t1);
}

// calculate arguments for second AES128 operation in C1 function
static void sm_c1_t3(key_t t2, bd_addr_t ia, bd_addr_t ra, key_t t3){
     // p2 = padding || ia || ra
    // "The least significant octet of ra becomes the least significant octet of p2 and
    // the most significant octet of padding becomes the most significant octet of p2.
    // For example, if 48-bit ia is 0xA1A2A3A4A5A6 and the 48-bit ra is
    // 0xB1B2B3B4B5B6 then p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6.
    
    key_t p2;
    memset(p2, 0, 16);
    memcpy(&p2[4],  ia, 6);
    memcpy(&p2[10], ra, 6);
    print_key("p2", p2);

    // c1 = e(k, t2_xor_p2)
    int i;
    for (i=0;i<16;i++){
        t3[i] = t2[i] ^ p2[i];
    }
    print_key("t3", t3);
}

static void sm_s1_r_prime(key_t r1, key_t r2, key_t r_prime){
    print_key("r1", r1);
    print_key("r2", r2);
    memcpy(&r_prime[8], &r2[8], 8);
    memcpy(&r_prime[0], &r1[8], 8);
}

static void sm_notify_client(uint8_t type, uint8_t addr_type, bd_addr_t address, uint32_t passkey){

    sm_event_t event;
    event.type = type;
    event.addr_type = addr_type;
    BD_ADDR_COPY(event.address, address);
    event.passkey = passkey;

    // dummy implementation
    printf("sm_notify_client: event 0x%02x, addres_type %u, address (), num '%06u'", event.type, event.addr_type, event.passkey);
}

void sm_reset_tk(){
    int i;
    for (i=0;i<16;i++){
        sm_tk[i] = 0;
    }
}

// decide on stk generation based on
// - pairing request
// - io capabilities
// - OOB data availability
static void sm_tk_setup(){

    // default: just works
    sm_stk_generation_method = JUST_WORKS;
    sm_reset_tk();

    // If both devices have out of band authentication data, then the Authentication
    // Requirements Flags shall be ignored when selecting the pairing method and the
    // Out of Band pairing method shall be used.
    if (sm_m_have_oob_data && (*sm_get_oob_data)(att_addr_type, &att_address, sm_tk)){
        sm_stk_generation_method = OOB;
        return;
    }

    // If both devices have not set the MITM option in the Authentication Requirements
    // Flags, then the IO capabilities shall be ignored and the Just Works association
    // model shall be used. 
    if ( ((sm_m_auth_req & 0x04) == 0x00) && ((sm_s_auth_req & 0x04) == 0)){
        return;
    }

    // Also use just works if unknown io capabilites
    if ((sm_m_io_capabilities > 4) || (sm_m_io_capabilities > 4)){
        return;
    }

    // Otherwise the IO capabilities of the devices shall be used to determine the
    // pairing method as defined in Table 2.4.
    sm_stk_generation_method = stk_generation_method[sm_m_io_capabilities][sm_s_io_capabilities];
}


static void sm_setup_key_distribution(uint8_t key_set){

    // TODO: handle initiator case here

    // distribute keys as requested by initiator
    sm_key_distribution_send_set = 0;
    sm_key_distribution_received_set = 0;
    
    if (key_set & SM_KEYDIST_ENC_KEY){
        sm_key_distribution_send_set |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
        sm_key_distribution_send_set |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;        
    }
    if (key_set & SM_KEYDIST_ID_KEY){
        sm_key_distribution_send_set |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
        sm_key_distribution_send_set |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;        
    }
    if (key_set & SM_KEYDIST_SIGN){
        sm_key_distribution_send_set |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
    }
}

static void sm_run(void){

    // assert that we can send either one
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;

    switch (sm_state_responding){

        case SM_STATE_SEND_SECURITY_REQUEST: {
            uint8_t buffer[2];
            buffer[0] = SM_CODE_SECURITY_REQUEST;
            buffer[1] = SM_AUTHREQ_BONDING;
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_state_responding = SM_STATE_IDLE;            
            return;
        }

        case SM_STATE_PH1_SEND_PAIRING_RESPONSE: {

            // TODO use locally defined max encryption key size

            uint8_t buffer[7];

            memcpy(buffer, sm_m_preq, 7);        
            buffer[0] = SM_CODE_PAIRING_RESPONSE;
            buffer[1] = sm_s_io_capabilities;
            buffer[2] = sm_stk_generation_method == OOB ? 1 : 0;
            buffer[3] = sm_s_auth_req;
            buffer[4] = 0x10;   // maxium encryption key size

            memcpy(sm_s_pres, buffer, 7);

            // for validate

            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_timeout_reset();

            // notify client for: JUST WORKS confirm, PASSKEY display or input
            sm_user_response = SM_USER_RESPONSE_IDLE;
            switch (sm_stk_generation_method){
                case PK_RESP_INPUT:
                    sm_user_response = SM_USER_RESPONSE_PENDING;
                    sm_notify_client(SM_PASSKEY_INPUT_NUMBER, sm_m_addr_type, sm_m_address, 0); 
                    break;
                case PK_INIT_INPUT:
                    sm_notify_client(SM_PASSKEY_DISPLAY_NUMBER, sm_m_addr_type, sm_m_address, READ_NET_32(sm_tk, 12)); 
                    break;
                case JUST_WORKS:
                    switch (sm_s_io_capabilities){
                        case IO_CAPABILITY_KEYBOARD_DISPLAY:
                        case IO_CAPABILITY_DISPLAY_YES_NO:
                            sm_user_response = SM_USER_RESPONSE_PENDING;
                            sm_notify_client(SM_JUST_WORKS_REQUEST, sm_m_addr_type, sm_m_address, READ_NET_32(sm_tk, 12));
                            break;
                        default:
                            // cannot ask user
                            break;  

                    }
                    break;
                default:
                    break;
            }

            sm_state_responding = SM_STATE_PH1_W4_PAIRING_CONFIRM;
            return;
        }

        case SM_STATE_SEND_LTK_REQUESTED_NEGATIVE_REPLY:
            hci_send_cmd(&hci_le_long_term_key_negative_reply, sm_response_handle);
            sm_state_responding = SM_STATE_IDLE;
            return;

        case SM_STATE_SEND_PAIRING_FAILED: {
            uint8_t buffer[2];
            buffer[0] = SM_CODE_PAIRING_FAILED;
            buffer[1] = sm_pairing_failed_reason;
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_timeout_stop();
            sm_state_responding = SM_STATE_IDLE;
            break;
        }

        case SM_STATE_SEND_PAIRING_RANDOM: {
            uint8_t buffer[17];
            buffer[0] = SM_CODE_PAIRING_RANDOM;
            swap128(sm_s_random, &buffer[1]);
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_timeout_reset();
            sm_state_responding = SM_STATE_PH2_W4_LTK_REQUEST;
            break;
        }

        case SM_STATE_PH2_GET_RANDOM_TK:
        case SM_STATE_PH2_C1_GET_RANDOM_A:
        case SM_STATE_PH2_C1_GET_RANDOM_B:
        case SM_STATE_PH3_GET_RANDOM:
        case SM_STATE_PH3_GET_DIV:
            hci_send_cmd(&hci_le_rand);
            sm_state_responding++;
            return;
        case SM_STATE_PH2_C1_GET_ENC_A:
        case SM_STATE_PH2_C1_GET_ENC_B:
        case SM_STATE_PH2_C1_GET_ENC_C:
        case SM_STATE_PH2_C1_GET_ENC_D:
        case SM_STATE_PH2_CALC_STK:
        case SM_STATE_PH3_DHK_GET_ENC:
        case SM_STATE_PH3_Y_GET_ENC:
        case SM_STATE_PH3_LTK_GET_ENC:
        case SM_STATE_PH3_IRK_GET_ENC:
        case SM_STATE_PH4_DHK_GET_ENC:
        case SM_STATE_PH4_Y_GET_ENC:
        case SM_STATE_PH4_LTK_GET_ENC:
            {
            key_t key_flipped, plaintext_flipped;
            swap128(sm_aes128_key, key_flipped);
            swap128(sm_aes128_plaintext, plaintext_flipped);
            hci_send_cmd(&hci_le_encrypt, key_flipped, plaintext_flipped);
            sm_state_responding++;
            }
            return;
        case SM_STATE_PH2_C1_SEND_PAIRING_CONFIRM: {
            uint8_t buffer[17];
            buffer[0] = SM_CODE_PAIRING_CONFIRM;
            swap128(sm_s_confirm, &buffer[1]);
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_timeout_reset();
            sm_state_responding = SM_STATE_PH2_W4_PAIRING_RANDOM;
            return;
        }
        case SM_STATE_PH2_SEND_STK: {
            key_t stk_flipped;
            swap128(sm_s_ltk, stk_flipped);
            hci_send_cmd(&hci_le_long_term_key_request_reply, sm_response_handle, stk_flipped);
            sm_state_responding = SM_STATE_PH2_W4_CONNECTION_ENCRYPTED;
            return;
        }
        case SM_STATE_PH4_SEND_LTK: {
            key_t ltk_flipped;
            swap128(sm_s_ltk, ltk_flipped);
            hci_send_cmd(&hci_le_long_term_key_request_reply, sm_response_handle, ltk_flipped);
            sm_state_responding = SM_STATE_IDLE;
            return;
        }

        case SM_STATE_DISTRIBUTE_KEYS:
            if (sm_key_distribution_send_set &   SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION){
                sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
                uint8_t buffer[17];
                buffer[0] = SM_CODE_ENCRYPTION_INFORMATION;
                swap128(sm_s_ltk, &buffer[1]);
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }
            if (sm_key_distribution_send_set &   SM_KEYDIST_FLAG_MASTER_IDENTIFICATION){
                sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                uint8_t buffer[11];
                buffer[0] = SM_CODE_MASTER_IDENTIFICATION;
                bt_store_16(buffer, 1, sm_s_ediv);
                swap64(sm_s_rand, &buffer[3]);
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }
            if (sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_INFORMATION){
                sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                uint8_t buffer[17];
                buffer[0] = SM_CODE_IDENTITY_INFORMATION;
                swap128(sm_persistent_irk, &buffer[1]);
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }
            if (sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION){
                sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                uint8_t buffer[8];
                buffer[0] = SM_CODE_IDENTITY_ADDRESS_INFORMATION;
                buffer[1] = sm_s_addr_type;
                bt_flip_addr(&buffer[2], sm_s_address);
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }
            if (sm_key_distribution_send_set &   SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION){
                sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
                uint8_t buffer[17];
                buffer[0] = SM_CODE_SIGNING_INFORMATION;
                swap128(sm_s_csrk, &buffer[1]);
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }
            sm_timeout_stop();
            sm_state_responding = SM_STATE_IDLE; 
            break;

        default:
            break;
    }
}

static void sm_pdu_received_in_wrong_state(){
    sm_pairing_failed_reason = SM_REASON_UNSPECIFIED_REASON;
    sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type != SM_DATA_PACKET) return;

    if (handle != sm_response_handle){
        printf("sm_packet_handler: packet from handle %u, but expecting from %u\n", handle, sm_response_handle);
        return;
    }

    // printf("sm_packet_handler, request %0x\n", packet[0]);

    // a sm timeout requries a new physical connection
    if (sm_state_responding == SM_STATE_TIMEOUT) return;

    switch (packet[0]){
        case SM_CODE_PAIRING_REQUEST: {

            if (sm_state_responding != SM_STATE_IDLE) {
                sm_pdu_received_in_wrong_state();
                break;;
            }

            // store key distribtion request
            sm_m_io_capabilities = packet[1];
            sm_m_have_oob_data = packet[2];
            sm_m_auth_req = packet[3];
            sm_m_max_encryption_key_size = packet[4];

            // setup key distribution
            sm_m_key_distribution = packet[5];
            sm_setup_key_distribution(packet[6]);

            // for validate
            memcpy(sm_m_preq, packet, 7);

            // start SM timeout
            sm_timeout_start();

            // decide on STK generation method
            sm_tk_setup();

            // check if STK generation method is acceptable by client
            int ok = 0;
            switch (sm_stk_generation_method){
                case JUST_WORKS:
                    ok = (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_JUST_WORKS) != 0;
                    break;
                case PK_RESP_INPUT:
                case PK_INIT_INPUT:
                case OK_BOTH_INPUT:
                    ok = (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_PASSKEY) != 0;
                    break;
                case OOB:
                    ok = (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_OOB) != 0;
                    break;
            }
            if (!ok){
                sm_pairing_failed_reason = SM_REASON_AUTHENTHICATION_REQUIREMENTS;
                sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
                break;
            }

            // generate random number first, if we need to show passkey
            if (sm_stk_generation_method == PK_INIT_INPUT){
                sm_state_responding = SM_STATE_PH2_GET_RANDOM_TK;
                break;
            }

            sm_state_responding = SM_STATE_PH1_SEND_PAIRING_RESPONSE;
            break;
        }

        case  SM_CODE_PAIRING_CONFIRM:

            if (sm_state_responding != SM_STATE_PH1_W4_PAIRING_CONFIRM) {
                sm_pdu_received_in_wrong_state();
                break;
            }

            // received confirm value
            swap128(&packet[1], sm_m_confirm);

            // notify client to hide shown passkey
            if (sm_stk_generation_method == PK_INIT_INPUT){
                sm_notify_client(SM_PASSKEY_DISPLAY_CANCEL, sm_m_addr_type, sm_m_address, 0);
            }

            // handle user cancel pairing?
            if (sm_user_response == SM_USER_RESPONSE_DECLINE){
                sm_pairing_failed_reason = SM_REASON_PASSKEYT_ENTRY_FAILED;
                sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
                break;
            }

            // wait for user action?
            if (sm_user_response == SM_USER_RESPONSE_PENDING){
                sm_state_responding = SM_STATE_PH1_W4_USER_RESPONSE;
                break;
            }

            // calculate and send s_confirm
            sm_state_responding = SM_STATE_PH2_C1_GET_RANDOM_A;
            break;


        case SM_CODE_PAIRING_RANDOM:

            if (sm_state_responding != SM_STATE_PH2_W4_PAIRING_RANDOM) {
                sm_pdu_received_in_wrong_state();
                break;
            }

            // received random value
            swap128(&packet[1], sm_m_random);

            // use aes128 engine
            // calculate m_confirm using aes128 engine - step 1
            sm_aes128_set_key(sm_tk);
            sm_c1_t1(sm_m_random, sm_m_preq, sm_s_pres, sm_m_addr_type, sm_s_addr_type, sm_aes128_plaintext);
            sm_state_responding = SM_STATE_PH2_C1_GET_ENC_C;
            break;

        case SM_CODE_ENCRYPTION_INFORMATION:
            if ((sm_state_responding != SM_STATE_DISTRIBUTE_KEYS) || ((sm_m_key_distribution & SM_KEYDIST_ENC_KEY) == 0)){
                sm_pdu_received_in_wrong_state();
                break;
            }
            sm_key_distribution_received_set |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
            swap128(&packet[1], sm_m_ltk);
            break;

        case SM_CODE_MASTER_IDENTIFICATION:
            if ((sm_state_responding != SM_STATE_DISTRIBUTE_KEYS) || ((sm_m_key_distribution & SM_KEYDIST_ENC_KEY) == 0)){
                sm_pdu_received_in_wrong_state();
                break;
            }
            sm_key_distribution_received_set |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
            sm_m_ediv = READ_BT_16(packet, 1);
            swap64(&packet[3], sm_m_rand);
            break;

        case SM_CODE_IDENTITY_INFORMATION:
            if ((sm_state_responding != SM_STATE_DISTRIBUTE_KEYS) || ((sm_m_key_distribution & SM_KEYDIST_ID_KEY) == 0)){
                sm_pdu_received_in_wrong_state();
                break;
            }
            sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
            swap128(&packet[1], sm_m_irk);
            break;

        case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
            if ((sm_state_responding != SM_STATE_DISTRIBUTE_KEYS) || ((sm_m_key_distribution & SM_KEYDIST_ID_KEY) == 0)){
                sm_pdu_received_in_wrong_state();
                break;
            }
            sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
            sm_m_addr_type = packet[1];
            BD_ADDR_COPY(sm_m_address, &packet[2]); 
            break;

        case SM_CODE_SIGNING_INFORMATION:
            if ((sm_state_responding != SM_STATE_DISTRIBUTE_KEYS) || ((sm_m_key_distribution & SM_KEYDIST_SIGN) == 0)){
                sm_pdu_received_in_wrong_state();
                break;
            }
            sm_key_distribution_received_set |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
            swap128(&packet[1], sm_m_csrk);
            break;
    }

    // try to send preparared packet
    sm_run();
}

void sm_set_er(key_t er){
    memcpy(sm_persistent_er, er, 16);
}

void sm_set_ir(key_t ir){
    memcpy(sm_persistent_ir, ir, 16);
    // sm_dhk(sm_persistent_ir, sm_persistent_dhk);
    // sm_irk(sm_persistent_ir, sm_persistent_irk);
}

void sm_init(){
    // set some (BTstack default) ER and IR
    int i;
    key_t er;
    key_t ir;
    for (i=0;i<16;i++){
        er[i] = 0x30 + i;
        ir[i] = 0x90 + i;
    }
    sm_set_er(er);
    sm_set_ir(ir);
    sm_state_responding = SM_STATE_IDLE;
    // defaults
    sm_accepted_stk_generation_methods = SM_STK_GENERATION_METHOD_JUST_WORKS
                                       | SM_STK_GENERATION_METHOD_OOB
                                       | SM_STK_GENERATION_METHOD_PASSKEY;
    sm_max_encrypted_key_size = 16;
    sm_min_encrypted_key_size = 7;
}

// END OF SM

// enable LE, setup ADV data
static void att_try_respond(void);
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 

    sm_run();

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (packet[2] == HCI_STATE_WORKING) {
                        printf("Working!\n");
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
					}
					break;
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_try_respond();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // only single connection for peripheral
                            if (sm_response_handle){
                                printf("Already connected, ignoring incoming connection\n");
                                return;
                            }

                            sm_response_handle = READ_BT_16(packet, 4);
                            sm_m_addr_type = packet[7];
                            bt_flip_addr(sm_m_address, &packet[8]);
                            sm_reset_tk();
                            // TODO support private addresses
                            sm_s_addr_type = 0;
                            BD_ADDR_COPY(sm_s_address, hci_local_bd_addr());
                            printf("Incoming connection, own address ");
                            print_bd_addr(sm_s_address);

                            // reset connection MTU
                            att_connection.mtu = 23;

                            // request security
                            if (sm_s_request_security){
                                sm_state_responding = SM_STATE_SEND_SECURITY_REQUEST;
                            }
                            break;

                        case HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST:
                            log_info("LTK Request: state %u", sm_state_responding);
                            if (sm_state_responding == SM_STATE_PH2_W4_LTK_REQUEST){
                                // calculate STK
                                log_info("LTK Request: calculating STK");
                                sm_aes128_set_key(sm_tk);
                                sm_s1_r_prime(sm_s_random, sm_m_random, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH2_CALC_STK;
                                break;
                            }

                            // re-establish previously used LTK using Rand and EDIV
                            swap64(&packet[5], sm_s_rand);
                            sm_s_ediv = READ_BT_16(packet, 13);

                            // assume that we don't have a LTK for ediv == 0 and random == null
                            if (sm_s_ediv == 0 && sm_is_null_random(sm_s_rand)){
                                printf("LTK Request: ediv & random are empty\n");
                                sm_state_responding = SM_STATE_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
                                break;
                            }

                            log_info("LTK Request: recalculating with ediv 0x%04x", sm_s_ediv);

                            // dhk = d1(IR, 1, 0) - enc
                            // y   = dm(dhk, rand) - enc
                            // div = y xor ediv
                            // ltk = d1(ER, div, 0) - enc

                            // DHK = d1(IR, 3, 0)
                            sm_aes128_set_key(sm_persistent_ir);
                            sm_d1_d_prime(3, 0, sm_aes128_plaintext);
                            sm_state_responding = SM_STATE_PH4_DHK_GET_ENC;

                            // sm_s_div = sm_div(sm_persistent_dhk, sm_s_rand, sm_s_ediv);
                            // sm_s_ltk(sm_persistent_er, sm_s_div, sm_s_ltk);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    log_info("Connection encrypted");
                    if (sm_state_responding == SM_STATE_PH2_W4_CONNECTION_ENCRYPTED) {
                        sm_state_responding = SM_STATE_PH3_GET_RANDOM;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // restart advertising if we have been connected before
                    // -> avoid sending advertise enable a second time before command complete was received 
                    if (att_response_handle) {
                        hci_send_cmd(&hci_le_set_advertise_enable, 1);
                    }

                    att_response_handle = 0;
                    att_response_size = 0;

                    sm_response_handle = 0;
                    sm_state_responding = SM_STATE_IDLE;
                    break;
                    
				case HCI_EVENT_COMMAND_COMPLETE:
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_parameters)){
                        // only needed for BLE Peripheral
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
                        break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
                        // only needed for BLE Peripheral
					   hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
                        // only needed for BLE Peripheral
					   hci_send_cmd(&hci_le_set_advertise_enable, 1);
					   break;
					}
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_encrypt)){
                        switch (sm_state_responding){
                            case SM_STATE_PH2_C1_W4_ENC_A:
                            case SM_STATE_PH2_C1_W4_ENC_C:
                                {
                                sm_aes128_set_key(sm_tk);
                                key_t t2;
                                swap128(&packet[6], t2);
                                sm_c1_t3(t2, sm_m_address, sm_s_address, sm_aes128_plaintext);
                                }
                                sm_state_responding++;
                                break;
                            case SM_STATE_PH2_C1_W4_ENC_B:
                                swap128(&packet[6], sm_s_confirm);
                                print_key("c1!", sm_s_confirm);
                                sm_state_responding++;
                                break;
                            case SM_STATE_PH2_C1_W4_ENC_D:
                                {
                                key_t m_confirm_test;
                                swap128(&packet[6], m_confirm_test);
                                print_key("c1!", m_confirm_test);
                                if (memcmp(sm_m_confirm, m_confirm_test, 16) == 0){
                                    // send s_random
                                    sm_state_responding = SM_STATE_SEND_PAIRING_RANDOM;
                                    break;
                                }
                                sm_pairing_failed_reason = SM_REASON_CONFIRM_VALUE_FAILED;
                                sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
                                }
                                break;
                            case SM_STATE_PH2_W4_STK:
                                swap128(&packet[6], sm_s_ltk);
                                print_key("stk", sm_s_ltk);
                                sm_state_responding = SM_STATE_PH2_SEND_STK;
                                break;
                            case SM_STATE_PH3_DHK_W4_ENC:
                            case SM_STATE_PH4_DHK_W4_ENC:
                                swap128(&packet[6], sm_persistent_dhk);
                                print_key("dhk", sm_persistent_dhk);
                                // PH3B2 - calculate Y from      - enc
                                // Y = dm(DHK, Rand)
                                sm_aes128_set_key(sm_persistent_dhk);
                                sm_dm_r_prime(sm_s_rand, sm_aes128_plaintext);
                                sm_state_responding++;
                                break;
                            case SM_STATE_PH3_Y_W4_ENC:{
                                key_t y128;
                                swap128(&packet[6], y128);
                                sm_s_y = READ_NET_16(y128, 14);
                                print_hex16("y", sm_s_y);
                                // PH3B3 - calculate EDIV
                                sm_s_ediv = sm_s_y ^ sm_s_div;
                                print_hex16("ediv", sm_s_ediv);
                                // PH3B4 - calculate LTK         - enc
                                // LTK = d1(ER, DIV, 0))
                                sm_aes128_set_key(sm_persistent_er);
                                sm_d1_d_prime(sm_s_div, 0, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH3_LTK_GET_ENC;
                                break;
                            }
                            case SM_STATE_PH4_Y_W4_ENC:{
                                key_t y128;
                                swap128(&packet[6], y128);
                                sm_s_y = READ_NET_16(y128, 14);
                                print_hex16("y", sm_s_y);
                                // PH3B3 - calculate DIV
                                sm_s_div = sm_s_y ^ sm_s_ediv;
                                print_hex16("ediv", sm_s_ediv);
                                // PH3B4 - calculate LTK         - enc
                                // LTK = d1(ER, DIV, 0))
                                sm_aes128_set_key(sm_persistent_er);
                                sm_d1_d_prime(sm_s_div, 0, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH4_LTK_GET_ENC;
                                break;
                            }
                            case SM_STATE_PH3_LTK_W4_ENC:
                                swap128(&packet[6], sm_s_ltk);
                                print_key("ltk", sm_s_ltk);
                                // IRK = d1(IR, 1, 0)
                                sm_aes128_set_key(sm_persistent_ir);
                                sm_d1_d_prime(1, 0, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH3_IRK_GET_ENC;
                                break;
                            case SM_STATE_PH3_IRK_W4_ENC:
                                swap128(&packet[6], sm_persistent_irk);
                                print_key("irk", sm_persistent_irk);
                                // distribute keys
                                sm_state_responding = SM_STATE_DISTRIBUTE_KEYS;
                                break;                                
                            case SM_STATE_PH4_LTK_W4_ENC:
                                swap128(&packet[6], sm_s_ltk);
                                print_key("ltk", sm_s_ltk);
                                sm_state_responding = SM_STATE_PH4_SEND_LTK;
                                break;                                
                            default:
                                break;
                        }
                    }
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_rand)){
                        switch (sm_state_responding){
                            case SM_STATE_PH2_W4_RANDOM_TK:
                            {
                                // map random to 0-999999 without speding much cycles on a modulus operation
                                uint32_t tk = * (uint32_t*) &packet[6]; // random endianess
                                tk = tk & 0xfffff;  // 1048575
                                if (tk >= 999999){
                                    tk = tk - 999999;
                                } 
                                sm_reset_tk();
                                net_store_32(sm_tk, 12, tk);
                                // continue with phase 1
                                sm_state_responding = SM_STATE_PH1_SEND_PAIRING_RESPONSE;
                                break;
                            }
                            case SM_STATE_PH2_C1_W4_RANDOM_A:

                                memcpy(&sm_s_random[0], &packet[6], 8); // random endinaness
                                sm_state_responding = SM_STATE_PH2_C1_GET_RANDOM_B;
                                break;
                            case SM_STATE_PH2_C1_W4_RANDOM_B:
                                memcpy(&sm_s_random[8], &packet[6], 8); // random endinaness

                                // calculate s_confirm manually
                                // sm_c1(sm_tk, sm_s_random, sm_m_preq, sm_s_pres, sm_m_addr_type, sm_s_addr_type, sm_m_address, sm_s_address, sm_s_confirm);

                                // calculate s_confirm using aes128 engine - step 1
                                sm_aes128_set_key(sm_tk);
                                sm_c1_t1(sm_s_random, sm_m_preq, sm_s_pres, sm_m_addr_type, sm_s_addr_type, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH2_C1_GET_ENC_A;
                                break;

                            case SM_STATE_PH3_W4_RANDOM:
                                swap64(&packet[6], sm_s_rand);
                                sm_state_responding = SM_STATE_PH3_GET_DIV;
                                break;
                            case SM_STATE_PH3_W4_DIV:
                                // use 16 bit from random value as div
                                sm_s_div = READ_NET_16(packet, 6);
                                print_hex16("div", sm_s_div);

                                // PLAN
                                // PH3B1 - calculate DHK from IR - enc
                                // PH3B2 - calculate Y from      - enc
                                // PH3B3 - calculate EDIV
                                // PH3B4 - calculate LTK         - enc

                                // DHK = d1(IR, 3, 0)
                                sm_aes128_set_key(sm_persistent_ir);
                                sm_d1_d_prime(3, 0, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH3_DHK_GET_ENC;

                                // // calculate EDIV and LTK
                                // sm_s_ediv = sm_ediv(sm_persistent_dhk, sm_s_rand, sm_s_div);
                                // sm_s_ltk(sm_persistent_er, sm_s_div, sm_s_ltk);
                                // print_key("ltk", sm_s_ltk);
                                // print_hex16("ediv", sm_s_ediv);
                                // // distribute keys
                                // sm_distribute_keys();
                                // // done
                                // sm_state_responding = SM_STATE_IDLE;
                                break;

                            default:
                                break;
                        }
                        break;
                    }
			}
	}

    sm_run();
}

// aes128 c implementation only code

static uint16_t sm_dm(key_t k, uint8_t r[8]){
    key_t r_prime;
    sm_dm_r_prime(r, r_prime);
    // dm(k, r) = e(k, r’) dm(k, r) = e(k, r’) 
    key_t dm128;
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, r_prime, dm128);

    uint16_t dm = READ_NET_16(dm128, 14);
    return dm;
}

static uint16_t sm_y(key_t dhk, uint8_t rand[8]){
    // Y = dm(DHK, Rand)
    return sm_dm(dhk, rand);
}

static uint16_t sm_ediv(key_t dhk, uint8_t rand[8], uint16_t div){
    // EDIV = Y xor DIV
    uint16_t y = sm_y(dhk, rand);
    uint16_t ediv = y ^ div;
    return ediv; 
}

static uint16_t sm_div(key_t dhk, uint8_t rand[8], uint16_t ediv){
    // DIV = Y xor EDIV
    uint16_t y = sm_y(dhk, rand);
    uint16_t div = y ^ ediv;
    return div;
}

static void sm_ltk(key_t er, uint16_t div, key_t ltk){
    // LTK = d1(ER, DIV, 0))
    sm_d1(er, div, 0, ltk);
}

static void sm_csrk(key_t er, uint16_t div, key_t csrk){
    // LTK = d1(ER, DIV, 0))
    sm_d1(er, div, 1, csrk);
}

static void sm_irk(key_t ir, key_t irk){
    // IRK = d1(IR, 1, 0)
    sm_d1(ir, 1, 0, irk);
}

static void sm_dhk(key_t ir, key_t dhk){
    // DHK = d1(IR, 3, 0)
    sm_d1(ir, 3, 0, dhk);
}

// 
// Endianess:
// - preq, pres as found in SM PDUs (little endian), we flip it here
// - everything else in big endian incl. result
static void sm_c1(key_t k, key_t r, uint8_t preq[7], uint8_t pres[7], uint8_t iat, uint8_t rat, bd_addr_t ia, bd_addr_t ra, key_t c1){

    printf("iat %u: ia ", iat);
    print_bd_addr(ia);
    printf("rat %u: ra ", rat);
    print_bd_addr(ra);

    print_key("k", k);

    // first operation
    key_t t1;
    sm_c1_t1(r, preq, pres, iat, rat, t1);
    
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    
    // t2 = e(k, r_xor_p1)
    key_t t2;
    rijndaelEncrypt(rk, nrounds, t1, t2);
    
    print_key("t2", t2);

    // second operation
    key_t t3;
    sm_c1_t3(t2, ia, ra, t3);    
    
    rijndaelEncrypt(rk, nrounds, t3, c1);
    
    print_key("c1", c1);
}

static void sm_s1(key_t k, key_t r1, key_t r2, key_t s1){
    printf("sm_s1\n");
    print_key("k", k);

    key_t r_prime;
    sm_s1_r_prime(r1, r2, r_prime);

    // setup aes decryption
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, r_prime, s1);
    print_key("s1", s1);

}

// test code using aes128 c implementation
static int sm_validate_m_confirm(void){
    printf("sm_validate_m_confirm\n");

    key_t c1;
    print_key("mc", sm_m_confirm);

    sm_c1(sm_tk, sm_m_random, sm_m_preq, sm_s_pres, sm_m_addr_type, sm_s_addr_type, sm_m_address, sm_s_address, c1);

    int m_confirm_valid = memcmp(c1, sm_m_confirm, 16) == 0;
    printf("m_confirm_valid: %u\n", m_confirm_valid);
    return m_confirm_valid;
}

static void sm_test(){
    key_t k;
    memset(k, 0, 16 );
    print_key("k", k);

    // c1
    key_t r = { 0x57, 0x83, 0xD5, 0x21, 0x56, 0xAD, 0x6F, 0x0E, 0x63, 0x88, 0x27, 0x4E, 0xC6, 0x70, 0x2E, 0xE0 };
    print_key("r", r);

    uint8_t preq[] = {0x01, 0x01, 0x00, 0x00, 0x10, 0x07, 0x07};
    uint8_t pres[] = {0x02, 0x03, 0x00, 0x00, 0x08, 0x00, 0x05};
    bd_addr_t ia = { 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6 };
    bd_addr_t ra = { 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6 };
    
    key_t c1;
    sm_c1(k, r, preq, pres, 1, 0, ia, ra, c1);
    
    // s1
    key_t s1;
    key_t r1 = { 0x00, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    key_t r2 = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};
    sm_s1(k, r1, r2, s1);
}

void sm_test2(){
    key_t k;
    memset(k, 0, 16 );
    print_key("k", k);

    key_t r = { 0x55, 0x05, 0x1D, 0xF4, 0x7C, 0xC9, 0xBC, 0x97, 0x3C, 0x6A, 0x7D, 0x0D, 0x0F, 0x57, 0x0E, 0xC4 };
    print_key("r", r);

    // preq [ 01 04 00 01 10 07 07 ]
    // pres [ 02 04 00 01 10 07 07 ]

    uint8_t preq[] = {0x01, 0x04, 0x00, 0x01, 0x10, 0x07, 0x07};
    uint8_t pres[] = {0x02, 0x04, 0x00, 0x01, 0x10, 0x07, 0x07};

    // Initiator
    // Peer_Address_Type: Random Device Address
    // Peer_Address: 5C:49:F9:4F:1F:04

    // Responder
    // Peer_Address_Type: Public Device Address
    // Peer_Address: 00:1B:DC:05:B5:DC
    bd_addr_t ia = { 0x5c, 0x49, 0xf9, 0x4f, 0x1f, 0x04 };
    bd_addr_t ra = { 0x00, 0x1b, 0xdc, 0x05, 0xB5, 0xdc };
    
    key_t c1;
    key_t c1_true = { 0xFB, 0xAB, 0x63, 0x6F, 0xE4, 0xB4, 0xA5, 0x16, 0xAF, 0x8D, 0x88, 0xED, 0xBD, 0xB6, 0xA6, 0xFE };

    bd_addr_t ia_le;
    bd_addr_t ra_le;
    bt_flip_addr(ia_le, ia);
    bt_flip_addr(ra_le, ra);

    sm_c1(k, r, preq, pres, 1, 0, ia, ra, c1);
    printf("Confirm value correct :%u\n", memcmp(c1, c1_true, 16) == 0);
}

// Security Manager Client API

void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data)){
    sm_get_oob_data = get_oob_data_callback;
}

void sm_set_accepted_stk_generation_method(uint8_t accepted_stk_generation_methods){
    sm_accepted_stk_generation_methods = accepted_stk_generation_methods;
}

void sm_set_max_encrypted_key_size(uint8_t size) {
    sm_max_encrypted_key_size = size;
}

void sm_set_min_encrypted_key_size(uint8_t size) {
    sm_min_encrypted_key_size = size;
}

void sm_set_authentication_requirements(uint8_t auth_req){
    sm_s_auth_req = auth_req;
}

void sm_set_io_capabilities(io_capability_t io_capability){
    sm_s_io_capabilities = io_capability;
}

void sm_set_request_security(int enable){
    sm_s_request_security = enable;
}

int sm_get_connection(uint8_t addr_type, bd_addr_t address){
    // TODO compare to current connection
    return 1;
}

void sm_bonding_decline(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return; // wrong connection
    sm_user_response = SM_USER_RESPONSE_DECLINE;

    if (sm_state_responding == SM_STATE_PH1_W4_USER_RESPONSE){
        sm_pairing_failed_reason = SM_REASON_PASSKEYT_ENTRY_FAILED;
        sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
    }
    sm_run();
}

void sm_just_works_confirm(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return; // wrong connection
    sm_user_response = SM_USER_RESPONSE_CONFIRM;
    if (sm_state_responding == SM_STATE_PH1_W4_USER_RESPONSE){
        sm_state_responding = SM_STATE_PH2_C1_GET_RANDOM_A;
    }
    sm_run();
}

void sm_passkey_input(uint8_t addr_type, bd_addr_t address, uint32_t passkey){
    if (!sm_get_connection(addr_type, address)) return; // wrong connection
    sm_reset_tk();
    net_store_32(sm_tk, 12, passkey);
    sm_user_response = SM_USER_RESPONSE_PASSKEY;
    if (sm_state_responding == SM_STATE_PH1_W4_USER_RESPONSE){
        sm_state_responding = SM_STATE_PH2_C1_GET_RANDOM_A;
    }
    sm_run();
}

// test profile
#include "profile.h"

static void att_try_respond(void){
    if (!att_response_size) return;
    if (!att_response_handle) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;
    
    // update state before sending packet
    uint16_t size = att_response_size;
    att_response_size = 0;
    l2cap_send_connectionless(att_response_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_buffer, size);
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;
    
    att_response_handle = handle;
    att_response_size = att_handle_request(&att_connection, packet, size, att_response_buffer);
    att_try_respond();
}

// write requests
static void att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x\n", handle);
    switch(handle){
        case 0x000b:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case 0x000d:
            printf("New value: %u\n", buffer[0]);
            break;
    }
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
    hci_init(transport, config, control, remote_db);

    // set up l2cap_le
    l2cap_init();
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_fixed_channel(sm_packet_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
    
    // set up ATT
    att_set_db(profile_data);
    att_set_write_callback(att_write_callback);
    att_dump_attributes();
    att_connection.mtu = 27;

    // setup SM
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING );
    sm_set_request_security(1);
}

int main(void)
{
    // sm_test();
    // sm_test2();
    // exit(0);

    setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}

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
 
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "ble/le_device_db.h"
#include "ble/core.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_memory.h"
#include "gap.h"
#include "hci.h"
#include "l2cap.h"

#ifdef ENABLE_LE_SECURE_CONNECTIONS
#ifdef HAVE_HCI_CONTROLLER_DHKEY_SUPPORT
#error "Support for DHKEY Support in HCI Controller not implemented yet. Please use software implementation" 
#else
#define USE_MBEDTLS_FOR_ECDH
#endif
#endif


// Software ECDH implementation provided by mbedtls
#ifdef USE_MBEDTLS_FOR_ECDH
#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/ecp.h"
#include "sm_mbedtls_allocator.h" 
#endif

//
// SM internal types and globals
//

typedef enum {
    DKG_W4_WORKING,
    DKG_CALC_IRK,
    DKG_W4_IRK,
    DKG_CALC_DHK,
    DKG_W4_DHK,
    DKG_READY
} derived_key_generation_t;

typedef enum {
    RAU_W4_WORKING,
    RAU_IDLE,
    RAU_GET_RANDOM,
    RAU_W4_RANDOM,
    RAU_GET_ENC,
    RAU_W4_ENC,
    RAU_SET_ADDRESS,
} random_address_update_t;

typedef enum {
    CMAC_IDLE,
    CMAC_CALC_SUBKEYS,
    CMAC_W4_SUBKEYS,
    CMAC_CALC_MI,
    CMAC_W4_MI,
    CMAC_CALC_MLAST,
    CMAC_W4_MLAST
} cmac_state_t;

typedef enum {
    JUST_WORKS,
    PK_RESP_INPUT,  // Initiator displays PK, responder inputs PK 
    PK_INIT_INPUT,  // Responder displays PK, initiator inputs PK
    OK_BOTH_INPUT,  // Only input on both, both input PK
    NK_BOTH_INPUT,  // Only numerical compparison (yes/no) on on both sides
    OOB             // OOB available on both sides
} stk_generation_method_t;

typedef enum {
    SM_USER_RESPONSE_IDLE,
    SM_USER_RESPONSE_PENDING,
    SM_USER_RESPONSE_CONFIRM,
    SM_USER_RESPONSE_PASSKEY,
    SM_USER_RESPONSE_DECLINE
} sm_user_response_t;

typedef enum {
    SM_AES128_IDLE,
    SM_AES128_ACTIVE
} sm_aes128_state_t;

typedef enum {
    ADDRESS_RESOLUTION_IDLE,
    ADDRESS_RESOLUTION_GENERAL,
    ADDRESS_RESOLUTION_FOR_CONNECTION,
} address_resolution_mode_t;

typedef enum {
    ADDRESS_RESOLUTION_SUCEEDED,
    ADDRESS_RESOLUTION_FAILED,
} address_resolution_event_t;

typedef enum {
    EC_KEY_GENERATION_IDLE,
    EC_KEY_GENERATION_ACTIVE,
    EC_KEY_GENERATION_DONE,
} ec_key_generation_state_t;

typedef enum {
    SM_STATE_VAR_DHKEY_COMMAND_RECEIVED = 1 << 0
} sm_state_var_t;

//
// GLOBAL DATA
//

static uint8_t test_use_fixed_local_csrk;

// configuration
static uint8_t sm_accepted_stk_generation_methods;
static uint8_t sm_max_encryption_key_size;
static uint8_t sm_min_encryption_key_size;
static uint8_t sm_auth_req = 0;
static uint8_t sm_io_capabilities = IO_CAPABILITY_NO_INPUT_NO_OUTPUT;
static uint8_t sm_slave_request_security;
#ifdef ENABLE_LE_SECURE_CONNECTIONS
static uint8_t sm_have_ec_keypair;
#endif

// Security Manager Master Keys, please use sm_set_er(er) and sm_set_ir(ir) with your own 128 bit random values
static sm_key_t sm_persistent_er;
static sm_key_t sm_persistent_ir;

// derived from sm_persistent_ir
static sm_key_t sm_persistent_dhk;
static sm_key_t sm_persistent_irk;
static uint8_t  sm_persistent_irk_ready = 0;    // used for testing
static derived_key_generation_t dkg_state;

// derived from sm_persistent_er
// ..

// random address update
static random_address_update_t rau_state;
static bd_addr_t sm_random_address;

// CMAC Calculation: General
static cmac_state_t sm_cmac_state;
static uint16_t     sm_cmac_message_len;
static sm_key_t     sm_cmac_k;
static sm_key_t     sm_cmac_x;
static sm_key_t     sm_cmac_m_last;
static uint8_t      sm_cmac_block_current;
static uint8_t      sm_cmac_block_count;
static uint8_t      (*sm_cmac_get_byte)(uint16_t offset);
static void         (*sm_cmac_done_handler)(uint8_t * hash);

// CMAC for ATT Signed Writes
static uint8_t      sm_cmac_header[3];
static const uint8_t * sm_cmac_message;
static uint8_t      sm_cmac_sign_counter[4];

// CMAC for Secure Connection functions
#ifdef ENABLE_LE_SECURE_CONNECTIONS
static sm_connection_t * sm_cmac_connection;
static uint8_t           sm_cmac_sc_buffer[80];
#endif

// resolvable private address lookup / CSRK calculation
static int       sm_address_resolution_test;
static int       sm_address_resolution_ah_calculation_active;
static uint8_t   sm_address_resolution_addr_type;
static bd_addr_t sm_address_resolution_address;
static void *    sm_address_resolution_context;
static address_resolution_mode_t sm_address_resolution_mode;
static btstack_linked_list_t sm_address_resolution_general_queue;

// aes128 crypto engine. store current sm_connection_t in sm_aes128_context
static sm_aes128_state_t  sm_aes128_state;
static void *             sm_aes128_context;

// random engine. store context (ususally sm_connection_t)
static void * sm_random_context;

// to receive hci events
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* to dispatch sm event */
static btstack_linked_list_t sm_event_handlers;


// Software ECDH implementation provided by mbedtls
#ifdef USE_MBEDTLS_FOR_ECDH
// group is always valid
static mbedtls_ecp_group   mbedtls_ec_group;
static ec_key_generation_state_t ec_key_generation_state;
static uint8_t ec_qx[32];
static uint8_t ec_qy[32];
static uint8_t ec_d[32];
#ifndef HAVE_MALLOC
// COMP Method with Window 2
// 1300 bytes with 23 allocations
// #define MBEDTLS_ALLOC_BUFFER_SIZE (1300+23*sizeof(void *))
// NAIVE Method with safe cond assignments (without safe cond, order changes and allocations fail)
#define MBEDTLS_ALLOC_BUFFER_SIZE (700+18*sizeof(void *))
static uint8_t mbedtls_memory_buffer[MBEDTLS_ALLOC_BUFFER_SIZE]; 
#endif
#endif

//
// Volume 3, Part H, Chapter 24
// "Security shall be initiated by the Security Manager in the device in the master role.
// The device in the slave role shall be the responding device."
// -> master := initiator, slave := responder
//

// data needed for security setup
typedef struct sm_setup_context {

    btstack_timer_source_t sm_timeout;

    // used in all phases
    uint8_t   sm_pairing_failed_reason;

    // user response, (Phase 1 and/or 2)
    uint8_t   sm_user_response;
    uint8_t   sm_keypress_notification;

    // defines which keys will be send after connection is encrypted - calculated during Phase 1, used Phase 3
    int       sm_key_distribution_send_set;
    int       sm_key_distribution_received_set;

    // Phase 2 (Pairing over SMP)
    stk_generation_method_t sm_stk_generation_method;
    sm_key_t  sm_tk;
    uint8_t   sm_use_secure_connections;

    sm_key_t  sm_c1_t3_value;   // c1 calculation 
    sm_pairing_packet_t sm_m_preq; // pairing request - needed only for c1
    sm_pairing_packet_t sm_s_pres; // pairing response - needed only for c1
    sm_key_t  sm_local_random;
    sm_key_t  sm_local_confirm;
    sm_key_t  sm_peer_random;
    sm_key_t  sm_peer_confirm;
    uint8_t   sm_m_addr_type;   // address and type can be removed
    uint8_t   sm_s_addr_type;   //  '' 
    bd_addr_t sm_m_address;     //  ''
    bd_addr_t sm_s_address;     //  ''
    sm_key_t  sm_ltk;

    uint8_t   sm_state_vars;
#ifdef ENABLE_LE_SECURE_CONNECTIONS
    uint8_t   sm_peer_qx[32];   // also stores random for EC key generation during init
    uint8_t   sm_peer_qy[32];   //  ''
    sm_key_t  sm_peer_nonce;    // might be combined with sm_peer_random
    sm_key_t  sm_local_nonce;   // might be combined with sm_local_random
    sm_key_t  sm_peer_dhkey_check;
    sm_key_t  sm_local_dhkey_check;
    sm_key_t  sm_ra;
    sm_key_t  sm_rb;
    sm_key_t  sm_t;             // used for f5 and h6
    sm_key_t  sm_mackey;
    uint8_t   sm_passkey_bit;   // also stores number of generated random bytes for EC key generation
#endif

    // Phase 3

    // key distribution, we generate
    uint16_t  sm_local_y;
    uint16_t  sm_local_div;
    uint16_t  sm_local_ediv;
    uint8_t   sm_local_rand[8];
    sm_key_t  sm_local_ltk;
    sm_key_t  sm_local_csrk;
    sm_key_t  sm_local_irk;
    // sm_local_address/addr_type not needed

    // key distribution, received from peer
    uint16_t  sm_peer_y;
    uint16_t  sm_peer_div;
    uint16_t  sm_peer_ediv;
    uint8_t   sm_peer_rand[8];
    sm_key_t  sm_peer_ltk;
    sm_key_t  sm_peer_irk;
    sm_key_t  sm_peer_csrk;
    uint8_t   sm_peer_addr_type;
    bd_addr_t sm_peer_address;

} sm_setup_context_t;

// 
static sm_setup_context_t the_setup;
static sm_setup_context_t * setup = &the_setup;

// active connection - the one for which the_setup is used for
static uint16_t sm_active_connection = 0;

// @returns 1 if oob data is available
// stores oob data in provided 16 byte buffer if not null
static int (*sm_get_oob_data)(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data) = NULL;

// horizontal: initiator capabilities
// vertial:    responder capabilities
static const stk_generation_method_t stk_generation_method [5] [5] = {
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    OK_BOTH_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
    { JUST_WORKS,      JUST_WORKS,       JUST_WORKS,      JUST_WORKS,    JUST_WORKS    },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    PK_INIT_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
};

// uses numeric comparison if one side has DisplayYesNo and KeyboardDisplay combinations
#ifdef ENABLE_LE_SECURE_CONNECTIONS
static const stk_generation_method_t stk_generation_method_with_secure_connection[5][5] = {
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { JUST_WORKS,      NK_BOTH_INPUT,    PK_INIT_INPUT,   JUST_WORKS,    NK_BOTH_INPUT },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    OK_BOTH_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
    { JUST_WORKS,      JUST_WORKS,       JUST_WORKS,      JUST_WORKS,    JUST_WORKS    },
    { PK_RESP_INPUT,   NK_BOTH_INPUT,    PK_INIT_INPUT,   JUST_WORKS,    NK_BOTH_INPUT },
};
#endif

static void sm_run(void);
static void sm_done_for_handle(hci_con_handle_t con_handle);
static sm_connection_t * sm_get_connection_for_handle(hci_con_handle_t con_handle);
static inline int sm_calc_actual_encryption_key_size(int other);
static int sm_validate_stk_generation_method(void);
static void sm_shift_left_by_one_bit_inplace(int len, uint8_t * data);

static void log_info_hex16(const char * name, uint16_t value){
    log_info("%-6s 0x%04x", name, value);
}

// @returns 1 if all bytes are 0
static int sm_is_null(uint8_t * data, int size){
    int i;
    for (i=0; i < size ; i++){
        if (data[i]) return 0;
    }
    return 1;
}

static int sm_is_null_random(uint8_t random[8]){
    return sm_is_null(random, 8);
}

static int sm_is_null_key(uint8_t * key){
    return sm_is_null(key, 16);
}

// Key utils
static void sm_reset_tk(void){
    int i;
    for (i=0;i<16;i++){
        setup->sm_tk[i] = 0;
    }
}

// "For example, if a 128-bit encryption key is 0x123456789ABCDEF0123456789ABCDEF0
// and it is reduced to 7 octets (56 bits), then the resulting key is 0x0000000000000000003456789ABCDEF0.""
static void sm_truncate_key(sm_key_t key, int max_encryption_size){
    int i;
    for (i = max_encryption_size ; i < 16 ; i++){
        key[15-i] = 0;
    }
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

static void sm_timeout_handler(btstack_timer_source_t * timer){
    log_info("SM timeout");
    sm_connection_t * sm_conn = (sm_connection_t*) btstack_run_loop_get_timer_context(timer);
    sm_conn->sm_engine_state = SM_GENERAL_TIMEOUT;
    sm_done_for_handle(sm_conn->sm_handle);

    // trigger handling of next ready connection
    sm_run();
}
static void sm_timeout_start(sm_connection_t * sm_conn){
    btstack_run_loop_remove_timer(&setup->sm_timeout);
    btstack_run_loop_set_timer_context(&setup->sm_timeout, sm_conn);
    btstack_run_loop_set_timer_handler(&setup->sm_timeout, sm_timeout_handler);
    btstack_run_loop_set_timer(&setup->sm_timeout, 30000); // 30 seconds sm timeout
    btstack_run_loop_add_timer(&setup->sm_timeout);
}
static void sm_timeout_stop(void){
    btstack_run_loop_remove_timer(&setup->sm_timeout);
}
static void sm_timeout_reset(sm_connection_t * sm_conn){
    sm_timeout_stop();
    sm_timeout_start(sm_conn);    
}

// end of sm timeout

// GAP Random Address updates
static gap_random_address_type_t gap_random_adress_type;
static btstack_timer_source_t gap_random_address_update_timer; 
static uint32_t gap_random_adress_update_period;

static void gap_random_address_trigger(void){
    if (rau_state != RAU_IDLE) return;
    log_info("gap_random_address_trigger");
    rau_state = RAU_GET_RANDOM;
    sm_run();
}

static void gap_random_address_update_handler(btstack_timer_source_t * timer){
    log_info("GAP Random Address Update due");
    btstack_run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    btstack_run_loop_add_timer(&gap_random_address_update_timer);
    gap_random_address_trigger();
}

static void gap_random_address_update_start(void){
    btstack_run_loop_set_timer_handler(&gap_random_address_update_timer, gap_random_address_update_handler);
    btstack_run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    btstack_run_loop_add_timer(&gap_random_address_update_timer);
}

static void gap_random_address_update_stop(void){
    btstack_run_loop_remove_timer(&gap_random_address_update_timer);
}


static void sm_random_start(void * context){
    sm_random_context = context;
    hci_send_cmd(&hci_le_rand);
}

// pre: sm_aes128_state != SM_AES128_ACTIVE, hci_can_send_command == 1
// context is made availabe to aes128 result handler by this
static void sm_aes128_start(sm_key_t key, sm_key_t plaintext, void * context){
    sm_aes128_state = SM_AES128_ACTIVE;
    sm_key_t key_flipped, plaintext_flipped;
    reverse_128(key, key_flipped);
    reverse_128(plaintext, plaintext_flipped);
    sm_aes128_context = context;
    hci_send_cmd(&hci_le_encrypt, key_flipped, plaintext_flipped);
}

// ah(k,r) helper
// r = padding || r
// r - 24 bit value
static void sm_ah_r_prime(uint8_t r[3], sm_key_t r_prime){
    // r'= padding || r
    memset(r_prime, 0, 16);
    memcpy(&r_prime[13], r, 3);
}

// d1 helper
// d' = padding || r || d
// d,r - 16 bit values
static void sm_d1_d_prime(uint16_t d, uint16_t r, sm_key_t d1_prime){
    // d'= padding || r || d
    memset(d1_prime, 0, 16);
    big_endian_store_16(d1_prime, 12, r);
    big_endian_store_16(d1_prime, 14, d);
}

// dm helper
// r’ = padding || r
// r - 64 bit value
static void sm_dm_r_prime(uint8_t r[8], sm_key_t r_prime){
    memset(r_prime, 0, 16);
    memcpy(&r_prime[8], r, 8);
}

// calculate arguments for first AES128 operation in C1 function
static void sm_c1_t1(sm_key_t r, uint8_t preq[7], uint8_t pres[7], uint8_t iat, uint8_t rat, sm_key_t t1){

    // p1 = pres || preq || rat’ || iat’
    // "The octet of iat’ becomes the least significant octet of p1 and the most signifi-
    // cant octet of pres becomes the most significant octet of p1.
    // For example, if the 8-bit iat’ is 0x01, the 8-bit rat’ is 0x00, the 56-bit preq
    // is 0x07071000000101 and the 56 bit pres is 0x05000800000302 then
    // p1 is 0x05000800000302070710000001010001."
    
    sm_key_t p1;
    reverse_56(pres, &p1[0]);
    reverse_56(preq, &p1[7]);
    p1[14] = rat;
    p1[15] = iat;
    log_info_key("p1", p1);
    log_info_key("r", r);
    
    // t1 = r xor p1
    int i;
    for (i=0;i<16;i++){
        t1[i] = r[i] ^ p1[i];
    }
    log_info_key("t1", t1);
}

// calculate arguments for second AES128 operation in C1 function
static void sm_c1_t3(sm_key_t t2, bd_addr_t ia, bd_addr_t ra, sm_key_t t3){
     // p2 = padding || ia || ra
    // "The least significant octet of ra becomes the least significant octet of p2 and
    // the most significant octet of padding becomes the most significant octet of p2.
    // For example, if 48-bit ia is 0xA1A2A3A4A5A6 and the 48-bit ra is
    // 0xB1B2B3B4B5B6 then p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6.
    
    sm_key_t p2;
    memset(p2, 0, 16);
    memcpy(&p2[4],  ia, 6);
    memcpy(&p2[10], ra, 6);
    log_info_key("p2", p2);

    // c1 = e(k, t2_xor_p2)
    int i;
    for (i=0;i<16;i++){
        t3[i] = t2[i] ^ p2[i];
    }
    log_info_key("t3", t3);
}

static void sm_s1_r_prime(sm_key_t r1, sm_key_t r2, sm_key_t r_prime){
    log_info_key("r1", r1);
    log_info_key("r2", r2);
    memcpy(&r_prime[8], &r2[8], 8);
    memcpy(&r_prime[0], &r1[8], 8);
}

#ifdef ENABLE_LE_SECURE_CONNECTIONS
// Software implementations of crypto toolbox for LE Secure Connection
// TODO: replace with code to use AES Engine of HCI Controller
typedef uint8_t sm_key24_t[3];
typedef uint8_t sm_key56_t[7];
typedef uint8_t sm_key256_t[32];

#if 0
static void aes128_calc_cyphertext(const uint8_t key[16], const uint8_t plaintext[16], uint8_t cyphertext[16]){
    uint32_t rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &key[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, plaintext, cyphertext);
}

static void calc_subkeys(sm_key_t k0, sm_key_t k1, sm_key_t k2){
    memcpy(k1, k0, 16);
    sm_shift_left_by_one_bit_inplace(16, k1);
    if (k0[0] & 0x80){
        k1[15] ^= 0x87;
    }
    memcpy(k2, k1, 16);
    sm_shift_left_by_one_bit_inplace(16, k2);
    if (k1[0] & 0x80){
        k2[15] ^= 0x87;
    } 
}

static void aes_cmac(sm_key_t aes_cmac, const sm_key_t key, const uint8_t * data, int cmac_message_len){
    sm_key_t k0, k1, k2, zero;
    memset(zero, 0, 16);

    aes128_calc_cyphertext(key, zero, k0);
    calc_subkeys(k0, k1, k2);

    int cmac_block_count = (cmac_message_len + 15) / 16;

    // step 3: ..
    if (cmac_block_count==0){
        cmac_block_count = 1;
    }

    // step 4: set m_last
    sm_key_t cmac_m_last;
    int sm_cmac_last_block_complete = cmac_message_len != 0 && (cmac_message_len & 0x0f) == 0;
    int i;
    if (sm_cmac_last_block_complete){
        for (i=0;i<16;i++){
            cmac_m_last[i] = data[cmac_message_len - 16 + i] ^ k1[i];
        }
    } else {
        int valid_octets_in_last_block = cmac_message_len & 0x0f;
        for (i=0;i<16;i++){
            if (i < valid_octets_in_last_block){
                cmac_m_last[i] = data[(cmac_message_len & 0xfff0) + i] ^ k2[i];
                continue;
            }
            if (i == valid_octets_in_last_block){
                cmac_m_last[i] = 0x80 ^ k2[i];
                continue;
            }
            cmac_m_last[i] = k2[i];
        }
    }

    // printf("sm_cmac_start: len %u, block count %u\n", cmac_message_len, cmac_block_count);
    // LOG_KEY(cmac_m_last);

    // Step 5
    sm_key_t cmac_x;
    memset(cmac_x, 0, 16);

    // Step 6
    sm_key_t sm_cmac_y;
    for (int block = 0 ; block < cmac_block_count-1 ; block++){
        for (i=0;i<16;i++){
            sm_cmac_y[i] = cmac_x[i] ^ data[block * 16 + i];
        }
        aes128_calc_cyphertext(key, sm_cmac_y, cmac_x);
    }
    for (i=0;i<16;i++){
        sm_cmac_y[i] = cmac_x[i] ^ cmac_m_last[i];
    }

    // Step 7
    aes128_calc_cyphertext(key, sm_cmac_y, aes_cmac);
}
#endif
#endif

static void sm_setup_event_base(uint8_t * event, int event_size, uint8_t type, hci_con_handle_t con_handle, uint8_t addr_type, bd_addr_t address){
    event[0] = type;
    event[1] = event_size - 2;
    little_endian_store_16(event, 2, con_handle);
    event[4] = addr_type;
    reverse_bd_addr(address, &event[5]);
}

static void sm_dispatch_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    // dispatch to all event handlers
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &sm_event_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_packet_callback_registration_t * entry = (btstack_packet_callback_registration_t*) btstack_linked_list_iterator_next(&it);
        entry->callback(packet_type, 0, packet, size);
    }
}

static void sm_notify_client_base(uint8_t type, hci_con_handle_t con_handle, uint8_t addr_type, bd_addr_t address){
    uint8_t event[11];
    sm_setup_event_base(event, sizeof(event), type, con_handle, addr_type, address);
    sm_dispatch_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void sm_notify_client_passkey(uint8_t type, hci_con_handle_t con_handle, uint8_t addr_type, bd_addr_t address, uint32_t passkey){
    uint8_t event[15];
    sm_setup_event_base(event, sizeof(event), type, con_handle, addr_type, address);
    little_endian_store_32(event, 11, passkey);
    sm_dispatch_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void sm_notify_client_index(uint8_t type, hci_con_handle_t con_handle, uint8_t addr_type, bd_addr_t address, uint16_t index){
    uint8_t event[13];
    sm_setup_event_base(event, sizeof(event), type, con_handle, addr_type, address);
    little_endian_store_16(event, 11, index);
    sm_dispatch_event(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void sm_notify_client_authorization(uint8_t type, hci_con_handle_t con_handle, uint8_t addr_type, bd_addr_t address, uint8_t result){

    uint8_t event[18];
    sm_setup_event_base(event, sizeof(event), type, con_handle, addr_type, address); 
    event[11] = result;
    sm_dispatch_event(HCI_EVENT_PACKET, 0, (uint8_t*) &event, sizeof(event));
}

// decide on stk generation based on
// - pairing request
// - io capabilities
// - OOB data availability
static void sm_setup_tk(void){

    // default: just works
    setup->sm_stk_generation_method = JUST_WORKS;

#ifdef ENABLE_LE_SECURE_CONNECTIONS
    setup->sm_use_secure_connections = ( sm_pairing_packet_get_auth_req(setup->sm_m_preq)
                                       & sm_pairing_packet_get_auth_req(setup->sm_s_pres)
                                       & SM_AUTHREQ_SECURE_CONNECTION ) != 0;
    memset(setup->sm_ra, 0, 16);
    memset(setup->sm_rb, 0, 16);
#else
    setup->sm_use_secure_connections = 0;
#endif
    
    // If both devices have not set the MITM option in the Authentication Requirements
    // Flags, then the IO capabilities shall be ignored and the Just Works association
    // model shall be used. 
    if (((sm_pairing_packet_get_auth_req(setup->sm_m_preq) & SM_AUTHREQ_MITM_PROTECTION) == 0) 
    &&  ((sm_pairing_packet_get_auth_req(setup->sm_s_pres) & SM_AUTHREQ_MITM_PROTECTION) == 0)){
        log_info("SM: MITM not required by both -> JUST WORKS");
        return;
    }

    // TODO: with LE SC, OOB is used to transfer data OOB during pairing, single device with OOB is sufficient

    // If both devices have out of band authentication data, then the Authentication
    // Requirements Flags shall be ignored when selecting the pairing method and the
    // Out of Band pairing method shall be used.
    if (sm_pairing_packet_get_oob_data_flag(setup->sm_m_preq)
    &&  sm_pairing_packet_get_oob_data_flag(setup->sm_s_pres)){
        log_info("SM: have OOB data");
        log_info_key("OOB", setup->sm_tk);
        setup->sm_stk_generation_method = OOB;
        return;
    }

    // Reset TK as it has been setup in sm_init_setup
    sm_reset_tk();

    // Also use just works if unknown io capabilites
    if ((sm_pairing_packet_get_io_capability(setup->sm_m_preq) > IO_CAPABILITY_KEYBOARD_DISPLAY) || (sm_pairing_packet_get_io_capability(setup->sm_s_pres) > IO_CAPABILITY_KEYBOARD_DISPLAY)){
        return;
    }

    // Otherwise the IO capabilities of the devices shall be used to determine the
    // pairing method as defined in Table 2.4.
    // see http://stackoverflow.com/a/1052837/393697 for how to specify pointer to 2-dimensional array
    const stk_generation_method_t (*generation_method)[5] = stk_generation_method;

#ifdef ENABLE_LE_SECURE_CONNECTIONS
    // table not define by default
    if (setup->sm_use_secure_connections){
        generation_method = stk_generation_method_with_secure_connection;
    }
#endif    
    setup->sm_stk_generation_method = generation_method[sm_pairing_packet_get_io_capability(setup->sm_s_pres)][sm_pairing_packet_get_io_capability(setup->sm_m_preq)];

    log_info("sm_setup_tk: master io cap: %u, slave io cap: %u -> method %u",
        sm_pairing_packet_get_io_capability(setup->sm_m_preq), sm_pairing_packet_get_io_capability(setup->sm_s_pres), setup->sm_stk_generation_method);
}

static int sm_key_distribution_flags_for_set(uint8_t key_set){
    int flags = 0;
    if (key_set & SM_KEYDIST_ENC_KEY){
        flags |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
        flags |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;        
    }
    if (key_set & SM_KEYDIST_ID_KEY){
        flags |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
        flags |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;        
    }
    if (key_set & SM_KEYDIST_SIGN){
        flags |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
    }
    return flags;
}

static void sm_setup_key_distribution(uint8_t key_set){
    setup->sm_key_distribution_received_set = 0;
    setup->sm_key_distribution_send_set = sm_key_distribution_flags_for_set(key_set);
}

// CSRK Key Lookup


static int sm_address_resolution_idle(void){
    return sm_address_resolution_mode == ADDRESS_RESOLUTION_IDLE;
}

static void sm_address_resolution_start_lookup(uint8_t addr_type, hci_con_handle_t con_handle, bd_addr_t addr, address_resolution_mode_t mode, void * context){
    memcpy(sm_address_resolution_address, addr, 6);
    sm_address_resolution_addr_type = addr_type;
    sm_address_resolution_test = 0;
    sm_address_resolution_mode = mode;
    sm_address_resolution_context = context;
    sm_notify_client_base(SM_EVENT_IDENTITY_RESOLVING_STARTED, con_handle, addr_type, addr);
}

int sm_address_resolution_lookup(uint8_t address_type, bd_addr_t address){
    // check if already in list
    btstack_linked_list_iterator_t it;
    sm_lookup_entry_t * entry;
    btstack_linked_list_iterator_init(&it, &sm_address_resolution_general_queue);
    while(btstack_linked_list_iterator_has_next(&it)){
        entry = (sm_lookup_entry_t *) btstack_linked_list_iterator_next(&it);
        if (entry->address_type != address_type) continue;
        if (memcmp(entry->address, address, 6))  continue;
        // already in list
        return BTSTACK_BUSY;
    }
    entry = btstack_memory_sm_lookup_entry_get();
    if (!entry) return BTSTACK_MEMORY_ALLOC_FAILED;
    entry->address_type = (bd_addr_type_t) address_type;
    memcpy(entry->address, address, 6);
    btstack_linked_list_add(&sm_address_resolution_general_queue, (btstack_linked_item_t *) entry);    
    sm_run();
    return 0;
}

// CMAC Implementation using AES128 engine
static void sm_shift_left_by_one_bit_inplace(int len, uint8_t * data){
    int i;
    int carry = 0;
    for (i=len-1; i >= 0 ; i--){
        int new_carry = data[i] >> 7;
        data[i] = data[i] << 1 | carry;
        carry = new_carry;
    }
}

// while x_state++ for an enum is possible in C, it isn't in C++. we use this helpers to avoid compile errors for now
static inline void sm_next_responding_state(sm_connection_t * sm_conn){
    sm_conn->sm_engine_state = (security_manager_state_t) (((int)sm_conn->sm_engine_state) + 1);
}
static inline void dkg_next_state(void){
    dkg_state = (derived_key_generation_t) (((int)dkg_state) + 1);
}
static inline void rau_next_state(void){
    rau_state = (random_address_update_t) (((int)rau_state) + 1);
}

// CMAC calculation using AES Engine

static inline void sm_cmac_next_state(void){
    sm_cmac_state = (cmac_state_t) (((int)sm_cmac_state) + 1);
}

static int sm_cmac_last_block_complete(void){
    if (sm_cmac_message_len == 0) return 0;
    return (sm_cmac_message_len & 0x0f) == 0;
}

int sm_cmac_ready(void){
    return sm_cmac_state == CMAC_IDLE;
}

// generic cmac calculation
void sm_cmac_general_start(const sm_key_t key, uint16_t message_len, uint8_t (*get_byte_callback)(uint16_t offset), void (*done_callback)(uint8_t hash[8])){
    // Generalized CMAC
    memcpy(sm_cmac_k, key, 16);
    memset(sm_cmac_x, 0, 16);
    sm_cmac_block_current = 0;
    sm_cmac_message_len  = message_len;
    sm_cmac_done_handler = done_callback;
    sm_cmac_get_byte     = get_byte_callback;

    // step 2: n := ceil(len/const_Bsize);
    sm_cmac_block_count = (sm_cmac_message_len + 15) / 16;

    // step 3: ..
    if (sm_cmac_block_count==0){
        sm_cmac_block_count = 1;
    }
    log_info("sm_cmac_general_start: len %u, block count %u", sm_cmac_message_len, sm_cmac_block_count);

    // first, we need to compute l for k1, k2, and m_last
    sm_cmac_state = CMAC_CALC_SUBKEYS;

    // let's go
    sm_run();
}

// cmac for ATT Message signing
static uint8_t sm_cmac_signed_write_message_get_byte(uint16_t offset){
    if (offset >= sm_cmac_message_len) {
        log_error("sm_cmac_signed_write_message_get_byte. out of bounds, access %u, len %u", offset, sm_cmac_message_len);
        return 0;
    }

    offset = sm_cmac_message_len - 1 - offset;

    // sm_cmac_header[3] | message[] | sm_cmac_sign_counter[4]
    if (offset < 3){
        return sm_cmac_header[offset];
    }
    int actual_message_len_incl_header = sm_cmac_message_len - 4;
    if (offset <  actual_message_len_incl_header){
        return sm_cmac_message[offset - 3];
    }
    return sm_cmac_sign_counter[offset - actual_message_len_incl_header];
}

void sm_cmac_signed_write_start(const sm_key_t k, uint8_t opcode, hci_con_handle_t con_handle, uint16_t message_len, const uint8_t * message, uint32_t sign_counter, void (*done_handler)(uint8_t * hash)){
    // ATT Message Signing
    sm_cmac_header[0] = opcode;
    little_endian_store_16(sm_cmac_header, 1, con_handle);
    little_endian_store_32(sm_cmac_sign_counter, 0, sign_counter);
    uint16_t total_message_len = 3 + message_len + 4;  // incl. virtually prepended att opcode, handle and appended sign_counter in LE
    sm_cmac_message = message;
    sm_cmac_general_start(k, total_message_len, &sm_cmac_signed_write_message_get_byte, done_handler);
}


static void sm_cmac_handle_aes_engine_ready(void){
    switch (sm_cmac_state){
        case CMAC_CALC_SUBKEYS: {
            sm_key_t const_zero;
            memset(const_zero, 0, 16);
            sm_cmac_next_state();
            sm_aes128_start(sm_cmac_k, const_zero, NULL);
            break;
        }
        case CMAC_CALC_MI: {
            int j;
            sm_key_t y;
            for (j=0;j<16;j++){
                y[j] = sm_cmac_x[j] ^ sm_cmac_get_byte(sm_cmac_block_current*16 + j);
            }
            sm_cmac_block_current++;
            sm_cmac_next_state();
            sm_aes128_start(sm_cmac_k, y, NULL);
            break;
        }
        case CMAC_CALC_MLAST: {
            int i;
            sm_key_t y;
            for (i=0;i<16;i++){
                y[i] = sm_cmac_x[i] ^ sm_cmac_m_last[i]; 
            }
            log_info_key("Y", y);
            sm_cmac_block_current++;
            sm_cmac_next_state();
            sm_aes128_start(sm_cmac_k, y, NULL);
            break;
        }
        default:
            log_info("sm_cmac_handle_aes_engine_ready called in state %u", sm_cmac_state);
            break;
    }
}

static void sm_cmac_handle_encryption_result(sm_key_t data){
    switch (sm_cmac_state){
        case CMAC_W4_SUBKEYS: {
            sm_key_t k1;
            memcpy(k1, data, 16);
            sm_shift_left_by_one_bit_inplace(16, k1);
            if (data[0] & 0x80){
                k1[15] ^= 0x87;
            }
            sm_key_t k2;
            memcpy(k2, k1, 16);
            sm_shift_left_by_one_bit_inplace(16, k2);
            if (k1[0] & 0x80){
                k2[15] ^= 0x87;
            } 

            log_info_key("k", sm_cmac_k);
            log_info_key("k1", k1);
            log_info_key("k2", k2);

            // step 4: set m_last
            int i;
            if (sm_cmac_last_block_complete()){
                for (i=0;i<16;i++){
                    sm_cmac_m_last[i] = sm_cmac_get_byte(sm_cmac_message_len - 16 + i) ^ k1[i];
                }
            } else {
                int valid_octets_in_last_block = sm_cmac_message_len & 0x0f;
                for (i=0;i<16;i++){
                    if (i < valid_octets_in_last_block){
                        sm_cmac_m_last[i] = sm_cmac_get_byte((sm_cmac_message_len & 0xfff0) + i) ^ k2[i];
                        continue;
                    }
                    if (i == valid_octets_in_last_block){
                        sm_cmac_m_last[i] = 0x80 ^ k2[i];
                        continue;
                    }
                    sm_cmac_m_last[i] = k2[i];
                }
            }

            // next
            sm_cmac_state = sm_cmac_block_current < sm_cmac_block_count - 1 ? CMAC_CALC_MI : CMAC_CALC_MLAST;  
            break;
        }
        case CMAC_W4_MI:
            memcpy(sm_cmac_x, data, 16);
            sm_cmac_state = sm_cmac_block_current < sm_cmac_block_count - 1 ? CMAC_CALC_MI : CMAC_CALC_MLAST;  
            break;
        case CMAC_W4_MLAST:
            // done
            log_info("Setting CMAC Engine to IDLE");
            sm_cmac_state = CMAC_IDLE;
            log_info_key("CMAC", data);
            sm_cmac_done_handler(data);
            break;
        default:
            log_info("sm_cmac_handle_encryption_result called in state %u", sm_cmac_state);
            break;
    }
}

static void sm_trigger_user_response(sm_connection_t * sm_conn){
    // notify client for: JUST WORKS confirm, Numeric comparison confirm, PASSKEY display or input
    setup->sm_user_response = SM_USER_RESPONSE_IDLE;
    switch (setup->sm_stk_generation_method){
        case PK_RESP_INPUT:
            if (sm_conn->sm_role){
                setup->sm_user_response = SM_USER_RESPONSE_PENDING;
                sm_notify_client_base(SM_EVENT_PASSKEY_INPUT_NUMBER, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address); 
            } else {
                sm_notify_client_passkey(SM_EVENT_PASSKEY_DISPLAY_NUMBER, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, big_endian_read_32(setup->sm_tk, 12)); 
            }
            break;
        case PK_INIT_INPUT:
            if (sm_conn->sm_role){
                sm_notify_client_passkey(SM_EVENT_PASSKEY_DISPLAY_NUMBER, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, big_endian_read_32(setup->sm_tk, 12)); 
            } else {
                setup->sm_user_response = SM_USER_RESPONSE_PENDING;
                sm_notify_client_base(SM_EVENT_PASSKEY_INPUT_NUMBER, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address); 
            }
            break;
        case OK_BOTH_INPUT:
            setup->sm_user_response = SM_USER_RESPONSE_PENDING;
            sm_notify_client_base(SM_EVENT_PASSKEY_INPUT_NUMBER, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address); 
            break;        
        case NK_BOTH_INPUT:
            setup->sm_user_response = SM_USER_RESPONSE_PENDING;
            sm_notify_client_passkey(SM_EVENT_NUMERIC_COMPARISON_REQUEST, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, big_endian_read_32(setup->sm_tk, 12)); 
            break;
        case JUST_WORKS:
            setup->sm_user_response = SM_USER_RESPONSE_PENDING;
            sm_notify_client_base(SM_EVENT_JUST_WORKS_REQUEST, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address);
            break;
        case OOB:
            // client already provided OOB data, let's skip notification.
            break;
    }
}

static int sm_key_distribution_all_received(sm_connection_t * sm_conn){
    int recv_flags;
    if (sm_conn->sm_role){
        // slave / responder
        recv_flags = sm_key_distribution_flags_for_set(sm_pairing_packet_get_initiator_key_distribution(setup->sm_s_pres));
    } else {
        // master / initiator
        recv_flags = sm_key_distribution_flags_for_set(sm_pairing_packet_get_responder_key_distribution(setup->sm_s_pres));
    } 
    log_debug("sm_key_distribution_all_received: received 0x%02x, expecting 0x%02x", setup->sm_key_distribution_received_set, recv_flags);       
    return recv_flags == setup->sm_key_distribution_received_set;
}

static void sm_done_for_handle(hci_con_handle_t con_handle){
    if (sm_active_connection == con_handle){
        sm_timeout_stop();
        sm_active_connection = 0;
        log_info("sm: connection 0x%x released setup context", con_handle);
    }
}

static int sm_key_distribution_flags_for_auth_req(void){
    int flags = SM_KEYDIST_ID_KEY | SM_KEYDIST_SIGN;
    if (sm_auth_req & SM_AUTHREQ_BONDING){
        // encryption information only if bonding requested
        flags |= SM_KEYDIST_ENC_KEY;
    }
    return flags;
}

static void sm_reset_setup(void){
    // fill in sm setup
    setup->sm_state_vars = 0;
    setup->sm_keypress_notification = 0xff;
    sm_reset_tk();
}

static void sm_init_setup(sm_connection_t * sm_conn){

    // fill in sm setup
    setup->sm_peer_addr_type = sm_conn->sm_peer_addr_type;
    memcpy(setup->sm_peer_address, sm_conn->sm_peer_address, 6);

    // query client for OOB data
    int have_oob_data = 0;
    if (sm_get_oob_data) {
        have_oob_data = (*sm_get_oob_data)(sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, setup->sm_tk);
    }
    
    sm_pairing_packet_t * local_packet;
    if (sm_conn->sm_role){
        // slave
        local_packet = &setup->sm_s_pres;
        gap_advertisements_get_address(&setup->sm_s_addr_type, setup->sm_s_address);
        setup->sm_m_addr_type = sm_conn->sm_peer_addr_type;
        memcpy(setup->sm_m_address, sm_conn->sm_peer_address, 6);
    } else {
        // master
        local_packet = &setup->sm_m_preq;
        gap_advertisements_get_address(&setup->sm_m_addr_type, setup->sm_m_address);
        setup->sm_s_addr_type = sm_conn->sm_peer_addr_type;
        memcpy(setup->sm_s_address, sm_conn->sm_peer_address, 6);

        int key_distribution_flags = sm_key_distribution_flags_for_auth_req();
        sm_pairing_packet_set_initiator_key_distribution(setup->sm_m_preq, key_distribution_flags);
        sm_pairing_packet_set_responder_key_distribution(setup->sm_m_preq, key_distribution_flags);
    }

    uint8_t auth_req = sm_auth_req;
    sm_pairing_packet_set_io_capability(*local_packet, sm_io_capabilities);
    sm_pairing_packet_set_oob_data_flag(*local_packet, have_oob_data);
    sm_pairing_packet_set_auth_req(*local_packet, auth_req);
    sm_pairing_packet_set_max_encryption_key_size(*local_packet, sm_max_encryption_key_size);
}

static int sm_stk_generation_init(sm_connection_t * sm_conn){

    sm_pairing_packet_t * remote_packet;
    int                   remote_key_request;
    if (sm_conn->sm_role){
        // slave / responder
        remote_packet      = &setup->sm_m_preq;
        remote_key_request = sm_pairing_packet_get_responder_key_distribution(setup->sm_m_preq);
    } else {
        // master / initiator
        remote_packet      = &setup->sm_s_pres;
        remote_key_request = sm_pairing_packet_get_initiator_key_distribution(setup->sm_s_pres);
    }

    // check key size
    sm_conn->sm_actual_encryption_key_size = sm_calc_actual_encryption_key_size(sm_pairing_packet_get_max_encryption_key_size(*remote_packet));
    if (sm_conn->sm_actual_encryption_key_size == 0) return SM_REASON_ENCRYPTION_KEY_SIZE;

    // decide on STK generation method
    sm_setup_tk();
    log_info("SMP: generation method %u", setup->sm_stk_generation_method);

    // check if STK generation method is acceptable by client
    if (!sm_validate_stk_generation_method()) return SM_REASON_AUTHENTHICATION_REQUIREMENTS;

    // identical to responder
    sm_setup_key_distribution(remote_key_request);

    // JUST WORKS doens't provide authentication
    sm_conn->sm_connection_authenticated = setup->sm_stk_generation_method == JUST_WORKS ? 0 : 1;

    return 0;
}

static void sm_address_resolution_handle_event(address_resolution_event_t event){

    // cache and reset context
    int matched_device_id = sm_address_resolution_test;
    address_resolution_mode_t mode = sm_address_resolution_mode;
    void * context = sm_address_resolution_context;
    
    // reset context
    sm_address_resolution_mode = ADDRESS_RESOLUTION_IDLE;
    sm_address_resolution_context = NULL;
    sm_address_resolution_test = -1;
    hci_con_handle_t con_handle = 0;

    sm_connection_t * sm_connection;
    sm_key_t ltk;
    switch (mode){
        case ADDRESS_RESOLUTION_GENERAL:
            break;
        case ADDRESS_RESOLUTION_FOR_CONNECTION:
            sm_connection = (sm_connection_t *) context;
            con_handle = sm_connection->sm_handle;
            switch (event){
                case ADDRESS_RESOLUTION_SUCEEDED:
                    sm_connection->sm_irk_lookup_state = IRK_LOOKUP_SUCCEEDED;
                    sm_connection->sm_le_db_index = matched_device_id;
                    log_info("ADDRESS_RESOLUTION_SUCEEDED, index %d", sm_connection->sm_le_db_index);
                    if (sm_connection->sm_role) break;
                    if (!sm_connection->sm_bonding_requested && !sm_connection->sm_security_request_received) break;
                    sm_connection->sm_security_request_received = 0;
                    sm_connection->sm_bonding_requested = 0;
                    le_device_db_encryption_get(sm_connection->sm_le_db_index, NULL, NULL, ltk, NULL, NULL, NULL);
                    if (!sm_is_null_key(ltk)){
                        sm_connection->sm_engine_state = SM_INITIATOR_PH0_HAS_LTK;
                    } else {
                        sm_connection->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;
                    }
                    break;
                case ADDRESS_RESOLUTION_FAILED:
                    sm_connection->sm_irk_lookup_state = IRK_LOOKUP_FAILED;
                    if (sm_connection->sm_role) break;
                    if (!sm_connection->sm_bonding_requested && !sm_connection->sm_security_request_received) break;
                    sm_connection->sm_security_request_received = 0;
                    sm_connection->sm_bonding_requested = 0;
                    sm_connection->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;
                    break;
            }
            break;
        default:
            break;
    }

    switch (event){
        case ADDRESS_RESOLUTION_SUCEEDED:
            sm_notify_client_index(SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED, con_handle, sm_address_resolution_addr_type, sm_address_resolution_address, matched_device_id);
            break;
        case ADDRESS_RESOLUTION_FAILED:
            sm_notify_client_base(SM_EVENT_IDENTITY_RESOLVING_FAILED, con_handle, sm_address_resolution_addr_type, sm_address_resolution_address);
            break;
    }
}

static void sm_key_distribution_handle_all_received(sm_connection_t * sm_conn){

    int le_db_index = -1;

    // lookup device based on IRK
    if (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_IDENTITY_INFORMATION){
        int i;
        for (i=0; i < le_device_db_count(); i++){
            sm_key_t irk;
            bd_addr_t address;
            int address_type;
            le_device_db_info(i, &address_type, address, irk);
            if (memcmp(irk, setup->sm_peer_irk, 16) == 0){
                log_info("sm: device found for IRK, updating");
                le_db_index = i;
                break;
            }
        }
    }

    // if not found, lookup via public address if possible
    log_info("sm peer addr type %u, peer addres %s", setup->sm_peer_addr_type, bd_addr_to_str(setup->sm_peer_address));
    if (le_db_index < 0 && setup->sm_peer_addr_type == BD_ADDR_TYPE_LE_PUBLIC){
        int i;
        for (i=0; i < le_device_db_count(); i++){
            bd_addr_t address;
            int address_type;
            le_device_db_info(i, &address_type, address, NULL);
            log_info("device %u, sm peer addr type %u, peer addres %s", i, address_type, bd_addr_to_str(address));
            if (address_type == BD_ADDR_TYPE_LE_PUBLIC && memcmp(address, setup->sm_peer_address, 6) == 0){
                log_info("sm: device found for public address, updating");
                le_db_index = i;
                break;
            }
        }
    }

    // if not found, add to db
    if (le_db_index < 0) {
        le_db_index = le_device_db_add(setup->sm_peer_addr_type, setup->sm_peer_address, setup->sm_peer_irk);
    }

    if (le_db_index >= 0){
        
        // store local CSRK
        if (setup->sm_key_distribution_send_set & SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION){
            log_info("sm: store local CSRK");
            le_device_db_local_csrk_set(le_db_index, setup->sm_local_csrk);
            le_device_db_local_counter_set(le_db_index, 0);
        }

        // store remote CSRK
        if (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION){
            log_info("sm: store remote CSRK");
            le_device_db_remote_csrk_set(le_db_index, setup->sm_peer_csrk);
            le_device_db_remote_counter_set(le_db_index, 0);
        }

        // store encryption information for secure connections: LTK generated by ECDH
        if (setup->sm_use_secure_connections){
            log_info("sm: store SC LTK (key size %u, authenticatd %u)", sm_conn->sm_actual_encryption_key_size, sm_conn->sm_connection_authenticated);
            uint8_t zero_rand[8];
            memset(zero_rand, 0, 8);
            le_device_db_encryption_set(le_db_index, 0, zero_rand, setup->sm_ltk, sm_conn->sm_actual_encryption_key_size,
                sm_conn->sm_connection_authenticated, sm_conn->sm_connection_authorization_state == AUTHORIZATION_GRANTED);
        } 

        // store encryption infromation for legacy pairing: peer LTK, EDIV, RAND
        else if ( (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION)   
               && (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_MASTER_IDENTIFICATION )){
            log_info("sm: set encryption information (key size %u, authenticatd %u)", sm_conn->sm_actual_encryption_key_size, sm_conn->sm_connection_authenticated);
            le_device_db_encryption_set(le_db_index, setup->sm_peer_ediv, setup->sm_peer_rand, setup->sm_peer_ltk,
                sm_conn->sm_actual_encryption_key_size, sm_conn->sm_connection_authenticated, sm_conn->sm_connection_authorization_state == AUTHORIZATION_GRANTED);

        }
    }

    // keep le_db_index
    sm_conn->sm_le_db_index = le_db_index;    
}

static void sm_pairing_error(sm_connection_t * sm_conn, uint8_t reason){
    setup->sm_pairing_failed_reason = reason;
    sm_conn->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
}

static inline void sm_pdu_received_in_wrong_state(sm_connection_t * sm_conn){
    sm_pairing_error(sm_conn, SM_REASON_UNSPECIFIED_REASON);
}

#ifdef ENABLE_LE_SECURE_CONNECTIONS

static void sm_sc_prepare_dhkey_check(sm_connection_t * sm_conn);
static int sm_passkey_used(stk_generation_method_t method);
static int sm_just_works_or_numeric_comparison(stk_generation_method_t method);

static void sm_log_ec_keypair(void){
    log_info("Elliptic curve: d");
    log_info_hexdump(ec_d,32);
    log_info("Elliptic curve: X");
    log_info_hexdump(ec_qx,32);
    log_info("Elliptic curve: Y");
    log_info_hexdump(ec_qy,32);
}

static void sm_sc_start_calculating_local_confirm(sm_connection_t * sm_conn){
    if (sm_passkey_used(setup->sm_stk_generation_method)){
        sm_conn->sm_engine_state = SM_SC_W2_GET_RANDOM_A;
    } else {
        sm_conn->sm_engine_state = SM_SC_W2_CMAC_FOR_CONFIRMATION;
    }
}

static void sm_sc_state_after_receiving_random(sm_connection_t * sm_conn){
    if (sm_conn->sm_role){
        // Responder
        sm_conn->sm_engine_state = SM_SC_SEND_PAIRING_RANDOM;
    } else {
        // Initiator role
        switch (setup->sm_stk_generation_method){
            case JUST_WORKS:
                sm_sc_prepare_dhkey_check(sm_conn);
                break;

            case NK_BOTH_INPUT:
                sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_G2;
                break;
            case PK_INIT_INPUT:
            case PK_RESP_INPUT:
            case OK_BOTH_INPUT:
                if (setup->sm_passkey_bit < 20) {
                    sm_sc_start_calculating_local_confirm(sm_conn);
                } else {
                    sm_sc_prepare_dhkey_check(sm_conn);
                }
                break;
            case OOB:
                // TODO: implement SC OOB
                break;
        } 
    }
}

static uint8_t sm_sc_cmac_get_byte(uint16_t offset){
    return sm_cmac_sc_buffer[offset];
}

static void sm_sc_cmac_done(uint8_t * hash){
    log_info("sm_sc_cmac_done: ");
    log_info_hexdump(hash, 16);

    sm_connection_t * sm_conn = sm_cmac_connection;
    sm_cmac_connection = NULL;
    link_key_type_t link_key_type;

    switch (sm_conn->sm_engine_state){
        case SM_SC_W4_CMAC_FOR_CONFIRMATION:
            memcpy(setup->sm_local_confirm, hash, 16);
            sm_conn->sm_engine_state = SM_SC_SEND_CONFIRMATION;
            break;
        case SM_SC_W4_CMAC_FOR_CHECK_CONFIRMATION:
            // check
            if (0 != memcmp(hash, setup->sm_peer_confirm, 16)){
                sm_pairing_error(sm_conn, SM_REASON_CONFIRM_VALUE_FAILED);
                break;
            }
            sm_sc_state_after_receiving_random(sm_conn);
            break;
        case SM_SC_W4_CALCULATE_G2: {
            uint32_t vab = big_endian_read_32(hash, 12) % 1000000;
            big_endian_store_32(setup->sm_tk, 12, vab);
            sm_conn->sm_engine_state = SM_SC_W4_USER_RESPONSE;
            sm_trigger_user_response(sm_conn);
            break;
        }
        case SM_SC_W4_CALCULATE_F5_SALT:
            memcpy(setup->sm_t, hash, 16);
            sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F5_MACKEY;
            break;            
        case SM_SC_W4_CALCULATE_F5_MACKEY:
            memcpy(setup->sm_mackey, hash, 16);
            sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F5_LTK;
            break;            
        case SM_SC_W4_CALCULATE_F5_LTK:
            // truncate sm_ltk, but keep full LTK for cross-transport key derivation in sm_local_ltk
            // Errata Service Release to the Bluetooth Specification: ESR09
            //   E6405 – Cross transport key derivation from a key of size less than 128 bits
            //   Note: When the BR/EDR link key is being derived from the LTK, the derivation is done before the LTK gets masked."
            memcpy(setup->sm_ltk, hash, 16);
            memcpy(setup->sm_local_ltk, hash, 16);
            sm_truncate_key(setup->sm_ltk, sm_conn->sm_actual_encryption_key_size);
            sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F6_FOR_DHKEY_CHECK;
            break; 
        case SM_SC_W4_CALCULATE_F6_FOR_DHKEY_CHECK:
            memcpy(setup->sm_local_dhkey_check, hash, 16);
            if (sm_conn->sm_role){
                // responder
                if (setup->sm_state_vars & SM_STATE_VAR_DHKEY_COMMAND_RECEIVED){
                    sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK;
                } else {
                    sm_conn->sm_engine_state = SM_SC_W4_DHKEY_CHECK_COMMAND;
                }
            } else {
                sm_conn->sm_engine_state = SM_SC_SEND_DHKEY_CHECK_COMMAND;
            } 
            break;
        case SM_SC_W4_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK:
            if (0 != memcmp(hash, setup->sm_peer_dhkey_check, 16) ){
                sm_pairing_error(sm_conn, SM_REASON_DHKEY_CHECK_FAILED);
                break;
            }
            if (sm_conn->sm_role){
                // responder
                sm_conn->sm_engine_state = SM_SC_SEND_DHKEY_CHECK_COMMAND;
            } else {
                // initiator
                sm_conn->sm_engine_state = SM_INITIATOR_PH3_SEND_START_ENCRYPTION;
            }
            break;
        case SM_SC_W4_CALCULATE_H6_ILK:
            memcpy(setup->sm_t, hash, 16);
            sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_H6_BR_EDR_LINK_KEY;
            break;
        case SM_SC_W4_CALCULATE_H6_BR_EDR_LINK_KEY:
            reverse_128(hash, setup->sm_t);
            link_key_type = sm_conn->sm_connection_authenticated ?
                AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256 : UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256;
            if (sm_conn->sm_role){
                gap_store_link_key_for_bd_addr(setup->sm_m_address, setup->sm_t, link_key_type);
                sm_conn->sm_engine_state = SM_RESPONDER_IDLE; 
            } else {
                gap_store_link_key_for_bd_addr(setup->sm_s_address, setup->sm_t, link_key_type);
                sm_conn->sm_engine_state = SM_INITIATOR_CONNECTED; 
            }
            sm_done_for_handle(sm_conn->sm_handle);
            break;
        default:
            log_error("sm_sc_cmac_done in state %u", sm_conn->sm_engine_state);
            break;
    }    
    sm_run();
}

static void f4_engine(sm_connection_t * sm_conn, const sm_key256_t u, const sm_key256_t v, const sm_key_t x, uint8_t z){
    const uint16_t message_len = 65;
    sm_cmac_connection = sm_conn;
    memcpy(sm_cmac_sc_buffer, u, 32);
    memcpy(sm_cmac_sc_buffer+32, v, 32);
    sm_cmac_sc_buffer[64] = z;
    log_info("f4 key");
    log_info_hexdump(x, 16);
    log_info("f4 message");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(x, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

static const sm_key_t f5_salt = { 0x6C ,0x88, 0x83, 0x91, 0xAA, 0xF5, 0xA5, 0x38, 0x60, 0x37, 0x0B, 0xDB, 0x5A, 0x60, 0x83, 0xBE};
static const uint8_t f5_key_id[] = { 0x62, 0x74, 0x6c, 0x65 };
static const uint8_t f5_length[] = { 0x01, 0x00};  

static void sm_sc_calculate_dhkey(sm_key256_t dhkey){
#ifdef USE_MBEDTLS_FOR_ECDH
    // da * Pb
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
    mbedtls_ecp_point DH;
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_point_init(&DH);
    mbedtls_mpi_read_binary(&d, ec_d, 32);
    mbedtls_mpi_read_binary(&Q.X, setup->sm_peer_qx, 32);
    mbedtls_mpi_read_binary(&Q.Y, setup->sm_peer_qy, 32);
    mbedtls_mpi_lset(&Q.Z, 1);
    mbedtls_ecp_mul(&mbedtls_ec_group, &DH, &d, &Q, NULL, NULL);
    mbedtls_mpi_write_binary(&DH.X, dhkey, 32);
    mbedtls_ecp_point_free(&DH);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);
#endif
    log_info("dhkey");
    log_info_hexdump(dhkey, 32);
}

static void f5_calculate_salt(sm_connection_t * sm_conn){
    // calculate DHKEY
    sm_key256_t dhkey;
    sm_sc_calculate_dhkey(dhkey);

    // calculate salt for f5
    const uint16_t message_len = 32;
    sm_cmac_connection = sm_conn;
    memcpy(sm_cmac_sc_buffer, dhkey, message_len);
    sm_cmac_general_start(f5_salt, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

static inline void f5_mackkey(sm_connection_t * sm_conn, sm_key_t t, const sm_key_t n1, const sm_key_t n2, const sm_key56_t a1, const sm_key56_t a2){
    const uint16_t message_len = 53;
    sm_cmac_connection = sm_conn;

    // f5(W, N1, N2, A1, A2) = AES-CMACT (Counter = 0 || keyID || N1 || N2|| A1|| A2 || Length = 256) -- this is the MacKey
    sm_cmac_sc_buffer[0] = 0;
    memcpy(sm_cmac_sc_buffer+01, f5_key_id, 4);
    memcpy(sm_cmac_sc_buffer+05, n1, 16);
    memcpy(sm_cmac_sc_buffer+21, n2, 16);
    memcpy(sm_cmac_sc_buffer+37, a1, 7);
    memcpy(sm_cmac_sc_buffer+44, a2, 7);
    memcpy(sm_cmac_sc_buffer+51, f5_length, 2);
    log_info("f5 key");
    log_info_hexdump(t, 16);
    log_info("f5 message for MacKey");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(t, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

static void f5_calculate_mackey(sm_connection_t * sm_conn){
    sm_key56_t bd_addr_master, bd_addr_slave;
    bd_addr_master[0] =  setup->sm_m_addr_type;
    bd_addr_slave[0]  =  setup->sm_s_addr_type;
    memcpy(&bd_addr_master[1], setup->sm_m_address, 6);
    memcpy(&bd_addr_slave[1],  setup->sm_s_address, 6);
    if (sm_conn->sm_role){
        // responder
        f5_mackkey(sm_conn, setup->sm_t, setup->sm_peer_nonce, setup->sm_local_nonce, bd_addr_master, bd_addr_slave);
    } else {
        // initiator
        f5_mackkey(sm_conn, setup->sm_t, setup->sm_local_nonce, setup->sm_peer_nonce, bd_addr_master, bd_addr_slave);
    }
}

// note: must be called right after f5_mackey, as sm_cmac_buffer[1..52] will be reused
static inline void f5_ltk(sm_connection_t * sm_conn, sm_key_t t){
    const uint16_t message_len = 53;
    sm_cmac_connection = sm_conn;
    sm_cmac_sc_buffer[0] = 1;
    // 1..52 setup before
    log_info("f5 key");
    log_info_hexdump(t, 16);
    log_info("f5 message for LTK");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(t, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

static void f5_calculate_ltk(sm_connection_t * sm_conn){
    f5_ltk(sm_conn, setup->sm_t);
}

static void f6_engine(sm_connection_t * sm_conn, const sm_key_t w, const sm_key_t n1, const sm_key_t n2, const sm_key_t r, const sm_key24_t io_cap, const sm_key56_t a1, const sm_key56_t a2){
    const uint16_t message_len = 65;
    sm_cmac_connection = sm_conn;
    memcpy(sm_cmac_sc_buffer, n1, 16);
    memcpy(sm_cmac_sc_buffer+16, n2, 16);
    memcpy(sm_cmac_sc_buffer+32, r, 16);
    memcpy(sm_cmac_sc_buffer+48, io_cap, 3);
    memcpy(sm_cmac_sc_buffer+51, a1, 7);
    memcpy(sm_cmac_sc_buffer+58, a2, 7);
    log_info("f6 key");
    log_info_hexdump(w, 16);
    log_info("f6 message");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(w, 65, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

// g2(U, V, X, Y) = AES-CMACX(U || V || Y) mod 2^32
// - U is 256 bits
// - V is 256 bits
// - X is 128 bits
// - Y is 128 bits
static void g2_engine(sm_connection_t * sm_conn, const sm_key256_t u, const sm_key256_t v, const sm_key_t x, const sm_key_t y){
    const uint16_t message_len = 80;
    sm_cmac_connection = sm_conn;
    memcpy(sm_cmac_sc_buffer, u, 32);  
    memcpy(sm_cmac_sc_buffer+32, v, 32);
    memcpy(sm_cmac_sc_buffer+64, y, 16);
    log_info("g2 key");
    log_info_hexdump(x, 16);
    log_info("g2 message");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(x, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

static void g2_calculate(sm_connection_t * sm_conn) {
    // calc Va if numeric comparison
    if (sm_conn->sm_role){
        // responder  
        g2_engine(sm_conn, setup->sm_peer_qx, ec_qx, setup->sm_peer_nonce, setup->sm_local_nonce);;
    } else {
        // initiator
        g2_engine(sm_conn, ec_qx, setup->sm_peer_qx, setup->sm_local_nonce, setup->sm_peer_nonce);
    }
}

static void sm_sc_calculate_local_confirm(sm_connection_t * sm_conn){
    uint8_t z = 0;
    if (setup->sm_stk_generation_method != JUST_WORKS && setup->sm_stk_generation_method != NK_BOTH_INPUT){
        // some form of passkey
        uint32_t pk = big_endian_read_32(setup->sm_tk, 12);
        z = 0x80 | ((pk >> setup->sm_passkey_bit) & 1);
        setup->sm_passkey_bit++;
    }
    f4_engine(sm_conn, ec_qx, setup->sm_peer_qx, setup->sm_local_nonce, z);
}

static void sm_sc_calculate_remote_confirm(sm_connection_t * sm_conn){
    uint8_t z = 0;
    if (setup->sm_stk_generation_method != JUST_WORKS && setup->sm_stk_generation_method != NK_BOTH_INPUT){
        // some form of passkey
        uint32_t pk = big_endian_read_32(setup->sm_tk, 12);
        // sm_passkey_bit was increased before sending confirm value
        z = 0x80 | ((pk >> (setup->sm_passkey_bit-1)) & 1);
    }
    f4_engine(sm_conn, setup->sm_peer_qx, ec_qx, setup->sm_peer_nonce, z);
}

static void sm_sc_prepare_dhkey_check(sm_connection_t * sm_conn){
    sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F5_SALT;
}

static void sm_sc_calculate_f6_for_dhkey_check(sm_connection_t * sm_conn){
    // calculate DHKCheck
    sm_key56_t bd_addr_master, bd_addr_slave;
    bd_addr_master[0] =  setup->sm_m_addr_type;
    bd_addr_slave[0]  =  setup->sm_s_addr_type;
    memcpy(&bd_addr_master[1], setup->sm_m_address, 6);
    memcpy(&bd_addr_slave[1],  setup->sm_s_address, 6);
    uint8_t iocap_a[3];
    iocap_a[0] = sm_pairing_packet_get_auth_req(setup->sm_m_preq);
    iocap_a[1] = sm_pairing_packet_get_oob_data_flag(setup->sm_m_preq);
    iocap_a[2] = sm_pairing_packet_get_io_capability(setup->sm_m_preq);
    uint8_t iocap_b[3];
    iocap_b[0] = sm_pairing_packet_get_auth_req(setup->sm_s_pres);
    iocap_b[1] = sm_pairing_packet_get_oob_data_flag(setup->sm_s_pres);
    iocap_b[2] = sm_pairing_packet_get_io_capability(setup->sm_s_pres);
    if (sm_conn->sm_role){
        // responder
        f6_engine(sm_conn, setup->sm_mackey, setup->sm_local_nonce, setup->sm_peer_nonce, setup->sm_ra, iocap_b, bd_addr_slave, bd_addr_master);
    } else {
        // initiator
        f6_engine(sm_conn, setup->sm_mackey, setup->sm_local_nonce, setup->sm_peer_nonce, setup->sm_rb, iocap_a, bd_addr_master, bd_addr_slave);
    }
}

static void sm_sc_calculate_f6_to_verify_dhkey_check(sm_connection_t * sm_conn){
    // validate E = f6()
    sm_key56_t bd_addr_master, bd_addr_slave;
    bd_addr_master[0] =  setup->sm_m_addr_type;
    bd_addr_slave[0]  =  setup->sm_s_addr_type;
    memcpy(&bd_addr_master[1], setup->sm_m_address, 6);
    memcpy(&bd_addr_slave[1],  setup->sm_s_address, 6);

    uint8_t iocap_a[3];
    iocap_a[0] = sm_pairing_packet_get_auth_req(setup->sm_m_preq);
    iocap_a[1] = sm_pairing_packet_get_oob_data_flag(setup->sm_m_preq);
    iocap_a[2] = sm_pairing_packet_get_io_capability(setup->sm_m_preq);
    uint8_t iocap_b[3];
    iocap_b[0] = sm_pairing_packet_get_auth_req(setup->sm_s_pres);
    iocap_b[1] = sm_pairing_packet_get_oob_data_flag(setup->sm_s_pres);
    iocap_b[2] = sm_pairing_packet_get_io_capability(setup->sm_s_pres);
    if (sm_conn->sm_role){
        // responder
        f6_engine(sm_conn, setup->sm_mackey, setup->sm_peer_nonce, setup->sm_local_nonce, setup->sm_rb, iocap_a, bd_addr_master, bd_addr_slave);
    } else {
        // initiator
        f6_engine(sm_conn, setup->sm_mackey, setup->sm_peer_nonce, setup->sm_local_nonce, setup->sm_ra, iocap_b, bd_addr_slave, bd_addr_master);
    }
}


//
// Link Key Conversion Function h6
//
// h6(W, keyID) = AES-CMACW(keyID)
// - W is 128 bits
// - keyID is 32 bits
static void h6_engine(sm_connection_t * sm_conn, const sm_key_t w, const uint32_t key_id){
    const uint16_t message_len = 4;
    sm_cmac_connection = sm_conn;
    big_endian_store_32(sm_cmac_sc_buffer, 0, key_id);
    log_info("h6 key");
    log_info_hexdump(w, 16);
    log_info("h6 message");
    log_info_hexdump(sm_cmac_sc_buffer, message_len);
    sm_cmac_general_start(w, message_len, &sm_sc_cmac_get_byte, &sm_sc_cmac_done);
}

// For SC, setup->sm_local_ltk holds full LTK (sm_ltk is already truncated)
// Errata Service Release to the Bluetooth Specification: ESR09
//   E6405 – Cross transport key derivation from a key of size less than 128 bits
//   "Note: When the BR/EDR link key is being derived from the LTK, the derivation is done before the LTK gets masked."
static void h6_calculate_ilk(sm_connection_t * sm_conn){
    h6_engine(sm_conn, setup->sm_local_ltk, 0x746D7031);    // "tmp1"
}

static void h6_calculate_br_edr_link_key(sm_connection_t * sm_conn){
    h6_engine(sm_conn, setup->sm_t, 0x6c656272);    // "lebr"
}

#endif

// key management legacy connections:
// - potentially two different LTKs based on direction. each device stores LTK provided by peer
// - master stores LTK, EDIV, RAND. responder optionally stored master LTK (only if it needs to reconnect)
// - initiators reconnects: initiator uses stored LTK, EDIV, RAND generated by responder
// - responder  reconnects: responder uses LTK receveived from master

// key management secure connections:
// - both devices store same LTK from ECDH key exchange. 

static void sm_load_security_info(sm_connection_t * sm_connection){
    int encryption_key_size;
    int authenticated;
    int authorized;

    // fetch data from device db - incl. authenticated/authorized/key size. Note all sm_connection_X require encryption enabled
    le_device_db_encryption_get(sm_connection->sm_le_db_index, &setup->sm_peer_ediv, setup->sm_peer_rand, setup->sm_peer_ltk,
                                &encryption_key_size, &authenticated, &authorized);
    log_info("db index %u, key size %u, authenticated %u, authorized %u", sm_connection->sm_le_db_index, encryption_key_size, authenticated, authorized);
    sm_connection->sm_actual_encryption_key_size = encryption_key_size;
    sm_connection->sm_connection_authenticated = authenticated;
    sm_connection->sm_connection_authorization_state = authorized ? AUTHORIZATION_GRANTED : AUTHORIZATION_UNKNOWN; 
}

static void sm_start_calculating_ltk_from_ediv_and_rand(sm_connection_t * sm_connection){
    memcpy(setup->sm_local_rand, sm_connection->sm_local_rand, 8);
    setup->sm_local_ediv = sm_connection->sm_local_ediv;
    // re-establish used key encryption size
    // no db for encryption size hack: encryption size is stored in lowest nibble of setup->sm_local_rand
    sm_connection->sm_actual_encryption_key_size = (setup->sm_local_rand[7] & 0x0f) + 1;
    // no db for authenticated flag hack: flag is stored in bit 4 of LSB
    sm_connection->sm_connection_authenticated = (setup->sm_local_rand[7] & 0x10) >> 4;
    log_info("sm: received ltk request with key size %u, authenticated %u",
            sm_connection->sm_actual_encryption_key_size, sm_connection->sm_connection_authenticated);
    sm_connection->sm_engine_state = SM_RESPONDER_PH4_Y_GET_ENC;
}

static void sm_run(void){

    btstack_linked_list_iterator_t it;    

    // assert that we can send at least commands
    if (!hci_can_send_command_packet_now()) return;

    //
    // non-connection related behaviour
    //

    // distributed key generation
    switch (dkg_state){
        case DKG_CALC_IRK:
            // already busy?
            if (sm_aes128_state == SM_AES128_IDLE) {
                // IRK = d1(IR, 1, 0)
                sm_key_t d1_prime;
                sm_d1_d_prime(1, 0, d1_prime);  // plaintext
                dkg_next_state();
                sm_aes128_start(sm_persistent_ir, d1_prime, NULL);
                return;
            }
            break;
        case DKG_CALC_DHK:
            // already busy?
            if (sm_aes128_state == SM_AES128_IDLE) {
                // DHK = d1(IR, 3, 0)
                sm_key_t d1_prime;
                sm_d1_d_prime(3, 0, d1_prime);  // plaintext
                dkg_next_state();
                sm_aes128_start(sm_persistent_ir, d1_prime, NULL);
                return;
            }
            break;
        default:
            break;  
    }

#ifdef USE_MBEDTLS_FOR_ECDH
    if (ec_key_generation_state == EC_KEY_GENERATION_ACTIVE){
        sm_random_start(NULL);
        return; 
    }
#endif

    // random address updates
    switch (rau_state){
        case RAU_GET_RANDOM:
            rau_next_state();
            sm_random_start(NULL);
            return;
        case RAU_GET_ENC:
            // already busy?
            if (sm_aes128_state == SM_AES128_IDLE) {
                sm_key_t r_prime;
                sm_ah_r_prime(sm_random_address, r_prime);
                rau_next_state();
                sm_aes128_start(sm_persistent_irk, r_prime, NULL);
                return;
            }
            break;
        case RAU_SET_ADDRESS:
            log_info("New random address: %s", bd_addr_to_str(sm_random_address));
            rau_state = RAU_IDLE;
            hci_send_cmd(&hci_le_set_random_address, sm_random_address);
            return;
        default:
            break;
    }

    // CMAC
    switch (sm_cmac_state){
        case CMAC_CALC_SUBKEYS:
        case CMAC_CALC_MI:
        case CMAC_CALC_MLAST:
            // already busy?
            if (sm_aes128_state == SM_AES128_ACTIVE) break;
            sm_cmac_handle_aes_engine_ready();
            return;
        default:
            break;
    }

    // CSRK Lookup
    // -- if csrk lookup ready, find connection that require csrk lookup
    if (sm_address_resolution_idle()){
        hci_connections_get_iterator(&it);
        while(btstack_linked_list_iterator_has_next(&it)){
            hci_connection_t * hci_connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
            sm_connection_t  * sm_connection  = &hci_connection->sm_connection;
            if (sm_connection->sm_irk_lookup_state == IRK_LOOKUP_W4_READY){
                // and start lookup
                sm_address_resolution_start_lookup(sm_connection->sm_peer_addr_type, sm_connection->sm_handle, sm_connection->sm_peer_address, ADDRESS_RESOLUTION_FOR_CONNECTION, sm_connection);
                sm_connection->sm_irk_lookup_state = IRK_LOOKUP_STARTED;
                break;
            }
        }
    }

    // -- if csrk lookup ready, resolved addresses for received addresses
    if (sm_address_resolution_idle()) {
        if (!btstack_linked_list_empty(&sm_address_resolution_general_queue)){
            sm_lookup_entry_t * entry = (sm_lookup_entry_t *) sm_address_resolution_general_queue;
            btstack_linked_list_remove(&sm_address_resolution_general_queue, (btstack_linked_item_t *) entry);
            sm_address_resolution_start_lookup(entry->address_type, 0, entry->address, ADDRESS_RESOLUTION_GENERAL, NULL);
            btstack_memory_sm_lookup_entry_free(entry);
        }
    }

    // -- Continue with CSRK device lookup by public or resolvable private address
    if (!sm_address_resolution_idle()){
        log_info("LE Device Lookup: device %u/%u", sm_address_resolution_test, le_device_db_count());
        while (sm_address_resolution_test < le_device_db_count()){
            int addr_type;
            bd_addr_t addr;
            sm_key_t irk;
            le_device_db_info(sm_address_resolution_test, &addr_type, addr, irk);
            log_info("device type %u, addr: %s", addr_type, bd_addr_to_str(addr));

            if (sm_address_resolution_addr_type == addr_type && memcmp(addr, sm_address_resolution_address, 6) == 0){
                log_info("LE Device Lookup: found CSRK by { addr_type, address} ");
                sm_address_resolution_handle_event(ADDRESS_RESOLUTION_SUCEEDED);
                break;
            }

            if (sm_address_resolution_addr_type == 0){
                sm_address_resolution_test++;
                continue;
            }

            if (sm_aes128_state == SM_AES128_ACTIVE) break;

            log_info("LE Device Lookup: calculate AH");
            log_info_key("IRK", irk);

            sm_key_t r_prime;
            sm_ah_r_prime(sm_address_resolution_address, r_prime);
            sm_address_resolution_ah_calculation_active = 1;
            sm_aes128_start(irk, r_prime, sm_address_resolution_context);   // keep context
            return;
        }

        if (sm_address_resolution_test >= le_device_db_count()){
            log_info("LE Device Lookup: not found");
            sm_address_resolution_handle_event(ADDRESS_RESOLUTION_FAILED);
        }
    }


    // 
    // active connection handling
    // -- use loop to handle next connection if lock on setup context is released 

    while (1) {

        // Find connections that requires setup context and make active if no other is locked
        hci_connections_get_iterator(&it);
        while(!sm_active_connection && btstack_linked_list_iterator_has_next(&it)){
            hci_connection_t * hci_connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
            sm_connection_t  * sm_connection = &hci_connection->sm_connection;
            // - if no connection locked and we're ready/waiting for setup context, fetch it and start
            int done = 1;
            int err;
            switch (sm_connection->sm_engine_state) {
                case SM_RESPONDER_SEND_SECURITY_REQUEST:
                    // send packet if possible,
                    if (l2cap_can_send_fixed_channel_packet_now(sm_connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL)){
                        const uint8_t buffer[2] = { SM_CODE_SECURITY_REQUEST, SM_AUTHREQ_BONDING};
                        sm_connection->sm_engine_state = SM_RESPONDER_PH1_W4_PAIRING_REQUEST;            
                        l2cap_send_connectionless(sm_connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    } else {
                        l2cap_request_can_send_fix_channel_now_event(sm_connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
                    }
                    // don't lock sxetup context yet
                    done = 0;
                    break;
                case SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED:
                    sm_reset_setup();
                    sm_init_setup(sm_connection);
                    // recover pairing request
                    memcpy(&setup->sm_m_preq, &sm_connection->sm_m_preq, sizeof(sm_pairing_packet_t));
                    err = sm_stk_generation_init(sm_connection);
                    if (err){
                        setup->sm_pairing_failed_reason = err;
                        sm_connection->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
                        break;
                    }
                    sm_timeout_start(sm_connection);
                    // generate random number first, if we need to show passkey
                    if (setup->sm_stk_generation_method == PK_INIT_INPUT){
                        sm_connection->sm_engine_state = SM_PH2_GET_RANDOM_TK;
                        break;
                    }
                    sm_connection->sm_engine_state = SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE;
                    break;
                case SM_INITIATOR_PH0_HAS_LTK:
                    sm_reset_setup();
                    sm_load_security_info(sm_connection);
                    sm_connection->sm_engine_state = SM_INITIATOR_PH0_SEND_START_ENCRYPTION;
                    break;
                case SM_RESPONDER_PH0_RECEIVED_LTK_REQUEST:
#ifdef ENABLE_LE_SECURE_CONNECTIONS
                    switch (sm_connection->sm_irk_lookup_state){
                        case IRK_LOOKUP_SUCCEEDED:
                            // assuming Secure Connection, we have a stored LTK and the EDIV/RAND are null
                            sm_reset_setup();
                            sm_load_security_info(sm_connection);
                            if (setup->sm_peer_ediv == 0 && sm_is_null_random(setup->sm_peer_rand) && !sm_is_null_key(setup->sm_peer_ltk)){
                                memcpy(setup->sm_ltk, setup->sm_peer_ltk, 16);
                                sm_connection->sm_engine_state = SM_RESPONDER_PH4_SEND_LTK_REPLY;
                                break;
                            }
                            log_info("LTK Request: ediv & random are empty, but no stored LTK (IRK Lookup Succeeded)");
                            sm_connection->sm_engine_state = SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
                            // don't lock setup context yet
                            done = 0;
                            break;
                        case IRK_LOOKUP_FAILED:
                            log_info("LTK Request: ediv & random are empty, but no stored LTK (IRK Lookup Failed)");
                            sm_connection->sm_engine_state = SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
                            // don't lock setup context yet
                            done = 0;
                            break;
                        default:
                            // just wait until IRK lookup is completed
                            // don't lock setup context yet
                            done = 0;
                            break;
                    }
#endif
                    break;
                case SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST:
                    sm_reset_setup();
                    sm_init_setup(sm_connection);
                    sm_timeout_start(sm_connection);
                    sm_connection->sm_engine_state = SM_INITIATOR_PH1_SEND_PAIRING_REQUEST;
                    break;
                default:
                    done = 0;
                    break;
            }
            if (done){
                sm_active_connection = sm_connection->sm_handle;
                log_info("sm: connection 0x%04x locked setup context as %s", sm_active_connection, sm_connection->sm_role ? "responder" : "initiator");
            }
        }

        // 
        // active connection handling
        // 

        if (sm_active_connection == 0) return;

        // assert that we could send a SM PDU - not needed for all of the following
        if (!l2cap_can_send_fixed_channel_packet_now(sm_active_connection, L2CAP_CID_SECURITY_MANAGER_PROTOCOL)) {
            l2cap_request_can_send_fix_channel_now_event(sm_active_connection, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
            return;
        }

        sm_connection_t * connection = sm_get_connection_for_handle(sm_active_connection);
        if (!connection) return;

        // send keypress notifications
        if (setup->sm_keypress_notification != 0xff){
            uint8_t buffer[2];
            buffer[0] = SM_CODE_KEYPRESS_NOTIFICATION;
            buffer[1] = setup->sm_keypress_notification;
            setup->sm_keypress_notification = 0xff;
            l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            return;
        }

        sm_key_t plaintext;
        int key_distribution_flags;

        log_info("sm_run: state %u", connection->sm_engine_state);

        // responding state
        switch (connection->sm_engine_state){

            // general
            case SM_GENERAL_SEND_PAIRING_FAILED: {
                uint8_t buffer[2];
                buffer[0] = SM_CODE_PAIRING_FAILED;
                buffer[1] = setup->sm_pairing_failed_reason;
                connection->sm_engine_state = connection->sm_role ? SM_RESPONDER_IDLE : SM_INITIATOR_CONNECTED;
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_done_for_handle(connection->sm_handle);
                break;
            }

#ifdef ENABLE_LE_SECURE_CONNECTIONS
            case SM_SC_W2_GET_RANDOM_A:
                sm_random_start(connection);
                connection->sm_engine_state = SM_SC_W4_GET_RANDOM_A;
                break;
            case SM_SC_W2_GET_RANDOM_B:
                sm_random_start(connection);
                connection->sm_engine_state = SM_SC_W4_GET_RANDOM_B;
                break;
            case SM_SC_W2_CMAC_FOR_CONFIRMATION:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CMAC_FOR_CONFIRMATION;
                sm_sc_calculate_local_confirm(connection);
                break;
            case SM_SC_W2_CMAC_FOR_CHECK_CONFIRMATION:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CMAC_FOR_CHECK_CONFIRMATION;
                sm_sc_calculate_remote_confirm(connection);
                break;
            case SM_SC_W2_CALCULATE_F6_FOR_DHKEY_CHECK:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_F6_FOR_DHKEY_CHECK;
                sm_sc_calculate_f6_for_dhkey_check(connection);
                break;
            case SM_SC_W2_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK;
                sm_sc_calculate_f6_to_verify_dhkey_check(connection);
                break;
            case SM_SC_W2_CALCULATE_F5_SALT:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_F5_SALT;
                f5_calculate_salt(connection);
                break;
            case SM_SC_W2_CALCULATE_F5_MACKEY:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_F5_MACKEY;
                f5_calculate_mackey(connection);
                break;
            case SM_SC_W2_CALCULATE_F5_LTK:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_F5_LTK;
                f5_calculate_ltk(connection);
                break;
            case SM_SC_W2_CALCULATE_G2:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_G2;
                g2_calculate(connection);
                break;
            case SM_SC_W2_CALCULATE_H6_ILK:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_H6_ILK;
                h6_calculate_ilk(connection);
                break;
            case SM_SC_W2_CALCULATE_H6_BR_EDR_LINK_KEY:
                if (!sm_cmac_ready()) break;
                connection->sm_engine_state = SM_SC_W4_CALCULATE_H6_BR_EDR_LINK_KEY;
                h6_calculate_br_edr_link_key(connection);
                break;

#endif
            // initiator side
            case SM_INITIATOR_PH0_SEND_START_ENCRYPTION: {
                sm_key_t peer_ltk_flipped;
                reverse_128(setup->sm_peer_ltk, peer_ltk_flipped);
                connection->sm_engine_state = SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED;
                log_info("sm: hci_le_start_encryption ediv 0x%04x", setup->sm_peer_ediv);
                uint32_t rand_high = big_endian_read_32(setup->sm_peer_rand, 0);
                uint32_t rand_low  = big_endian_read_32(setup->sm_peer_rand, 4);
                hci_send_cmd(&hci_le_start_encryption, connection->sm_handle,rand_low, rand_high, setup->sm_peer_ediv, peer_ltk_flipped);
                return;
            }

            case SM_INITIATOR_PH1_SEND_PAIRING_REQUEST:
                sm_pairing_packet_set_code(setup->sm_m_preq, SM_CODE_PAIRING_REQUEST);
                connection->sm_engine_state = SM_INITIATOR_PH1_W4_PAIRING_RESPONSE;
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) &setup->sm_m_preq, sizeof(sm_pairing_packet_t));
                sm_timeout_reset(connection);
                break;

            // responder side
            case SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY:
                connection->sm_engine_state = SM_RESPONDER_IDLE;
                hci_send_cmd(&hci_le_long_term_key_negative_reply, connection->sm_handle);
                sm_done_for_handle(connection->sm_handle);
                return;

#ifdef ENABLE_LE_SECURE_CONNECTIONS
            case SM_SC_SEND_PUBLIC_KEY_COMMAND: {
                uint8_t buffer[65];
                buffer[0] = SM_CODE_PAIRING_PUBLIC_KEY;
                //
                reverse_256(ec_qx, &buffer[1]);
                reverse_256(ec_qy, &buffer[33]);

                // stk generation method
                // passkey entry: notify app to show passkey or to request passkey
                switch (setup->sm_stk_generation_method){
                    case JUST_WORKS:
                    case NK_BOTH_INPUT:
                        if (connection->sm_role){
                            // responder
                            sm_sc_start_calculating_local_confirm(connection);
                        } else {
                            // initiator
                            connection->sm_engine_state = SM_SC_W4_PUBLIC_KEY_COMMAND;
                        }
                        break;
                    case PK_INIT_INPUT:
                    case PK_RESP_INPUT:
                    case OK_BOTH_INPUT:
                        // use random TK for display
                        memcpy(setup->sm_ra, setup->sm_tk, 16);
                        memcpy(setup->sm_rb, setup->sm_tk, 16);
                        setup->sm_passkey_bit = 0;

                        if (connection->sm_role){
                            // responder
                            connection->sm_engine_state = SM_SC_W4_CONFIRMATION;
                        } else {
                            // initiator
                            connection->sm_engine_state = SM_SC_W4_PUBLIC_KEY_COMMAND;
                        }
                        sm_trigger_user_response(connection);
                        break;
                    case OOB:
                        // TODO: implement SC OOB
                        break;
                } 

                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);
                break;
            }
            case SM_SC_SEND_CONFIRMATION: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_CONFIRM;
                reverse_128(setup->sm_local_confirm, &buffer[1]);
                if (connection->sm_role){
                    connection->sm_engine_state = SM_SC_W4_PAIRING_RANDOM;
                } else {
                    connection->sm_engine_state = SM_SC_W4_CONFIRMATION;
                }
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);
                break;
            }
            case SM_SC_SEND_PAIRING_RANDOM: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_RANDOM;
                reverse_128(setup->sm_local_nonce, &buffer[1]);
                if (setup->sm_stk_generation_method != JUST_WORKS && setup->sm_stk_generation_method != NK_BOTH_INPUT && setup->sm_passkey_bit < 20){
                    if (connection->sm_role){
                        // responder
                        connection->sm_engine_state = SM_SC_W4_CONFIRMATION;
                    } else {
                        // initiator
                        connection->sm_engine_state = SM_SC_W4_PAIRING_RANDOM;
                    }                    
                } else {
                    if (connection->sm_role){
                        // responder
                        if (setup->sm_stk_generation_method == NK_BOTH_INPUT){
                            connection->sm_engine_state = SM_SC_W2_CALCULATE_G2;
                        } else {
                            sm_sc_prepare_dhkey_check(connection);
                        }
                    } else {
                        // initiator
                        connection->sm_engine_state = SM_SC_W4_PAIRING_RANDOM;
                    }                    
                }
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);
                break;
            }
            case SM_SC_SEND_DHKEY_CHECK_COMMAND: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_DHKEY_CHECK;
                reverse_128(setup->sm_local_dhkey_check, &buffer[1]);

                if (connection->sm_role){
                    connection->sm_engine_state = SM_SC_W4_LTK_REQUEST_SC;
                } else {
                    connection->sm_engine_state = SM_SC_W4_DHKEY_CHECK_COMMAND;
                }

                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);            
                break;
            }

#endif
            case SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE:
                // echo initiator for now
                sm_pairing_packet_set_code(setup->sm_s_pres,SM_CODE_PAIRING_RESPONSE);
                key_distribution_flags = sm_key_distribution_flags_for_auth_req();

                if (setup->sm_use_secure_connections){
                    connection->sm_engine_state = SM_SC_W4_PUBLIC_KEY_COMMAND;
                    // skip LTK/EDIV for SC
                    log_info("sm: dropping encryption information flag");
                    key_distribution_flags &= ~SM_KEYDIST_ENC_KEY;
                } else {
                    connection->sm_engine_state = SM_RESPONDER_PH1_W4_PAIRING_CONFIRM;
                }

                sm_pairing_packet_set_initiator_key_distribution(setup->sm_s_pres, sm_pairing_packet_get_initiator_key_distribution(setup->sm_m_preq) & key_distribution_flags);
                sm_pairing_packet_set_responder_key_distribution(setup->sm_s_pres, sm_pairing_packet_get_responder_key_distribution(setup->sm_m_preq) & key_distribution_flags);
                // update key distribution after ENC was dropped
                sm_setup_key_distribution(sm_pairing_packet_get_responder_key_distribution(setup->sm_s_pres));

                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) &setup->sm_s_pres, sizeof(sm_pairing_packet_t));
                sm_timeout_reset(connection);
                // SC Numeric Comparison will trigger user response after public keys & nonces have been exchanged
                if (!setup->sm_use_secure_connections || setup->sm_stk_generation_method == JUST_WORKS){
                    sm_trigger_user_response(connection);
                }
                return;

            case SM_PH2_SEND_PAIRING_RANDOM: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_RANDOM;
                reverse_128(setup->sm_local_random, &buffer[1]);
                if (connection->sm_role){
                    connection->sm_engine_state = SM_RESPONDER_PH2_W4_LTK_REQUEST;
                } else {
                    connection->sm_engine_state = SM_INITIATOR_PH2_W4_PAIRING_RANDOM;
                }
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);
                break;
            }

            case SM_PH2_GET_RANDOM_TK:
            case SM_PH2_C1_GET_RANDOM_A:
            case SM_PH2_C1_GET_RANDOM_B:
            case SM_PH3_GET_RANDOM:
            case SM_PH3_GET_DIV:
                sm_next_responding_state(connection);
                sm_random_start(connection);
                return;

            case SM_PH2_C1_GET_ENC_B:
            case SM_PH2_C1_GET_ENC_D:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                sm_next_responding_state(connection);
                sm_aes128_start(setup->sm_tk, setup->sm_c1_t3_value, connection);
                return;

            case SM_PH3_LTK_GET_ENC:
            case SM_RESPONDER_PH4_LTK_GET_ENC:
                // already busy?
                if (sm_aes128_state == SM_AES128_IDLE) {
                    sm_key_t d_prime;
                    sm_d1_d_prime(setup->sm_local_div, 0, d_prime);
                    sm_next_responding_state(connection);
                    sm_aes128_start(sm_persistent_er, d_prime, connection);
                    return;
                }
                break;

            case SM_PH3_CSRK_GET_ENC:
                // already busy?
                if (sm_aes128_state == SM_AES128_IDLE) {
                    sm_key_t d_prime;
                    sm_d1_d_prime(setup->sm_local_div, 1, d_prime);
                    sm_next_responding_state(connection);
                    sm_aes128_start(sm_persistent_er, d_prime, connection);
                    return;
                }
                break;

            case SM_PH2_C1_GET_ENC_C:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                // calculate m_confirm using aes128 engine - step 1
                sm_c1_t1(setup->sm_peer_random, (uint8_t*) &setup->sm_m_preq, (uint8_t*) &setup->sm_s_pres, setup->sm_m_addr_type, setup->sm_s_addr_type, plaintext);
                sm_next_responding_state(connection);
                sm_aes128_start(setup->sm_tk, plaintext, connection);
                break;
            case SM_PH2_C1_GET_ENC_A:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                // calculate confirm using aes128 engine - step 1
                sm_c1_t1(setup->sm_local_random, (uint8_t*) &setup->sm_m_preq, (uint8_t*) &setup->sm_s_pres, setup->sm_m_addr_type, setup->sm_s_addr_type, plaintext);
                sm_next_responding_state(connection);
                sm_aes128_start(setup->sm_tk, plaintext, connection);
                break;
            case SM_PH2_CALC_STK:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                // calculate STK
                if (connection->sm_role){
                    sm_s1_r_prime(setup->sm_local_random, setup->sm_peer_random, plaintext);
                } else {
                    sm_s1_r_prime(setup->sm_peer_random, setup->sm_local_random, plaintext);
                }
                sm_next_responding_state(connection);
                sm_aes128_start(setup->sm_tk, plaintext, connection);
                break;
            case SM_PH3_Y_GET_ENC:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                // PH3B2 - calculate Y from      - enc
                // Y = dm(DHK, Rand)
                sm_dm_r_prime(setup->sm_local_rand, plaintext);
                sm_next_responding_state(connection);
                sm_aes128_start(sm_persistent_dhk, plaintext, connection);
                return;
            case SM_PH2_C1_SEND_PAIRING_CONFIRM: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_CONFIRM;
                reverse_128(setup->sm_local_confirm, &buffer[1]);
                if (connection->sm_role){
                    connection->sm_engine_state = SM_RESPONDER_PH2_W4_PAIRING_RANDOM;
                } else {
                    connection->sm_engine_state = SM_INITIATOR_PH2_W4_PAIRING_CONFIRM;
                }
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset(connection);
                return;
            }
            case SM_RESPONDER_PH2_SEND_LTK_REPLY: {
                sm_key_t stk_flipped;
                reverse_128(setup->sm_ltk, stk_flipped);
                connection->sm_engine_state = SM_PH2_W4_CONNECTION_ENCRYPTED;
                hci_send_cmd(&hci_le_long_term_key_request_reply, connection->sm_handle, stk_flipped);
                return;
            }
            case SM_INITIATOR_PH3_SEND_START_ENCRYPTION: {
                sm_key_t stk_flipped;
                reverse_128(setup->sm_ltk, stk_flipped);
                connection->sm_engine_state = SM_PH2_W4_CONNECTION_ENCRYPTED;
                hci_send_cmd(&hci_le_start_encryption, connection->sm_handle, 0, 0, 0, stk_flipped);
                return;
            }
            case SM_RESPONDER_PH4_SEND_LTK_REPLY: {
                sm_key_t ltk_flipped;
                reverse_128(setup->sm_ltk, ltk_flipped);
                connection->sm_engine_state = SM_RESPONDER_IDLE;
                hci_send_cmd(&hci_le_long_term_key_request_reply, connection->sm_handle, ltk_flipped);
                return;
            }
            case SM_RESPONDER_PH4_Y_GET_ENC:
                // already busy?
                if (sm_aes128_state == SM_AES128_ACTIVE) break;
                log_info("LTK Request: recalculating with ediv 0x%04x", setup->sm_local_ediv);
                // Y = dm(DHK, Rand)
                sm_dm_r_prime(setup->sm_local_rand, plaintext);
                sm_next_responding_state(connection);
                sm_aes128_start(sm_persistent_dhk, plaintext, connection);
                return;

            case SM_PH3_DISTRIBUTE_KEYS:
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
                    uint8_t buffer[17];
                    buffer[0] = SM_CODE_ENCRYPTION_INFORMATION;
                    reverse_128(setup->sm_ltk, &buffer[1]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_MASTER_IDENTIFICATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                    uint8_t buffer[11];
                    buffer[0] = SM_CODE_MASTER_IDENTIFICATION;
                    little_endian_store_16(buffer, 1, setup->sm_local_ediv);
                    reverse_64(setup->sm_local_rand, &buffer[3]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_INFORMATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                    uint8_t buffer[17];
                    buffer[0] = SM_CODE_IDENTITY_INFORMATION;
                    reverse_128(sm_persistent_irk, &buffer[1]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                    bd_addr_t local_address;
                    uint8_t buffer[8];
                    buffer[0] = SM_CODE_IDENTITY_ADDRESS_INFORMATION;
                    gap_advertisements_get_address(&buffer[1], local_address);
                    reverse_bd_addr(local_address, &buffer[2]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;

                    // hack to reproduce test runs
                    if (test_use_fixed_local_csrk){
                        memset(setup->sm_local_csrk, 0xcc, 16);
                    }

                    uint8_t buffer[17];
                    buffer[0] = SM_CODE_SIGNING_INFORMATION;
                    reverse_128(setup->sm_local_csrk, &buffer[1]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }

                // keys are sent
                if (connection->sm_role){
                    // slave -> receive master keys if any
                    if (sm_key_distribution_all_received(connection)){
                        sm_key_distribution_handle_all_received(connection);
                        connection->sm_engine_state = SM_RESPONDER_IDLE; 
                        sm_done_for_handle(connection->sm_handle);
                    } else {
                        connection->sm_engine_state = SM_PH3_RECEIVE_KEYS;
                    }
                } else {
                    // master -> all done
                    connection->sm_engine_state = SM_INITIATOR_CONNECTED; 
                    sm_done_for_handle(connection->sm_handle);
                }
                break;

            default:
                break;
        }

        // check again if active connection was released
        if (sm_active_connection) break;
    }
}

// note: aes engine is ready as we just got the aes result
static void sm_handle_encryption_result(uint8_t * data){

    sm_aes128_state = SM_AES128_IDLE;

    if (sm_address_resolution_ah_calculation_active){
        sm_address_resolution_ah_calculation_active = 0;
        // compare calulated address against connecting device
        uint8_t hash[3];
        reverse_24(data, hash);
        if (memcmp(&sm_address_resolution_address[3], hash, 3) == 0){
            log_info("LE Device Lookup: matched resolvable private address");
            sm_address_resolution_handle_event(ADDRESS_RESOLUTION_SUCEEDED);
            return;
        }
        // no match, try next
        sm_address_resolution_test++;
        return;
    }

    switch (dkg_state){
        case DKG_W4_IRK:
            reverse_128(data, sm_persistent_irk);
            log_info_key("irk", sm_persistent_irk);
            dkg_next_state();
            return;
        case DKG_W4_DHK:
            reverse_128(data, sm_persistent_dhk);
            log_info_key("dhk", sm_persistent_dhk);
            dkg_next_state();
            // SM Init Finished
            return;
        default:
            break;
    }

    switch (rau_state){
        case RAU_W4_ENC:
            reverse_24(data, &sm_random_address[3]);
            rau_next_state();
            return;
        default:
            break;
    }

    switch (sm_cmac_state){
        case CMAC_W4_SUBKEYS:
        case CMAC_W4_MI:
        case CMAC_W4_MLAST:
            {
            sm_key_t t;
            reverse_128(data, t);
            sm_cmac_handle_encryption_result(t);
            }
            return;
        default:
            break;
    }

    // retrieve sm_connection provided to sm_aes128_start_encryption
    sm_connection_t * connection = (sm_connection_t*) sm_aes128_context;
    if (!connection) return;
    switch (connection->sm_engine_state){
        case SM_PH2_C1_W4_ENC_A:
        case SM_PH2_C1_W4_ENC_C:
            {
            sm_key_t t2;
            reverse_128(data, t2);
            sm_c1_t3(t2, setup->sm_m_address, setup->sm_s_address, setup->sm_c1_t3_value);
            }
            sm_next_responding_state(connection);
            return;
        case SM_PH2_C1_W4_ENC_B:
            reverse_128(data, setup->sm_local_confirm);
            log_info_key("c1!", setup->sm_local_confirm);
            connection->sm_engine_state = SM_PH2_C1_SEND_PAIRING_CONFIRM;
            return;
        case SM_PH2_C1_W4_ENC_D:
            {
            sm_key_t peer_confirm_test;
            reverse_128(data, peer_confirm_test);
            log_info_key("c1!", peer_confirm_test);
            if (memcmp(setup->sm_peer_confirm, peer_confirm_test, 16) != 0){
                setup->sm_pairing_failed_reason = SM_REASON_CONFIRM_VALUE_FAILED;
                connection->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
                return;
            }
            if (connection->sm_role){
                connection->sm_engine_state = SM_PH2_SEND_PAIRING_RANDOM;
            } else {
                connection->sm_engine_state = SM_PH2_CALC_STK;
            }
            }
            return;
        case SM_PH2_W4_STK:
            reverse_128(data, setup->sm_ltk);
            sm_truncate_key(setup->sm_ltk, connection->sm_actual_encryption_key_size);
            log_info_key("stk", setup->sm_ltk);
            if (connection->sm_role){
                connection->sm_engine_state = SM_RESPONDER_PH2_SEND_LTK_REPLY;
            } else {
                connection->sm_engine_state = SM_INITIATOR_PH3_SEND_START_ENCRYPTION;
            }
            return;
        case SM_PH3_Y_W4_ENC:{
            sm_key_t y128;
            reverse_128(data, y128);
            setup->sm_local_y = big_endian_read_16(y128, 14);
            log_info_hex16("y", setup->sm_local_y);
            // PH3B3 - calculate EDIV
            setup->sm_local_ediv = setup->sm_local_y ^ setup->sm_local_div;
            log_info_hex16("ediv", setup->sm_local_ediv);
            // PH3B4 - calculate LTK         - enc
            // LTK = d1(ER, DIV, 0))
            connection->sm_engine_state = SM_PH3_LTK_GET_ENC;
            return;
        }
        case SM_RESPONDER_PH4_Y_W4_ENC:{
            sm_key_t y128;
            reverse_128(data, y128);
            setup->sm_local_y = big_endian_read_16(y128, 14);
            log_info_hex16("y", setup->sm_local_y);

            // PH3B3 - calculate DIV
            setup->sm_local_div = setup->sm_local_y ^ setup->sm_local_ediv;
            log_info_hex16("ediv", setup->sm_local_ediv);
            // PH3B4 - calculate LTK         - enc
            // LTK = d1(ER, DIV, 0))
            connection->sm_engine_state = SM_RESPONDER_PH4_LTK_GET_ENC;
            return;
        }
        case SM_PH3_LTK_W4_ENC:
            reverse_128(data, setup->sm_ltk);
            log_info_key("ltk", setup->sm_ltk);
            // calc CSRK next
            connection->sm_engine_state = SM_PH3_CSRK_GET_ENC;
            return;
        case SM_PH3_CSRK_W4_ENC:
            reverse_128(data, setup->sm_local_csrk);
            log_info_key("csrk", setup->sm_local_csrk);
            if (setup->sm_key_distribution_send_set){
                connection->sm_engine_state = SM_PH3_DISTRIBUTE_KEYS;
            } else {
                // no keys to send, just continue
                if (connection->sm_role){
                    // slave -> receive master keys
                    connection->sm_engine_state = SM_PH3_RECEIVE_KEYS;
                } else {
                    if (setup->sm_use_secure_connections && (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION)){
                        connection->sm_engine_state = SM_SC_W2_CALCULATE_H6_ILK;
                    } else {
                        // master -> all done
                        connection->sm_engine_state = SM_INITIATOR_CONNECTED; 
                        sm_done_for_handle(connection->sm_handle);
                    }
                }                
            }
            return;                                
        case SM_RESPONDER_PH4_LTK_W4_ENC:
            reverse_128(data, setup->sm_ltk);
            sm_truncate_key(setup->sm_ltk, connection->sm_actual_encryption_key_size);
            log_info_key("ltk", setup->sm_ltk);
            connection->sm_engine_state = SM_RESPONDER_PH4_SEND_LTK_REPLY;
            return;                                
        default:
            break;
    }
}

#ifdef USE_MBEDTLS_FOR_ECDH

static int sm_generate_f_rng(void * context, unsigned char * buffer, size_t size){
    int offset = setup->sm_passkey_bit;
    log_info("sm_generate_f_rng: size %u - offset %u", (int) size, offset);
    while (size) {
        if (offset < 32){
            *buffer++ = setup->sm_peer_qx[offset++];
        } else {
            *buffer++ = setup->sm_peer_qx[offset++ - 32];
        }
        size--;
    }
    setup->sm_passkey_bit = offset;
    return 0;
}
#endif

// note: random generator is ready. this doesn NOT imply that aes engine is unused!
static void sm_handle_random_result(uint8_t * data){

#ifdef USE_MBEDTLS_FOR_ECDH
    if (ec_key_generation_state == EC_KEY_GENERATION_ACTIVE){
        int num_bytes = setup->sm_passkey_bit;
        if (num_bytes < 32){
            memcpy(&setup->sm_peer_qx[num_bytes], data, 8);
        } else {
            memcpy(&setup->sm_peer_qx[num_bytes-32], data, 8);
        }
        num_bytes += 8;
        setup->sm_passkey_bit = num_bytes;

        if (num_bytes >= 64){

            // generate EC key
            setup->sm_passkey_bit = 0;
            mbedtls_mpi d;
            mbedtls_ecp_point P;
            mbedtls_mpi_init(&d);
            mbedtls_ecp_point_init(&P);
            int res = mbedtls_ecp_gen_keypair(&mbedtls_ec_group, &d, &P, &sm_generate_f_rng, NULL);
            log_info("gen keypair %x", res);
            mbedtls_mpi_write_binary(&P.X, ec_qx, 32);
            mbedtls_mpi_write_binary(&P.Y, ec_qy, 32);
            mbedtls_mpi_write_binary(&d, ec_d, 32);
            mbedtls_ecp_point_free(&P);
            mbedtls_mpi_free(&d);
            ec_key_generation_state = EC_KEY_GENERATION_DONE;
            sm_log_ec_keypair();

#if 0
            int i;
            sm_key256_t dhkey;
            for (i=0;i<10;i++){
                // printf("test dhkey check\n");
                memcpy(setup->sm_peer_qx, ec_qx, 32);
                memcpy(setup->sm_peer_qy, ec_qy, 32);
                sm_sc_calculate_dhkey(dhkey);
                // printf("test dhkey check end\n");
            }
#endif

        }
    }
#endif

    switch (rau_state){
        case RAU_W4_RANDOM:
            // non-resolvable vs. resolvable
            switch (gap_random_adress_type){
                case GAP_RANDOM_ADDRESS_RESOLVABLE:
                    // resolvable: use random as prand and calc address hash
                    // "The two most significant bits of prand shall be equal to ‘0’ and ‘1"
                    memcpy(sm_random_address, data, 3);
                    sm_random_address[0] &= 0x3f;
                    sm_random_address[0] |= 0x40;
                    rau_state = RAU_GET_ENC;
                    break;
                case GAP_RANDOM_ADDRESS_NON_RESOLVABLE:
                default:
                    // "The two most significant bits of the address shall be equal to ‘0’""
                    memcpy(sm_random_address, data, 6);
                    sm_random_address[0] &= 0x3f; 
                    rau_state = RAU_SET_ADDRESS;
                    break;
            }
            return;
        default:
            break;
    }

    // retrieve sm_connection provided to sm_random_start
    sm_connection_t * connection = (sm_connection_t *) sm_random_context;
    if (!connection) return;
    switch (connection->sm_engine_state){
#ifdef ENABLE_LE_SECURE_CONNECTIONS
        case SM_SC_W4_GET_RANDOM_A:
            memcpy(&setup->sm_local_nonce[0], data, 8);
            connection->sm_engine_state = SM_SC_W2_GET_RANDOM_B;
            break;
        case SM_SC_W4_GET_RANDOM_B:
            memcpy(&setup->sm_local_nonce[8], data, 8);
            // initiator & jw/nc -> send pairing random
            if (connection->sm_role == 0 && sm_just_works_or_numeric_comparison(setup->sm_stk_generation_method)){
                connection->sm_engine_state = SM_SC_SEND_PAIRING_RANDOM;
                break;
            } else {
                connection->sm_engine_state = SM_SC_W2_CMAC_FOR_CONFIRMATION;
            }
            break;
#endif

        case SM_PH2_W4_RANDOM_TK:
        {
            // map random to 0-999999 without speding much cycles on a modulus operation
            uint32_t tk = little_endian_read_32(data,0);
            tk = tk & 0xfffff;  // 1048575
            if (tk >= 999999){
                tk = tk - 999999;
            } 
            sm_reset_tk();
            big_endian_store_32(setup->sm_tk, 12, tk);
            if (connection->sm_role){
                connection->sm_engine_state = SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE;
            } else {
                if (setup->sm_use_secure_connections){
                    connection->sm_engine_state = SM_SC_SEND_PUBLIC_KEY_COMMAND;
                } else {
                    connection->sm_engine_state = SM_PH1_W4_USER_RESPONSE;
                    sm_trigger_user_response(connection);
                    // response_idle == nothing <--> sm_trigger_user_response() did not require response
                    if (setup->sm_user_response == SM_USER_RESPONSE_IDLE){
                        connection->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
                    }
                }
            }
            return;
        }
        case SM_PH2_C1_W4_RANDOM_A:
            memcpy(&setup->sm_local_random[0], data, 8); // random endinaness
            connection->sm_engine_state = SM_PH2_C1_GET_RANDOM_B;
            return;
        case SM_PH2_C1_W4_RANDOM_B:
            memcpy(&setup->sm_local_random[8], data, 8); // random endinaness
            connection->sm_engine_state = SM_PH2_C1_GET_ENC_A;
            return;
        case SM_PH3_W4_RANDOM:
            reverse_64(data, setup->sm_local_rand);
            // no db for encryption size hack: encryption size is stored in lowest nibble of setup->sm_local_rand
            setup->sm_local_rand[7] = (setup->sm_local_rand[7] & 0xf0) + (connection->sm_actual_encryption_key_size - 1);
            // no db for authenticated flag hack: store flag in bit 4 of LSB
            setup->sm_local_rand[7] = (setup->sm_local_rand[7] & 0xef) + (connection->sm_connection_authenticated << 4);
            connection->sm_engine_state = SM_PH3_GET_DIV;
            return;
        case SM_PH3_W4_DIV:
            // use 16 bit from random value as div
            setup->sm_local_div = big_endian_read_16(data, 0);
            log_info_hex16("div", setup->sm_local_div);
            connection->sm_engine_state = SM_PH3_Y_GET_ENC;
            return;
        default:
            break;
    }
}

static void sm_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    sm_connection_t  * sm_conn;
    hci_con_handle_t con_handle;

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        log_info("HCI Working!");

                        // set local addr for le device db
                        bd_addr_t local_bd_addr;
                        gap_local_bd_addr(local_bd_addr);
                        le_device_db_set_local_bd_addr(local_bd_addr);

                        dkg_state = sm_persistent_irk_ready ? DKG_CALC_DHK : DKG_CALC_IRK;
                        rau_state = RAU_IDLE;
#ifdef USE_MBEDTLS_FOR_ECDH
                        if (!sm_have_ec_keypair){
                            setup->sm_passkey_bit = 0;
                            ec_key_generation_state = EC_KEY_GENERATION_ACTIVE;
                        }
#endif
                        sm_run();
					}
					break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:

                            log_info("sm: connected");

                            if (packet[3]) return; // connection failed

                            con_handle = little_endian_read_16(packet, 4);
                            sm_conn = sm_get_connection_for_handle(con_handle);
                            if (!sm_conn) break;

                            sm_conn->sm_handle = con_handle;
                            sm_conn->sm_role = packet[6];
                            sm_conn->sm_peer_addr_type = packet[7];
                            reverse_bd_addr(&packet[8],
                                            sm_conn->sm_peer_address);

                            log_info("New sm_conn, role %s", sm_conn->sm_role ? "slave" : "master");

                            // reset security properties
                            sm_conn->sm_connection_encrypted = 0;
                            sm_conn->sm_connection_authenticated = 0;
                            sm_conn->sm_connection_authorization_state = AUTHORIZATION_UNKNOWN;
                            sm_conn->sm_le_db_index = -1;

                            // prepare CSRK lookup (does not involve setup)
                            sm_conn->sm_irk_lookup_state = IRK_LOOKUP_W4_READY;

                            // just connected -> everything else happens in sm_run()
                            if (sm_conn->sm_role){
                                // slave - state already could be SM_RESPONDER_SEND_SECURITY_REQUEST instead
                                if (sm_conn->sm_engine_state == SM_GENERAL_IDLE){
                                    if (sm_slave_request_security) {
                                        // request security if requested by app
                                        sm_conn->sm_engine_state = SM_RESPONDER_SEND_SECURITY_REQUEST;
                                    } else {
                                        // otherwise, wait for pairing request 
                                        sm_conn->sm_engine_state = SM_RESPONDER_IDLE;
                                    }
                                }
                                break;
                            } else {
                                // master
                                sm_conn->sm_engine_state = SM_INITIATOR_CONNECTED;
                            }
                            break;

                        case HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST:
                            con_handle = little_endian_read_16(packet, 3);
                            sm_conn = sm_get_connection_for_handle(con_handle);
                            if (!sm_conn) break;

                            log_info("LTK Request: state %u", sm_conn->sm_engine_state);
                            if (sm_conn->sm_engine_state == SM_RESPONDER_PH2_W4_LTK_REQUEST){
                                sm_conn->sm_engine_state = SM_PH2_CALC_STK;
                                break;
                            }
                            if (sm_conn->sm_engine_state == SM_SC_W4_LTK_REQUEST_SC){
                                // PH2 SEND LTK as we need to exchange keys in PH3
                                sm_conn->sm_engine_state = SM_RESPONDER_PH2_SEND_LTK_REPLY;
                                break;
                            }

                            // store rand and ediv
                            reverse_64(&packet[5], sm_conn->sm_local_rand);
                            sm_conn->sm_local_ediv   = little_endian_read_16(packet, 13);

                            // For Legacy Pairing (<=> EDIV != 0 || RAND != NULL), we need to recalculated our LTK as a
                            // potentially stored LTK is from the master
                            if (sm_conn->sm_local_ediv != 0 || !sm_is_null_random(sm_conn->sm_local_rand)){
                                sm_start_calculating_ltk_from_ediv_and_rand(sm_conn);
                                break;
                            }

#ifdef ENABLE_LE_SECURE_CONNECTIONS
                            sm_conn->sm_engine_state = SM_RESPONDER_PH0_RECEIVED_LTK_REQUEST;
#else
                            log_info("LTK Request: ediv & random are empty, but LE Secure Connections not supported");
                            sm_conn->sm_engine_state = SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
#endif
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    con_handle = little_endian_read_16(packet, 3);
                    sm_conn = sm_get_connection_for_handle(con_handle);
                    if (!sm_conn) break;

                    sm_conn->sm_connection_encrypted = packet[5];
                    log_info("Encryption state change: %u, key size %u", sm_conn->sm_connection_encrypted,
                        sm_conn->sm_actual_encryption_key_size);
                    log_info("event handler, state %u", sm_conn->sm_engine_state);
                    if (!sm_conn->sm_connection_encrypted) break;
                    // continue if part of initial pairing
                    switch (sm_conn->sm_engine_state){
                        case SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED:
                            sm_conn->sm_engine_state = SM_INITIATOR_CONNECTED; 
                            sm_done_for_handle(sm_conn->sm_handle);
                            break;                        
                        case SM_PH2_W4_CONNECTION_ENCRYPTED:
                            if (sm_conn->sm_role){
                                // slave
                                if (setup->sm_use_secure_connections){
                                    sm_conn->sm_engine_state = SM_PH3_DISTRIBUTE_KEYS;
                                } else {
                                    sm_conn->sm_engine_state = SM_PH3_GET_RANDOM;
                                }
                            } else {
                                // master
                                if (sm_key_distribution_all_received(sm_conn)){
                                    // skip receiving keys as there are none
                                    sm_key_distribution_handle_all_received(sm_conn);
                                    sm_conn->sm_engine_state = SM_PH3_GET_RANDOM; 
                                } else {
                                    sm_conn->sm_engine_state = SM_PH3_RECEIVE_KEYS;
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE:
                    con_handle = little_endian_read_16(packet, 3);
                    sm_conn = sm_get_connection_for_handle(con_handle);
                    if (!sm_conn) break;

                    log_info("Encryption key refresh complete, key size %u", sm_conn->sm_actual_encryption_key_size);
                    log_info("event handler, state %u", sm_conn->sm_engine_state);
                    // continue if part of initial pairing
                    switch (sm_conn->sm_engine_state){
                        case SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED:
                            sm_conn->sm_engine_state = SM_INITIATOR_CONNECTED; 
                            sm_done_for_handle(sm_conn->sm_handle);
                            break;                        
                        case SM_PH2_W4_CONNECTION_ENCRYPTED:
                            if (sm_conn->sm_role){
                                // slave
                                sm_conn->sm_engine_state = SM_PH3_GET_RANDOM;
                            } else {
                                // master
                                sm_conn->sm_engine_state = SM_PH3_RECEIVE_KEYS;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                    

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    con_handle = little_endian_read_16(packet, 3);
                    sm_done_for_handle(con_handle);
                    sm_conn = sm_get_connection_for_handle(con_handle);
                    if (!sm_conn) break;

                    // delete stored bonding on disconnect with authentication failure in ph0
                    if (sm_conn->sm_role == 0 
                        && sm_conn->sm_engine_state == SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED
                        && packet[2] == ERROR_CODE_AUTHENTICATION_FAILURE){
                        le_device_db_remove(sm_conn->sm_le_db_index);
                    }

                    sm_conn->sm_engine_state = SM_GENERAL_IDLE;
                    sm_conn->sm_handle = 0;
                    break;
                    
				case HCI_EVENT_COMMAND_COMPLETE:
                    if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_le_encrypt)){
                        sm_handle_encryption_result(&packet[6]);
                        break;
                    }
                    if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_le_rand)){
                        sm_handle_random_result(&packet[6]);
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

    sm_run();
}

static inline int sm_calc_actual_encryption_key_size(int other){
    if (other < sm_min_encryption_key_size) return 0;
    if (other < sm_max_encryption_key_size) return other;
    return sm_max_encryption_key_size;
}


#ifdef ENABLE_LE_SECURE_CONNECTIONS
static int sm_just_works_or_numeric_comparison(stk_generation_method_t method){
    switch (method){
        case JUST_WORKS:
        case NK_BOTH_INPUT:
            return 1;
        default:
            return 0;        
    }
}
// responder

static int sm_passkey_used(stk_generation_method_t method){
    switch (method){
        case PK_RESP_INPUT:
            return 1;
        default:
            return 0;        
    }
}
#endif

/**
 * @return ok
 */
static int sm_validate_stk_generation_method(void){
    // check if STK generation method is acceptable by client
    switch (setup->sm_stk_generation_method){
        case JUST_WORKS:
            return (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_JUST_WORKS) != 0;
        case PK_RESP_INPUT:
        case PK_INIT_INPUT:
        case OK_BOTH_INPUT:
            return (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_PASSKEY) != 0;
        case OOB:
            return (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_OOB) != 0;
        case NK_BOTH_INPUT:
            return (sm_accepted_stk_generation_methods & SM_STK_GENERATION_METHOD_NUMERIC_COMPARISON) != 0;
            return 1;
        default:
            return 0;
    }
}

static void sm_pdu_handler(uint8_t packet_type, hci_con_handle_t con_handle, uint8_t *packet, uint16_t size){

    if (packet_type == HCI_EVENT_PACKET && packet[0] == L2CAP_EVENT_CAN_SEND_NOW){
        sm_run();
    }

    if (packet_type != SM_DATA_PACKET) return;

    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;

    if (packet[0] == SM_CODE_PAIRING_FAILED){
        sm_conn->sm_engine_state = sm_conn->sm_role ? SM_RESPONDER_IDLE : SM_INITIATOR_CONNECTED;
        return;
    }

    log_debug("sm_pdu_handler: state %u, pdu 0x%02x", sm_conn->sm_engine_state, packet[0]);

    int err;

    if (packet[0] == SM_CODE_KEYPRESS_NOTIFICATION){
        uint8_t buffer[5];
        buffer[0] = SM_EVENT_KEYPRESS_NOTIFICATION;
        buffer[1] = 3;
        little_endian_store_16(buffer, 2, con_handle);
        buffer[4] = packet[1];
        sm_dispatch_event(HCI_EVENT_PACKET, 0, buffer, sizeof(buffer));
        return;
    }

    switch (sm_conn->sm_engine_state){
        
        // a sm timeout requries a new physical connection
        case SM_GENERAL_TIMEOUT:
            return;

        // Initiator
        case SM_INITIATOR_CONNECTED:
            if ((packet[0] != SM_CODE_SECURITY_REQUEST) || (sm_conn->sm_role)){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }
            if (sm_conn->sm_irk_lookup_state == IRK_LOOKUP_FAILED){
                sm_conn->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;
                break;
            }
            if (sm_conn->sm_irk_lookup_state == IRK_LOOKUP_SUCCEEDED){
                sm_key_t ltk;
                le_device_db_encryption_get(sm_conn->sm_le_db_index, NULL, NULL, ltk, NULL, NULL, NULL);
                if (!sm_is_null_key(ltk)){
                    log_info("sm: Setting up previous ltk/ediv/rand for device index %u", sm_conn->sm_le_db_index);
                    sm_conn->sm_engine_state = SM_INITIATOR_PH0_HAS_LTK;
                } else {
                    sm_conn->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;            
                }
                break;
            }
            // otherwise, store security request
            sm_conn->sm_security_request_received = 1;
            break;

        case SM_INITIATOR_PH1_W4_PAIRING_RESPONSE:
            if (packet[0] != SM_CODE_PAIRING_RESPONSE){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }
            // store pairing request
            memcpy(&setup->sm_s_pres, packet, sizeof(sm_pairing_packet_t));
            err = sm_stk_generation_init(sm_conn);
            if (err){
                setup->sm_pairing_failed_reason = err;
                sm_conn->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
                break;
            }

            // generate random number first, if we need to show passkey
            if (setup->sm_stk_generation_method == PK_RESP_INPUT){
                sm_conn->sm_engine_state = SM_PH2_GET_RANDOM_TK;
                break;
            }

#ifdef ENABLE_LE_SECURE_CONNECTIONS
            if (setup->sm_use_secure_connections){
                // SC Numeric Comparison will trigger user response after public keys & nonces have been exchanged                
                if (setup->sm_stk_generation_method == JUST_WORKS){
                    sm_conn->sm_engine_state = SM_PH1_W4_USER_RESPONSE;
                    sm_trigger_user_response(sm_conn);
                    if (setup->sm_user_response == SM_USER_RESPONSE_IDLE){
                        sm_conn->sm_engine_state = SM_SC_SEND_PUBLIC_KEY_COMMAND;
                    } 
                } else {
                    sm_conn->sm_engine_state = SM_SC_SEND_PUBLIC_KEY_COMMAND;
                }
                break;
            } 
#endif  
            sm_conn->sm_engine_state = SM_PH1_W4_USER_RESPONSE;
            sm_trigger_user_response(sm_conn);
            // response_idle == nothing <--> sm_trigger_user_response() did not require response
            if (setup->sm_user_response == SM_USER_RESPONSE_IDLE){
                sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
            } 
            break;                        

        case SM_INITIATOR_PH2_W4_PAIRING_CONFIRM:
            if (packet[0] != SM_CODE_PAIRING_CONFIRM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }

            // store s_confirm
            reverse_128(&packet[1], setup->sm_peer_confirm);
            sm_conn->sm_engine_state = SM_PH2_SEND_PAIRING_RANDOM;
            break;

        case SM_INITIATOR_PH2_W4_PAIRING_RANDOM:
            if (packet[0] != SM_CODE_PAIRING_RANDOM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;;
            }

            // received random value
            reverse_128(&packet[1], setup->sm_peer_random);
            sm_conn->sm_engine_state = SM_PH2_C1_GET_ENC_C;
            break;

        // Responder
        case SM_RESPONDER_IDLE:
        case SM_RESPONDER_SEND_SECURITY_REQUEST: 
        case SM_RESPONDER_PH1_W4_PAIRING_REQUEST:
            if (packet[0] != SM_CODE_PAIRING_REQUEST){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;;
            }

            // store pairing request
            memcpy(&sm_conn->sm_m_preq, packet, sizeof(sm_pairing_packet_t));
            sm_conn->sm_engine_state = SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED;
            break;

#ifdef ENABLE_LE_SECURE_CONNECTIONS
        case SM_SC_W4_PUBLIC_KEY_COMMAND:
            if (packet[0] != SM_CODE_PAIRING_PUBLIC_KEY){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }

            // store public key for DH Key calculation
            reverse_256(&packet[01], setup->sm_peer_qx);
            reverse_256(&packet[33], setup->sm_peer_qy);

#ifdef USE_MBEDTLS_FOR_ECDH
            // validate public key
            mbedtls_ecp_point Q;
            mbedtls_ecp_point_init( &Q );
            mbedtls_mpi_read_binary(&Q.X, setup->sm_peer_qx, 32);
            mbedtls_mpi_read_binary(&Q.Y, setup->sm_peer_qy, 32);
            mbedtls_mpi_lset(&Q.Z, 1);
            err = mbedtls_ecp_check_pubkey(&mbedtls_ec_group, &Q);
            mbedtls_ecp_point_free( & Q);
            if (err){
                log_error("sm: peer public key invalid %x", err);
                // uses "unspecified reason", there is no "public key invalid" error code
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }

#endif
            if (sm_conn->sm_role){
                // responder
                sm_conn->sm_engine_state = SM_SC_SEND_PUBLIC_KEY_COMMAND;
            } else {
                // initiator
                // stk generation method
                // passkey entry: notify app to show passkey or to request passkey
                switch (setup->sm_stk_generation_method){
                    case JUST_WORKS:
                    case NK_BOTH_INPUT:
                        sm_conn->sm_engine_state = SM_SC_W4_CONFIRMATION;
                        break;
                    case PK_RESP_INPUT:
                        sm_sc_start_calculating_local_confirm(sm_conn);
                        break;
                    case PK_INIT_INPUT:
                    case OK_BOTH_INPUT:
                        if (setup->sm_user_response != SM_USER_RESPONSE_PASSKEY){
                            sm_conn->sm_engine_state = SM_SC_W4_USER_RESPONSE;
                            break;
                        }
                        sm_sc_start_calculating_local_confirm(sm_conn);
                        break;
                    case OOB:
                        // TODO: implement SC OOB
                        break;
                } 
            }
            break;

        case SM_SC_W4_CONFIRMATION:
            if (packet[0] != SM_CODE_PAIRING_CONFIRM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }
            // received confirm value
            reverse_128(&packet[1], setup->sm_peer_confirm);

            if (sm_conn->sm_role){
                // responder
                if (sm_passkey_used(setup->sm_stk_generation_method)){
                    if (setup->sm_user_response != SM_USER_RESPONSE_PASSKEY){
                        // still waiting for passkey
                        sm_conn->sm_engine_state = SM_SC_W4_USER_RESPONSE;
                        break;
                    } 
                }
                sm_sc_start_calculating_local_confirm(sm_conn);
            } else {
                // initiator
                if (sm_just_works_or_numeric_comparison(setup->sm_stk_generation_method)){
                    sm_conn->sm_engine_state = SM_SC_W2_GET_RANDOM_A;
                } else {
                    sm_conn->sm_engine_state = SM_SC_SEND_PAIRING_RANDOM;                
                }
            }
            break;        

        case SM_SC_W4_PAIRING_RANDOM:
            if (packet[0] != SM_CODE_PAIRING_RANDOM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }

            // received random value
            reverse_128(&packet[1], setup->sm_peer_nonce);

            // validate confirm value if Cb = f4(Pkb, Pka, Nb, z) 
            // only check for JUST WORK/NC in initiator role AND passkey entry
            if (sm_conn->sm_role || sm_passkey_used(setup->sm_stk_generation_method)) {
                 sm_conn->sm_engine_state = SM_SC_W2_CMAC_FOR_CHECK_CONFIRMATION;
            }

            sm_sc_state_after_receiving_random(sm_conn);
            break;

        case SM_SC_W2_CALCULATE_G2:
        case SM_SC_W4_CALCULATE_G2:
        case SM_SC_W2_CALCULATE_F5_SALT:
        case SM_SC_W4_CALCULATE_F5_SALT:
        case SM_SC_W2_CALCULATE_F5_MACKEY:
        case SM_SC_W4_CALCULATE_F5_MACKEY:
        case SM_SC_W2_CALCULATE_F5_LTK:
        case SM_SC_W4_CALCULATE_F5_LTK:
        case SM_SC_W2_CALCULATE_F6_FOR_DHKEY_CHECK:
        case SM_SC_W4_DHKEY_CHECK_COMMAND:
        case SM_SC_W4_CALCULATE_F6_FOR_DHKEY_CHECK:
            if (packet[0] != SM_CODE_PAIRING_DHKEY_CHECK){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }
            // store DHKey Check
            setup->sm_state_vars |= SM_STATE_VAR_DHKEY_COMMAND_RECEIVED;
            reverse_128(&packet[01], setup->sm_peer_dhkey_check);

            // have we been only waiting for dhkey check command?
            if (sm_conn->sm_engine_state == SM_SC_W4_DHKEY_CHECK_COMMAND){
                sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_F6_TO_VERIFY_DHKEY_CHECK;
            }
            break;
#endif

        case SM_RESPONDER_PH1_W4_PAIRING_CONFIRM:
            if (packet[0] != SM_CODE_PAIRING_CONFIRM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;
            }

            // received confirm value
            reverse_128(&packet[1], setup->sm_peer_confirm);

            // notify client to hide shown passkey
            if (setup->sm_stk_generation_method == PK_INIT_INPUT){
                sm_notify_client_base(SM_EVENT_PASSKEY_DISPLAY_CANCEL, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address);
            }

            // handle user cancel pairing?
            if (setup->sm_user_response == SM_USER_RESPONSE_DECLINE){
                setup->sm_pairing_failed_reason = SM_REASON_PASSKEYT_ENTRY_FAILED;
                sm_conn->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
                break;
            }

            // wait for user action?
            if (setup->sm_user_response == SM_USER_RESPONSE_PENDING){
                sm_conn->sm_engine_state = SM_PH1_W4_USER_RESPONSE;
                break;
            }

            // calculate and send local_confirm
            sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
            break;

        case SM_RESPONDER_PH2_W4_PAIRING_RANDOM:
            if (packet[0] != SM_CODE_PAIRING_RANDOM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;;
            }

            // received random value
            reverse_128(&packet[1], setup->sm_peer_random);
            sm_conn->sm_engine_state = SM_PH2_C1_GET_ENC_C;
            break;

        case SM_PH3_RECEIVE_KEYS:
            switch(packet[0]){
                case SM_CODE_ENCRYPTION_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
                    reverse_128(&packet[1], setup->sm_peer_ltk);
                    break;

                case SM_CODE_MASTER_IDENTIFICATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                    setup->sm_peer_ediv = little_endian_read_16(packet, 1);
                    reverse_64(&packet[3], setup->sm_peer_rand);
                    break;

                case SM_CODE_IDENTITY_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                    reverse_128(&packet[1], setup->sm_peer_irk);
                    break;

                case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                    setup->sm_peer_addr_type = packet[1];
                    reverse_bd_addr(&packet[2], setup->sm_peer_address); 
                    break;

                case SM_CODE_SIGNING_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
                    reverse_128(&packet[1], setup->sm_peer_csrk);
                    break;
                default:
                    // Unexpected PDU
                    log_info("Unexpected PDU %u in SM_PH3_RECEIVE_KEYS", packet[0]);
                    break;
            }     
            // done with key distribution?         
            if (sm_key_distribution_all_received(sm_conn)){

                sm_key_distribution_handle_all_received(sm_conn);

                if (sm_conn->sm_role){
                    if (setup->sm_use_secure_connections && (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION)){
                        sm_conn->sm_engine_state = SM_SC_W2_CALCULATE_H6_ILK;
                    } else {
                        sm_conn->sm_engine_state = SM_RESPONDER_IDLE; 
                        sm_done_for_handle(sm_conn->sm_handle);
                    }
                } else {
                    if (setup->sm_use_secure_connections){
                        sm_conn->sm_engine_state = SM_PH3_DISTRIBUTE_KEYS;
                    } else {
                        sm_conn->sm_engine_state = SM_PH3_GET_RANDOM; 
                    }
                }
            }
            break;
        default:
            // Unexpected PDU
            log_info("Unexpected PDU %u in state %u", packet[0], sm_conn->sm_engine_state);
            break;
    }

    // try to send preparared packet
    sm_run();
}

// Security Manager Client API
void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data)){
    sm_get_oob_data = get_oob_data_callback;
}

void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_add_tail(&sm_event_handlers, (btstack_linked_item_t*) callback_handler);
}

void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods){
    sm_accepted_stk_generation_methods = accepted_stk_generation_methods;
}

void sm_set_encryption_key_size_range(uint8_t min_size, uint8_t max_size){
	sm_min_encryption_key_size = min_size;
	sm_max_encryption_key_size = max_size;
}

void sm_set_authentication_requirements(uint8_t auth_req){
    sm_auth_req = auth_req;
}

void sm_set_io_capabilities(io_capability_t io_capability){
    sm_io_capabilities = io_capability;
}

void sm_set_request_security(int enable){
    sm_slave_request_security = enable;
}

void sm_set_er(sm_key_t er){
    memcpy(sm_persistent_er, er, 16);
}

void sm_set_ir(sm_key_t ir){
    memcpy(sm_persistent_ir, ir, 16);
}

// Testing support only
void sm_test_set_irk(sm_key_t irk){
    memcpy(sm_persistent_irk, irk, 16);
    sm_persistent_irk_ready = 1;
}

void sm_test_use_fixed_local_csrk(void){
    test_use_fixed_local_csrk = 1;
}

void sm_init(void){
    // set some (BTstack default) ER and IR
    int i;
    sm_key_t er;
    sm_key_t ir;
    for (i=0;i<16;i++){
        er[i] = 0x30 + i;
        ir[i] = 0x90 + i;
    }
    sm_set_er(er);
    sm_set_ir(ir);
    // defaults
    sm_accepted_stk_generation_methods = SM_STK_GENERATION_METHOD_JUST_WORKS
                                       | SM_STK_GENERATION_METHOD_OOB
                                       | SM_STK_GENERATION_METHOD_PASSKEY
                                       | SM_STK_GENERATION_METHOD_NUMERIC_COMPARISON;

    sm_max_encryption_key_size = 16;
    sm_min_encryption_key_size = 7;
    
    sm_cmac_state  = CMAC_IDLE;
    dkg_state = DKG_W4_WORKING;
    rau_state = RAU_W4_WORKING;
    sm_aes128_state = SM_AES128_IDLE;
    sm_address_resolution_test = -1;    // no private address to resolve yet
    sm_address_resolution_ah_calculation_active = 0;
    sm_address_resolution_mode = ADDRESS_RESOLUTION_IDLE;
    sm_address_resolution_general_queue = NULL;
    
    gap_random_adress_update_period = 15 * 60 * 1000L;

    sm_active_connection = 0;

    test_use_fixed_local_csrk = 0;

    // register for HCI Events from HCI
    hci_event_callback_registration.callback = &sm_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // and L2CAP PDUs + L2CAP_EVENT_CAN_SEND_NOW
    l2cap_register_fixed_channel(sm_pdu_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);

#ifdef USE_MBEDTLS_FOR_ECDH
    ec_key_generation_state = EC_KEY_GENERATION_IDLE;

#ifndef HAVE_MALLOC
    sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
#endif
    mbedtls_ecp_group_init(&mbedtls_ec_group);
    mbedtls_ecp_group_load(&mbedtls_ec_group, MBEDTLS_ECP_DP_SECP256R1);
#if 0
    // test
    sm_test_use_fixed_ec_keypair();
    if (sm_have_ec_keypair){
        printf("test dhkey check\n");
        sm_key256_t dhkey;
        memcpy(setup->sm_peer_qx, ec_qx, 32);
        memcpy(setup->sm_peer_qy, ec_qy, 32);
        sm_sc_calculate_dhkey(dhkey);
    }
#endif
#endif
}

void sm_use_fixed_ec_keypair(uint8_t * qx, uint8_t * qy, uint8_t * d){
#ifdef ENABLE_LE_SECURE_CONNECTIONS
    memcpy(ec_qx, qx, 32);
    memcpy(ec_qy, qy, 32);
    memcpy(ec_d, d, 32);
    sm_have_ec_keypair = 1;
    ec_key_generation_state = EC_KEY_GENERATION_DONE;
#endif
}

void sm_test_use_fixed_ec_keypair(void){
#ifdef ENABLE_LE_SECURE_CONNECTIONS
#ifdef USE_MBEDTLS_FOR_ECDH
    // use test keypair from spec
    mbedtls_mpi x;
    mbedtls_mpi_init(&x);
    mbedtls_mpi_read_string( &x, 16, "3f49f6d4a3c55f3874c9b3e3d2103f504aff607beb40b7995899b8a6cd3c1abd");
    mbedtls_mpi_write_binary(&x, ec_d, 32);
    mbedtls_mpi_read_string( &x, 16, "20b003d2f297be2c5e2c83a7e9f9a5b9eff49111acf4fddbcc0301480e359de6");
    mbedtls_mpi_write_binary(&x, ec_qx, 32);
    mbedtls_mpi_read_string( &x, 16, "dc809c49652aeb6d63329abf5a52155c766345c28fed3024741c8ed01589d28b");
    mbedtls_mpi_write_binary(&x, ec_qy, 32);
    mbedtls_mpi_free(&x);
#endif
    sm_have_ec_keypair = 1;
    ec_key_generation_state = EC_KEY_GENERATION_DONE;
#endif
}

static sm_connection_t * sm_get_connection_for_handle(hci_con_handle_t con_handle){
    hci_connection_t * hci_con = hci_connection_for_handle(con_handle);
    if (!hci_con) return NULL;
    return &hci_con->sm_connection;    
}

// @returns 0 if not encrypted, 7-16 otherwise
int sm_encryption_key_size(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return 0;     // wrong connection
    if (!sm_conn->sm_connection_encrypted) return 0;
    return sm_conn->sm_actual_encryption_key_size;
}

int sm_authenticated(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return 0;     // wrong connection
    if (!sm_conn->sm_connection_encrypted) return 0; // unencrypted connection cannot be authenticated
    return sm_conn->sm_connection_authenticated;
}

authorization_state_t sm_authorization_state(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return AUTHORIZATION_UNKNOWN;     // wrong connection
    if (!sm_conn->sm_connection_encrypted)               return AUTHORIZATION_UNKNOWN; // unencrypted connection cannot be authorized
    if (!sm_conn->sm_connection_authenticated)           return AUTHORIZATION_UNKNOWN; // unauthenticatd connection cannot be authorized
    return sm_conn->sm_connection_authorization_state;
}

static void sm_send_security_request_for_connection(sm_connection_t * sm_conn){
    switch (sm_conn->sm_engine_state){
        case SM_GENERAL_IDLE:
        case SM_RESPONDER_IDLE:
            sm_conn->sm_engine_state = SM_RESPONDER_SEND_SECURITY_REQUEST;
            sm_run();
            break;
        default:
            break;
    }
}

/** 
 * @brief Trigger Security Request
 */
void sm_send_security_request(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;
    sm_send_security_request_for_connection(sm_conn);
}

// request pairing
void sm_request_pairing(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection

    log_info("sm_request_pairing in role %u, state %u", sm_conn->sm_role, sm_conn->sm_engine_state);
    if (sm_conn->sm_role){
        sm_send_security_request_for_connection(sm_conn);
    } else {
        // used as a trigger to start central/master/initiator security procedures
        uint16_t ediv;
        sm_key_t ltk;
        if (sm_conn->sm_engine_state == SM_INITIATOR_CONNECTED){
            switch (sm_conn->sm_irk_lookup_state){
                case IRK_LOOKUP_FAILED:
                    sm_conn->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;            
                    break;
                case IRK_LOOKUP_SUCCEEDED:
                        le_device_db_encryption_get(sm_conn->sm_le_db_index, &ediv, NULL, ltk, NULL, NULL, NULL);
                        if (!sm_is_null_key(ltk) || ediv){
                            log_info("sm: Setting up previous ltk/ediv/rand for device index %u", sm_conn->sm_le_db_index);
                            sm_conn->sm_engine_state = SM_INITIATOR_PH0_HAS_LTK;
                        } else {
                            sm_conn->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;            
                        }
                        break;
                default:
                    sm_conn->sm_bonding_requested = 1;
                    break;
            }
        } else if (sm_conn->sm_engine_state == SM_GENERAL_IDLE){
            sm_conn->sm_bonding_requested = 1;
        }
    }
    sm_run();
}

// called by client app on authorization request
void sm_authorization_decline(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    sm_conn->sm_connection_authorization_state = AUTHORIZATION_DECLINED;
    sm_notify_client_authorization(SM_EVENT_AUTHORIZATION_RESULT, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0);
}

void sm_authorization_grant(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    sm_conn->sm_connection_authorization_state = AUTHORIZATION_GRANTED;
    sm_notify_client_authorization(SM_EVENT_AUTHORIZATION_RESULT, sm_conn->sm_handle, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 1);
}

// GAP Bonding API

void sm_bonding_decline(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    setup->sm_user_response = SM_USER_RESPONSE_DECLINE;

    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        switch (setup->sm_stk_generation_method){
            case PK_RESP_INPUT:
            case PK_INIT_INPUT:
            case OK_BOTH_INPUT:
                sm_pairing_error(sm_conn, SM_GENERAL_SEND_PAIRING_FAILED);
                break;        
            case NK_BOTH_INPUT:
                sm_pairing_error(sm_conn, SM_REASON_NUMERIC_COMPARISON_FAILED);
                break;
            case JUST_WORKS:
            case OOB:
                sm_pairing_error(sm_conn, SM_REASON_UNSPECIFIED_REASON);
                break;
        }
    }
    sm_run();
}

void sm_just_works_confirm(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    setup->sm_user_response = SM_USER_RESPONSE_CONFIRM;
    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        if (setup->sm_use_secure_connections){
            sm_conn->sm_engine_state = SM_SC_SEND_PUBLIC_KEY_COMMAND;
        } else {
            sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
        }
    }

#ifdef ENABLE_LE_SECURE_CONNECTIONS
    if (sm_conn->sm_engine_state == SM_SC_W4_USER_RESPONSE){
        sm_sc_prepare_dhkey_check(sm_conn);
    }
#endif

    sm_run();
}

void sm_numeric_comparison_confirm(hci_con_handle_t con_handle){
    // for now, it's the same
    sm_just_works_confirm(con_handle);
}

void sm_passkey_input(hci_con_handle_t con_handle, uint32_t passkey){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    sm_reset_tk();
    big_endian_store_32(setup->sm_tk, 12, passkey);
    setup->sm_user_response = SM_USER_RESPONSE_PASSKEY;
    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
    }
#ifdef ENABLE_LE_SECURE_CONNECTIONS
    memcpy(setup->sm_ra, setup->sm_tk, 16);
    memcpy(setup->sm_rb, setup->sm_tk, 16);
    if (sm_conn->sm_engine_state == SM_SC_W4_USER_RESPONSE){
        sm_sc_start_calculating_local_confirm(sm_conn);
    }
#endif
    sm_run();
}

void sm_keypress_notification(hci_con_handle_t con_handle, uint8_t action){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return;     // wrong connection
    if (action > SM_KEYPRESS_PASSKEY_ENTRY_COMPLETED) return;
    setup->sm_keypress_notification = action;
    sm_run();
}

/**
 * @brief Identify device in LE Device DB
 * @param handle
 * @returns index from le_device_db or -1 if not found/identified
 */
int sm_le_device_index(hci_con_handle_t con_handle ){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return -1;
    return sm_conn->sm_le_db_index;
}

// GAP LE API
void gap_random_address_set_mode(gap_random_address_type_t random_address_type){
    gap_random_address_update_stop();
    gap_random_adress_type = random_address_type;
    if (random_address_type == GAP_RANDOM_ADDRESS_TYPE_OFF) return;
    gap_random_address_update_start();
    gap_random_address_trigger();
}

gap_random_address_type_t gap_random_address_get_mode(void){
    return gap_random_adress_type;
}

void gap_random_address_set_update_period(int period_ms){
    gap_random_adress_update_period = period_ms;
    if (gap_random_adress_type == GAP_RANDOM_ADDRESS_TYPE_OFF) return;
    gap_random_address_update_stop();
    gap_random_address_update_start();
}

void gap_random_address_set(bd_addr_t addr){
    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_OFF);
    memcpy(sm_random_address, addr, 6);
    rau_state = RAU_SET_ADDRESS;
    sm_run();
}

/*
 * @brief Set Advertisement Paramters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note own_address_type is used from gap_random_address_set_mode
 */
void gap_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy){
    hci_le_advertisements_set_params(adv_int_min, adv_int_max, adv_type, gap_random_adress_type,
        direct_address_typ, direct_address, channel_map, filter_policy);
}


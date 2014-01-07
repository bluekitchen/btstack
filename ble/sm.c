/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */
 
#include <stdio.h>
#include <strings.h>

#include "debug.h"
#include "hci.h"
#include "l2cap.h"
#include "central_device_db.h"
#include "sm.h"
#include "gap_le.h"

//
// SM internal types and globals
//

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
    SM_STATE_PH3_Y_GET_ENC,
    SM_STATE_PH3_Y_W4_ENC,
    SM_STATE_PH3_LTK_GET_ENC,
    SM_STATE_PH3_LTK_W4_ENC,

    //
    SM_STATE_DISTRIBUTE_KEYS,

    // re establish previously distribued LTK
    SM_STATE_PH4_Y_GET_ENC,
    SM_STATE_PH4_Y_W4_ENC,
    SM_STATE_PH4_LTK_GET_ENC,
    SM_STATE_PH4_LTK_W4_ENC,
    SM_STATE_PH4_SEND_LTK,

    SM_STATE_TIMEOUT, // no other security messages are exchanged

} security_manager_state_t;

typedef enum {
    DKG_W4_WORKING,
    DKG_CALC_IRK,
    DKG_W4_IRK,
    DKG_CALC_DHK,
    DKG_W4_DHK,
    DKG_READY
} derived_key_generation_t;

typedef enum {
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
static sm_key_t sm_persistent_er;
static sm_key_t sm_persistent_ir;

// derived from sm_persistent_ir
static sm_key_t sm_persistent_dhk;
static sm_key_t sm_persistent_irk;

// derived from sm_persistent_er
// ..

static uint8_t sm_accepted_stk_generation_methods;
static uint8_t sm_max_encryption_key_size;
static uint8_t sm_min_encryption_key_size;

static uint8_t sm_s_auth_req = 0;
static uint8_t sm_s_io_capabilities = IO_CAPABILITY_UNKNOWN;
static uint8_t sm_s_request_security = 0;

// 
static derived_key_generation_t dkg_state = DKG_W4_WORKING;

// random address update
static random_address_update_t rau_state = RAU_IDLE;
static bd_addr_t sm_random_address;

// resolvable private address lookup
static int sm_central_device_test;
static int sm_central_device_matched;
static int sm_central_ah_calculation_active;

//
static uint8_t   sm_s_addr_type;
static bd_addr_t sm_s_address;
static uint8_t   sm_actual_encryption_key_size;
static uint8_t   sm_connection_encrypted;
static uint8_t   sm_connection_authenticated;   // [0..1]
static uint8_t   sm_connection_authorization_state;

// PER INSTANCE DATA

static security_manager_state_t sm_state_responding = SM_STATE_IDLE;
static uint16_t sm_response_handle = 0;
static uint8_t  sm_pairing_failed_reason = 0;

// SM timeout
static timer_source_t sm_timeout;

// data to send to aes128 crypto engine, see sm_aes128_set_key and sm_aes128_set_plaintext
static sm_key_t   sm_aes128_key;
static sm_key_t   sm_aes128_plaintext;
static uint8_t    sm_aes128_active;

// generation method and temporary key for STK - STK is stored in sm_s_ltk
static stk_generation_method_t sm_stk_generation_method;
static sm_key_t sm_tk;

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
static sm_key_t  sm_m_random;
static sm_key_t  sm_m_confirm;

static sm_key_t  sm_s_random;
static sm_key_t  sm_s_confirm;
static uint8_t   sm_s_pres[7];

// key distribution, slave sends
static sm_key_t  sm_s_ltk;
static uint16_t  sm_s_y;
static uint16_t  sm_s_div;
static uint16_t  sm_s_ediv;
static uint8_t   sm_s_rand[8];
// commented keys are not used in Perihperal role
// static sm_key_t  sm_s_csrk;

// key distribution, received from master
// commented keys that are not stored or used by Peripheral role
// static sm_key_t  sm_m_ltk;
// static uint16_t  sm_m_ediv;
// static uint8_t   sm_m_rand[8];
static uint8_t   sm_m_addr_type;
static bd_addr_t sm_m_address;
static sm_key_t  sm_m_csrk;
static sm_key_t  sm_m_irk;

// CMAC calculation - only used by signed writes
static cmac_state_t sm_cmac_state;
static sm_key_t     sm_cmac_k;
static uint16_t     sm_cmac_message_len;
static uint8_t *    sm_cmac_message;
static sm_key_t     sm_cmac_m_last;
static sm_key_t     sm_cmac_x;
static uint8_t      sm_cmac_block_current;
static uint8_t      sm_cmac_block_count;
static void (*sm_cmac_done_handler)(uint8_t hash[8]);

// @returns 1 if oob data is available
// stores oob data in provided 16 byte buffer if not null
static int (*sm_get_oob_data)(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data) = NULL;

// used to notify applicationss that user interaction is neccessary, see sm_notify_t below
static btstack_packet_handler_t sm_client_packet_handler = NULL;

// horizontal: initiator capabilities
// vertial:    responder capabilities
static const stk_generation_method_t stk_generation_method[5][5] = {
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { JUST_WORKS,      JUST_WORKS,       PK_INIT_INPUT,   JUST_WORKS,    PK_INIT_INPUT },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    OK_BOTH_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
    { JUST_WORKS,      JUST_WORKS,       JUST_WORKS,      JUST_WORKS,    JUST_WORKS    },
    { PK_RESP_INPUT,   PK_RESP_INPUT,    PK_INIT_INPUT,   JUST_WORKS,    PK_RESP_INPUT },
};

static void sm_run();

// Utils
static inline void swapX(uint8_t *src, uint8_t *dst, int len){
    int i;
    for (i = 0; i < len; i++)
        dst[len - 1 - i] = src[i];
}
static inline void swap24(uint8_t src[3], uint8_t dst[3]){
    swapX(src, dst, 3);
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

static void print_hex16(const char * name, uint16_t value){
    printf("%-6s 0x%04x\n", name, value);
}

// @returns 1 if all bytes are 0
static int sm_is_null_random(uint8_t random[8]){
    int i;
    for (i=0; i < 8 ; i++){
        if (random[i]) return 0;
    }
    return 1;
}

// Key utils
static void sm_reset_tk(){
    int i;
    for (i=0;i<16;i++){
        sm_tk[i] = 0;
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

static void sm_timeout_handler(timer_source_t * timer){
    printf("SM timeout\n");
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

// GAP Random Address updates
static gap_random_address_type_t gap_random_adress_type;
static timer_source_t gap_random_address_update_timer; 
static uint32_t gap_random_adress_update_period;

static void gap_random_address_update_handler(timer_source_t * timer){
    printf("GAP Random Address Update due\n");
    run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    run_loop_add_timer(&gap_random_address_update_timer);
    if (rau_state != RAU_IDLE) return;
    rau_state = RAU_GET_RANDOM;
    sm_run();
}

static void gap_random_address_update_start(){
    run_loop_set_timer_handler(&gap_random_address_update_timer, gap_random_address_update_handler);
    run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    run_loop_add_timer(&gap_random_address_update_timer);
}

static void gap_random_address_update_stop(){
    run_loop_remove_timer(&gap_random_address_update_timer);
}

static inline void sm_aes128_set_key(sm_key_t key){
    memcpy(sm_aes128_key, key, 16);
} 

static inline void sm_aes128_set_plaintext(sm_key_t plaintext){
    memcpy(sm_aes128_plaintext, plaintext, 16);
} 

// asserts: sm_aes128_active == 0, hci_can_send_command == 1
static void sm_aes128_start(sm_key_t key, sm_key_t plaintext){
    sm_aes128_active = 1;
    sm_key_t key_flipped, plaintext_flipped;
    swap128(key, key_flipped);
    swap128(plaintext, plaintext_flipped);
    hci_send_cmd(&hci_le_encrypt, key_flipped, plaintext_flipped);
}

static void sm_ah_r_prime(uint8_t r[3], sm_key_t d1_prime){
    // r'= padding || r
    memset(d1_prime, 0, 16);
    memcpy(&d1_prime[13], r, 3);
}

static void sm_d1_d_prime(uint16_t d, uint16_t r, sm_key_t d1_prime){
    // d'= padding || r || d
    memset(d1_prime, 0, 16);
    net_store_16(d1_prime, 12, r);
    net_store_16(d1_prime, 14, d);
}

static void sm_dm_r_prime(uint8_t r[8], sm_key_t r_prime){
    // r’ = padding || r
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
    print_key("p2", p2);

    // c1 = e(k, t2_xor_p2)
    int i;
    for (i=0;i<16;i++){
        t3[i] = t2[i] ^ p2[i];
    }
    print_key("t3", t3);
}

static void sm_s1_r_prime(sm_key_t r1, sm_key_t r2, sm_key_t r_prime){
    print_key("r1", r1);
    print_key("r2", r2);
    memcpy(&r_prime[8], &r2[8], 8);
    memcpy(&r_prime[0], &r1[8], 8);
}

static void sm_notify_client(uint8_t type, uint8_t addr_type, bd_addr_t address, uint32_t passkey, uint16_t index){

    sm_event_t event;
    event.type = type;
    event.addr_type = addr_type;
    BD_ADDR_COPY(event.address, address);
    event.passkey = passkey;
    event.central_device_db_index = index;

    log_info("sm_notify_client %02x, addres_type %u, address (), num '%06u', index %u", event.type, event.addr_type, event.passkey, event.central_device_db_index);

    if (!sm_client_packet_handler) return;
    sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) &event, sizeof(event));
}

static void sm_notify_client_authorization(uint8_t type, uint8_t addr_type, bd_addr_t address, uint8_t result){

    sm_event_t event;
    event.type = type;
    event.addr_type = addr_type;
    BD_ADDR_COPY(event.address, address);
    event.authorization_result = result;

    log_info("sm_notify_client_authorization %02x, address_type %u, address (), result %u", event.type, event.addr_type, event.authorization_result);

    if (!sm_client_packet_handler) return;
    sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) &event, sizeof(event));
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
    if (sm_m_have_oob_data && (*sm_get_oob_data)(sm_m_addr_type, &sm_m_address, sm_tk)){
        sm_stk_generation_method = OOB;
        return;
    }

    // If both devices have not set the MITM option in the Authentication Requirements
    // Flags, then the IO capabilities shall be ignored and the Just Works association
    // model shall be used. 
    if ( ((sm_m_auth_req & SM_AUTHREQ_MITM_PROTECTION) == 0x00) && ((sm_s_auth_req & SM_AUTHREQ_MITM_PROTECTION) == 0)){
        return;
    }

    // Also use just works if unknown io capabilites
    if ((sm_m_io_capabilities > IO_CAPABILITY_KEYBOARD_DISPLAY) || (sm_m_io_capabilities > IO_CAPABILITY_KEYBOARD_DISPLAY)){
        return;
    }

    // Otherwise the IO capabilities of the devices shall be used to determine the
    // pairing method as defined in Table 2.4.
    sm_stk_generation_method = stk_generation_method[sm_s_io_capabilities][sm_m_io_capabilities];
    printf("sm_tk_setup: master io cap: %u, slave io cap: %u -> method %u\n",
        sm_m_io_capabilities, sm_s_io_capabilities, sm_stk_generation_method);
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

    // TODO: handle initiator case here

    // distribute keys as requested by initiator
    sm_key_distribution_received_set = 0;
    sm_key_distribution_send_set = sm_key_distribution_flags_for_set(key_set);
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

static int sm_cmac_last_block_complete(){
    if (sm_cmac_message_len == 0) return 0;
    return (sm_cmac_message_len & 0x0f) == 0;
}

void sm_cmac_start(sm_key_t k, uint16_t message_len, uint8_t * message, void (*done_handler)(uint8_t hash[8])){
    memcpy(sm_cmac_k, k, 16);
    sm_cmac_message_len = message_len;
    sm_cmac_message = message;
    sm_cmac_done_handler = done_handler;
    sm_cmac_block_current = 0;
    memset(sm_cmac_x, 0, 16);

    // step 2: n := ceil(len/const_Bsize);
    sm_cmac_block_count = (message_len + 15) / 16;

    // step 3: ..
    if (sm_cmac_block_count==0){
        sm_cmac_block_count = 1;
    }

    // first, we need to compute l for k1, k2, and m_last
    sm_cmac_state = CMAC_CALC_SUBKEYS;

    // let's go
    sm_run();
}

int sm_cmac_ready(){
    return sm_cmac_state == CMAC_IDLE;
}

static void sm_cmac_handle_aes_engine_ready(){
    switch (sm_cmac_state){
        case CMAC_CALC_SUBKEYS:
            {
            sm_key_t const_zero;
            memset(const_zero, 0, 16);
            sm_aes128_start(sm_cmac_k, const_zero);
            sm_cmac_state++;
            break;
            }
        case CMAC_CALC_MI: {
            int j;
            sm_key_t y;
            for (j=0;j<16;j++){
                y[j] = sm_cmac_x[j] ^ sm_cmac_message[sm_cmac_block_current*16 + j];
            }
            sm_cmac_block_current++;
            sm_aes128_start(sm_cmac_k, y);
            sm_cmac_state++;
            break;
        }
        case CMAC_CALC_MLAST: {
            int i;
            sm_key_t y;
            for (i=0;i<16;i++){
                y[i] = sm_cmac_x[i] ^ sm_cmac_m_last[i]; 
            }
            print_key("Y", y);
            sm_cmac_block_current++;
            sm_aes128_start(sm_cmac_k, y);
            sm_cmac_state++;
            break;
        }
        default:
            printf("sm_cmac_handle_aes_engine_ready called in state %u\n", sm_cmac_state);
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

            print_key("k", sm_cmac_k);
            print_key("k1", k1);
            print_key("k2", k2);

            // step 4: set m_last
            int i;
            if (sm_cmac_last_block_complete()){
                for (i=0;i<16;i++){
                    sm_cmac_m_last[i] = sm_cmac_message[sm_cmac_message_len - 16 + i] ^ k1[i];
                }
            } else {
                int valid_octets_in_last_block = sm_cmac_message_len & 0x0f;
                for (i=0;i<16;i++){
                    if (i < valid_octets_in_last_block){
                        sm_cmac_m_last[i] = sm_cmac_message[(sm_cmac_message_len & 0xfff0) + i] ^ k2[i];
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
            print_key("CMAC", data);
            sm_cmac_done_handler(data);
            break;
        default:
            printf("sm_cmac_handle_encryption_result called in state %u\n", sm_cmac_state);
            break;
    }
}

static int sm_key_distribution_done(){
    if (sm_key_distribution_send_set) return 0;
    int recv_flags = sm_key_distribution_flags_for_set(sm_m_key_distribution);
    return recv_flags == sm_key_distribution_received_set;
}

static void sm_pdu_received_in_wrong_state(){
    sm_pairing_failed_reason = SM_REASON_UNSPECIFIED_REASON;
    sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
}

static void sm_run(void){

    // assert that we can send either one
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;

    // distributed key generation
    switch (dkg_state){
        case DKG_CALC_IRK:
            // already busy?
            if (sm_aes128_active) break;
            {
            // IRK = d1(IR, 1, 0)
            sm_key_t d1_prime;
            sm_d1_d_prime(1, 0, d1_prime);  // plaintext
            sm_aes128_start(sm_persistent_ir, d1_prime);
            dkg_state++;
            }
        case DKG_CALC_DHK:
            // already busy?
            if (sm_aes128_active) break;
            {
            // DHK = d1(IR, 3, 0)
            sm_key_t d1_prime;
            sm_d1_d_prime(3, 0, d1_prime);  // plaintext
            sm_aes128_start(sm_persistent_ir, d1_prime);
            dkg_state++;
            }
            return;
        default:
            break;  
    }

    // random address updates
    switch (rau_state){
        case RAU_GET_RANDOM:
            hci_send_cmd(&hci_le_rand);
            rau_state++;
            return;
        case RAU_GET_ENC:
            // already busy?
            if (sm_aes128_active) break;
            {
            sm_key_t r_prime;
            sm_ah_r_prime(sm_random_address, r_prime);
            sm_aes128_start(sm_persistent_irk, r_prime);
            rau_state++;
            return;
            }
        case RAU_SET_ADDRESS:
            printf("New random address: ");
            print_bd_addr(sm_random_address);
            printf("\n");
            hci_send_cmd(&hci_le_set_random_address, sm_random_address);
            rau_state = RAU_IDLE;
            return;
        default:
            break;
    }

    // CSRK device lookup by public or resolvable private address
    if (sm_central_device_test >= 0){
        printf("Central Device Lookup: device %u/%u\n", sm_central_device_test, central_device_db_count());
        while (sm_central_device_test < central_device_db_count()){
            int addr_type;
            bd_addr_t addr;
            sm_key_t irk;
            central_device_db_info(sm_central_device_test, &addr_type, addr, irk);
            printf("device type %u, addr: ", addr_type);
            print_bd_addr(addr);
            printf("\n");

            if (sm_m_addr_type == addr_type && memcmp(addr, sm_m_address, 6) == 0){
                printf("Central Device Lookup: found CSRK by { addr_type, address} \n");
                sm_central_device_matched = sm_central_device_test;
                sm_central_device_test = -1;
                central_device_db_csrk(sm_central_device_matched, sm_m_csrk);
                sm_notify_client(SM_IDENTITY_RESOLVING_SUCCEEDED, sm_m_addr_type, sm_m_address, 0, sm_central_device_matched);
                break;
            }

            if (sm_m_addr_type == 0){
                sm_central_device_test++;
                continue;
            }

            if (sm_aes128_active) break;

            printf("Central Device Lookup: calculate AH\n");
            print_key("IRK", irk);

            sm_key_t r_prime;
            sm_ah_r_prime(sm_m_address, r_prime);
            sm_aes128_start(irk, r_prime);
            sm_central_ah_calculation_active = 1;
            return;
        }

        if (sm_central_device_test >= central_device_db_count()){
            printf("Central Device Lookup: not found\n");
            sm_central_device_test = -1;
            sm_notify_client(SM_IDENTITY_RESOLVING_FAILED, sm_m_addr_type, sm_m_address, 0, 0);
        }
    }

    // cmac
    switch (sm_cmac_state){
        case CMAC_CALC_SUBKEYS:
        case CMAC_CALC_MI:
        case CMAC_CALC_MLAST:
            // already busy?
            if (sm_aes128_active) break;
            sm_cmac_handle_aes_engine_ready();
            return;
        default:
            break;
    }

    // responding state
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

            uint8_t buffer[7];

            memcpy(buffer, sm_m_preq, 7);        
            buffer[0] = SM_CODE_PAIRING_RESPONSE;
            buffer[1] = sm_s_io_capabilities;
            buffer[2] = sm_stk_generation_method == OOB ? 1 : 0;
            buffer[3] = sm_s_auth_req;
            buffer[4] = sm_max_encryption_key_size;

            memcpy(sm_s_pres, buffer, 7);

            // for validate

            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_timeout_reset();

            // notify client for: JUST WORKS confirm, PASSKEY display or input
            sm_user_response = SM_USER_RESPONSE_IDLE;
            switch (sm_stk_generation_method){
                case PK_RESP_INPUT:
                    sm_user_response = SM_USER_RESPONSE_PENDING;
                    sm_notify_client(SM_PASSKEY_INPUT_NUMBER, sm_m_addr_type, sm_m_address, 0, 0); 
                    break;
                case PK_INIT_INPUT:
                    sm_notify_client(SM_PASSKEY_DISPLAY_NUMBER, sm_m_addr_type, sm_m_address, READ_NET_32(sm_tk, 12), 0); 
                    break;
                case JUST_WORKS:
                    switch (sm_s_io_capabilities){
                        case IO_CAPABILITY_KEYBOARD_DISPLAY:
                        case IO_CAPABILITY_DISPLAY_YES_NO:
                            sm_user_response = SM_USER_RESPONSE_PENDING;
                            sm_notify_client(SM_JUST_WORKS_REQUEST, sm_m_addr_type, sm_m_address, READ_NET_32(sm_tk, 12), 0);
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
        case SM_STATE_PH3_Y_GET_ENC:
        case SM_STATE_PH3_LTK_GET_ENC:
        case SM_STATE_PH4_Y_GET_ENC:
        case SM_STATE_PH4_LTK_GET_ENC:
            // already busy?
            if (sm_aes128_active) break;
            sm_aes128_start(sm_aes128_key, sm_aes128_plaintext);
            sm_state_responding++;
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
            sm_key_t stk_flipped;
            swap128(sm_s_ltk, stk_flipped);
            hci_send_cmd(&hci_le_long_term_key_request_reply, sm_response_handle, stk_flipped);
            sm_state_responding = SM_STATE_PH2_W4_CONNECTION_ENCRYPTED;
            return;
        }
        case SM_STATE_PH4_SEND_LTK: {
            sm_key_t ltk_flipped;
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
                // swap128(sm_s_csrk, &buffer[1]);
                memset(&buffer[1], 0, 16);  // csrk not calculated
                l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                sm_timeout_reset();
                return;
            }

            if (sm_key_distribution_done()){
                sm_timeout_stop();
                sm_state_responding = SM_STATE_IDLE; 
            }

            break;

        default:
            break;
    }
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type != SM_DATA_PACKET) return;

    if (handle != sm_response_handle){
        printf("sm_packet_handler: packet from handle %u, but expecting from %u\n", handle, sm_response_handle);
        return;
    }

    if (packet[0] == SM_CODE_PAIRING_FAILED){
        sm_state_responding = SM_STATE_IDLE;
        return;
    }

    switch (sm_state_responding){
        
        // a sm timeout requries a new physical connection
        case SM_STATE_TIMEOUT:
            return;

        case SM_STATE_IDLE: {
            if (packet[0] != SM_CODE_PAIRING_REQUEST){
                sm_pdu_received_in_wrong_state();
                break;;
            }

            // store key distribtion request
            sm_m_io_capabilities = packet[1];
            sm_m_have_oob_data = packet[2];
            sm_m_auth_req = packet[3];
            sm_m_max_encryption_key_size = packet[4];

            // assert max encryption size above our minimum
            if (sm_m_max_encryption_key_size < sm_min_encryption_key_size){
                sm_pairing_failed_reason = SM_REASON_ENCRYPTION_KEY_SIZE;
                sm_state_responding = SM_STATE_SEND_PAIRING_FAILED;
                break;
            }

            // min{}
            sm_actual_encryption_key_size = sm_max_encryption_key_size;
            if (sm_m_max_encryption_key_size < sm_max_encryption_key_size){
                sm_actual_encryption_key_size = sm_m_max_encryption_key_size;
            }

            // setup key distribution
            sm_m_key_distribution = packet[5];
            sm_setup_key_distribution(packet[6]);

            // for validate
            memcpy(sm_m_preq, packet, 7);

            // start SM timeout
            sm_timeout_start();

            // decide on STK generation method
            sm_tk_setup();
            printf("SMP: generation method %u\n", sm_stk_generation_method);

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

            // JUST WORKS doens't provide authentication
            sm_connection_authenticated = sm_stk_generation_method == JUST_WORKS ? 0 : 1;

            // generate random number first, if we need to show passkey
            if (sm_stk_generation_method == PK_INIT_INPUT){
                sm_state_responding = SM_STATE_PH2_GET_RANDOM_TK;
                break;
            }

            sm_state_responding = SM_STATE_PH1_SEND_PAIRING_RESPONSE;
            break;            
        }

        case SM_STATE_PH1_W4_PAIRING_CONFIRM:
            if (packet[0] != SM_CODE_PAIRING_CONFIRM){
                sm_pdu_received_in_wrong_state();
                break;;
            }

            // received confirm value
            swap128(&packet[1], sm_m_confirm);

            // notify client to hide shown passkey
            if (sm_stk_generation_method == PK_INIT_INPUT){
                sm_notify_client(SM_PASSKEY_DISPLAY_CANCEL, sm_m_addr_type, sm_m_address, 0, 0);
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

        case SM_STATE_PH2_W4_PAIRING_RANDOM:
            if (packet[0] != SM_CODE_PAIRING_RANDOM){
                sm_pdu_received_in_wrong_state();
                break;;
            }

            // received random value
            swap128(&packet[1], sm_m_random);

            // use aes128 engine
            // calculate m_confirm using aes128 engine - step 1
            sm_aes128_set_key(sm_tk);
            sm_c1_t1(sm_m_random, sm_m_preq, sm_s_pres, sm_m_addr_type, sm_s_addr_type, sm_aes128_plaintext);
            sm_state_responding = SM_STATE_PH2_C1_GET_ENC_C;
            break;

        case SM_STATE_DISTRIBUTE_KEYS:
            switch(packet[0]){
                case SM_CODE_ENCRYPTION_INFORMATION:
                    sm_key_distribution_received_set |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
                    // swap128(&packet[1], sm_m_ltk);
                    break;

                case SM_CODE_MASTER_IDENTIFICATION:
                    sm_key_distribution_received_set |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                    // sm_m_ediv = READ_BT_16(packet, 1);
                    // swap64(&packet[3], sm_m_rand);
                    break;

                case SM_CODE_IDENTITY_INFORMATION:
                    sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                    swap128(&packet[1], sm_m_irk);
                    break;

                case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
                    sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                    sm_m_addr_type = packet[1];
                    BD_ADDR_COPY(sm_m_address, &packet[2]); 
                    break;

                case SM_CODE_SIGNING_INFORMATION:
                    sm_key_distribution_received_set |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
                    swap128(&packet[1], sm_m_csrk);

                    // store, if: it's a public address, or, we got an IRK
                    if (sm_m_addr_type == 0 || (sm_key_distribution_received_set & SM_KEYDIST_FLAG_IDENTITY_INFORMATION)) {
                        sm_central_device_matched =  central_device_db_add(sm_m_addr_type, sm_m_address, sm_m_irk, sm_m_csrk);
                        break;
                    } 
                    break;
                default:
                    // Unexpected PDU
                    printf("Unexpected PDU %u in SM_STATE_DISTRIBUTE_KEYS\n", packet[0]);
                    break;
            }     
            // done with key distribution?         
            if (sm_key_distribution_done()){
                sm_timeout_stop();
                sm_state_responding = SM_STATE_IDLE; 
            }
            break;
        default:
            // Unexpected PDU
            printf("Unexpected PDU %u in state %u\n", packet[0], sm_state_responding);
            break;
    }

    // try to send preparared packet
    sm_run();
}

static void sm_event_packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    sm_run();

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (packet[2] == HCI_STATE_WORKING) {
                        printf("HCI Working!\n");
                        dkg_state = DKG_CALC_IRK;

                        sm_run();
                        return; // don't notify app packet handler just yet
					}
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
                            
                            hci_le_advertisement_address(&sm_s_addr_type, &sm_s_address);
                            printf("Incoming connection, own address ");
                            print_bd_addr(sm_s_address);

                            // reset security properties
                            sm_connection_encrypted = 0;
                            sm_connection_authenticated = 0;
                            sm_connection_authorization_state = AUTHORIZATION_UNKNOWN;

                            // request security
                            if (sm_s_request_security){
                                sm_state_responding = SM_STATE_SEND_SECURITY_REQUEST;
                            }

                            // try to lookup device
                            sm_central_device_test = 0;
                            sm_central_device_matched = -1;
                            sm_notify_client(SM_IDENTITY_RESOLVING_STARTED, sm_m_addr_type, sm_m_address, 0, 0);
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

                            // re-establish used key encryption size
                            // no db for encryption size hack: encryption size is stored in lowest nibble of sm_s_rand
                            sm_actual_encryption_key_size = (sm_s_rand[7] & 0x0f) + 1;

                            // no db for authenticated flag hack: flag is stored in bit 4 of LSB
                            sm_connection_authenticated = (sm_s_rand[7] & 0x10) >> 4;

                            log_info("LTK Request: recalculating with ediv 0x%04x", sm_s_ediv);

                            // dhk = d1(IR, 3, 0) - enc
                            // y   = dm(dhk, rand) - enc
                            // div = y xor ediv
                            // ltk = d1(ER, div, 0) - enc

                            // Y = dm(DHK, Rand)
                            sm_aes128_set_key(sm_persistent_dhk);
                            sm_dm_r_prime(sm_s_rand, sm_aes128_plaintext);
                            sm_state_responding = SM_STATE_PH4_Y_GET_ENC;

                            // sm_s_div = sm_div(sm_persistent_dhk, sm_s_rand, sm_s_ediv);
                            // sm_s_ltk(sm_persistent_er, sm_s_div, sm_s_ltk);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    if (sm_response_handle != READ_BT_16(packet, 3)) break;
                    sm_connection_encrypted = packet[5];
                    log_info("Eencryption state change: %u", sm_connection_encrypted);
                    if (!sm_connection_encrypted) break;
                    if (sm_state_responding == SM_STATE_PH2_W4_CONNECTION_ENCRYPTED) {
                        sm_state_responding = SM_STATE_PH3_GET_RANDOM;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    sm_state_responding = SM_STATE_IDLE;
                    sm_response_handle = 0;
                    break;
                    
				case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_encrypt)){
                        sm_aes128_active = 0;
                        if (sm_central_ah_calculation_active){
                            sm_central_ah_calculation_active = 0;
                            // compare calulated address against connecting device
                            uint8_t hash[3];
                            swap24(&packet[6], hash);
                            if (memcmp(&sm_m_address[3], hash, 3) == 0){
                                // found
                                sm_central_device_matched = sm_central_device_test;
                                sm_central_device_test = -1;
                                central_device_db_csrk(sm_central_device_matched, sm_m_csrk);
				                sm_notify_client(SM_IDENTITY_RESOLVING_SUCCEEDED, sm_m_addr_type, sm_m_address, 0, sm_central_device_matched);
                                log_info("Central Device Lookup: matched resolvable private address");
                                break;
                            }
                            // no match
                            sm_central_device_test++;
                            break;
                        }
                        switch (dkg_state){
                            case DKG_W4_IRK:
                                swap128(&packet[6], sm_persistent_irk);
                                print_key("irk", sm_persistent_irk);
                                dkg_state++;
                                break;
                            case DKG_W4_DHK:
                                swap128(&packet[6], sm_persistent_dhk);
                                print_key("dhk", sm_persistent_dhk);
                                dkg_state ++;

                                // SM INIT FINISHED, start application code - TODO untangle that
                                if (sm_client_packet_handler)
                                {
                                    uint8_t event[] = { BTSTACK_EVENT_STATE, 0, HCI_STATE_WORKING };
                                    sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) event, sizeof(event));
                                }
                                break;
                            default:
                                break;
                        }
                        switch (rau_state){
                            case RAU_W4_ENC:
                                swap24(&packet[6], &sm_random_address[3]);
                                rau_state++;
                                break;
                            default:
                                break;
                        }
                        switch (sm_cmac_state){
                            case CMAC_W4_SUBKEYS:
                            case CMAC_W4_MI:
                            case CMAC_W4_MLAST:
                                {
                                sm_key_t t;
                                swap128(&packet[6], t);
                                sm_cmac_handle_encryption_result(t);
                                }
                                break;
                            default:
                                break;
                        }
                        switch (sm_state_responding){
                            case SM_STATE_PH2_C1_W4_ENC_A:
                            case SM_STATE_PH2_C1_W4_ENC_C:
                                {
                                sm_aes128_set_key(sm_tk);
                                sm_key_t t2;
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
                                sm_key_t m_confirm_test;
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
                                sm_truncate_key(sm_s_ltk, sm_actual_encryption_key_size);
                                print_key("stk", sm_s_ltk);
                                sm_state_responding = SM_STATE_PH2_SEND_STK;
                                break;
                            case SM_STATE_PH3_Y_W4_ENC:{
                                sm_key_t y128;
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
                                sm_key_t y128;
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
                                // distribute keys
                                sm_state_responding = SM_STATE_DISTRIBUTE_KEYS;
                                break;                                
                            case SM_STATE_PH4_LTK_W4_ENC:
                                swap128(&packet[6], sm_s_ltk);
                                sm_truncate_key(sm_s_ltk, sm_actual_encryption_key_size);
                                print_key("ltk", sm_s_ltk);
                                sm_state_responding = SM_STATE_PH4_SEND_LTK;
                                break;                                
                            default:
                                break;
                        }
                    }
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_rand)){
                        switch (rau_state){
                            case RAU_W4_RANDOM:
                                // non-resolvable vs. resolvable
                                switch (gap_random_adress_type){
                                    case GAP_RANDOM_ADDRESS_RESOLVABLE:
                                        // resolvable: use random as prand and calc address hash
                                        // "The two most significant bits of prand shall be equal to ‘0’ and ‘1"
                                        memcpy(sm_random_address, &packet[6], 3);
                                        sm_random_address[0] &= 0x3f;
                                        sm_random_address[0] |= 0x40;
                                        rau_state = RAU_GET_ENC;
                                        break;
                                    case GAP_RANDOM_ADDRESS_NON_RESOLVABLE:
                                    default:
                                        // "The two most significant bits of the address shall be equal to ‘0’""
                                        memcpy(sm_random_address, &packet[6], 6);
                                        sm_random_address[0] &= 0x3f; 
                                        rau_state = RAU_SET_ADDRESS;
                                        break;
                                }
                                break;
                            default:
                                break;
                        }
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
                                // no db for encryption size hack: encryption size is stored in lowest nibble of sm_s_rand
                                sm_s_rand[7] = (sm_s_rand[7] & 0xf0) + (sm_actual_encryption_key_size - 1);
                                // no db for authenticated flag hack: store flag in bit 4 of LSB
                                sm_s_rand[7] = (sm_s_rand[7] & 0xef) + (sm_connection_authenticated << 4);
                                sm_state_responding = SM_STATE_PH3_GET_DIV;
                                break;
                            case SM_STATE_PH3_W4_DIV:
                                // use 16 bit from random value as div
                                sm_s_div = READ_NET_16(packet, 6);
                                print_hex16("div", sm_s_div);

                                // PH3B2 - calculate Y from      - enc
                                // Y = dm(DHK, Rand)
                                sm_aes128_set_key(sm_persistent_dhk);
                                sm_dm_r_prime(sm_s_rand, sm_aes128_plaintext);
                                sm_state_responding = SM_STATE_PH3_Y_GET_ENC;
                                break;

                            default:
                                break;
                        }
                        break;
                    }
			}

            // forward packet to higher layer
            if (sm_client_packet_handler){
                sm_client_packet_handler(packet_type, 0, packet, size);
            }
	}

    sm_run();
}


// Security Manager Client API
void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data)){
    sm_get_oob_data = get_oob_data_callback;
}

void sm_register_packet_handler(btstack_packet_handler_t handler){
    sm_client_packet_handler = handler;    
}

void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods){
    sm_accepted_stk_generation_methods = accepted_stk_generation_methods;
}

void sm_set_encrypted_key_size_range(uint8_t min_size, uint8_t max_size){
	sm_min_encryption_key_size = min_size;
	sm_max_encryption_key_size = max_size;
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

void sm_set_er(sm_key_t er){
    memcpy(sm_persistent_er, er, 16);
}

void sm_set_ir(sm_key_t ir){
    memcpy(sm_persistent_ir, ir, 16);
    // sm_dhk(sm_persistent_ir, sm_persistent_dhk);
    // sm_irk(sm_persistent_ir, sm_persistent_irk);
}

void sm_init(){
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
    sm_state_responding = SM_STATE_IDLE;
    // defaults
    sm_accepted_stk_generation_methods = SM_STK_GENERATION_METHOD_JUST_WORKS
                                       | SM_STK_GENERATION_METHOD_OOB
                                       | SM_STK_GENERATION_METHOD_PASSKEY;
    sm_max_encryption_key_size = 16;
    sm_min_encryption_key_size = 7;
    sm_aes128_active = 0;
    
    sm_cmac_state = CMAC_IDLE;

    sm_central_device_test = -1;    // no private address to resolve yet
    sm_central_ah_calculation_active = 0;

    gap_random_adress_update_period = 15 * 60 * 1000;

    // attach to lower layers
    l2cap_register_fixed_channel(sm_packet_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
    l2cap_register_packet_handler(sm_event_packet_handler);
}

static int sm_get_connection(uint8_t addr_type, bd_addr_t address){
    // TODO compare to current connection
    return 1;
}

// @returns 0 if not encrypted, 7-16 otherwise
int sm_encryption_key_size(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return 0; // wrong connection
    if (!sm_connection_encrypted) return 0;
    return sm_actual_encryption_key_size;
}

int sm_authenticated(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return 0; // wrong connection
    if (!sm_connection_encrypted) return 0; // unencrypted connection cannot be authenticated
    return sm_connection_authenticated;
}

authorization_state_t sm_authorization_state(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return 0; // wrong connection
    if (!sm_connection_encrypted) return 0;    // unencrypted connection cannot be authorized
    if (!sm_connection_authenticated) return 0; // unauthenticatd connection cannot be authorized
    return sm_connection_authorization_state;
}

// request authorization
void sm_request_authorization(uint8_t addr_type, bd_addr_t address){
    sm_connection_authorization_state = AUTHORIZATION_PENDING;
    sm_notify_client(SM_AUTHORIZATION_REQUEST, sm_m_addr_type, sm_m_address, 0, 0);
}

// called by client app on authorization request
void sm_authorization_decline(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return; // wrong connection
    sm_connection_authorization_state = AUTHORIZATION_DECLINED;
    sm_notify_client_authorization(SM_AUTHORIZATION_RESULT, sm_m_addr_type, sm_m_address, 0);
}

void sm_authorization_grant(uint8_t addr_type, bd_addr_t address){
    if (!sm_get_connection(addr_type, address)) return; // wrong connection
    sm_connection_authorization_state = AUTHORIZATION_GRANTED;
    sm_notify_client_authorization(SM_AUTHORIZATION_RESULT, sm_m_addr_type, sm_m_address, 1);
}

// GAP Bonding API

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

// GAP LE API
void gap_random_address_set_mode(gap_random_address_type_t random_address_type){
    gap_random_address_update_stop();
    gap_random_adress_type = random_address_type;
    if (random_address_type == GAP_RANDOM_ADDRESS_TYPE_OFF) return;
    gap_random_address_update_start();
}

void gap_random_address_set_update_period(int period_ms){
    gap_random_adress_update_period = period_ms;
    if (gap_random_adress_type == GAP_RANDOM_ADDRESS_TYPE_OFF) return;
    gap_random_address_update_stop();
    gap_random_address_update_start();
}
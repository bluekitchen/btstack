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

#include <btstack/linked_list.h>

#include "btstack_memory.h"
#include "debug.h"
#include "hci.h"
#include "l2cap.h"
#include "le_device_db.h"
#include "sm.h"
#include "gap_le.h"

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

// CMAC calculation
static cmac_state_t sm_cmac_state;
static sm_key_t     sm_cmac_k;
static uint8_t      sm_cmac_header[3];
static uint16_t     sm_cmac_message_len;
static uint8_t *    sm_cmac_message;
static uint8_t      sm_cmac_sign_counter[4];
static sm_key_t     sm_cmac_m_last;
static sm_key_t     sm_cmac_x;
static uint8_t      sm_cmac_block_current;
static uint8_t      sm_cmac_block_count;
static void (*sm_cmac_done_handler)(uint8_t hash[8]);

// resolvable private address lookup / CSRK calculation
static int       sm_address_resolution_test;
static int       sm_address_resolution_ah_calculation_active;
static uint8_t   sm_address_resolution_addr_type;
static bd_addr_t sm_address_resolution_address;
static void *    sm_address_resolution_context;
static address_resolution_mode_t sm_address_resolution_mode;
static linked_list_t sm_address_resolution_general_queue;

// aes128 crypto engine. store current sm_connection_t in sm_aes128_context
static sm_aes128_state_t  sm_aes128_state;
static void *             sm_aes128_context;

// random engine. store context (ususally sm_connection_t)
static void * sm_random_context;

//
// Volume 3, Part H, Chapter 24
// "Security shall be initiated by the Security Manager in the device in the master role.
// The device in the slave role shall be the responding device."
// -> master := initiator, slave := responder
//

// data needed for security setup
typedef struct sm_setup_context {

    timer_source_t sm_timeout;

    // used in all phases
    uint8_t   sm_pairing_failed_reason;

    // user response, (Phase 1 and/or 2)
    uint8_t   sm_user_response;

    // defines which keys will be send after connection is encrypted - calculated during Phase 1, used Phase 3
    int       sm_key_distribution_send_set;
    int       sm_key_distribution_received_set;

    // Phase 2 (Pairing over SMP)
    stk_generation_method_t sm_stk_generation_method;
    sm_key_t  sm_tk;

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

static void sm_run(void);
static void sm_done_for_handle(uint16_t handle);
static void sm_notify_client(uint8_t type, uint8_t addr_type, bd_addr_t address, uint32_t passkey, uint16_t index);
static sm_connection_t * sm_get_connection_for_handle(uint16_t handle);
static inline int sm_calc_actual_encryption_key_size(int other);
static int sm_validate_stk_generation_method(void);

static void log_info_hex16(const char * name, uint16_t value){
    log_info("%-6s 0x%04x", name, value);
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

static void sm_timeout_handler(timer_source_t * timer){
    log_info("SM timeout");
    sm_connection_t * sm_conn = (sm_connection_t *) linked_item_get_user((linked_item_t*) timer);
    sm_conn->sm_engine_state = SM_GENERAL_TIMEOUT;
    sm_done_for_handle(sm_conn->sm_handle);

    // trigger handling of next ready connection
    sm_run();
}
static void sm_timeout_start(sm_connection_t * sm_conn){
    run_loop_remove_timer(&setup->sm_timeout);
    run_loop_set_timer_handler(&setup->sm_timeout, sm_timeout_handler);
    run_loop_set_timer(&setup->sm_timeout, 30000); // 30 seconds sm timeout
    linked_item_set_user((linked_item_t*) &setup->sm_timeout, sm_conn);
    run_loop_add_timer(&setup->sm_timeout);
}
static void sm_timeout_stop(void){
    run_loop_remove_timer(&setup->sm_timeout);
}
static void sm_timeout_reset(sm_connection_t * sm_conn){
    sm_timeout_stop();
    sm_timeout_start(sm_conn);    
}

// end of sm timeout

// GAP Random Address updates
static gap_random_address_type_t gap_random_adress_type;
static timer_source_t gap_random_address_update_timer; 
static uint32_t gap_random_adress_update_period;

static void gap_random_address_trigger(void){
    if (rau_state != RAU_IDLE) return;
    log_info("gap_random_address_trigger");
    rau_state = RAU_GET_RANDOM;
    sm_run();
}

static void gap_random_address_update_handler(timer_source_t * timer){
    log_info("GAP Random Address Update due");
    run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    run_loop_add_timer(&gap_random_address_update_timer);
    gap_random_address_trigger();
}

static void gap_random_address_update_start(void){
    run_loop_set_timer_handler(&gap_random_address_update_timer, gap_random_address_update_handler);
    run_loop_set_timer(&gap_random_address_update_timer, gap_random_adress_update_period);
    run_loop_add_timer(&gap_random_address_update_timer);
}

static void gap_random_address_update_stop(void){
    run_loop_remove_timer(&gap_random_address_update_timer);
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
    swap128(key, key_flipped);
    swap128(plaintext, plaintext_flipped);
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
    net_store_16(d1_prime, 12, r);
    net_store_16(d1_prime, 14, d);
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
    swap56(pres, &p1[0]);
    swap56(preq, &p1[7]);
    p1[14] = rat;
    p1[15] = iat;
    log_key("p1", p1);
    log_key("r", r);
    
    // t1 = r xor p1
    int i;
    for (i=0;i<16;i++){
        t1[i] = r[i] ^ p1[i];
    }
    log_key("t1", t1);
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
    log_key("p2", p2);

    // c1 = e(k, t2_xor_p2)
    int i;
    for (i=0;i<16;i++){
        t3[i] = t2[i] ^ p2[i];
    }
    log_key("t3", t3);
}

static void sm_s1_r_prime(sm_key_t r1, sm_key_t r2, sm_key_t r_prime){
    log_key("r1", r1);
    log_key("r2", r2);
    memcpy(&r_prime[8], &r2[8], 8);
    memcpy(&r_prime[0], &r1[8], 8);
}

static void sm_notify_client(uint8_t type, uint8_t addr_type, bd_addr_t address, uint32_t passkey, uint16_t index){

    sm_event_t event;
    event.type = type;
    event.addr_type = addr_type;
    BD_ADDR_COPY(event.address, address);
    event.passkey = passkey;
    event.le_device_db_index = index;

    log_info("sm_notify_client %02x, addres_type %u, address %s, num '%06"PRIu32"', index %u", event.type, event.addr_type, bd_addr_to_str(event.address), event.passkey, event.le_device_db_index);

    if (!sm_client_packet_handler) return;
    sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) &event, sizeof(event));
}

static void sm_notify_client_authorization(uint8_t type, uint8_t addr_type, bd_addr_t address, uint8_t result){

    sm_event_t event;
    event.type = type;
    event.addr_type = addr_type;
    BD_ADDR_COPY(event.address, address);
    event.authorization_result = result;

    log_info("sm_notify_client_authorization %02x, address_type %u, address %s, result %u", event.type, event.addr_type, bd_addr_to_str(event.address), event.authorization_result);

    if (!sm_client_packet_handler) return;
    sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) &event, sizeof(event));
}

// decide on stk generation based on
// - pairing request
// - io capabilities
// - OOB data availability
static void sm_setup_tk(void){

    // default: just works
    setup->sm_stk_generation_method = JUST_WORKS;
    
    // If both devices have out of band authentication data, then the Authentication
    // Requirements Flags shall be ignored when selecting the pairing method and the
    // Out of Band pairing method shall be used.
    if (setup->sm_m_preq.oob_data_flag && setup->sm_s_pres.oob_data_flag){
        log_info("SM: have OOB data");
        log_key("OOB", setup->sm_tk);
        setup->sm_stk_generation_method = OOB;
        return;
    }

    // Reset TK as it has been setup in sm_init_setup
    sm_reset_tk();

    // If both devices have not set the MITM option in the Authentication Requirements
    // Flags, then the IO capabilities shall be ignored and the Just Works association
    // model shall be used. 
    if ( ((setup->sm_m_preq.auth_req & SM_AUTHREQ_MITM_PROTECTION) == 0x00) && ((setup->sm_s_pres.auth_req & SM_AUTHREQ_MITM_PROTECTION) == 0)){
        return;
    }

    // Also use just works if unknown io capabilites
    if ((setup->sm_m_preq.io_capability > IO_CAPABILITY_KEYBOARD_DISPLAY) || (setup->sm_m_preq.io_capability > IO_CAPABILITY_KEYBOARD_DISPLAY)){
        return;
    }

    // Otherwise the IO capabilities of the devices shall be used to determine the
    // pairing method as defined in Table 2.4.
    setup->sm_stk_generation_method = stk_generation_method[setup->sm_s_pres.io_capability][setup->sm_m_preq.io_capability];
    log_info("sm_setup_tk: master io cap: %u, slave io cap: %u -> method %u",
        setup->sm_m_preq.io_capability, setup->sm_s_pres.io_capability, setup->sm_stk_generation_method);
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

static void sm_address_resolution_start_lookup(uint8_t addr_type, bd_addr_t addr, address_resolution_mode_t mode, void * context){
    memcpy(sm_address_resolution_address, addr, 6);
    sm_address_resolution_addr_type = addr_type;
    sm_address_resolution_test = 0;
    sm_address_resolution_mode = mode;
    sm_address_resolution_context = context;
    sm_notify_client(SM_IDENTITY_RESOLVING_STARTED, addr_type, addr, 0, 0);
}

int sm_address_resolution_lookup(uint8_t address_type, bd_addr_t address){
    // check if already in list
    linked_list_iterator_t it;
    sm_lookup_entry_t * entry;
    linked_list_iterator_init(&it, &sm_address_resolution_general_queue);
    while(linked_list_iterator_has_next(&it)){
        entry = (sm_lookup_entry_t *) linked_list_iterator_next(&it);
        if (entry->address_type != address_type) continue;
        if (memcmp(entry->address, address, 6))  continue;
        // already in list
        return BTSTACK_BUSY;
    }
    entry = btstack_memory_sm_lookup_entry_get();
    if (!entry) return BTSTACK_MEMORY_ALLOC_FAILED;
    entry->address_type = (bd_addr_type_t) address_type;
    memcpy(entry->address, address, 6);
    linked_list_add(&sm_address_resolution_general_queue, (linked_item_t *) entry);    
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
static inline void sm_cmac_next_state(void){
    sm_cmac_state = (cmac_state_t) (((int)sm_cmac_state) + 1);
}
static int sm_cmac_last_block_complete(void){
    if (sm_cmac_message_len == 0) return 0;
    return (sm_cmac_message_len & 0x0f) == 0;
}
static inline uint8_t sm_cmac_message_get_byte(int offset){
    if (offset >= sm_cmac_message_len) {
        log_error("sm_cmac_message_get_byte. out of bounds, access %u, len %u", offset, sm_cmac_message_len);
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

void sm_cmac_start(sm_key_t k, uint8_t opcode, uint16_t handle, uint16_t message_len, uint8_t * message, uint32_t sign_counter, void (*done_handler)(uint8_t hash[8])){
    memcpy(sm_cmac_k, k, 16);
    sm_cmac_header[0] = opcode;
    bt_store_16(sm_cmac_header, 1, handle);
    bt_store_32(sm_cmac_sign_counter, 0, sign_counter);
    sm_cmac_message_len = 3 + message_len + 4;  // incl. virtually prepended att opcode, handle and appended sign_counter in LE
    sm_cmac_message = message;
    sm_cmac_done_handler = done_handler;
    sm_cmac_block_current = 0;
    memset(sm_cmac_x, 0, 16);

    // step 2: n := ceil(len/const_Bsize);
    sm_cmac_block_count = (sm_cmac_message_len + 15) / 16;

    // step 3: ..
    if (sm_cmac_block_count==0){
        sm_cmac_block_count = 1;
    }

    log_info("sm_cmac_start: len %u, block count %u", sm_cmac_message_len, sm_cmac_block_count);

    // first, we need to compute l for k1, k2, and m_last
    sm_cmac_state = CMAC_CALC_SUBKEYS;

    // let's go
    sm_run();
}

int sm_cmac_ready(void){
    return sm_cmac_state == CMAC_IDLE;
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
                y[j] = sm_cmac_x[j] ^ sm_cmac_message_get_byte(sm_cmac_block_current*16 + j);
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
            log_key("Y", y);
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

            log_key("k", sm_cmac_k);
            log_key("k1", k1);
            log_key("k2", k2);

            // step 4: set m_last
            int i;
            if (sm_cmac_last_block_complete()){
                for (i=0;i<16;i++){
                    sm_cmac_m_last[i] = sm_cmac_message_get_byte(sm_cmac_message_len - 16 + i) ^ k1[i];
                }
            } else {
                int valid_octets_in_last_block = sm_cmac_message_len & 0x0f;
                for (i=0;i<16;i++){
                    if (i < valid_octets_in_last_block){
                        sm_cmac_m_last[i] = sm_cmac_message_get_byte((sm_cmac_message_len & 0xfff0) + i) ^ k2[i];
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
            log_key("CMAC", data);
            sm_cmac_done_handler(data);
            sm_cmac_state = CMAC_IDLE;
            break;
        default:
            log_info("sm_cmac_handle_encryption_result called in state %u", sm_cmac_state);
            break;
    }
}

static void sm_trigger_user_response(sm_connection_t * sm_conn){
    // notify client for: JUST WORKS confirm, PASSKEY display or input
    setup->sm_user_response = SM_USER_RESPONSE_IDLE;
    switch (setup->sm_stk_generation_method){
        case PK_RESP_INPUT:
            if (sm_conn->sm_role){
                setup->sm_user_response = SM_USER_RESPONSE_PENDING;
                sm_notify_client(SM_PASSKEY_INPUT_NUMBER, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0, 0); 
            } else {
                sm_notify_client(SM_PASSKEY_DISPLAY_NUMBER, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, READ_NET_32(setup->sm_tk, 12), 0); 
            }
            break;
        case PK_INIT_INPUT:
            if (sm_conn->sm_role){
                sm_notify_client(SM_PASSKEY_DISPLAY_NUMBER, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, READ_NET_32(setup->sm_tk, 12), 0); 
            } else {
                setup->sm_user_response = SM_USER_RESPONSE_PENDING;
                sm_notify_client(SM_PASSKEY_INPUT_NUMBER, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0, 0); 
            }
            break;
        case OK_BOTH_INPUT:
            setup->sm_user_response = SM_USER_RESPONSE_PENDING;
            sm_notify_client(SM_PASSKEY_INPUT_NUMBER, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0, 0); 
            break;        
        case JUST_WORKS:
            setup->sm_user_response = SM_USER_RESPONSE_PENDING;
            sm_notify_client(SM_JUST_WORKS_REQUEST, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, READ_NET_32(setup->sm_tk, 12), 0);
            break;
        case OOB:
            // client already provided OOB data, let's skip notification.
            break;
    }
}

static int sm_key_distribution_all_received(sm_connection_t * sm_conn){
    int recv_flags;
    if (sm_conn->sm_role){
        // slave / responser
        recv_flags = sm_key_distribution_flags_for_set(setup->sm_s_pres.initiator_key_distribution);
    } else {
        // master / initiator
        recv_flags = sm_key_distribution_flags_for_set(setup->sm_s_pres.responder_key_distribution);
    } 
    log_debug("sm_key_distribution_all_received: received 0x%02x, expecting 0x%02x", setup->sm_key_distribution_received_set, recv_flags);       
    return recv_flags == setup->sm_key_distribution_received_set;
}

static void sm_done_for_handle(uint16_t handle){
    if (sm_active_connection == handle){
        sm_timeout_stop();
        sm_active_connection = 0;
        log_info("sm: connection 0x%x released setup context", handle);
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

static void sm_init_setup(sm_connection_t * sm_conn){

    // fill in sm setup
    sm_reset_tk();
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
        hci_le_advertisement_address(&setup->sm_s_addr_type, setup->sm_s_address);
        setup->sm_m_addr_type = sm_conn->sm_peer_addr_type;
        memcpy(setup->sm_m_address, sm_conn->sm_peer_address, 6);
    } else {
        // master
        local_packet = &setup->sm_m_preq;
        hci_le_advertisement_address(&setup->sm_m_addr_type, setup->sm_m_address);
        setup->sm_s_addr_type = sm_conn->sm_peer_addr_type;
        memcpy(setup->sm_s_address, sm_conn->sm_peer_address, 6);

        int key_distribution_flags = sm_key_distribution_flags_for_auth_req();
        setup->sm_m_preq.initiator_key_distribution = key_distribution_flags;
        setup->sm_m_preq.responder_key_distribution = key_distribution_flags;
    }

    local_packet->io_capability = sm_io_capabilities;
    local_packet->oob_data_flag = have_oob_data;
    local_packet->auth_req = sm_auth_req;
    local_packet->max_encryption_key_size = sm_max_encryption_key_size;
}

static int sm_stk_generation_init(sm_connection_t * sm_conn){

    sm_pairing_packet_t * remote_packet;
    int                   remote_key_request;
    if (sm_conn->sm_role){
        // slave / responser
        remote_packet      = &setup->sm_m_preq;
        remote_key_request = setup->sm_m_preq.responder_key_distribution;
    } else {
        // master / initiator
        remote_packet      = &setup->sm_s_pres;
        remote_key_request = setup->sm_s_pres.initiator_key_distribution;
    }

    // check key size
    sm_conn->sm_actual_encryption_key_size = sm_calc_actual_encryption_key_size(remote_packet->max_encryption_key_size);
    if (sm_conn->sm_actual_encryption_key_size == 0) return SM_REASON_ENCRYPTION_KEY_SIZE;

    // setup key distribution
    sm_setup_key_distribution(remote_key_request);

    // identical to responder

    // decide on STK generation method
    sm_setup_tk();
    log_info("SMP: generation method %u", setup->sm_stk_generation_method);

    // check if STK generation method is acceptable by client
    if (!sm_validate_stk_generation_method()) return SM_REASON_AUTHENTHICATION_REQUIREMENTS;

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

    sm_connection_t * sm_connection;
    uint16_t ediv;
    switch (mode){
        case ADDRESS_RESOLUTION_GENERAL:
            break;
        case ADDRESS_RESOLUTION_FOR_CONNECTION:
            sm_connection = (sm_connection_t *) context;
            switch (event){
                case ADDRESS_RESOLUTION_SUCEEDED:
                    sm_connection->sm_irk_lookup_state = IRK_LOOKUP_SUCCEEDED;
                    sm_connection->sm_le_db_index = matched_device_id;
                    log_info("ADDRESS_RESOLUTION_SUCEEDED, index %d", sm_connection->sm_le_db_index);
                    if (sm_connection->sm_role) break;
                    if (!sm_connection->sm_bonding_requested && !sm_connection->sm_security_request_received) break;
                    sm_connection->sm_security_request_received = 0;
                    sm_connection->sm_bonding_requested = 0;
                    le_device_db_encryption_get(sm_connection->sm_le_db_index, &ediv, NULL, NULL, NULL, NULL, NULL);
                    if (ediv){
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
            sm_notify_client(SM_IDENTITY_RESOLVING_SUCCEEDED, sm_address_resolution_addr_type, sm_address_resolution_address, 0, matched_device_id);
            break;
        case ADDRESS_RESOLUTION_FAILED:
            sm_notify_client(SM_IDENTITY_RESOLVING_FAILED, sm_address_resolution_addr_type, sm_address_resolution_address, 0, 0);
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
        le_device_db_local_counter_set(le_db_index, 0);
        
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

        // store encryption information
        if (setup->sm_key_distribution_received_set & SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION   
            && setup->sm_key_distribution_received_set &  SM_KEYDIST_FLAG_MASTER_IDENTIFICATION){
            log_info("sm: set encryption information (key size %u, authenticatd %u)", sm_conn->sm_actual_encryption_key_size, sm_conn->sm_connection_authenticated);
            le_device_db_encryption_set(le_db_index, setup->sm_peer_ediv, setup->sm_peer_rand, setup->sm_peer_ltk,
                sm_conn->sm_actual_encryption_key_size, sm_conn->sm_connection_authenticated, sm_conn->sm_connection_authorization_state == AUTHORIZATION_GRANTED);
        }                
    }

    // keep le_db_index
    sm_conn->sm_le_db_index = le_db_index;    
}

static void sm_run(void){

    linked_list_iterator_t it;    

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
        while(linked_list_iterator_has_next(&it)){
            hci_connection_t * hci_connection = (hci_connection_t *) linked_list_iterator_next(&it);
            sm_connection_t  * sm_connection  = &hci_connection->sm_connection;
            if (sm_connection->sm_irk_lookup_state == IRK_LOOKUP_W4_READY){
                // and start lookup
                sm_address_resolution_start_lookup(sm_connection->sm_peer_addr_type, sm_connection->sm_peer_address, ADDRESS_RESOLUTION_FOR_CONNECTION, sm_connection);
                sm_connection->sm_irk_lookup_state = IRK_LOOKUP_STARTED;
                break;
            }
        }
    }

    // -- if csrk lookup ready, resolved addresses for received addresses
    if (sm_address_resolution_idle()) {
        if (!linked_list_empty(&sm_address_resolution_general_queue)){
            sm_lookup_entry_t * entry = (sm_lookup_entry_t *) sm_address_resolution_general_queue;
            linked_list_remove(&sm_address_resolution_general_queue, (linked_item_t *) entry);
            sm_address_resolution_start_lookup(entry->address_type, entry->address, ADDRESS_RESOLUTION_GENERAL, NULL);
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
            log_key("IRK", irk);

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
        while(!sm_active_connection && linked_list_iterator_has_next(&it)){
            hci_connection_t * hci_connection = (hci_connection_t *) linked_list_iterator_next(&it);
            sm_connection_t  * sm_connection = &hci_connection->sm_connection;
            // - if no connection locked and we're ready/waiting for setup context, fetch it and start
            int done = 1;
            int err;
            int encryption_key_size;
            int authenticated;
            int authorized;
            switch (sm_connection->sm_engine_state) {
                case SM_RESPONDER_SEND_SECURITY_REQUEST:
                    // send packet if possible,
                    if (l2cap_can_send_fixed_channel_packet_now(sm_connection->sm_handle)){
                        uint8_t buffer[2];
                        buffer[0] = SM_CODE_SECURITY_REQUEST;
                        buffer[1] = SM_AUTHREQ_BONDING;
                        sm_connection->sm_engine_state = SM_RESPONDER_PH1_W4_PAIRING_REQUEST;            
                        l2cap_send_connectionless(sm_connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    }
                    // don't lock setup context yet
                    done = 0;
                    break;
                case SM_RESPONDER_PH1_PAIRING_REQUEST_RECEIVED:
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
                    // fetch data from device db - incl. authenticated/authorized/key size. Note all sm_connection_X require encryption enabled
                    le_device_db_encryption_get(sm_connection->sm_le_db_index, &setup->sm_peer_ediv, setup->sm_peer_rand, setup->sm_peer_ltk,
                                                &encryption_key_size, &authenticated, &authorized);
                    log_info("db index %u, key size %u, authenticated %u, authorized %u", sm_connection->sm_le_db_index, encryption_key_size, authenticated, authorized);
                    sm_connection->sm_actual_encryption_key_size = encryption_key_size;
                    sm_connection->sm_connection_authenticated = authenticated;
                    sm_connection->sm_connection_authorization_state = authorized ? AUTHORIZATION_GRANTED : AUTHORIZATION_UNKNOWN; 
                    sm_connection->sm_engine_state = SM_INITIATOR_PH0_SEND_START_ENCRYPTION;
                    break;
                case SM_RESPONDER_PH0_RECEIVED_LTK:
                    // re-establish previously used LTK using Rand and EDIV
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
                    break;
                case SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST:
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
        if (!l2cap_can_send_fixed_channel_packet_now(sm_active_connection)) return;

        sm_connection_t * connection = sm_get_connection_for_handle(sm_active_connection);
        if (!connection) return;

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

            // initiator side
            case SM_INITIATOR_PH0_SEND_START_ENCRYPTION: {
                sm_key_t peer_ltk_flipped;
                swap128(setup->sm_peer_ltk, peer_ltk_flipped);
                connection->sm_engine_state = SM_INITIATOR_PH0_W4_CONNECTION_ENCRYPTED;
                log_info("sm: hci_le_start_encryption ediv 0x%04x", setup->sm_peer_ediv);
                uint32_t rand_high = READ_NET_32(setup->sm_peer_rand, 0);
                uint32_t rand_low  = READ_NET_32(setup->sm_peer_rand, 4);
                hci_send_cmd(&hci_le_start_encryption, connection->sm_handle,rand_low, rand_high, setup->sm_peer_ediv, peer_ltk_flipped);
                return;
            }

            case SM_INITIATOR_PH1_SEND_PAIRING_REQUEST:
                setup->sm_m_preq.code = SM_CODE_PAIRING_REQUEST;
                connection->sm_engine_state = SM_INITIATOR_PH1_W4_PAIRING_RESPONSE;
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) &setup->sm_m_preq, sizeof(sm_pairing_packet_t));
                sm_timeout_reset(connection);
                break;

            // responder side
            case SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY:
                connection->sm_engine_state = SM_RESPONDER_IDLE;
                hci_send_cmd(&hci_le_long_term_key_negative_reply, connection->sm_handle);
                return;

            case SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE:
                // echo initiator for now
                setup->sm_s_pres.code = SM_CODE_PAIRING_RESPONSE;
                key_distribution_flags = sm_key_distribution_flags_for_auth_req();
                setup->sm_s_pres.initiator_key_distribution = setup->sm_m_preq.initiator_key_distribution & key_distribution_flags;
                setup->sm_s_pres.responder_key_distribution = setup->sm_m_preq.responder_key_distribution & key_distribution_flags;
                connection->sm_engine_state = SM_RESPONDER_PH1_W4_PAIRING_CONFIRM;
                l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) &setup->sm_s_pres, sizeof(sm_pairing_packet_t));
                sm_timeout_reset(connection);
                sm_trigger_user_response(connection);
                return;

            case SM_PH2_SEND_PAIRING_RANDOM: {
                uint8_t buffer[17];
                buffer[0] = SM_CODE_PAIRING_RANDOM;
                swap128(setup->sm_local_random, &buffer[1]);
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
                swap128(setup->sm_local_confirm, &buffer[1]);
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
                swap128(setup->sm_ltk, stk_flipped);
                connection->sm_engine_state = SM_PH2_W4_CONNECTION_ENCRYPTED;
                hci_send_cmd(&hci_le_long_term_key_request_reply, connection->sm_handle, stk_flipped);
                return;
            }
            case SM_INITIATOR_PH3_SEND_START_ENCRYPTION: {
                sm_key_t stk_flipped;
                swap128(setup->sm_ltk, stk_flipped);
                connection->sm_engine_state = SM_PH2_W4_CONNECTION_ENCRYPTED;
                hci_send_cmd(&hci_le_start_encryption, connection->sm_handle, 0, 0, 0, stk_flipped);
                return;
            }
            case SM_RESPONDER_PH4_SEND_LTK: {
                sm_key_t ltk_flipped;
                swap128(setup->sm_ltk, ltk_flipped);
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
                    swap128(setup->sm_ltk, &buffer[1]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_MASTER_IDENTIFICATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                    uint8_t buffer[11];
                    buffer[0] = SM_CODE_MASTER_IDENTIFICATION;
                    bt_store_16(buffer, 1, setup->sm_local_ediv);
                    swap64(setup->sm_local_rand, &buffer[3]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_INFORMATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                    uint8_t buffer[17];
                    buffer[0] = SM_CODE_IDENTITY_INFORMATION;
                    swap128(sm_persistent_irk, &buffer[1]);
                    l2cap_send_connectionless(connection->sm_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
                    sm_timeout_reset(connection);
                    return;
                }
                if (setup->sm_key_distribution_send_set &   SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION){
                    setup->sm_key_distribution_send_set &= ~SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                    bd_addr_t local_address;
                    uint8_t buffer[8];
                    buffer[0] = SM_CODE_IDENTITY_ADDRESS_INFORMATION;
                    hci_le_advertisement_address(&buffer[1], local_address);
                    bt_flip_addr(&buffer[2], local_address);
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
                    swap128(setup->sm_local_csrk, &buffer[1]);
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
        swap24(data, hash);
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
            swap128(data, sm_persistent_irk);
            log_key("irk", sm_persistent_irk);
            dkg_next_state();
            return;
        case DKG_W4_DHK:
            swap128(data, sm_persistent_dhk);
            log_key("dhk", sm_persistent_dhk);
            dkg_next_state();

            // SM INIT FINISHED, start application code - TODO untangle that
            if (sm_client_packet_handler)
            {
                uint8_t event[] = { BTSTACK_EVENT_STATE, 1, HCI_STATE_WORKING };
                sm_client_packet_handler(HCI_EVENT_PACKET, 0, (uint8_t*) event, sizeof(event));
            }
            return;
        default:
            break;
    }

    switch (rau_state){
        case RAU_W4_ENC:
            swap24(data, &sm_random_address[3]);
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
            swap128(data, t);
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
            swap128(data, t2);
            sm_c1_t3(t2, setup->sm_m_address, setup->sm_s_address, setup->sm_c1_t3_value);
            }
            sm_next_responding_state(connection);
            return;
        case SM_PH2_C1_W4_ENC_B:
            swap128(data, setup->sm_local_confirm);
            log_key("c1!", setup->sm_local_confirm);
            connection->sm_engine_state = SM_PH2_C1_SEND_PAIRING_CONFIRM;
            return;
        case SM_PH2_C1_W4_ENC_D:
            {
            sm_key_t peer_confirm_test;
            swap128(data, peer_confirm_test);
            log_key("c1!", peer_confirm_test);
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
            swap128(data, setup->sm_ltk);
            sm_truncate_key(setup->sm_ltk, connection->sm_actual_encryption_key_size);
            log_key("stk", setup->sm_ltk);
            if (connection->sm_role){
                connection->sm_engine_state = SM_RESPONDER_PH2_SEND_LTK_REPLY;
            } else {
                connection->sm_engine_state = SM_INITIATOR_PH3_SEND_START_ENCRYPTION;
            }
            return;
        case SM_PH3_Y_W4_ENC:{
            sm_key_t y128;
            swap128(data, y128);
            setup->sm_local_y = READ_NET_16(y128, 14);
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
            swap128(data, y128);
            setup->sm_local_y = READ_NET_16(y128, 14);
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
            swap128(data, setup->sm_ltk);
            log_key("ltk", setup->sm_ltk);
            // calc CSRK next
            connection->sm_engine_state = SM_PH3_CSRK_GET_ENC;
            return;
        case SM_PH3_CSRK_W4_ENC:
            swap128(data, setup->sm_local_csrk);
            log_key("csrk", setup->sm_local_csrk);
            if (setup->sm_key_distribution_send_set){
                connection->sm_engine_state = SM_PH3_DISTRIBUTE_KEYS;
            } else {
                // no keys to send, just continue
                if (connection->sm_role){
                    // slave -> receive master keys
                    connection->sm_engine_state = SM_PH3_RECEIVE_KEYS;
                } else {
                    // master -> all done
                    connection->sm_engine_state = SM_INITIATOR_CONNECTED; 
                    sm_done_for_handle(connection->sm_handle);
                }                
            }
            return;                                
        case SM_RESPONDER_PH4_LTK_W4_ENC:
            swap128(data, setup->sm_ltk);
            sm_truncate_key(setup->sm_ltk, connection->sm_actual_encryption_key_size);
            log_key("ltk", setup->sm_ltk);
            connection->sm_engine_state = SM_RESPONDER_PH4_SEND_LTK;
            return;                                
        default:
            break;
    }
}

// note: random generator is ready. this doesn NOT imply that aes engine is unused!
static void sm_handle_random_result(uint8_t * data){

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
        case SM_PH2_W4_RANDOM_TK:
        {
            // map random to 0-999999 without speding much cycles on a modulus operation
            uint32_t tk = READ_BT_32(data,0);
            tk = tk & 0xfffff;  // 1048575
            if (tk >= 999999){
                tk = tk - 999999;
            } 
            sm_reset_tk();
            net_store_32(setup->sm_tk, 12, tk);
            if (connection->sm_role){
                connection->sm_engine_state = SM_RESPONDER_PH1_SEND_PAIRING_RESPONSE;
            } else {
                connection->sm_engine_state = SM_PH1_W4_USER_RESPONSE;
                sm_trigger_user_response(connection);
                // response_idle == nothing <--> sm_trigger_user_response() did not require response
                if (setup->sm_user_response == SM_USER_RESPONSE_IDLE){
                    connection->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
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
            swap64(data, setup->sm_local_rand);
            // no db for encryption size hack: encryption size is stored in lowest nibble of setup->sm_local_rand
            setup->sm_local_rand[7] = (setup->sm_local_rand[7] & 0xf0) + (connection->sm_actual_encryption_key_size - 1);
            // no db for authenticated flag hack: store flag in bit 4 of LSB
            setup->sm_local_rand[7] = (setup->sm_local_rand[7] & 0xef) + (connection->sm_connection_authenticated << 4);
            connection->sm_engine_state = SM_PH3_GET_DIV;
            return;
        case SM_PH3_W4_DIV:
            // use 16 bit from random value as div
            setup->sm_local_div = READ_NET_16(data, 0);
            log_info_hex16("div", setup->sm_local_div);
            connection->sm_engine_state = SM_PH3_Y_GET_ENC;
            return;
        default:
            break;
    }
}

static void sm_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    sm_connection_t  * sm_conn;
    uint16_t handle;

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (packet[2] == HCI_STATE_WORKING) {
                        log_info("HCI Working!");
                        dkg_state = sm_persistent_irk_ready ? DKG_CALC_DHK : DKG_CALC_IRK;
                        rau_state = RAU_IDLE;

                        sm_run();
                        return; // don't notify app packet handler just yet
					}
					break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:

                            log_info("sm: connected");

                            if (packet[3]) return; // connection failed

                            handle = READ_BT_16(packet, 4);
                            sm_conn = sm_get_connection_for_handle(handle);
                            if (!sm_conn) break;

                            sm_conn->sm_handle = handle;
                            sm_conn->sm_role = packet[6];
                            sm_conn->sm_peer_addr_type = packet[7];
                            bt_flip_addr(sm_conn->sm_peer_address, &packet[8]);

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
                            handle = READ_BT_16(packet, 3);
                            sm_conn = sm_get_connection_for_handle(handle);
                            if (!sm_conn) break;

                            log_info("LTK Request: state %u", sm_conn->sm_engine_state);
                            if (sm_conn->sm_engine_state == SM_RESPONDER_PH2_W4_LTK_REQUEST){
                                sm_conn->sm_engine_state = SM_PH2_CALC_STK;
                                break;
                            }

                            // assume that we don't have a LTK for ediv == 0 and random == null
                            if (READ_BT_16(packet, 13) == 0 && sm_is_null_random(&packet[5])){
                                log_info("LTK Request: ediv & random are empty");
                                sm_conn->sm_engine_state = SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
                                break;
                            }

                            // store rand and ediv
                            swap64(&packet[5], sm_conn->sm_local_rand);
                            sm_conn->sm_local_ediv   = READ_BT_16(packet, 13);
                            sm_conn->sm_engine_state = SM_RESPONDER_PH0_RECEIVED_LTK;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    handle = READ_BT_16(packet, 3);
                    sm_conn = sm_get_connection_for_handle(handle);
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
                                sm_conn->sm_engine_state = SM_PH3_GET_RANDOM;
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
                    handle = READ_BT_16(packet, 3);
                    sm_conn = sm_get_connection_for_handle(handle);
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
                    handle = READ_BT_16(packet, 3);
                    sm_done_for_handle(handle);
                    sm_conn = sm_get_connection_for_handle(handle);
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
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_encrypt)){
                        sm_handle_encryption_result(&packet[6]);
                        break;
                    }
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_rand)){
                        sm_handle_random_result(&packet[6]);
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

static inline int sm_calc_actual_encryption_key_size(int other){
    if (other < sm_min_encryption_key_size) return 0;
    if (other < sm_max_encryption_key_size) return other;
    return sm_max_encryption_key_size;
}

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
        default:
            return 0;
    }
}

// helper for sm_packet_handler, calls sm_run on exit
static void sm_pdu_received_in_wrong_state(sm_connection_t * sm_conn){
    setup->sm_pairing_failed_reason = SM_REASON_UNSPECIFIED_REASON;
    sm_conn->sm_engine_state = sm_conn->sm_role ? SM_RESPONDER_IDLE : SM_INITIATOR_CONNECTED;
    sm_done_for_handle(sm_conn->sm_handle);
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type == HCI_EVENT_PACKET) {
        sm_event_packet_handler(packet_type, handle, packet, size);
        return;
    }

    if (packet_type != SM_DATA_PACKET) return;

    sm_connection_t * sm_conn = sm_get_connection_for_handle(handle);
    if (!sm_conn) return;

    if (packet[0] == SM_CODE_PAIRING_FAILED){
        sm_conn->sm_engine_state = sm_conn->sm_role ? SM_RESPONDER_IDLE : SM_INITIATOR_CONNECTED;
        return;
    }

    log_debug("sm_packet_handler: state %u, pdu 0x%02x", sm_conn->sm_engine_state, packet[0]);

    int err;

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
                uint16_t ediv;
                le_device_db_encryption_get(sm_conn->sm_le_db_index, &ediv, NULL, NULL, NULL, NULL, NULL);
                if (ediv){
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
            swap128(&packet[1], setup->sm_peer_confirm);
            sm_conn->sm_engine_state = SM_PH2_SEND_PAIRING_RANDOM;
            break;

        case SM_INITIATOR_PH2_W4_PAIRING_RANDOM:
            if (packet[0] != SM_CODE_PAIRING_RANDOM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;;
            }

            // received random value
            swap128(&packet[1], setup->sm_peer_random);
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

        case SM_RESPONDER_PH1_W4_PAIRING_CONFIRM:
            if (packet[0] != SM_CODE_PAIRING_CONFIRM){
                sm_pdu_received_in_wrong_state(sm_conn);
                break;;
            }

            // received confirm value
            swap128(&packet[1], setup->sm_peer_confirm);

            // notify client to hide shown passkey
            if (setup->sm_stk_generation_method == PK_INIT_INPUT){
                sm_notify_client(SM_PASSKEY_DISPLAY_CANCEL, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0, 0);
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
            swap128(&packet[1], setup->sm_peer_random);
            sm_conn->sm_engine_state = SM_PH2_C1_GET_ENC_C;
            break;

        case SM_PH3_RECEIVE_KEYS:
            switch(packet[0]){
                case SM_CODE_ENCRYPTION_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION;
                    swap128(&packet[1], setup->sm_peer_ltk);
                    break;

                case SM_CODE_MASTER_IDENTIFICATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_MASTER_IDENTIFICATION;
                    setup->sm_peer_ediv = READ_BT_16(packet, 1);
                    swap64(&packet[3], setup->sm_peer_rand);
                    break;

                case SM_CODE_IDENTITY_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_INFORMATION;
                    swap128(&packet[1], setup->sm_peer_irk);
                    break;

                case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION;
                    setup->sm_peer_addr_type = packet[1];
                    bt_flip_addr(setup->sm_peer_address, &packet[2]); 
                    break;

                case SM_CODE_SIGNING_INFORMATION:
                    setup->sm_key_distribution_received_set |= SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION;
                    swap128(&packet[1], setup->sm_peer_csrk);
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
                    sm_conn->sm_engine_state = SM_RESPONDER_IDLE; 
                    sm_done_for_handle(sm_conn->sm_handle);
                } else {
                    sm_conn->sm_engine_state = SM_PH3_GET_RANDOM; 
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

void sm_register_packet_handler(btstack_packet_handler_t handler){
    sm_client_packet_handler = handler;    
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
                                       | SM_STK_GENERATION_METHOD_PASSKEY;
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

    // attach to lower layers
    l2cap_register_fixed_channel(sm_packet_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
}

static sm_connection_t * sm_get_connection_for_handle(uint16_t con_handle){
    hci_connection_t * hci_con = hci_connection_for_handle((hci_con_handle_t) con_handle);
    if (!hci_con) return NULL;
    return &hci_con->sm_connection;    
}

static sm_connection_t * sm_get_connection(uint8_t addr_type, bd_addr_t address){
    hci_connection_t * hci_con = hci_connection_for_bd_addr_and_type(address, (bd_addr_type_t)addr_type);
    if (!hci_con)  return NULL;
    return &hci_con->sm_connection;
}

// @returns 0 if not encrypted, 7-16 otherwise
int sm_encryption_key_size(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return 0;     // wrong connection
    if (!sm_conn->sm_connection_encrypted) return 0;
    return sm_conn->sm_actual_encryption_key_size;
}

int sm_authenticated(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return 0;     // wrong connection
    if (!sm_conn->sm_connection_encrypted) return 0; // unencrypted connection cannot be authenticated
    return sm_conn->sm_connection_authenticated;
}

authorization_state_t sm_authorization_state(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
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
void sm_send_security_request(uint16_t handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(handle);
    if (!sm_conn) return;
    sm_send_security_request_for_connection(sm_conn);
}

// request pairing
void sm_request_pairing(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection

    log_info("sm_request_pairing in role %u, state %u", sm_conn->sm_role, sm_conn->sm_engine_state);
    if (sm_conn->sm_role){
        sm_send_security_request_for_connection(sm_conn);
    } else {
        // used as a trigger to start central/master/initiator security procedures
            uint16_t ediv;
            if (sm_conn->sm_engine_state == SM_INITIATOR_CONNECTED){
            switch (sm_conn->sm_irk_lookup_state){
                case IRK_LOOKUP_FAILED:
                    sm_conn->sm_engine_state = SM_INITIATOR_PH1_W2_SEND_PAIRING_REQUEST;            
                    break;
                case IRK_LOOKUP_SUCCEEDED:
                        le_device_db_encryption_get(sm_conn->sm_le_db_index, &ediv, NULL, NULL, NULL, NULL, NULL);
                        if (ediv){
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
        }
    }
    sm_run();
}

// called by client app on authorization request
void sm_authorization_decline(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection
    sm_conn->sm_connection_authorization_state = AUTHORIZATION_DECLINED;
    sm_notify_client_authorization(SM_AUTHORIZATION_RESULT, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 0);
}

void sm_authorization_grant(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection
    sm_conn->sm_connection_authorization_state = AUTHORIZATION_GRANTED;
    sm_notify_client_authorization(SM_AUTHORIZATION_RESULT, sm_conn->sm_peer_addr_type, sm_conn->sm_peer_address, 1);
}

// GAP Bonding API

void sm_bonding_decline(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection
    setup->sm_user_response = SM_USER_RESPONSE_DECLINE;

    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        sm_done_for_handle(sm_conn->sm_handle);
        setup->sm_pairing_failed_reason = SM_REASON_PASSKEYT_ENTRY_FAILED;
        sm_conn->sm_engine_state = SM_GENERAL_SEND_PAIRING_FAILED;
    }
    sm_run();
}

void sm_just_works_confirm(uint8_t addr_type, bd_addr_t address){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection
    setup->sm_user_response = SM_USER_RESPONSE_CONFIRM;
    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
    }
    sm_run();
}

void sm_passkey_input(uint8_t addr_type, bd_addr_t address, uint32_t passkey){
    sm_connection_t * sm_conn = sm_get_connection(addr_type, address);
    if (!sm_conn) return;     // wrong connection
    sm_reset_tk();
    net_store_32(setup->sm_tk, 12, passkey);
    setup->sm_user_response = SM_USER_RESPONSE_PASSKEY;
    if (sm_conn->sm_engine_state == SM_PH1_W4_USER_RESPONSE){
        sm_conn->sm_engine_state = SM_PH2_C1_GET_RANDOM_A;
    }
    sm_run();
}

/**
 * @brief Identify device in LE Device DB
 * @param handle
 * @returns index from le_device_db or -1 if not found/identified
 */
int sm_le_device_index(uint16_t handle ){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(handle);
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


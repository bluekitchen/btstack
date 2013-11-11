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

//Key distribution flags
#define SM_KEYDIST_ENC_KEY 0X01
#define SM_KEYDIST_ID_KEY  0x02
#define SM_KEYDIST_SIGN    0x04

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

typedef uint8_t key_t[16];

typedef enum {
    SM_STATE_IDLE,

    SM_STATE_C1_GET_RANDOM_A,
    SM_STATE_C1_W4_RANDOM_A,
    SM_STATE_C1_GET_RANDOM_B,
    SM_STATE_C1_W4_RANDOM_B,
    SM_STATE_C1_GET_ENC_A,
    SM_STATE_C1_W4_ENC_A,
    SM_STATE_C1_GET_ENC_B,
    SM_STATE_C1_W4_ENC_B,
    SM_STATE_C1_SEND,

    SM_STATE_W4_LTK_REQUEST,
    SM_STATE_W4_CONNECTION_ENCRYPTED,

    SM_STATE_PH3_GET_RANDOM,
    SM_STATE_PH3_W4_RANDOM,
    SM_STATE_PH3_GET_DIV,
    SM_STATE_PH3_W4_DIV,

} security_manager_state_t;

static att_connection_t att_connection;
static uint16_t         att_response_handle = 0;
static uint16_t         att_response_size   = 0;
static uint8_t          att_response_buffer[28];

// Security Manager Master Keys, please use sm_set_er(er) and sm_set_ir(ir) with your own 128 bit random values
static key_t sm_persistent_er;
static key_t sm_persistent_ir;

// derived from sm_persistent_ir
static key_t sm_persistent_dhk;
static key_t sm_persistent_irk;

// derived from sm_persistent_er


static uint16_t         sm_response_handle = 0;
static uint16_t         sm_response_size   = 0;
static uint8_t          sm_response_buffer[28];

// defines which keys will be send  after connection is encrypted
static int sm_key_distribution_set = 0;

static security_manager_state_t sm_state_responding = SM_STATE_IDLE;

static int sm_send_security_request = 0;
static int sm_send_encryption_information = 0;
static int sm_send_master_identification = 0;
static int sm_send_identity_information = 0;
static int sm_send_identity_address_information = 0;
static int sm_send_signing_identification = 0;
static int sm_send_pairing_failed = 0;
static int sm_send_s_random = 0;

static int sm_received_encryption_information = 0;
static int sm_received_master_identification = 0;
static int sm_received_identity_information = 0;
static int sm_received_identity_address_information = 0;
static int sm_received_signing_identification = 0;

static key_t   sm_tk;
static key_t   sm_m_random;
static key_t   sm_m_confirm;
static uint8_t sm_preq[7];
static uint8_t sm_pres[7];

static key_t   sm_s_random;
static key_t   sm_s_confirm;

static uint8_t sm_pairing_failed_reason = 0;
static uint16_t  sm_s_div;

// key distribution, slave sends
static key_t     sm_s_ltk;
static uint16_t  sm_s_ediv;
static uint8_t   sm_s_rand[8];
static uint8_t   sm_s_addr_type;
static bd_addr_t sm_s_address;
static key_t     sm_s_csrk;
static key_t     sm_s_irk;

// key distribution, received from master
static key_t     sm_m_ltk;
static uint16_t  sm_m_ediv;
static uint8_t   sm_m_rand[8];
static uint8_t   sm_m_addr_type;
static bd_addr_t sm_m_address;
static key_t     sm_m_csrk;
static key_t     sm_m_irk;

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

// SECURITY MANAGER (SM) MATERIALIZES HERE

static inline void swap128(uint8_t src[16], uint8_t dst[16]){
    int i;
    for (i = 0; i < 16; i++)
        dst[15 - i] = src[i];
}

static inline void swap56(uint8_t src[7], uint8_t dst[7]){
    int i;
    for (i = 0; i < 7; i++)
        dst[6 - i] = src[i];
}

static void sm_d1(key_t k, uint16_t d, uint16_t r, key_t d1){
    // d'= padding || r || d
    key_t d1_prime;
    memset(d1_prime, 0, 16);
    net_store_16(d1_prime, 12, r);
    net_store_16(d1_prime, 14, d);
    // d1(k,d,r) = e(k, d'),
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, d1_prime, d1);
}

static uint16_t sm_dm(key_t k, uint8_t r[8]){
    // r’ = padding || r
    key_t r_prime;
    memset(r_prime, 0, 16);
    memcpy(&r_prime[8], r, 8);
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


// Endianess:
// - preq, pres as found in SM PDUs (little endian), we flip it here
// - everything else in big endian incl. result
static void sm_c1(key_t k, key_t r, uint8_t preq[7], uint8_t pres[7], uint8_t iat, uint8_t rat, bd_addr_t ia, bd_addr_t ra, key_t c1){

    printf("iat %u: ia ", iat);
    print_bd_addr(ia);
    printf("rat %u: ra ", rat);
    print_bd_addr(ra);

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
    printf("p1  "); hexdump(p1, 16);
    
    // p2 = padding || ia || ra
    // "The least significant octet of ra becomes the least significant octet of p2 and
    // the most significant octet of padding becomes the most significant octet of p2.
    // For example, if 48-bit ia is 0xA1A2A3A4A5A6 and the 48-bit ra is
    // 0xB1B2B3B4B5B6 then p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6.
    
    key_t p2;
    memset(p2, 0, 16);
    memcpy(&p2[4],  ia, 6);
    memcpy(&p2[10], ra, 6);
    printf("p2  "); hexdump(p2, 16);

    printf("r   "); hexdump(r, 16);
    
    // t1 = r xor p1
    int i;
    key_t t1;
    for (i=0;i<16;i++){
        t1[i] = r[i] ^ p1[i];
    }
    printf("t1' "); hexdump(t1, 16);
    
    printf("k   "); hexdump(k, 16);
    
    // setup aes decryption
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    
    // t2 = e(k, r_xor_p1)
    key_t t2;
    rijndaelEncrypt(rk, nrounds, t1, t2);
    
    printf("t2' "); hexdump(t2, 16);
    
    key_t t3;
    for (i=0;i<16;i++){
        t3[i] = t2[i] ^ p2[i];
    }
    printf("t3' "); hexdump(t3, 16);
    
    rijndaelEncrypt(rk, nrounds, t3, c1);
    
    printf("c1' "); hexdump(c1, 16);
}

static void sm_s1(key_t k, key_t r1, key_t r2, key_t s1){
    printf("sm_s1\n");
    printf("k:  "); hexdump(k, 16);
    printf("r1: "); hexdump(r1, 16);
    printf("r2: "); hexdump(r2, 16);

    key_t r_prime;
    memcpy(&r_prime[8], &r2[8], 8);
    memcpy(&r_prime[0], &r1[8], 8);
    printf("r': "); hexdump(r_prime, 16);
    
    // setup aes decryption
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &k[0], KEYBITS);
    rijndaelEncrypt(rk, nrounds, r_prime, s1);
    printf("s1: "); hexdump(s1, 16);
}

static void sm_test(){
    
    key_t k;
    memset(k, 0, 16 );
    printf("k:  "); hexdump(k, 16);

    // c1
    key_t r = { 0x57, 0x83, 0xD5, 0x21, 0x56, 0xAD, 0x6F, 0x0E, 0x63, 0x88, 0x27, 0x4E, 0xC6, 0x70, 0x2E, 0xE0 };
    printf("r:  "); hexdump(r, 16);

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
    printf("k:  "); hexdump(k, 16);

    key_t r = { 0x55, 0x05, 0x1D, 0xF4, 0x7C, 0xC9, 0xBC, 0x97, 0x3C, 0x6A, 0x7D, 0x0D, 0x0F, 0x57, 0x0E, 0xC4 };
    printf("r:  "); hexdump(r, 16);

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

static int sm_validate_m_confirm(void){
    printf("sm_validate_m_confirm\n");

    key_t c1;
    sm_c1(sm_tk, sm_m_random, sm_preq, sm_pres, sm_m_addr_type, sm_s_addr_type, sm_m_address, sm_s_address, c1);
    printf("mc: "); hexdump(sm_m_confirm, 16);

    int m_confirm_valid = memcmp(c1, sm_m_confirm, 16) == 0;
    printf("m_confirm_valid: %u\n", m_confirm_valid);
    return m_confirm_valid;
}

static void sm_run(void){

    // assert that we can send either one
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;

    switch (sm_state_responding){
        case SM_STATE_C1_GET_RANDOM_A:
        case SM_STATE_C1_GET_RANDOM_B:
        case SM_STATE_PH3_GET_RANDOM:
        case SM_STATE_PH3_GET_DIV:
            hci_send_cmd(&hci_le_rand);
            sm_state_responding++;
            return;
        case SM_STATE_C1_GET_ENC_A:
        case SM_STATE_C1_GET_ENC_B:
            break;
        case SM_STATE_C1_SEND: {
            uint8_t buffer[17];
            buffer[0] = SM_CODE_PAIRING_CONFIRM;
            swap128(sm_s_confirm, &buffer[1]);
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_state_responding = SM_STATE_W4_LTK_REQUEST;
            return;
        }
        default:
            break;
    }

    // send security manager packet
    if (sm_response_size){
        uint16_t size = sm_response_size;
        sm_response_size = 0;
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) sm_response_buffer, size);
    }
    // send security request
    if (sm_send_security_request){
        sm_send_security_request = 0;
        uint8_t buffer[2];
        buffer[0] = SM_CODE_SECURITY_REQUEST;
        buffer[1] = SM_AUTHREQ_BONDING;
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_encryption_information){
        sm_send_encryption_information = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_ENCRYPTION_INFORMATION;
        memcpy(&buffer[1], sm_s_ltk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_master_identification){
        sm_send_master_identification = 0;
        uint8_t buffer[11];
        buffer[0] = SM_CODE_MASTER_IDENTIFICATION;
        bt_store_16(buffer, 1, sm_s_ediv);
        memcpy(&buffer[3],sm_s_rand,8); 
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_identity_information){
        sm_send_identity_information = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_IDENTITY_INFORMATION;
        memcpy(&buffer[1], sm_s_irk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_identity_address_information ){
        sm_send_identity_address_information = 0;
        uint8_t buffer[8];
        buffer[0] = SM_CODE_IDENTITY_ADDRESS_INFORMATION;
        buffer[1] = sm_s_addr_type;
        bt_flip_addr(&buffer[2], sm_s_address);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_signing_identification){
        sm_send_signing_identification = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_SIGNING_INFORMATION;
        memcpy(&buffer[1], sm_s_csrk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_pairing_failed){
        sm_send_pairing_failed = 0;
        uint8_t buffer[2];
        buffer[0] = SM_CODE_PAIRING_FAILED;
        buffer[1] = sm_pairing_failed_reason;
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_s_random){
        sm_send_s_random = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_PAIRING_RANDOM;
        swap128(sm_s_random, &buffer[1]);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type != SM_DATA_PACKET) return;

    printf("sm_packet_handler, request %0x\n", packet[0]);

    switch (packet[0]){
        case SM_CODE_PAIRING_REQUEST:
            
            // store key distribtion request
            sm_key_distribution_set = packet[6];

            // for validate
            memcpy(sm_preq, packet, 7);
            
            // TODO use provided IO capabilites
            // TOOD use local MITM flag
            // TODO provide callback to request OOB data

            memcpy(sm_response_buffer, packet, size);        
            sm_response_buffer[0] = SM_CODE_PAIRING_RESPONSE;
            // sm_response_buffer[1] = 0x00;   // io capability: DisplayOnly
            // sm_response_buffer[1] = 0x02;   // io capability: KeyboardOnly
            // sm_response_buffer[1] = 0x03;   // io capability: NoInputNoOutput
            sm_response_buffer[1] = 0x04;   // io capability: KeyboardDisplay
            sm_response_buffer[2] = 0x00;   // no oob data available
            sm_response_buffer[3] = sm_response_buffer[3] & 3;  // remove MITM flag
            sm_response_buffer[4] = 0x10;   // maxium encryption key size
            sm_response_size = 7;

            // for validate
            memcpy(sm_pres, sm_response_buffer, 7);
            break;

        case  SM_CODE_PAIRING_CONFIRM:
            // received confirm value
            swap128(&packet[1], sm_m_confirm);

            // calculate and send s_confirm
            sm_state_responding = SM_STATE_C1_GET_RANDOM_A;
            break;

        case SM_CODE_PAIRING_RANDOM:
            // received random value
            swap128(&packet[1], sm_m_random);

            // validate m confirm
            if (!sm_validate_m_confirm()){
                sm_send_pairing_failed = 1;
                sm_pairing_failed_reason = SM_REASON_CONFIRM_VALUE_FAILED;
                break;
            }
            // send s_random
            sm_send_s_random = 1;
            break;

        case SM_CODE_ENCRYPTION_INFORMATION:
            sm_received_encryption_information = 1;
            memcpy(sm_m_ltk, &packet[1], 16);
            break;

        case SM_CODE_MASTER_IDENTIFICATION:
            sm_received_master_identification = 1;
            sm_m_ediv = READ_BT_16(packet, 1);
            memcpy(sm_m_rand, &packet[3],8);
            break;

        case SM_CODE_IDENTITY_INFORMATION:
            sm_received_identity_information = 1;
            memcpy(sm_m_irk, &packet[1], 16);
            break;

        case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
            sm_received_identity_address_information = 1;
            sm_m_addr_type = packet[1];
            BD_ADDR_COPY(sm_m_address, &packet[2]); 
            break;

        case SM_CODE_SIGNING_INFORMATION:
            sm_received_signing_identification = 1;
            memcpy(sm_m_csrk, &packet[1], 16);
            break;
    }

    // try to send preparared packet
    sm_run();
}

void sm_reset_tk(){
    int i;
    for (i=0;i<16;i++){
        sm_tk[i] = 0;
    }
}

static void sm_distribute_keys(){

    // TODO: handle initiator case here

    // distribute keys as requested by initiator
    if (sm_key_distribution_set & SM_KEYDIST_ENC_KEY)
        sm_send_encryption_information = 1;
    sm_send_master_identification = 1;
    if (sm_key_distribution_set & SM_KEYDIST_ID_KEY)
        sm_send_identity_information = 1;
    sm_send_identity_address_information = 1;
    if (sm_key_distribution_set & SM_KEYDIST_SIGN)
        sm_send_signing_identification = 1;  
}

void sm_set_er(key_t er){
    memcpy(sm_persistent_er, er, 16);
}

void sm_set_ir(key_t ir){
    memcpy(sm_persistent_ir, ir, 16);
    sm_dhk(sm_persistent_ir, sm_persistent_dhk);
    sm_irk(sm_persistent_ir, sm_persistent_irk);
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
}


// END OF SM

// enable LE, setup ADV data
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
                            sm_response_handle = READ_BT_16(packet, 4);
                            sm_m_addr_type = packet[7];
                            bt_flip_addr(sm_m_address, &packet[8]);
                            // TODO use non-null TK if appropriate
                            sm_reset_tk();
                            // TODO support private addresses
                            sm_s_addr_type = 0;
                            BD_ADDR_COPY(sm_s_address, hci_local_bd_addr());
                            printf("Incoming connection, own address ");
                            print_bd_addr(sm_s_address);
                            // request security
                            sm_send_security_request = 1;

                            // reset connection MTU
                            att_connection.mtu = 23;
                            break;

                        case HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST:
                            log_info("LTK Request, state %u", sm_state_responding);
                            if (sm_state_responding == SM_STATE_W4_LTK_REQUEST){
                                // calculate STK
                                log_info("calculating STK");
                                key_t sm_stk;
                                sm_s1(sm_tk, sm_s_random, sm_m_random, sm_stk);
                                key_t sm_stk_flipped;
                                swap128(sm_stk, sm_stk_flipped);
                                hci_send_cmd(&hci_le_long_term_key_request_reply, READ_BT_16(packet, 3), sm_stk_flipped);
                                sm_state_responding = SM_STATE_W4_CONNECTION_ENCRYPTED;
                                break;
                            }
                            // re-establish previously used LTK using Rand and EDIV
                            log_info("recalculating LTK");
                            memcpy(sm_s_rand, &packet[5], 8);
                            sm_s_ediv = READ_BT_16(packet, 13);
                            sm_s_div  = sm_div(sm_persistent_dhk, sm_s_rand, sm_s_ediv);
                            sm_ltk(sm_persistent_er, sm_s_div, sm_s_ltk);
                            hci_send_cmd(&hci_le_long_term_key_request_reply, READ_BT_16(packet, 3), sm_s_ltk);
                            sm_state_responding = SM_STATE_IDLE;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    log_info("Connection encrypted");
                    if (sm_state_responding == SM_STATE_W4_CONNECTION_ENCRYPTED) {
                        sm_state_responding = SM_STATE_PH3_GET_RANDOM;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    att_response_handle = 0;
                    att_response_size = 0;
                    
                    // restart advertising
                    hci_send_cmd(&hci_le_set_advertise_enable, 1);
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
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_rand)){
                        switch (sm_state_responding){
                            case SM_STATE_C1_W4_RANDOM_A:
                                memcpy(&sm_s_random[0], &packet[6], 8);
                                sm_state_responding = SM_STATE_C1_GET_RANDOM_B;
                                break;
                            case SM_STATE_C1_W4_RANDOM_B:
                                memcpy(&sm_s_random[8], &packet[6], 8);

                                // calculate s_confirm
                                sm_c1(sm_tk, sm_s_random, sm_preq, sm_pres, sm_m_addr_type, sm_s_addr_type, sm_m_address, sm_s_address, sm_s_confirm);
                                
                                // send s_confirm
                                sm_state_responding = SM_STATE_C1_SEND;
                                break;
                            case SM_STATE_PH3_W4_RANDOM:
                                memcpy(sm_s_rand, &packet[6], 8);
                                sm_state_responding = SM_STATE_PH3_GET_DIV;
                                break;
                            case SM_STATE_PH3_W4_DIV:
                                // use 16 bit from random value as div
                                sm_s_div = READ_NET_16(packet, 6);

                                // done
                                sm_state_responding = SM_STATE_IDLE;

                                // calculate EDIV and LTK
                                sm_s_ediv = sm_ediv(sm_persistent_dhk, sm_s_rand, sm_s_div);
                                sm_ltk(sm_persistent_er, sm_s_div, sm_s_ltk);

                                // distribute keys
                                sm_distribute_keys();
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

// test profile
#include "profile.h"

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

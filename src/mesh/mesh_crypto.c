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

#define __BTSTACK_FILE__ "mesh_crypto.c"

#include <string.h>

#include "btstack_debug.h"
#include "btstack_util.h"

#include "mesh/mesh_crypto.h"

// mesh k1 - might get moved to btstack_crypto and all vars go into btstack_crypto_mesh_k1_t struct
static uint8_t         mesh_k1_temp[16];
static void (*         mesh_k1_callback)(void * arg);
static void *          mesh_k1_arg;
static const uint8_t * mesh_k1_p;
static uint16_t        mesh_k1_p_len;
static uint8_t *       mesh_k1_result;

static void mesh_k1_temp_calculated(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    btstack_crypto_aes128_cmac_message(request, mesh_k1_temp, mesh_k1_p_len, mesh_k1_p, mesh_k1_result, mesh_k1_callback, mesh_k1_arg);
}

void mesh_k1(btstack_crypto_aes128_cmac_t * request, const uint8_t * n, uint16_t n_len, const uint8_t * salt,
    const uint8_t * p, const uint16_t p_len, uint8_t * result, void (* callback)(void * arg), void * callback_arg){
    mesh_k1_callback = callback;
    mesh_k1_arg      = callback_arg;
    mesh_k1_p        = p;
    mesh_k1_p_len    = p_len;
    mesh_k1_result   = result;
    btstack_crypto_aes128_cmac_message(request, salt, n_len, n, mesh_k1_temp, mesh_k1_temp_calculated, request);
}

// mesh k2 - might get moved to btstack_crypto and all vars go into btstack_crypto_mesh_k2_t struct
static void (*         mesh_k2_callback)(void * arg);
static void *          mesh_k2_arg;
static uint8_t       * mesh_k2_result;
static uint8_t         mesh_k2_t[16];
static uint8_t         mesh_k2_t1[18];
static uint8_t         mesh_k2_t2[16];

static const uint8_t mesh_salt_smk2[] = { 0x4f, 0x90, 0x48, 0x0c, 0x18, 0x71, 0xbf, 0xbf, 0xfd, 0x16, 0x97, 0x1f, 0x4d, 0x8d, 0x10, 0xb1 };

static void mesh_k2_callback_d(void * arg){
    // btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    UNUSED(arg);
    log_info("PrivacyKey: ");
    log_info_hexdump(mesh_k2_t, 16);
    // collect result
    memcpy(&mesh_k2_result[17], mesh_k2_t2, 16);
    //
    (*mesh_k2_callback)(mesh_k2_arg);        
}
static void mesh_k2_callback_c(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    log_info("EncryptionKey: ");
    log_info_hexdump(mesh_k2_t, 16);
    // collect result
    memcpy(&mesh_k2_result[1], mesh_k2_t2, 16);
    //
    memcpy(mesh_k2_t1, mesh_k2_t2, 16);
    mesh_k2_t1[16] = 0; // p
    mesh_k2_t1[17] = 0x03;
    btstack_crypto_aes128_cmac_message(request, mesh_k2_t, 18, mesh_k2_t1, mesh_k2_t2, mesh_k2_callback_d, request);
}
static void mesh_k2_callback_b(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    log_info("NID: 0x%02x\n", mesh_k2_t2[15] & 0x7f);
    // collect result
    mesh_k2_result[0] = mesh_k2_t2[15] & 0x7f;
    //
    memcpy(mesh_k2_t1, mesh_k2_t2, 16);
    mesh_k2_t1[16] = 0; // p
    mesh_k2_t1[17] = 0x02;
    btstack_crypto_aes128_cmac_message(request, mesh_k2_t, 18, mesh_k2_t1, mesh_k2_t2, mesh_k2_callback_c, request);
}
static void mesh_k2_callback_a(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    log_info("T:");
    log_info_hexdump(mesh_k2_t, 16);
    mesh_k2_t1[0] = 0; // p
    mesh_k2_t1[1] = 0x01;
    btstack_crypto_aes128_cmac_message(request, mesh_k2_t, 2, mesh_k2_t1, mesh_k2_t2, mesh_k2_callback_b, request);
}
void mesh_k2(btstack_crypto_aes128_cmac_t * request, const uint8_t * n, uint8_t * result, void (* callback)(void * arg), void * callback_arg){
    mesh_k2_callback = callback;
    mesh_k2_arg      = callback_arg;
    mesh_k2_result   = result;
    btstack_crypto_aes128_cmac_message(request, mesh_salt_smk2, 16, n, mesh_k2_t, mesh_k2_callback_a, request);
}


// mesh k3 - might get moved to btstack_crypto and all vars go into btstack_crypto_mesh_k3_t struct
static const uint8_t   mesh_k3_tag[5] = { 'i', 'd', '6', '4', 0x01}; 
static uint8_t         mesh_k3_temp[16];
static uint8_t         mesh_k3_result128[16];
static void (*         mesh_k3_callback)(void * arg);
static void *          mesh_k3_arg;
static const uint8_t * mesh_k3_n;
static uint8_t       * mesh_k3_result;

// AES-CMAC_ZERO('smk3')
static const uint8_t mesh_salt_smk3[] = { 0x00, 0x36, 0x44, 0x35, 0x03, 0xf1, 0x95, 0xcc, 0x8a, 0x71, 0x6e, 0x13, 0x62, 0x91, 0xc3, 0x02, };

static void mesh_k3_result128_calculated(void * arg){
    UNUSED(arg);
    memcpy(mesh_k3_result, &mesh_k3_result128[8], 8);
    (*mesh_k3_callback)(mesh_k3_arg);        
}
static void mesh_k3_temp_callback(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    btstack_crypto_aes128_cmac_message(request, mesh_k3_temp, sizeof(mesh_k3_tag), mesh_k3_tag, mesh_k3_result128, mesh_k3_result128_calculated, request);
}
void mesh_k3(btstack_crypto_aes128_cmac_t * request, const uint8_t * n, uint8_t * result, void (* callback)(void * arg), void * callback_arg){
    mesh_k3_callback = callback;
    mesh_k3_arg      = callback_arg;
    mesh_k3_n        = n;
    mesh_k3_result   = result;
    btstack_crypto_aes128_cmac_message(request, mesh_salt_smk3, 16, mesh_k3_n, mesh_k3_temp, mesh_k3_temp_callback, request);
}

// mesh k4 - might get moved to btstack_crypto and all vars go into btstack_crypto_mesh_k4_t struct
// k4N     63964771734fbd76e3b40519d1d94a48
// k4 SALT 0e9ac1b7cefa66874c97ee54ac5f49be
// k4T     921cb4f908cc5932e1d7b059fc163ce6
// k4 CMAC(id6|0x01) 5f79cf09bbdab560e7f1ee404fd341a6
// AID 26
static const uint8_t   mesh_k4_tag[4] = { 'i', 'd', '6', 0x01}; 
static uint8_t         mesh_k4_temp[16];
static uint8_t         mesh_k4_result128[16];
static void (*         mesh_k4_callback)(void * arg);
static void *          mesh_k4_arg;
static const uint8_t * mesh_k4_n;
static uint8_t       * mesh_k4_result;

// AES-CMAC_ZERO('smk4')
static const uint8_t mesh_salt_smk4[] = { 0x0E, 0x9A, 0xC1, 0xB7, 0xCE, 0xFA, 0x66, 0x87, 0x4C, 0x97, 0xEE, 0x54, 0xAC, 0x5F, 0x49, 0xBE };
static void mesh_k4_result128_calculated(void * arg){
    UNUSED(arg);
    mesh_k4_result[0] = mesh_k4_result128[15] & 0x3f;
    (*mesh_k4_callback)(mesh_k4_arg);
}
static void mesh_k4_temp_callback(void * arg){
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    btstack_crypto_aes128_cmac_message(request, mesh_k4_temp, sizeof(mesh_k4_tag), mesh_k4_tag, mesh_k4_result128, mesh_k4_result128_calculated, request);
}
void mesh_k4(btstack_crypto_aes128_cmac_t * request, const uint8_t * n, uint8_t * result, void (* callback)(void * arg), void * callback_arg){
    mesh_k4_callback = callback;
    mesh_k4_arg      = callback_arg;
    mesh_k4_n        = n;
    mesh_k4_result   = result;
    btstack_crypto_aes128_cmac_message(request, mesh_salt_smk4, 16, mesh_k4_n, mesh_k4_temp, mesh_k4_temp_callback, request);
}

// mesh virtual address hash - might get moved to btstack_crypto and all vars go into btstack_crypto_mesh_virtual_address_t struct

static uint8_t mesh_salt_vtad[] = { 0xce, 0xf7, 0xfa, 0x9d, 0xc4, 0x7b, 0xaf, 0x5d, 0xaa, 0xee, 0xd1, 0x94, 0x06, 0x09, 0x4f, 0x37, };
static void *  mesh_virtual_address_arg;
static void (* mesh_virtual_address_callback)(void * arg);
static uint16_t * mesh_virtual_address_hash;
static uint8_t mesh_virtual_address_temp[16];

static void mesh_virtual_address_temp_callback(void * arg){
    UNUSED(arg);
    uint16_t addr = (big_endian_read_16(mesh_virtual_address_temp, 14) & 0x3fff) | 0x8000;
    *mesh_virtual_address_hash = addr;
    (*mesh_virtual_address_callback)(mesh_virtual_address_arg);
}

void mesh_virtual_address(btstack_crypto_aes128_cmac_t * request, const uint8_t * label_uuid, uint16_t * addr, void (* callback)(void * arg), void * callback_arg){
    mesh_virtual_address_callback  = callback;
    mesh_virtual_address_arg       = callback_arg;
    mesh_virtual_address_hash      = addr;
    btstack_crypto_aes128_cmac_message(request, mesh_salt_vtad, 16, label_uuid, mesh_virtual_address_temp, mesh_virtual_address_temp_callback, request);
}

//
static void *  mesh_network_key_derive_arg;
static void (* mesh_network_key_derive_callback)(void * arg);
static mesh_network_key_t * mesh_network_key_derive_key;

// AES-CMAC_ZERO('nhbk')
static const uint8_t mesh_salt_nhbk[] = {
        0x2c, 0x24, 0x61, 0x9a, 0xb7, 0x93, 0xc1, 0x23, 0x3f, 0x6e, 0x22, 0x67, 0x38, 0x39, 0x3d, 0xec, };

// AES-CMAC_ZERO('nkik')
static const uint8_t mesh_salt_nkik[] = {
        0xF8, 0x79, 0x5A, 0x1A, 0xAB, 0xF1, 0x82, 0xE4, 0xF1, 0x63, 0xD8, 0x6E, 0x24, 0x5E, 0x19, 0xF4};

static const uint8_t id128_tag[] = { 'i', 'd', '1', '2', '8', 0x01};

// k2: NID (7), EncryptionKey (128), PrivacyKey (128)
static uint8_t k2_result[33];

static void mesh_network_key_derive_network_id_calculated(void * arg) {
    UNUSED(arg);
    // done
    (*mesh_network_key_derive_callback)(mesh_network_key_derive_arg);
}

static void mesh_network_key_derive_k2_calculated(void * arg){
    // store
    mesh_network_key_derive_key->nid = k2_result[0];
    memcpy(mesh_network_key_derive_key->encryption_key, &k2_result[1], 16);
    memcpy(mesh_network_key_derive_key->privacy_key, &k2_result[17], 16);

    // calculate Network ID / k3
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    mesh_k3(request, mesh_network_key_derive_key->net_key, mesh_network_key_derive_key->network_id, &mesh_network_key_derive_network_id_calculated, request);
}

static void mesh_network_key_derive_identity_key_calculated(void *arg) {
    // calc k2
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    mesh_k2(request, mesh_network_key_derive_key->net_key, k2_result, &mesh_network_key_derive_k2_calculated, request);
}

static void mesh_network_key_derive_beacon_key_calculated(void *arg){
    // calc identity key
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
    mesh_k1(request, mesh_network_key_derive_key->net_key, 16, mesh_salt_nkik, id128_tag, sizeof(id128_tag),
            mesh_network_key_derive_key->identity_key, &mesh_network_key_derive_identity_key_calculated, request);
}

void mesh_network_key_derive(btstack_crypto_aes128_cmac_t * request, mesh_network_key_t * network_key, void (* callback)(void * arg), void * callback_arg){
    mesh_network_key_derive_callback = callback;
    mesh_network_key_derive_arg = callback_arg;
    mesh_network_key_derive_key = network_key;

    // calc k1 using
    mesh_k1(request, mesh_network_key_derive_key->net_key, 16, mesh_salt_nhbk, id128_tag, sizeof(id128_tag),
            mesh_network_key_derive_key->beacon_key, &mesh_network_key_derive_beacon_key_calculated, request);
}

void mesh_transport_key_calc_aid(btstack_crypto_aes128_cmac_t * request, mesh_transport_key_t * app_key, void (* callback)(void * arg), void * callback_arg){
    mesh_k4(request, app_key->key, &app_key->aid, callback, callback_arg);
}


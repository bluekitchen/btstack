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

#include <stdint.h>
#include <string.h>
#include "btstack_debug.h"
#include "mesh_crypto.h"

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
    btstack_crypto_aes128_cmac_t * request = (btstack_crypto_aes128_cmac_t*) arg;
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

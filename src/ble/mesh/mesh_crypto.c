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

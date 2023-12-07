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

/**
 * Crypto-related functions
 *
 * Central place for all crypto-related functions with completion callbacks to allow
 * using of MCU crypto peripherals or the Bluetooth controller
 */

#ifndef BTSTACK_CTRYPTO_H
#define BTSTACK_CTRYPTO_H

#include "btstack_defines.h"
#include "btstack_config.h"

#if defined __cplusplus
extern "C" {
#endif

#define CMAC_TEMP_API

typedef enum {
	BTSTACK_CRYPTO_RANDOM,
	BTSTACK_CRYPTO_AES128,
	BTSTACK_CRYPTO_CMAC_GENERATOR,
	BTSTACK_CRYPTO_CMAC_MESSAGE,
	BTSTACK_CRYPTO_ECC_P256_GENERATE_KEY,
	BTSTACK_CRYPTO_ECC_P256_CALCULATE_DHKEY,
	BTSTACK_CRYPTO_CCM_DIGEST_BLOCK,
	BTSTACK_CRYPTO_CCM_ENCRYPT_BLOCK,
	BTSTACK_CRYPTO_CCM_DECRYPT_BLOCK,
} btstack_crypto_operation_t;

typedef struct {
	btstack_context_callback_registration_t context_callback;
	btstack_crypto_operation_t              operation;	
} btstack_crypto_t;

typedef struct {
	btstack_crypto_t btstack_crypto;
	uint8_t  * buffer;
	uint16_t   size;
} btstack_crypto_random_t;

typedef struct {
	btstack_crypto_t btstack_crypto;
	const uint8_t  * key;
	const uint8_t  * plaintext;
	uint8_t  * ciphertext;
} btstack_crypto_aes128_t;

typedef struct {
	btstack_crypto_t btstack_crypto;
	const uint8_t  * key;
	uint16_t         size;
	union {
		uint8_t (*get_byte_callback)(uint16_t pos);
		const uint8_t * message;
	} data;
	uint8_t  * hash;
} btstack_crypto_aes128_cmac_t;

typedef struct {
	btstack_crypto_t btstack_crypto;
	uint8_t * public_key;
    uint8_t * dhkey;
} btstack_crypto_ecc_p256_t;

typedef enum {
    CCM_CALCULATE_X1,
    CCM_W4_X1,
    CCM_CALCULATE_AAD_XN,
    CCM_W4_AAD_XN,
    CCM_CALCULATE_XN,
    CCM_W4_XN,
    CCM_CALCULATE_S0,
    CCM_W4_S0,
    CCM_CALCULATE_SN,
    CCM_W4_SN,
} btstack_crypto_ccm_state_t;

typedef struct {
	btstack_crypto_t btstack_crypto;
	btstack_crypto_ccm_state_t state;
	const uint8_t * key;
	const uint8_t * nonce;
	const uint8_t * input;
	uint8_t       * output;
	uint8_t         x_i[16];
	uint16_t        aad_offset;
	uint16_t        aad_len;
	uint16_t        message_len;
	uint16_t        counter;
	uint16_t        block_len;
	uint8_t         auth_len;
	uint8_t         aad_remainder_len;
} btstack_crypto_ccm_t;

/** 
 * Initialize crypto functions
 */
void btstack_crypto_init(void);

/** 
 * Generate random data
 * @param request
 * @param buffer for output
 * @param size of requested random data
 * @param callback
 * @param callback_arg
 * @note request needs to stay avaliable until callback (i.e. not provided on stack)
 */
void btstack_crypto_random_generate(btstack_crypto_random_t * request, uint8_t * buffer, uint16_t size, void (* callback)(void * arg), void * callback_arg);

/** 
 * Encrypt plaintext using AES128
 * @param request
 * @param key (16 bytes)
 * @param plaintext (16 bytes)
 * @param ciphertext (16 bytes)
 * @param callback
 * @param callback_arg
 * @note request needs to stay avaliable until callback (i.e. not provided on stack)
 */
void btstack_crypto_aes128_encrypt(btstack_crypto_aes128_t * request, const uint8_t * key, const uint8_t * plaintext, uint8_t * ciphertext, void (* callback)(void * arg), void * callback_arg);

/**
 * Calculate Cipher-based Message Authentication Code (CMAC) using AES128 and a generator function to provide data
 * @param request
 * @param key (16 bytes)
 * @param size of message
 * @param generator provides byte at requested position
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_aes128_cmac_generator(btstack_crypto_aes128_cmac_t * request, const uint8_t * key, uint16_t size, uint8_t (*get_byte_callback)(uint16_t pos), uint8_t * hash, void (* callback)(void * arg), void * callback_arg);

/**
 * Calculate Cipher-based Message Authentication Code (CMAC) using AES128 and complete message
 * @param request
 * @param key (16 bytes)
 * @param size of message
 * @param message
 * @param hash result
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_aes128_cmac_message(btstack_crypto_aes128_cmac_t * request, const uint8_t * key, uint16_t size, const uint8_t * message,  uint8_t * hash, void (* callback)(void * arg), void * callback_arg);

/**
 * Calculate AES128-CMAC with key ZERO and complete message
 * @param request
 * @param size of message
 * @param message
 * @param hash
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_aes128_cmac_zero(btstack_crypto_aes128_cmac_t * request, uint16_t size, const uint8_t * message, uint8_t * hash, void (* callback)(void * arg), void * callback_arg);

/**
 * Generate Elliptic Curve Public/Private Key Pair (FIPS P-256)
 * @note BTstack uses a single ECC key pair per reset. 
 * @note If LE Controller is used for ECC, private key cannot be read or managed
 * @param request
 * @param public_key (64 bytes)
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_ecc_p256_generate_key(btstack_crypto_ecc_p256_t * request, uint8_t * public_key, void (* callback)(void * arg), void * callback_arg);

/**
 * Calculate Diffie-Hellman Key based on local private key and remote public key
 * @note Bluetooth Core v5.1+ requires the Controller to return an error if the public key is invalid
 *       In this case, dhkey will be set to 0xff..ff
 * @param request
 * @param public_key (64 bytes)
 * @param dhkey (32 bytes)
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_ecc_p256_calculate_dhkey(btstack_crypto_ecc_p256_t * request, const uint8_t * public_key, uint8_t * dhkey, void (* callback)(void * arg), void * callback_arg);

/*
 * Validate public key
 * @note Not implemented for ECC in Controller. @see btstack_crypto_ecc_p256_calculate_dhkey
 * @param public_key (64 bytes)
 * @result 0 == valid
 */
int btstack_crypto_ecc_p256_validate_public_key(const uint8_t * public_key);

/** 
 * Initialize Counter with CBC-MAC for Bluetooth Mesh (L=2)
 * @param request
 * @param nonce
 * @param key
 * @param message_len
 * @param additional_authenticated_data_len must be smaller than 0xff00
 * @param auth_len
 */
void btstack_crypto_ccm_init(btstack_crypto_ccm_t * request, const uint8_t * key, const uint8_t * nonce, uint16_t message_len, uint16_t additional_authenticated_data_len, uint8_t auth_len);

/** 
 * Get authentication value after encrypt or decrypt operation
 * @param request
 * @param authentication_value
 */
void btstack_crypto_ccm_get_authentication_value(btstack_crypto_ccm_t * request, uint8_t * authentication_value);

/** 
 * Digest Additional Authentication Data - can be called multipled times up to total additional_authenticated_data_len specified in btstack_crypto_ccm_init
 * @param request
 * @param additional_authenticated_data
 * @param additional_authenticated_data_len
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_ccm_digest(btstack_crypto_ccm_t * request, uint8_t * additional_authenticated_data, uint16_t additional_authenticated_data_len, void (* callback)(void * arg), void * callback_arg);

/**
 * Encrypt block - can be called multiple times. len must be a multiply of 16 for all but the last call
 * @param request
 * @param len (16 bytes for all but the last block)
 * @param plaintext  (16 bytes)
 * @param ciphertext (16 bytes)
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_ccm_encrypt_block(btstack_crypto_ccm_t * request, uint16_t len, const uint8_t * plaintext, uint8_t * ciphertext, void (* callback)(void * arg), void * callback_arg);

/**
 * Decrypt block - can be called multiple times. len must be a multiply of 16 for all but the last call
 * @param request
 * @param len (16 for all but last block)
 * @param ciphertext (16 bytes)
 * @param plaintext  (16 bytes)
 * @param callback
 * @param callback_arg
 */
void btstack_crypto_ccm_decrypt_block(btstack_crypto_ccm_t * request, uint16_t len, const uint8_t * ciphertext, uint8_t * plaintext, void (* callback)(void * arg), void * callback_arg);

#if defined(ENABLE_SOFTWARE_AES128) || defined (HAVE_AES128)
/** 
 * Encrypt plaintext using AES128
 * @note Prototype for custom AES128 implementation
 * @param key (16 bytes)
 * @param plaintext (16 bytes)
 * @param ciphertext (16 bytes)
 */
void btstack_aes128_calc(const uint8_t * key, const uint8_t * plaintext, uint8_t * ciphertext);
#endif

/**
 * @brief De-Init BTstack Crypto
 */
void btstack_crypto_deinit(void);

// PTS testing only - not possible when using Buetooth Controller for ECC operations
void btstack_crypto_ecc_p256_set_key(const uint8_t * public_key, const uint8_t * private_key);

// Unit testing
int btstack_crypto_idle(void);
void btstack_crypto_reset(void);

#if defined __cplusplus
}
#endif

#endif /* BTSTACK_CTRYPTO_H */

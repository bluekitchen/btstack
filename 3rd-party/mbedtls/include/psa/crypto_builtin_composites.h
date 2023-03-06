/*
 *  Context structure declaration of the Mbed TLS software-based PSA drivers
 *  called through the PSA Crypto driver dispatch layer.
 *  This file contains the context structures of those algorithms which need to
 *  rely on other algorithms, i.e. are 'composite' algorithms.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * \note This header and its content is not part of the Mbed TLS API and
 * applications must not depend on it. Its main purpose is to define the
 * multi-part state objects of the Mbed TLS software-based PSA drivers. The
 * definition of these objects are then used by crypto_struct.h to define the
 * implementation-defined types of PSA multi-part state objects.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef PSA_CRYPTO_BUILTIN_COMPOSITES_H
#define PSA_CRYPTO_BUILTIN_COMPOSITES_H
#include "mbedtls/private_access.h"

#include <psa/crypto_driver_common.h>

/*
 * MAC multi-part operation definitions.
 */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
#define MBEDTLS_PSA_BUILTIN_MAC
#endif

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
typedef struct {
    /** The HMAC algorithm in use */
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    /** The hash context. */
    struct psa_hash_operation_s hash_ctx;
    /** The HMAC part of the context. */
    uint8_t MBEDTLS_PRIVATE(opad)[PSA_HMAC_MAX_HASH_BLOCK_SIZE];
} mbedtls_psa_hmac_operation_t;

#define MBEDTLS_PSA_HMAC_OPERATION_INIT { 0, PSA_HASH_OPERATION_INIT, { 0 } }
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */

#include "mbedtls/cmac.h"

typedef struct {
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    union {
        unsigned MBEDTLS_PRIVATE(dummy); /* Make the union non-empty even with no supported algorithms. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_psa_hmac_operation_t MBEDTLS_PRIVATE(hmac);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_cipher_context_t MBEDTLS_PRIVATE(cmac);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
    } MBEDTLS_PRIVATE(ctx);
} mbedtls_psa_mac_operation_t;

#define MBEDTLS_PSA_MAC_OPERATION_INIT { 0, { 0 } }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
#define MBEDTLS_PSA_BUILTIN_AEAD  1
#endif

/* Context structure for the Mbed TLS AEAD implementation. */
typedef struct {
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    psa_key_type_t MBEDTLS_PRIVATE(key_type);

    unsigned int MBEDTLS_PRIVATE(is_encrypt) : 1;

    uint8_t MBEDTLS_PRIVATE(tag_length);

    union {
        unsigned dummy; /* Enable easier initializing of the union. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
        mbedtls_ccm_context MBEDTLS_PRIVATE(ccm);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
        mbedtls_gcm_context MBEDTLS_PRIVATE(gcm);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
        mbedtls_chachapoly_context MBEDTLS_PRIVATE(chachapoly);
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */

    } ctx;

} mbedtls_psa_aead_operation_t;

#define MBEDTLS_PSA_AEAD_OPERATION_INIT { 0, 0, 0, 0, { 0 } }

#include "mbedtls/ecdsa.h"

/* Context structure for the Mbed TLS interruptible sign hash implementation. */
typedef struct {
#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
    mbedtls_ecdsa_context *MBEDTLS_PRIVATE(ctx);
    mbedtls_ecdsa_restart_ctx MBEDTLS_PRIVATE(restart_ctx);

    uint32_t MBEDTLS_PRIVATE(num_ops);

    size_t MBEDTLS_PRIVATE(coordinate_bytes);
    psa_algorithm_t MBEDTLS_PRIVATE(alg);
    mbedtls_md_type_t MBEDTLS_PRIVATE(md_alg);
    uint8_t MBEDTLS_PRIVATE(hash)[PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS)];
    size_t MBEDTLS_PRIVATE(hash_length);

#else
    /* Make the struct non-empty if algs not supported. */
    unsigned MBEDTLS_PRIVATE(dummy);

#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) &&
        * defined( MBEDTLS_ECP_RESTARTABLE ) */
} mbedtls_psa_sign_hash_interruptible_operation_t;

#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
#define MBEDTLS_PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { { 0 }, { 0 }, 0, 0, 0, 0, 0, 0 }
#else
#define MBEDTLS_PSA_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#endif

/* Context structure for the Mbed TLS interruptible verify hash
 * implementation.*/
typedef struct {
#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)

    mbedtls_ecdsa_context *MBEDTLS_PRIVATE(ctx);
    mbedtls_ecdsa_restart_ctx MBEDTLS_PRIVATE(restart_ctx);

    uint32_t MBEDTLS_PRIVATE(num_ops);

    uint8_t MBEDTLS_PRIVATE(hash)[PSA_BITS_TO_BYTES(PSA_VENDOR_ECC_MAX_CURVE_BITS)];
    size_t MBEDTLS_PRIVATE(hash_length);

    mbedtls_mpi MBEDTLS_PRIVATE(r);
    mbedtls_mpi MBEDTLS_PRIVATE(s);

#else
    /* Make the struct non-empty if algs not supported. */
    unsigned MBEDTLS_PRIVATE(dummy);

#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) &&
        * defined( MBEDTLS_ECP_RESTARTABLE ) */

} mbedtls_psa_verify_hash_interruptible_operation_t;

#if (defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)) && \
    defined(MBEDTLS_ECP_RESTARTABLE)
#define MBEDTLS_VERIFY_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { { 0 }, { 0 }, 0, 0, 0, 0, { 0 }, \
        { 0 } }
#else
#define MBEDTLS_VERIFY_SIGN_HASH_INTERRUPTIBLE_OPERATION_INIT { 0 }
#endif



#endif /* PSA_CRYPTO_BUILTIN_COMPOSITES_H */

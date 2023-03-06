/*
 * Test driver for key agreement functions.
 */
/*  Copyright The Mbed TLS Contributors
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

#include <test/helpers.h>

#if defined(MBEDTLS_PSA_CRYPTO_DRIVERS) && defined(PSA_CRYPTO_DRIVER_TEST)

#include "psa/crypto.h"
#include "psa_crypto_core.h"
#include "psa_crypto_ecp.h"

#include "test/drivers/key_agreement.h"
#include "test/drivers/test_driver.h"

#include <string.h>

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1)
#include "libtestdriver1/include/psa/crypto.h"
#include "libtestdriver1/library/psa_crypto_ecp.h"
#endif

mbedtls_test_driver_key_agreement_hooks_t
    mbedtls_test_driver_key_agreement_hooks = MBEDTLS_TEST_DRIVER_KEY_AGREEMENT_INIT;

psa_status_t mbedtls_test_transparent_key_agreement(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *peer_key,
    size_t peer_key_length,
    uint8_t *shared_secret,
    size_t shared_secret_size,
    size_t *shared_secret_length)
{
    mbedtls_test_driver_key_agreement_hooks.hits++;

    if (mbedtls_test_driver_key_agreement_hooks.forced_status != PSA_SUCCESS) {
        return mbedtls_test_driver_key_agreement_hooks.forced_status;
    }

    if (mbedtls_test_driver_key_agreement_hooks.forced_output != NULL) {
        if (mbedtls_test_driver_key_agreement_hooks.forced_output_length > shared_secret_size) {
            return PSA_ERROR_BUFFER_TOO_SMALL;
        }

        memcpy(shared_secret, mbedtls_test_driver_key_agreement_hooks.forced_output,
               mbedtls_test_driver_key_agreement_hooks.forced_output_length);
        *shared_secret_length = mbedtls_test_driver_key_agreement_hooks.forced_output_length;

        return PSA_SUCCESS;
    }

    if (PSA_ALG_IS_ECDH(alg)) {
#if (defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
        defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_ALG_ECDH))
        return libtestdriver1_mbedtls_psa_key_agreement_ecdh(
            (const libtestdriver1_psa_key_attributes_t *) attributes,
            key_buffer, key_buffer_size,
            alg, peer_key, peer_key_length,
            shared_secret, shared_secret_size,
            shared_secret_length);
#elif defined(MBEDTLS_PSA_BUILTIN_ALG_ECDH)
        return mbedtls_psa_key_agreement_ecdh(
            attributes,
            key_buffer, key_buffer_size,
            alg, peer_key, peer_key_length,
            shared_secret, shared_secret_size,
            shared_secret_length);
#else
        (void) attributes;
        (void) key_buffer;
        (void) key_buffer_size;
        (void) peer_key;
        (void) peer_key_length;
        (void) shared_secret;
        (void) shared_secret_size;
        (void) shared_secret_length;
        return PSA_ERROR_NOT_SUPPORTED;
#endif
    } else {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

}

psa_status_t mbedtls_test_opaque_key_agreement(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *peer_key,
    size_t peer_key_length,
    uint8_t *shared_secret,
    size_t shared_secret_size,
    size_t *shared_secret_length)
{
    (void) attributes;
    (void) key_buffer;
    (void) key_buffer_size;
    (void) alg;
    (void) peer_key;
    (void) peer_key_length;
    (void) shared_secret;
    (void) shared_secret_size;
    (void) shared_secret_length;
    return PSA_ERROR_NOT_SUPPORTED;
}

#endif /* MBEDTLS_PSA_CRYPTO_DRIVERS && PSA_CRYPTO_DRIVER_TEST */

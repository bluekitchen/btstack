/**
 * \file ecp_invasive.h
 *
 * \brief ECP module: interfaces for invasive testing only.
 *
 * The interfaces in this file are intended for testing purposes only.
 * They SHOULD NOT be made available in library integrations except when
 * building the library for testing.
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
#ifndef MBEDTLS_ECP_INVASIVE_H
#define MBEDTLS_ECP_INVASIVE_H

#include "common.h"
#include "mbedtls/bignum.h"
#include "bignum_mod.h"
#include "mbedtls/ecp.h"

#if defined(MBEDTLS_TEST_HOOKS) && defined(MBEDTLS_ECP_C)

#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) ||   \
    defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) ||   \
    defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
/* Preconditions:
 *   - bits is a multiple of 64 or is 224
 *   - c is -1 or -2
 *   - 0 <= N < 2^bits
 *   - N has room for bits plus one limb
 *
 * Behavior:
 * Set N to c * 2^bits + old_value_of_N.
 */
void mbedtls_ecp_fix_negative(mbedtls_mpi *N, signed char c, size_t bits);
#endif

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
/** Generate a private key on a Montgomery curve (Curve25519 or Curve448).
 *
 * This function implements key generation for the set of secret keys
 * specified in [Curve25519] p. 5 and in [Curve448]. The resulting value
 * has the lower bits masked but is not necessarily canonical.
 *
 * \note            - [Curve25519] http://cr.yp.to/ecdh/curve25519-20060209.pdf
 *                  - [RFC7748] https://tools.ietf.org/html/rfc7748
 *
 * \p high_bit      The position of the high-order bit of the key to generate.
 *                  This is the bit-size of the key minus 1:
 *                  254 for Curve25519 or 447 for Curve448.
 * \param d         The randomly generated key. This is a number of size
 *                  exactly \p n_bits + 1 bits, with the least significant bits
 *                  masked as specified in [Curve25519] and in [RFC7748] §5.
 * \param f_rng     The RNG function.
 * \param p_rng     The RNG context to be passed to \p f_rng.
 *
 * \return          \c 0 on success.
 * \return          \c MBEDTLS_ERR_ECP_xxx or MBEDTLS_ERR_MPI_xxx on failure.
 */
int mbedtls_ecp_gen_privkey_mx(size_t n_bits,
                               mbedtls_mpi *d,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng);

#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)

/** Fast quasi-reduction modulo p192 (FIPS 186-3 D.2.1)
 *
 * This operation expects a 384 bit MPI and the result of the reduction
 * is a 192 bit MPI.
 *
 * \param[in,out]   Np  The address of the MPI to be converted.
 *                      Must have twice as many limbs as the modulus.
 *                      Upon return this holds the reduced value. The bitlength
 *                      of the reduced value is the same as that of the modulus
 *                      (192 bits).
 * \param[in]       Nn  The length of \p Np in limbs.
 */
MBEDTLS_STATIC_TESTABLE
int mbedtls_ecp_mod_p192_raw(mbedtls_mpi_uint *Np, size_t Nn);

#endif /* MBEDTLS_ECP_DP_SECP192R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)

/** Fast quasi-reduction modulo p521 = 2^521 - 1 (FIPS 186-3 D.2.5)
 *
 * \param[in,out]   X       The address of the MPI to be converted.
 *                          Must have twice as many limbs as the modulus
 *                          (the modulus is 521 bits long). Upon return this
 *                          holds the reduced value. The reduced value is
 *                          in range `0 <= X < 2 * N` (where N is the modulus).
 *                          and its the bitlength is one plus the bitlength
 *                          of the modulus.
 * \param[in]       X_limbs The length of \p X in limbs.
 *
 * \return          \c 0 on success.
 * \return          #MBEDTLS_ERR_ECP_BAD_INPUT_DATA if \p X_limbs does not have
 *                  twice as many limbs as the modulus.
 */
MBEDTLS_STATIC_TESTABLE
int mbedtls_ecp_mod_p521_raw(mbedtls_mpi_uint *X, size_t X_limbs);

#endif /* MBEDTLS_ECP_DP_SECP521R1_ENABLED */

/** Initialise a modulus with hard-coded const curve data.
 *
 * \note            The caller is responsible for the \p N modulus' memory.
 *                  mbedtls_mpi_mod_modulus_free(&N) should be invoked at the
 *                  end of its lifecycle.
 *
 * \param[in,out] N The address of the modulus structure to populate.
 *                  Must be initialized.
 * \param[in] id    The mbedtls_ecp_group_id for which to initialise the modulus.
 * \param[in] ctype The mbedtls_ecp_curve_type identifier for a coordinate modulus (P)
 *                  or a scalar modulus (N).
 *
 * \return          \c 0 if successful.
 * \return          #MBEDTLS_ERR_ECP_BAD_INPUT_DATA if the given MPIs do not
 *                  have the correct number of limbs.
 *
 */
MBEDTLS_STATIC_TESTABLE
int mbedtls_ecp_modulus_setup(mbedtls_mpi_mod_modulus *N,
                              const mbedtls_ecp_group_id id,
                              const mbedtls_ecp_curve_type ctype);

#endif /* MBEDTLS_TEST_HOOKS && MBEDTLS_ECP_C */

#endif /* MBEDTLS_ECP_INVASIVE_H */

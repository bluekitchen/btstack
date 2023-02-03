/*
 *  Configuration of mbedTLS containing only the ASN.1 parser.
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Copyright (C) 2016, Linaro Ltd
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
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

/*
 * Minimal configuration for using TLS in the bootloader
 *
 * - ed25519 signature verification
 */

#ifndef MCUBOOT_MBEDTLS_CONFIG_ED25519
#define MCUBOOT_MBEDTLS_CONFIG_ED25519

#ifdef CONFIG_MCUBOOT_SERIAL
/* Mcuboot uses mbedts-base64 for serial protocol encoding. */
#define MBEDTLS_BASE64_C
#endif

/* System support */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

/* STD functions */
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS

#define MBEDTLS_PLATFORM_EXIT_ALT
#define MBEDTLS_PLATFORM_PRINTF_ALT
#define MBEDTLS_PLATFORM_SNPRINTF_ALT

#if !defined(CONFIG_ARM)
#define MBEDTLS_HAVE_ASM
#endif

#define MBEDTLS_CIPHER_MODE_CTR

/* mbed TLS modules */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_AES_C

/* Save RAM by adjusting to our exact needs */
//#define MBEDTLS_ECP_MAX_BITS             2048

#define MBEDTLS_MPI_MAX_SIZE              64

//#define MBEDTLS_SSL_MAX_CONTENT_LEN 1024

/* Save ROM and a few bytes of RAM by specifying our own ciphersuite list */
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

#include "mbedtls/check_config.h"

#endif /* MCUBOOT_MBEDTLS_CONFIG_RSA */

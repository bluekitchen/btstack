
#include <zephyr.h>
#include <string.h>

#include "os/os_heap.h"

#ifdef CONFIG_BOOT_USE_MBEDTLS

#include <mbedtls/platform.h>
#include <mbedtls/memory_buffer_alloc.h>

/*
 * This is the heap for mbed TLS.  The value needed depends on the key
 * size and algorithm used.
 *
 * - RSA-2048 signing without encryption is known to work well with 6144 bytes;
 * - When using RSA-2048-OAEP encryption + RSA-2048 signing, or RSA-3072
 *   signing (no encryption) 10240 bytes seems to be enough.
 *
 * NOTE: RSA-3072 signing + RSA-2048-OAEP might require growing the size...
 */
#if (CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN == 2048) && !defined(CONFIG_BOOT_ENCRYPT_RSA)
#define CRYPTO_HEAP_SIZE 6144
#else
#  if !defined(MBEDTLS_RSA_NO_CRT)
#  define CRYPTO_HEAP_SIZE 10240
#  else
#  define CRYPTO_HEAP_SIZE 16384
#  endif
#endif

static unsigned char mempool[CRYPTO_HEAP_SIZE];

/*
 * Initialize mbedtls to be able to use the local heap.
 */
void os_heap_init(void)
{
    mbedtls_memory_buffer_alloc_init(mempool, sizeof(mempool));
}
#else
void os_heap_init(void)
{
}
#endif

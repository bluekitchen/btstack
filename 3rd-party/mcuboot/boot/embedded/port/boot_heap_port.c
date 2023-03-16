
#include "port/boot_heap_port.h"

#include <mcuboot_config/mcuboot_config.h>
//#include <mbedtls/platform.h>
//#include <mbedtls/memory_buffer_alloc.h>

#ifdef MCUBOOT_USE_MBED_TLS
    #define CRYPTO_HEAP_SIZE (1024 * 16)
    static unsigned char crypto_heap[CRYPTO_HEAP_SIZE];
#endif

void  boot_port_heap_init( void )
{
#ifdef MCUBOOT_USE_MBED_TLS
    mbedtls_memory_buffer_alloc_init(crypto_heap, sizeof(crypto_heap));
#else
    /* Tinycrypt does not require a heap.
     * However, if MCUBoot loader is used, this needs implementation. */
#endif
}

void *boot_port_malloc( size_t size )
{
    return (void *)NULL;
}

void  boot_port_free( void *mem )
{

}

void *boot_port_realloc( void *ptr, size_t size )
{
    return (void *)NULL;
}


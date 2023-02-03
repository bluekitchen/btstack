
#ifndef _BOOT_HEAP_PORT_H_
#define _BOOT_HEAP_PORT_H_

#include <stddef.h>

void  boot_port_heap_init( void );
void *boot_port_malloc( size_t size );
void  boot_port_free( void *mem );
void *boot_port_realloc( void *ptr, size_t size );

#endif
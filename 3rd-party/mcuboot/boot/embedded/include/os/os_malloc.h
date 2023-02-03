
#ifndef H_OS_MALLOC_
#define H_OS_MALLOC_

#include "port/boot_heap_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef  malloc
#define malloc  boot_port_malloc

#undef  free
#define free    boot_port_free

#undef  realloc
#define realloc  boot_port_realloc

#ifdef __cplusplus
}
#endif

#endif

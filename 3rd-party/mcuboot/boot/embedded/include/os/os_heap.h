#ifndef H_OS_HEAP_
#define H_OS_HEAP_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *os_malloc(size_t size);
void os_free(void *mem);
void *os_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif


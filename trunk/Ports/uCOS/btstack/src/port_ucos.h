/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  port_ucos.h
 *
 *  uC/OS porting layer.
 *
 *  Created by Albis Technologies.
 */

#ifndef PORT_UCOS_H
#define PORT_UCOS_H

/* uC/OS-II include files. */
#include <ucos_ii.h>
#include <lib_mem.h>
#include <lib_str.h>

/* These standard includes are allowed. */
#include <stdarg.h>
#include <stdint.h>

/* OS version depending macros. */
#define USE_OS_NO_ERROR    OS_NO_ERR
#define USE_OS_TIMEOUT     OS_TIMEOUT

/* Convert msec to OS ticks. */
#define MSEC_TO_TICKS(ms) \
    ((ms > 0u) ? (((ms * OS_TICKS_PER_SEC) + 1000u - 1u) / 1000u) : 0u)

/* Map stdlib functions to uC/LIB ones. */
#define memcpy(a, b, c)    (Mem_Copy((void *)(a), (void *)(b), c))
#define memset(a, b, c)    (Mem_Set((void *)(a), b, c))
#define bzero(a, c)        (Mem_Set((void *)(a), 0, c))
#define memcmp(a, b, c)    (!Mem_Cmp((void *)(a), (void *)(b), c))
#define strncpy(a, b, c)   (Str_Copy_N((CPU_CHAR *)(a), (CPU_CHAR *)b, c))
#define strncmp(a, b, c)   (Str_Cmp_N((CPU_CHAR *)(a), (CPU_CHAR *)b, c))
#define strlen(a)          (Str_Len((CPU_CHAR *)(a)))

extern void * memmove(void *dst, const void *src, unsigned int length);

/* There are no such uC-LIB implementations. */
#define sprintf(a, b, ...) __error__
#define sscanf(a, ...)     __error__

/* Time function. */
struct timeval
{
    unsigned long tv_sec;
    unsigned long tv_usec;
};
int gettimeofday(struct timeval *tv, void *tzp);

/* Host name. */
int gethostname(char *name, size_t len);

/* Debug output. */
#include "bsp_debug.h"

#define ENABLE_BTSTACK_DEBUG_OUTPUT 0

#if(ENABLE_BTSTACK_DEBUG_OUTPUT == 1)
  #define printf(...) { printos(__VA_ARGS__); }
  #define fprintf(_fd, ...) { printos(__VA_ARGS__); }
  #define log_debug(...) { printos(__VA_ARGS__); }
  #define log_info(...) { printos(__VA_ARGS__); }
  #define log_error(...) { printos(__VA_ARGS__); }
#else
  #define printf(...)
  #define fprintf(_fd, ...)
  #define log_debug(...)
  #define log_info(...)
  #define log_error(...)
#endif /* ENABLE_BTSTACK_DEBUG_OUTPUT */

#endif /* PORT_UCOS_H */

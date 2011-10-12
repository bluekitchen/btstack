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
 *  port_ucos.c
 *
 *  uC/OS porting layer.
 *
 *  Created by Albis Technologies.
 */

#include <btstack/port_ucos.h>

#include "../config.h"

/*=============================================================================
=============================================================================*/
int gettimeofday(struct timeval *tv, void *tzp)
{
    INT32U ticks = OSTimeGet();

    tv->tv_sec = ticks / OS_TICKS_PER_SEC;
    tv->tv_usec = (ticks - (tv->tv_sec * OS_TICKS_PER_SEC)) *
                  (1000000UL / OS_TICKS_PER_SEC);

    return 0;
}

/*=============================================================================
=============================================================================*/
uint32_t embedded_get_ticks(void)
{
    return OSTimeGet();
}

/*=============================================================================
=============================================================================*/
uint32_t embedded_ticks_for_ms(uint32_t time_in_ms)
{
    return MSEC_TO_TICKS(time_in_ms);
}

/*=============================================================================
=============================================================================*/
int gethostname(char *name, size_t len)
{
    Str_Copy_N((CPU_CHAR *)name, (CPU_CHAR *)BT_DEV_NAME, len - 1);
    return 0;
}

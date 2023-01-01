/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
 *  retarget_chibios.c
 * 
 *  retarget printf and friends for ChibiOS/HAL
 */

#define BTSTACK_FILE__ "hal_uart_dma_chibios.c"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

// retarget printf and friends

#ifndef ENABLE_SEGGER_RTT

#include "hal.h"

/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */

int _write(int file, char *ptr, int len);
int _write(int file, char *ptr, int len){
    UNUSED(file);
#ifdef HAL_DEBUG_SERIAL
    sdWrite(&HAL_DEBUG_SERIAL, (const uint8_t *) ptr, len);
#endif
    errno = EIO;
    return -1;
}
#endif

int _read(int file, char * ptr, int len){
    UNUSED(file);
    UNUSED(ptr);
    UNUSED(len);
    return -1;
}

int _lseek(int file){
    UNUSED(file);
    return -1;
}

int _close(int file){
    UNUSED(file);
    return -1;
}

int _isatty(int file){
    UNUSED(file);
    return -1;
}

int _fstat(int file){
    UNUSED(file);
    return -1;
}

void _exit(int err){
    UNUSED(err);
    while(1);
}

int _kill(int pid){
    UNUSED(pid);
    return -1;
}

int _getpid(void){
    return -1;
}

void * _sbrk(intptr_t increment){
    UNUSED(increment);
    // btstack_assert(false);
    return (void*) -1;
}

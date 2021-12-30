/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "hal_compat.c"

 /**
 * various functions to deal with flaws and portability issues
 */

#include "hal_compat.h"
#include <msp430x54x.h>

// __delay_cycles is limited
void waitAboutOneSecond(void){
    int i;
    for (i=0;i<1000;i++) __delay_cycles(16000);
}

// access far text for MSP430X platform
#if defined(__GNUC__) && (__MSP430X__ > 0)

uint8_t FlashReadByte (uint32_t addr){
    
    uint8_t result;
    uint32_t register sr, flash;
    
    __asm__ __volatile__ (
                          "mov    r2  , %1   \n"
                          "bic    %3  , r2   \n"
                          "nop               \n"
                          "movx.a %4  , %2   \n"
                          "movx.b @%2, %0    \n"
                          "mov    %1 , r2    \n"
                          :"=X"(result),"=r"(sr),"=r"(flash)
                          :"i"(GIE),"m"(addr));
    
    return result;
}

// argument order matches memcpy
void FlashReadBlock(uint8_t *buffer, uint32_t addr,  uint16_t len){
    while (len){
        *buffer++ = FlashReadByte(addr++);
        len--;
    }
}

#endif

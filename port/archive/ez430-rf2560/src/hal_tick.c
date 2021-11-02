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


/*
 *  hal_tick.c
 *
 *  Implementation for MSP430 Experimenter board using 250 ms ticks provided by Timer A1
 *
 */

#include <msp430x54x.h>
#include <stdlib.h>
#include "hal_compat.h"

#include "hal_tick.h"

static void dummy_handler(void){};

static void (*tick_handler)(void) = &dummy_handler;

// Auxillary Clock (ACLK) = 32768 hz
// 8192 ticks = 1/4 second

#define TIMER_COUNTDOWN 8192

void hal_tick_init(void){
    TA1CCTL0 = CCIE;                   // CCR0 interrupt enabled
    TA1CTL = TASSEL_1 | MC_2 | TACLR;  // use ACLK (32768), contmode, clear TAR
    TA1CCR0 = TIMER_COUNTDOWN;    // -> 1/4 s
}

void hal_tick_set_handler(void (*handler)(void)){
    if (handler == NULL){
        tick_handler = &dummy_handler;
        return;
    }
    tick_handler = handler;
}

int  hal_tick_get_tick_period_in_ms(void){
    return 250;
}

// Timer A1 interrupt service routine
#ifdef __GNUC__
__attribute__((interrupt(TIMER1_A0_VECTOR)))
#endif
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=TIMER1_A0_VECTOR
__interrupt
#endif
void timerA0ISR(void){
    TA1CCR0 += TIMER_COUNTDOWN;
    (*tick_handler)();
    
    // force exit low power mode
    __bic_SR_register_on_exit(LPM0_bits);   // Exit active CPU
}

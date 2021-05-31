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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */


/*
 *  hal_cpu.c
 *
 *  Implementation for MSP430 Experimenter board using low power mode 0/3
 *
 */

#include "hal_cpu.h"

#include "hal_board.h"
#include "hal_compat.h"

#include <msp430x54x.h>

static uint8_t low_power_mode_for_sleep = LPM0_bits;

void hal_cpu_disable_irqs(void){

    // LED off
    LED1_OUT &= ~LED1_PIN;
    
    // disable irq
    __bic_SR_register(GIE);  
}

void hal_cpu_enable_irqs(void){

    // enable irq
    __bis_SR_register(GIE);  
    
    // LED on
    LED1_OUT |= LED1_PIN;
}

void hal_cpu_set_uart_needed_during_sleep(uint8_t enabled){
    if (enabled){
        LED2_OUT |= LED2_PIN;
        low_power_mode_for_sleep = LPM0_bits;
        return;
    }
    LED2_OUT &= ~LED2_PIN;
    low_power_mode_for_sleep = LPM3_bits;
}

void hal_cpu_enable_irqs_and_sleep(void){

    // enable irq and enter lpm0
    __bis_SR_register(low_power_mode_for_sleep + GIE);  

    // LED on
    LED1_OUT |= LED1_PIN;
}



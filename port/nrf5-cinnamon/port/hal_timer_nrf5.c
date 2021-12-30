/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hal_timer_nrf5.c"

/*
 *  hal_timer.c
 *  HAL for 32.768 kHz low power timer with 16 bit resolution
 *  @note Only uses one of multiple RTCs and only a single Capture-Compare unit
 */

#include <stddef.h>

#include "hal_timer.h"
#include "btstack_debug.h"

#include "nrf.h"

static void (*hal_timer_callback)(void);

void RTC0_IRQHandler(void){
    if (NRF_RTC0->EVENTS_COMPARE[0]){
        NRF_RTC0->EVENTS_COMPARE[0] = 0;
        btstack_assert(hal_timer_callback != NULL);
        (*hal_timer_callback)();
    }
}

void hal_timer_init(void) {
    /* Stop the timer first */
    NRF_RTC0->TASKS_STOP = 1;
    NRF_RTC0->TASKS_CLEAR = 1;

    /* Always no prescaler */
    NRF_RTC0->PRESCALER = 0;

    /* Clear overflow events and set overflow interrupt */
    NRF_RTC0->EVENTS_OVRFLW = 0;
    NRF_RTC0->INTENSET = RTC_INTENSET_OVRFLW_Msk;

    /* Start the timer */
    NRF_RTC0->TASKS_START = 1;

    /* Set isr in vector table and enable interrupt */
    NVIC_EnableIRQ( RTC0_IRQn );
}

void hal_timer_set_callback(void (*callback)(void)){
    hal_timer_callback = callback;
}

uint32_t hal_timer_get_ticks(void){
    return NRF_RTC0->COUNTER;
}

void hal_timer_stop(void){
    NRF_RTC0->INTENCLR =RTC_INTENCLR_COMPARE0_Msk;
}

void hal_timer_start(uint32_t timeout_ticks){
    NRF_RTC0->CC[0] = timeout_ticks & 0x00ffffff;
    NRF_RTC0->EVENTS_COMPARE[0] = 0;
    NRF_RTC0->INTENSET =RTC_INTENSET_COMPARE0_Msk;
}

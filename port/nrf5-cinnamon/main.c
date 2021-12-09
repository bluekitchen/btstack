/**
 * Copyright (c) 2014 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 * This file contains the source code for a sample application to blink LEDs.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "SEGGER_RTT.h"

#include "nrf.h"
#include "nrf52.h"
#include "nrf_delay.h"
#include "nrfx_clock.h"
#include "hal_timer.h"
#include "radio.h"
#include <stdio.h>

#include "btstack_memory.h"
#include "btstack_run_loop_embedded.h"
#include "controller.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "ble/le_device_db_tlv.h"
#include "hci_dump.h"
#include "hci_dump_segger_rtt_stdout.h"
#include "hci_dump_segger_rtt_binary.h"
#include "hci_dump_embedded_stdout.h"

void btstack_assert_failed(const char * file, uint16_t line_nr){
    printf("Assert: file %s, line %u\n", file, line_nr);
    while (1);
}

/** hal_time_ms.h */
#include "hal_time_ms.h"
extern uint32_t hal_timer_get_ticks(void);
uint32_t hal_time_ms(void){
    uint32_t ticks = hal_timer_get_ticks();
    uint32_t seconds = ticks >> 15; // / 32768
    uint32_t remaining_ms = (ticks & 0x7fff) * 1000 / 32768;
    return seconds * 1000 + remaining_ms;
}

/** hal_cpu.h */

// TODO: implement
void hal_cpu_disable_irqs(void){
    __disable_irq();
}

void hal_cpu_enable_irqs(void){
    __enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
    __asm__("wfe");	// go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}

static void lf_clock_init(void) {
    // select 32.768 kHz XTAL as LF Clock source and start
    NRF_CLOCK->LFCLKSRC = NRF_CLOCK_LFCLK_Xtal;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);
}

void btstack_main(void);
int main(void){

    // system init
    lf_clock_init();
    hal_timer_init();

#if 0
    // get startup time, around 9 ticks and verify that we don't need to wait until it's disabled
    uint32_t t0 = hal_timer_get_ticks();
    radio_hf_clock_enable(true);
    uint32_t t1 = hal_timer_get_ticks();
    radio_hf_clock_disable();
    radio_hf_clock_enable(true);
    radio_hf_clock_disable();
    printf("HF Startup time: %lu ticks\n", t1-t0);
#endif


    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // initialize controller
    controller_init();

    // get virtual HCI transpoft
    const hci_transport_t * hci_transport = controller_get_hci_transport();

    // TODO: use flash storage

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, NULL);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, NULL);

    // init HCI
    hci_init(hci_transport, NULL);

    // uncomment one of the options to enable packet logger
#ifdef ENABLE_SEGGER_RTT
    // hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());

    // hci_dump_segger_rtt_binary_open(HCI_DUMP_PACKETLOGGER);
    // hci_dump_init(hci_dump_segger_rtt_binary_get_instance());
#else
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif

    // hand over to btstack embedded code
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};}

/**
 *@}
 **/

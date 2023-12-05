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

#define BTSTACK_FILE__ "btstack_run_loop_embedded.c"

/*
 *  btstack_run_loop_embedded.c
 *
 *  For this run loop, we assume that there's no global way to wait for a list
 *  of data sources to get ready. Instead, each data source has to queried
 *  individually. Calling ds->isReady() before calling ds->process() doesn't 
 *  make sense, so we just poll each data source round robin.
 *
 *  To support an idle state, where an MCU could go to sleep, the process function
 *  has to return if it has to called again as soon as possible
 *
 *  After calling process() on every data source and evaluating the pending timers,
 *  the idle hook gets called if no data source did indicate that it needs to be
 *  called right away.
 *
 */


#include "btstack_run_loop_embedded.h"

#include "btstack_run_loop.h"
#include "btstack_linked_list.h"
#include "btstack_util.h"
#include "hal_tick.h"
#include "hal_cpu.h"

#include "btstack_debug.h"

#include <stddef.h> // NULL

#ifdef HAVE_EMBEDDED_TIME_MS
#include "hal_time_ms.h"
#endif

#if defined(HAVE_EMBEDDED_TICK) && defined(HAVE_EMBEDDED_TIME_MS)
#error "Please specify either HAVE_EMBEDDED_TICK or HAVE_EMBEDDED_TIME_MS"
#endif

#if defined(HAVE_EMBEDDED_TICK) || defined(HAVE_EMBEDDED_TIME_MS)
#define TIMER_SUPPORT
#endif

// the run loop

#ifdef HAVE_EMBEDDED_TICK
static volatile uint32_t system_ticks;
#endif

static bool trigger_event_received;

static bool run_loop_exit_requested;

// set timer
static void btstack_run_loop_embedded_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
#ifdef HAVE_EMBEDDED_TICK
    uint32_t ticks = btstack_run_loop_embedded_ticks_for_ms(timeout_in_ms);
    if (ticks == 0) ticks++;
    // time until next tick is < hal_tick_get_tick_period_in_ms() and we don't know, so we add one
    ts->timeout = system_ticks + 1 + ticks; 
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
    ts->timeout = hal_time_ms() + timeout_in_ms + 1;
#endif
}

/**
 * Execute run_loop once
 */
void btstack_run_loop_embedded_execute_once(void) {

    // poll data sources
    btstack_run_loop_base_poll_data_sources();

    // execute callbacks
    btstack_run_loop_base_execute_callbacks();

#ifdef TIMER_SUPPORT

#ifdef HAVE_EMBEDDED_TICK
    uint32_t now = system_ticks;
#endif
#ifdef HAVE_EMBEDDED_TIME_MS
    uint32_t now = hal_time_ms();
#endif

    // process timers
    btstack_run_loop_base_process_timers(now);
#endif
    
    // disable IRQs and check if run loop iteration has been requested. if not, go to sleep
    hal_cpu_disable_irqs();
    if (trigger_event_received){
        trigger_event_received = false;
        hal_cpu_enable_irqs();
    } else {
        hal_cpu_enable_irqs_and_sleep();
    }
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_embedded_execute(void) {
    while (run_loop_exit_requested == false) {
        btstack_run_loop_embedded_execute_once();
    }
}

static void btstack_run_loop_embedded_trigger_exit(void){
    run_loop_exit_requested = true;
}

#ifdef HAVE_EMBEDDED_TICK
static void btstack_run_loop_embedded_tick_handler(void){
    system_ticks++;
    trigger_event_received = true;
}

uint32_t btstack_run_loop_embedded_get_ticks(void){
    return system_ticks;
}

uint32_t btstack_run_loop_embedded_ticks_for_ms(uint32_t time_in_ms){
    return time_in_ms / hal_tick_get_tick_period_in_ms();
}
#endif

static uint32_t btstack_run_loop_embedded_get_time_ms(void){
#if   defined(HAVE_EMBEDDED_TIME_MS)
    return hal_time_ms();
#elif defined(HAVE_EMBEDDED_TICK)
    return system_ticks * hal_tick_get_tick_period_in_ms();
#else
    return 0;
#endif
}

static void btstack_run_loop_embedded_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    btstack_run_loop_base_add_callback(callback_registration);
    trigger_event_received = true;
}

static void btstack_run_loop_embedded_poll_data_sources_from_irq(void){
    trigger_event_received = true;
}

// @deprecated Use btstack_run_loop_poll_data_sources_from_irq() instead
void btstack_run_loop_embedded_trigger(void){
    btstack_run_loop_embedded_poll_data_sources_from_irq();
}

static void btstack_run_loop_embedded_init(void){
    btstack_run_loop_base_init();

#ifdef HAVE_EMBEDDED_TICK
    system_ticks = 0;
    hal_tick_init();
    hal_tick_set_handler(&btstack_run_loop_embedded_tick_handler);
#endif
}

/**
 * Provide btstack_run_loop_embedded instance 
 */
const btstack_run_loop_t * btstack_run_loop_embedded_get_instance(void){
    static const btstack_run_loop_t btstack_run_loop_embedded = {
        &btstack_run_loop_embedded_init,
        &btstack_run_loop_base_add_data_source,
        &btstack_run_loop_base_remove_data_source,
        &btstack_run_loop_base_enable_data_source_callbacks,
        &btstack_run_loop_base_disable_data_source_callbacks,
        &btstack_run_loop_embedded_set_timer,
        &btstack_run_loop_base_add_timer,
        &btstack_run_loop_base_remove_timer,
        &btstack_run_loop_embedded_execute,
        &btstack_run_loop_base_dump_timer,
        &btstack_run_loop_embedded_get_time_ms,
        &btstack_run_loop_embedded_poll_data_sources_from_irq,
        &btstack_run_loop_embedded_execute_on_main_thread,
        &btstack_run_loop_embedded_trigger_exit,
    };

    return &btstack_run_loop_embedded;
}


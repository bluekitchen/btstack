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
 *  btstack_run_loop_chibios.c
 *
 *  Run loop on dedicated thread on FreeRTOS
 *  The run loop is triggered from other task/ISRs either via Event Groups
 *  or Task Notifications if HAVE_FREERTOS_TASK_NOTIFICATIONS is defined
 */

#define BTSTACK_FILE__ "btstack_run_loop_chibios.c"

#include <stddef.h> // NULL

#include "btstack_run_loop_chibios.h"

#include "btstack_linked_list.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hal_time_ms.h"

#include "ch.h"


// main event
#define EVT_TRIGGER EVENT_MASK(0)

// checks, assert that these are enabled
// CH_CFG_USE_EVENTS
// CH_CFG_USE_EVENTS_TIMEOUT

static thread_t * btstack_thread;
static mutex_t btstack_run_loop_callbacks_mutex;

// the run loop
static bool run_loop_exit_requested;

static uint32_t btstack_run_loop_chibios_get_time_ms(void){
    return (uint32_t) chTimeI2MS(chVTGetSystemTime());
}

// set timer
static void btstack_run_loop_chibios_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = btstack_run_loop_chibios_get_time_ms() + timeout_in_ms + 1;
}

static void btstack_run_loop_chibios_trigger_from_thread(void){
    chEvtSignal(btstack_thread, EVT_TRIGGER);
}

static void btstack_run_loop_chibios_poll_data_sources_from_irq(void){
    chSysLockFromISR();
    chEvtSignalI(btstack_thread, EVT_TRIGGER);
    chSysUnlockFromISR();
}

static void btstack_run_loop_chibios_trigger_exit_internal(void){
    run_loop_exit_requested = true;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_chibios_execute(void) {
    log_debug("RL: execute");
    
    run_loop_exit_requested = false;

    while (true) {

        // process data sources
        btstack_run_loop_base_poll_data_sources();

        // execute callbacks - protect list with mutex
        while (1){
            chMtxLock(&btstack_run_loop_callbacks_mutex);
            btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
            chMtxUnlock(&btstack_run_loop_callbacks_mutex);
            if (callback_registration == NULL){
                break;
            }
            (*callback_registration->callback)(callback_registration->context);
        }

        // process timers
        uint32_t now = btstack_run_loop_chibios_get_time_ms();
        btstack_run_loop_base_process_timers(now);

        // exit triggered by btstack_run_loop_trigger_exit (main thread or other thread)
        if (run_loop_exit_requested) break;

        // wait for timeout or event notification
        int32_t timeout_next_timer_ms = btstack_run_loop_base_get_time_until_timeout(now);

        if (timeout_next_timer_ms > 0){
            chEvtWaitOneTimeout(EVT_TRIGGER, chTimeMS2I(timeout_next_timer_ms));
        } else {
            chEvtWaitOne(EVT_TRIGGER);
        }
    }
}

static void btstack_run_loop_chibios_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    // protect list with mutex
    chMtxLock(&btstack_run_loop_callbacks_mutex);
    btstack_run_loop_base_add_callback(callback_registration);
    chMtxUnlock(&btstack_run_loop_callbacks_mutex);
    btstack_run_loop_chibios_trigger_from_thread();
}

static void btstack_run_loop_chibios_init(void){
    btstack_run_loop_base_init();
    chMtxObjectInit(&btstack_run_loop_callbacks_mutex);
    btstack_thread = chThdGetSelfX();
}

/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */

static const btstack_run_loop_t btstack_run_loop_chibios = {
    &btstack_run_loop_chibios_init,
    &btstack_run_loop_base_add_data_source,
    &btstack_run_loop_base_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &btstack_run_loop_chibios_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_chibios_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_chibios_get_time_ms,
    &btstack_run_loop_chibios_poll_data_sources_from_irq,
    &btstack_run_loop_chibios_execute_on_main_thread,
    &btstack_run_loop_chibios_trigger_exit_internal,
};

const btstack_run_loop_t * btstack_run_loop_chibios_get_instance(void){
    return &btstack_run_loop_chibios;
}

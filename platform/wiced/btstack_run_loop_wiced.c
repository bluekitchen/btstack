/*
 * Copyright (C) 2015 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_run_loop_wiced.c"

/*
 *  btstack_run_loop_wiced.c
 *
 *  Run loop for Broadcom WICED SDK which currently supports FreeRTOS and ThreadX
 *  WICED 3.3.1 does not support Event Flags on FreeRTOS so a queue is used instead
 */

#include "wiced.h"

#include "btstack_run_loop_wiced.h"

#include "btstack_linked_list.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"

#include <stddef.h> // NULL
 
typedef struct function_call {
    wiced_result_t (*fn)(void * arg);
    void * arg;
} function_call_t;

static const btstack_run_loop_t btstack_run_loop_wiced;

static wiced_queue_t btstack_run_loop_queue;

// the run loop
static uint32_t btstack_run_loop_wiced_get_time_ms(void){
    wiced_time_t time;
    wiced_time_get_time(&time);
    return time;
}

// set timer
static void btstack_run_loop_wiced_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = btstack_run_loop_wiced_get_time_ms() + timeout_in_ms + 1;
}

// schedules execution similar to wiced_rtos_send_asynchronous_event for worker threads
void btstack_run_loop_wiced_execute_code_on_main_thread(wiced_result_t (*fn)(void *arg), void * arg){
    function_call_t message;
    message.fn  = fn;
    message.arg = arg;
    wiced_rtos_push_to_queue(&btstack_run_loop_queue, &message, WICED_NEVER_TIMEOUT);
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_wiced_execute(void) {
    while (true) {

        // process timers
        uint32_t now = btstack_run_loop_wiced_get_time_ms();
        btstack_run_loop_base_process_timers(now);

        // get time until next timeout
        int32_t timeout_next_timer_ms = btstack_run_loop_base_get_time_until_timeout(now);
        uint32_t timeout_ms = WICED_NEVER_TIMEOUT;
        if (timeout_next_timer_ms >= 0){
            timeout_ms = (uint32_t) timeout_next_timer_ms;
        }

        // pop function call
        function_call_t message = { NULL, NULL };
        wiced_rtos_pop_from_queue( &btstack_run_loop_queue, &message, timeout_ms);
        if (message.fn){
            // execute code on run loop
            message.fn(message.arg);
        }
    }
}

static void btstack_run_loop_wiced_btstack_run_loop_init(void){
    btstack_run_loop_base_init();

    // queue to receive events: up to 2 calls from transport, up to 3 for app
    wiced_rtos_init_queue(&btstack_run_loop_queue, "BTstack Run Loop", sizeof(function_call_t), 5);
}

/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */
const btstack_run_loop_t * btstack_run_loop_wiced_get_instance(void){
    return &btstack_run_loop_wiced;
}

static const btstack_run_loop_t btstack_run_loop_wiced = {
    &btstack_run_loop_wiced_btstack_run_loop_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &btstack_run_loop_wiced_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_wiced_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_wiced_get_time_ms,
};

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

/*
 *  run_loop_wiced.c
 *
 *  Run loop for Broadcom WICED SDK which currently supports FreeRTOS and ThreadX
 *  WICED 3.3.1 does not support Event Flags on FreeRTOS so a queue is used instead
 */

#include "wiced.h"

#include <stddef.h> // NULL

#include "bk_linked_list.h"
#include "debug.h"
#include "run_loop.h"
#include "run_loop_private.h"

typedef struct function_call {
    wiced_result_t (*fn)(void * arg);
    void * arg;
} function_call_t;

static wiced_queue_t run_loop_queue;

// the run loop
static bk_linked_list_t timers;

/**
 * Add data_source to run_loop
 */
static void wiced_add_data_source(data_source_t *ds){
    log_error("run_loop_add_data_source not supported in run_loop_wiced");
}

/**
 * Remove data_source from run loop
 */
static int wiced_remove_data_source(data_source_t *ds){
    log_error("run_loop_add_data_source not supported in run_loop_wiced");
    return 0;
}

static uint32_t wiced_get_time_ms(void){
    wiced_time_t time;
    wiced_time_get_time(&time);
    return time;
}

// set timer
static void wiced_set_timer(timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = wiced_get_time_ms() + timeout_in_ms + 1;
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void wiced_add_timer(timer_source_t *ts){
    linked_item_t *it;
    for (it = (linked_item_t *) &timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((timer_source_t *) it->next == ts){
            log_error( "run_loop_timer_add error: timer to add already in list!");
            return;
        }
        if (ts->timeout < ((timer_source_t *) it->next)->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (linked_item_t *) ts;
}

/**
 * Remove timer from run loop
 */
static int wiced_remove_timer(timer_source_t *ts){
    return linked_list_remove(&timers, (linked_item_t *) ts);
}

static void wiced_dump_timer(void){
#ifdef ENABLE_LOG_INFO 
    linked_item_t *it;
    int i = 0;
    for (it = (linked_item_t *) timers; it ; it = it->next){
        timer_source_t *ts = (timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
}

// schedules execution similar to wiced_rtos_send_asynchronous_event for worker threads
void wiced_execute_code_on_run_loop(wiced_result_t (*fn)(void *arg), void * arg){
    function_call_t message;
    message.fn  = fn;
    message.arg = arg;
    wiced_rtos_push_to_queue(&run_loop_queue, &message, WICED_NEVER_TIMEOUT);
}

/**
 * Execute run_loop
 */
static void wiced_execute(void) {
    while (1) {
        // get next timeout
        uint32_t timeout_ms = WICED_NEVER_TIMEOUT;
        if (timers) {
            timer_source_t * ts = (timer_source_t *) timers;
            uint32_t now = wiced_get_time_ms();
            if (ts->timeout < now){
                // remove timer before processing it to allow handler to re-register with run loop
                run_loop_remove_timer(ts);
                // printf("RL: timer %p\n", ts->process);
                ts->process(ts);
                continue;
            }
            timeout_ms = ts->timeout - now;
        }
                
        // pop function call
        function_call_t message = { NULL, NULL };
        wiced_rtos_pop_from_queue( &run_loop_queue, &message, timeout_ms);
        if (message.fn){
            // execute code on run loop
            // printf("RL: execute %p\n", message.fn);
            message.fn(message.arg);
        }
    }
}

static void wiced_run_loop_init(void){
    timers = NULL;

    // queue to receive events: up to 2 calls from transport, up to 3 for app
    wiced_rtos_init_queue(&run_loop_queue, "BTstack Run Loop", sizeof(function_call_t), 5);
}

const run_loop_t run_loop_wiced = {
    &wiced_run_loop_init,
    &wiced_add_data_source,
    &wiced_remove_data_source,
    &wiced_set_timer,
    &wiced_add_timer,
    &wiced_remove_timer,
    &wiced_execute,
    &wiced_dump_timer,
    &wiced_get_time_ms,
};

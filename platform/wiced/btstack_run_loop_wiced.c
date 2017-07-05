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

#define __BTSTACK_FILE__ "btstack_run_loop_wiced.c"

/*
 *  btstack_run_loop_wiced.c
 *
 *  Run loop for Broadcom WICED SDK which currently supports FreeRTOS and ThreadX
 *  WICED 3.3.1 does not support Event Flags on FreeRTOS so a queue is used instead
 */

#include "wiced.h"

#include <stddef.h> // NULL

#include "btstack_linked_list.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_wiced.h"
 
typedef struct function_call {
    wiced_result_t (*fn)(void * arg);
    void * arg;
} function_call_t;

static const btstack_run_loop_t btstack_run_loop_wiced;

static wiced_queue_t btstack_run_loop_queue;

// the run loop
static btstack_linked_list_t timers;

static uint32_t btstack_run_loop_wiced_get_time_ms(void){
    wiced_time_t time;
    wiced_time_get_time(&time);
    return time;
}

// set timer
static void btstack_run_loop_wiced_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = btstack_run_loop_wiced_get_time_ms() + timeout_in_ms + 1;
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_wiced_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        if (ts->timeout < ((btstack_timer_source_t *) it->next)->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
}

/**
 * Remove timer from run loop
 */
static int btstack_run_loop_wiced_remove_timer(btstack_timer_source_t *ts){
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
}

static void btstack_run_loop_wiced_dump_timer(void){
#ifdef ENABLE_LOG_INFO 
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
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
    while (1) {
        // get next timeout
        uint32_t timeout_ms = WICED_NEVER_TIMEOUT;
        if (timers) {
            btstack_timer_source_t * ts = (btstack_timer_source_t *) timers;
            uint32_t now = btstack_run_loop_wiced_get_time_ms();
            if (ts->timeout < now){
                // remove timer before processing it to allow handler to re-register with run loop
                btstack_run_loop_wiced_remove_timer(ts);
                // printf("RL: timer %p\n", ts->process);
                ts->process(ts);
                continue;
            }
            timeout_ms = ts->timeout - now;
        }
                
        // pop function call
        function_call_t message = { NULL, NULL };
        wiced_rtos_pop_from_queue( &btstack_run_loop_queue, &message, timeout_ms);
        if (message.fn){
            // execute code on run loop
            // printf("RL: execute %p\n", message.fn);
            message.fn(message.arg);
        }
    }
}

static void btstack_run_loop_wiced_btstack_run_loop_init(void){
    timers = NULL;

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
    &btstack_run_loop_wiced_add_timer,
    &btstack_run_loop_wiced_remove_timer,
    &btstack_run_loop_wiced_execute,
    &btstack_run_loop_wiced_dump_timer,
    &btstack_run_loop_wiced_get_time_ms,
};

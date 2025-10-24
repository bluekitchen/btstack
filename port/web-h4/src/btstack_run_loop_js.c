/*
 * Copyright (C) 2025 BlueKitchen GmbH
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
 *  btstack_run_loop_js.c
 *
 */


#include "btstack_run_loop_js.h"

#include "btstack_run_loop.h"
#include "btstack_debug.h"

#include <emscripten/emscripten.h>

// the run loop

// get current time in ms from emscripten
static uint32_t btstack_run_loop_js_get_time_ms(void){
    return (uint32_t) emscripten_get_now();
}

// store relative timeout in absolute timeout field
static void btstack_run_loop_js_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = timeout_in_ms;
}

// trampoline to call process functions from js
EMSCRIPTEN_KEEPALIVE
void btstack_run_loop_js_process_timer(btstack_timer_source_t *ts){
    ts->process(ts);
}

// implement timer using JS setTimeout
static void btstack_run_loop_js_add_timer(btstack_timer_source_t *ts){
    int timerId = EM_ASM_INT({
        timerId = setTimeout(() => {
            _btstack_run_loop_js_process_timer($1);
        }, $0, $1);
        return timerId;
    }, ts->timeout, ts);
    // store timer id in timeout field, to allow for timer removal
    ts->timeout = timerId;
}

// remove timer via timer id
static bool btstack_run_loop_js_remove_timer(btstack_timer_source_t *ts) {
    EM_ASM({
        clearTimeout($0);
    }, ts->timeout);
    return true;
}

// set 0 ms timeout to trigger run loop processing
static void btstack_run_loop_js_trigger(void) {
    EM_ASM_INT({
       setTimeout(() => {
           _btstack_run_loop_js_execute_once();
       }, 0);
   });
}

// add callback and trigger run loop
static void btstack_run_loop_js_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    btstack_run_loop_base_add_callback(callback_registration);
    btstack_run_loop_js_trigger();
}

// poll data sources from irq by triggering run loop
// note: js environment is generally single threaded
static void btstack_run_loop_js_poll_data_sources_from_irq(void){
    btstack_run_loop_js_trigger();
}

// not used as js event loop is re-useed
static void btstack_run_loop_js_execute(void) {
    btstack_unreachable();
}

// not supported
static void btstack_run_loop_js_trigger_exit(void){
    btstack_unreachable();
}

// execute once: poll data sources and execute callbacks
EMSCRIPTEN_KEEPALIVE
void btstack_run_loop_js_execute_once(void) {
    // poll data sources
    btstack_run_loop_base_poll_data_sources();

    // execute callbacks
    btstack_run_loop_base_execute_callbacks();
}

/**
 * Provide btstack_run_loop_embedded instance
 */
const btstack_run_loop_t * btstack_run_loop_js_get_instance(void){
    static const btstack_run_loop_t btstack_run_loop_js_get_instance = {
        .init = &btstack_run_loop_base_init,
        .add_data_source = &btstack_run_loop_base_add_data_source,
        .remove_data_source = &btstack_run_loop_base_remove_data_source,
        .enable_data_source_callbacks = &btstack_run_loop_base_enable_data_source_callbacks,
        .disable_data_source_callbacks = &btstack_run_loop_base_disable_data_source_callbacks,
        .set_timer = &btstack_run_loop_js_set_timer,
        .add_timer = &btstack_run_loop_js_add_timer,
        .remove_timer = &btstack_run_loop_js_remove_timer,
        .execute = &btstack_run_loop_js_execute,
        .dump_timer = &btstack_run_loop_base_dump_timer,
        .get_time_ms = &btstack_run_loop_js_get_time_ms,
        .poll_data_sources_from_irq = &btstack_run_loop_js_poll_data_sources_from_irq,
        .execute_on_main_thread = &btstack_run_loop_js_execute_on_main_thread,
        .trigger_exit = &btstack_run_loop_js_trigger_exit,
    };
    return &btstack_run_loop_js_get_instance;
}

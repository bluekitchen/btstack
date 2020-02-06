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

#define BTSTACK_FILE__ "btstack_run_loop_windows.c"

/*
 *  btstack_run_loop_windows.c
 */

#include "btstack_run_loop.h"
#include "btstack_run_loop_windows.h"
#include "btstack_linked_list.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

static void btstack_run_loop_windows_dump_timer(void);

// the run loop
static btstack_linked_list_t data_sources;
static int data_sources_modified;
static btstack_linked_list_t timers;
// start time. 
static ULARGE_INTEGER start_time;

/**
 * Add data_source to run_loop
 */
static void btstack_run_loop_windows_add_data_source(btstack_data_source_t *ds){
    data_sources_modified = 1;
    // log_info("btstack_run_loop_windows_add_data_source %x with fd %u\n", (int) ds, ds->fd);
    btstack_linked_list_add(&data_sources, (btstack_linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
static bool btstack_run_loop_windows_remove_data_source(btstack_data_source_t *ds){
    data_sources_modified = 1;
    // log_info("btstack_run_loop_windows_remove_data_source %x\n", (int) ds);
    return btstack_linked_list_remove(&data_sources, (btstack_linked_item_t *) ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_windows_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        btstack_timer_source_t * next = (btstack_timer_source_t *) it->next;
        if (next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        // exit if list timeout is after new timeout
        uint32_t list_timeout = ((btstack_timer_source_t *) it->next)->timeout;
        int32_t delta = btstack_time_delta(ts->timeout, list_timeout);
        if (delta < 0) break;
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
    log_debug("Added timer %p at %u\n", ts, ts->timeout);
    // btstack_run_loop_windows_dump_timer();
}

/**
 * Remove timer from run loop
 */
static bool btstack_run_loop_windows_remove_timer(btstack_timer_source_t *ts){
    // log_info("Removed timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    // btstack_run_loop_windows_dump_timer();
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
}

static void btstack_run_loop_windows_dump_timer(void){
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, ts->timeout);
    }
}

static void btstack_run_loop_windows_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags |= callback_types;
}

static void btstack_run_loop_windows_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags &= ~callback_types;
}

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_windows_get_time_ms(void){

    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER now_time;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    now_time.LowPart =  file_time.dwLowDateTime;
    now_time.HighPart = file_time.dwHighDateTime;
    uint32_t time_ms = (now_time.QuadPart - start_time.QuadPart) / 10000; 
    log_debug("btstack_run_loop_windows_get_time_ms: %u", time_ms);
    return time_ms;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_windows_execute(void) {

    btstack_timer_source_t *ts;
    btstack_linked_list_iterator_t it;

    while (true) {

        // collect handles to wait for
        HANDLE handles[100];
        memset(handles, 0, sizeof(handles));
        int num_handles = 0;     
        btstack_linked_list_iterator_init(&it, &data_sources);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            if (ds->source.handle == 0) continue;
            if (ds->flags & (DATA_SOURCE_CALLBACK_READ | DATA_SOURCE_CALLBACK_WRITE)){
                handles[num_handles++] = ds->source.handle;
                log_debug("btstack_run_loop_execute adding handle %p", ds->source.handle);
            }
        }

        // get next timeout
        int32_t timeout_ms = INFINITE;
        if (timers) {
            ts = (btstack_timer_source_t *) timers;
            uint32_t now_ms = btstack_run_loop_windows_get_time_ms();
            timeout_ms = btstack_time_delta(ts->timeout, now_ms);
            if (timeout_ms < 0){
                timeout_ms = 0;
            }
            log_debug("btstack_run_loop_execute next timeout in %u ms", timeout_ms);
        }
        
        int res;
        if (num_handles){
            // wait for ready Events or timeout
            res = WaitForMultipleObjects(num_handles, &handles[0], 0, timeout_ms);
        } else {
            // just wait for timeout
            Sleep(timeout_ms);
            res = WAIT_TIMEOUT;
        }
        
        // process data source
        if (WAIT_OBJECT_0 <= res && res < (WAIT_OBJECT_0 + num_handles)){
            void * triggered_handle = handles[res - WAIT_OBJECT_0];
            btstack_linked_list_iterator_init(&it, &data_sources);
            while (btstack_linked_list_iterator_has_next(&it)){
                btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
                log_debug("btstack_run_loop_windows_execute: check ds %p with handle %p\n", ds, ds->source.handle);
                if (triggered_handle == ds->source.handle){
                    if (ds->flags & DATA_SOURCE_CALLBACK_READ){
                        log_debug("btstack_run_loop_windows_execute: process read ds %p with handle %p\n", ds, ds->source.handle);
                        ds->process(ds, DATA_SOURCE_CALLBACK_READ);
                    } else if (ds->flags & DATA_SOURCE_CALLBACK_WRITE){
                        log_debug("btstack_run_loop_windows_execute: process write ds %p with handle %p\n", ds, ds->source.handle);
                        ds->process(ds, DATA_SOURCE_CALLBACK_WRITE);
                    }
                    break;
                }
            }
        }

        // process timers
        uint32_t now_ms = btstack_run_loop_windows_get_time_ms();
        while (timers) {
            ts = (btstack_timer_source_t *) timers;
            if (ts->timeout > now_ms) break;
            log_debug("btstack_run_loop_windows_execute: process timer %p\n", ts);
            
            // remove timer before processing it to allow handler to re-register with run loop
            btstack_run_loop_windows_remove_timer(ts);
            ts->process(ts);
        }
    }
}

// set timer
static void btstack_run_loop_windows_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    uint32_t time_ms = btstack_run_loop_windows_get_time_ms();
    a->timeout = time_ms + timeout_in_ms;
    log_debug("btstack_run_loop_windows_set_timer to %u ms (now %u, timeout %u)", a->timeout, time_ms, timeout_in_ms);
}

static void btstack_run_loop_windows_init(void){
    data_sources = NULL;
    timers = NULL;

    // store start time
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    start_time.LowPart =  file_time.dwLowDateTime;
    start_time.HighPart = file_time.dwHighDateTime;

    log_debug("btstack_run_loop_windows_init");
}


static const btstack_run_loop_t btstack_run_loop_windows = {
    &btstack_run_loop_windows_init,
    &btstack_run_loop_windows_add_data_source,
    &btstack_run_loop_windows_remove_data_source,
    &btstack_run_loop_windows_enable_data_source_callbacks,
    &btstack_run_loop_windows_disable_data_source_callbacks,
    &btstack_run_loop_windows_set_timer,
    &btstack_run_loop_windows_add_timer,
    &btstack_run_loop_windows_remove_timer,
    &btstack_run_loop_windows_execute,
    &btstack_run_loop_windows_dump_timer,
    &btstack_run_loop_windows_get_time_ms,
};

/**
 * Provide btstack_run_loop_windows instance
 */
const btstack_run_loop_t * btstack_run_loop_windows_get_instance(void){
    return &btstack_run_loop_windows;
}


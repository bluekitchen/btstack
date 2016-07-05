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
 *  btstack_run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "btstack_linked_list.h"
#include "btstack_debug.h"

#ifdef _WIN32
#include "Winsock2.h"
#else
#include <sys/select.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

static void btstack_run_loop_posix_dump_timer(void);

// the run loop
static btstack_linked_list_t data_sources;
static int data_sources_modified;
static btstack_linked_list_t timers;
// start time. tv_usec = 0
static struct timeval init_tv;

/**
 * Add data_source to run_loop
 */
static void btstack_run_loop_posix_add_data_source(btstack_data_source_t *ds){
    data_sources_modified = 1;
    // log_info("btstack_run_loop_posix_add_data_source %x with fd %u\n", (int) ds, ds->fd);
    btstack_linked_list_add(&data_sources, (btstack_linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
static int btstack_run_loop_posix_remove_data_source(btstack_data_source_t *ds){
    data_sources_modified = 1;
    // log_info("btstack_run_loop_posix_remove_data_source %x\n", (int) ds);
    return btstack_linked_list_remove(&data_sources, (btstack_linked_item_t *) ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_posix_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        btstack_timer_source_t * next = (btstack_timer_source_t *) it->next;
        if (next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        if (next->timeout > ts->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
    log_debug("Added timer %p at %u\n", ts, ts->timeout);
    // btstack_run_loop_posix_dump_timer();
}

/**
 * Remove timer from run loop
 */
static int btstack_run_loop_posix_remove_timer(btstack_timer_source_t *ts){
    // log_info("Removed timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    // btstack_run_loop_posix_dump_timer();
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
}

static void btstack_run_loop_posix_dump_timer(void){
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, ts->timeout);
    }
}

static void btstack_run_loop_posix_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags |= callback_types;
}

static void btstack_run_loop_posix_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags &= ~callback_types;
}

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_posix_get_time_ms(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t time_ms = (uint32_t)((tv.tv_sec  - init_tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
    log_debug("btstack_run_loop_posix_get_time_ms: %u <- %u / %u", time_ms, (int) tv.tv_sec, (int) tv.tv_usec);
    return time_ms;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_posix_execute(void) {
    fd_set descriptors_read;
    fd_set descriptors_write;
    
    btstack_timer_source_t       *ts;
    btstack_linked_list_iterator_t it;
    struct timeval * timeout;
    struct timeval tv;
    uint32_t now_ms;

    while (1) {
        // collect FDs
        FD_ZERO(&descriptors_read);
        FD_ZERO(&descriptors_write);
        int highest_fd = -1;
        btstack_linked_list_iterator_init(&it, &data_sources);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            if (ds->fd < 0) continue;
            if (ds->flags & DATA_SOURCE_CALLBACK_READ){
                FD_SET(ds->fd, &descriptors_read);
                if (ds->fd > highest_fd) {
                    highest_fd = ds->fd;
                }
                log_debug("btstack_run_loop_execute adding fd %u for read", ds->fd);
            }
            if (ds->flags & DATA_SOURCE_CALLBACK_WRITE){
                FD_SET(ds->fd, &descriptors_write);
                if (ds->fd > highest_fd) {
                    highest_fd = ds->fd;
                }
                log_debug("btstack_run_loop_execute adding fd %u for write", ds->fd);
            }
        }
        
        // get next timeout
        timeout = NULL;
        if (timers) {
            ts = (btstack_timer_source_t *) timers;
            timeout = &tv;
            now_ms = btstack_run_loop_posix_get_time_ms();
            int delta = ts->timeout - now_ms;
            if (delta < 0){
                delta = 0;
            }
            tv.tv_sec  = delta / 1000;
            tv.tv_usec = (int) (delta - (tv.tv_sec * 1000)) * 1000;
            log_debug("btstack_run_loop_execute next timeout in %u ms", delta);
        }
                
        // wait for ready FDs
        select( highest_fd+1 , &descriptors_read, &descriptors_write, NULL, timeout);
                

        data_sources_modified = 0;
        btstack_linked_list_iterator_init(&it, &data_sources);
        while (btstack_linked_list_iterator_has_next(&it) && !data_sources_modified){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            log_debug("btstack_run_loop_posix_execute: check ds %p with fd %u\n", ds, ds->fd);
            if (FD_ISSET(ds->fd, &descriptors_read)) {
                log_debug("btstack_run_loop_posix_execute: process read ds %p with fd %u\n", ds, ds->fd);
                ds->process(ds, DATA_SOURCE_CALLBACK_READ);
            }
            if (FD_ISSET(ds->fd, &descriptors_write)) {
                log_debug("btstack_run_loop_posix_execute: process write ds %p with fd %u\n", ds, ds->fd);
                ds->process(ds, DATA_SOURCE_CALLBACK_WRITE);
            }
        }
        log_debug("btstack_run_loop_posix_execute: after ds check\n");
        
        // process timers
        now_ms = btstack_run_loop_posix_get_time_ms();
        while (timers) {
            ts = (btstack_timer_source_t *) timers;
            if (ts->timeout > now_ms) break;
            log_debug("btstack_run_loop_posix_execute: process timer %p\n", ts);
            
            // remove timer before processing it to allow handler to re-register with run loop
            btstack_run_loop_remove_timer(ts);
            ts->process(ts);
        }
    }
}

// set timer
static void btstack_run_loop_posix_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    uint32_t time_ms = btstack_run_loop_posix_get_time_ms();
    a->timeout = time_ms + timeout_in_ms;
    log_debug("btstack_run_loop_posix_set_timer to %u ms (now %u, timeout %u)", a->timeout, time_ms, timeout_in_ms);
}

static void btstack_run_loop_posix_init(void){
    data_sources = NULL;
    timers = NULL;
    // just assume that we started at tv_usec == 0
    gettimeofday(&init_tv, NULL);
    init_tv.tv_usec = 0;
    log_debug("btstack_run_loop_posix_init at %u/%u", (int) init_tv.tv_sec, 0);
}


static const btstack_run_loop_t btstack_run_loop_posix = {
    &btstack_run_loop_posix_init,
    &btstack_run_loop_posix_add_data_source,
    &btstack_run_loop_posix_remove_data_source,
    &btstack_run_loop_posix_enable_data_source_callbacks,
    &btstack_run_loop_posix_disable_data_source_callbacks,
    &btstack_run_loop_posix_set_timer,
    &btstack_run_loop_posix_add_timer,
    &btstack_run_loop_posix_remove_timer,
    &btstack_run_loop_posix_execute,
    &btstack_run_loop_posix_dump_timer,
    &btstack_run_loop_posix_get_time_ms,
};

/**
 * Provide btstack_run_loop_posix instance
 */
const btstack_run_loop_t * btstack_run_loop_posix_get_instance(void){
    return &btstack_run_loop_posix;
}


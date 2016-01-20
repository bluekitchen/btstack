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

static void btstack_run_loop_posix_dump_timer(void);
static int  btstack_run_loop_posix_timeval_compare(struct timeval *a, struct timeval *b);
static int  btstack_run_loop_posix_timer_compare(btstack_timer_source_t *a, btstack_timer_source_t *b);

// the run loop
static btstack_linked_list_t data_sources;
static int data_sources_modified;
static btstack_linked_list_t timers;
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
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        if (btstack_run_loop_posix_timer_compare( (btstack_timer_source_t *) it->next, ts) > 0) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
    // log_info("Added timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
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
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout.tv_sec);
    }
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_posix_execute(void) {
    fd_set descriptors;
    
    btstack_timer_source_t       *ts;
    struct timeval current_tv;
    struct timeval next_tv;
    struct timeval *timeout;
    btstack_linked_list_iterator_t it;
    
    while (1) {
        // collect FDs
        FD_ZERO(&descriptors);
        int highest_fd = 0;
        btstack_linked_list_iterator_init(&it, &data_sources);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            if (ds->fd >= 0) {
                FD_SET(ds->fd, &descriptors);
                if (ds->fd > highest_fd) {
                    highest_fd = ds->fd;
                }
            }
        }
        
        // get next timeout
        // pre: 0 <= tv_usec < 1000000
        timeout = NULL;
        if (timers) {
            gettimeofday(&current_tv, NULL);
            ts = (btstack_timer_source_t *) timers;
            next_tv.tv_usec = ts->timeout.tv_usec - current_tv.tv_usec;
            next_tv.tv_sec  = ts->timeout.tv_sec  - current_tv.tv_sec;
            while (next_tv.tv_usec < 0){
                next_tv.tv_usec += 1000000;
                next_tv.tv_sec--;
            }
            if (next_tv.tv_sec < 0){
                next_tv.tv_sec  = 0; 
                next_tv.tv_usec = 0;
            }
            timeout = &next_tv;
        }
                
        // wait for ready FDs
        select( highest_fd+1 , &descriptors, NULL, NULL, timeout);
        
        // process data sources very carefully
        // bt_control.close() triggered from a client can remove a different data source
        
        // log_info("btstack_run_loop_posix_execute: before ds check\n");
        data_sources_modified = 0;
        btstack_linked_list_iterator_init(&it, &data_sources);
        while (btstack_linked_list_iterator_has_next(&it) && !data_sources_modified){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            // log_info("btstack_run_loop_posix_execute: check %x with fd %u\n", (int) ds, ds->fd);
            if (FD_ISSET(ds->fd, &descriptors)) {
                // log_info("btstack_run_loop_posix_execute: process %x with fd %u\n", (int) ds, ds->fd);
                ds->process(ds);
            }
        }
        // log_info("btstack_run_loop_posix_execute: after ds check\n");
        
        // process timers
        // pre: 0 <= tv_usec < 1000000
        while (timers) {
            gettimeofday(&current_tv, NULL);
            ts = (btstack_timer_source_t *) timers;
            if (ts->timeout.tv_sec  > current_tv.tv_sec) break;
            if (ts->timeout.tv_sec == current_tv.tv_sec && ts->timeout.tv_usec > current_tv.tv_usec) break;
            // log_info("btstack_run_loop_posix_execute: process times %x\n", (int) ts);
            
            // remove timer before processing it to allow handler to re-register with run loop
            btstack_run_loop_remove_timer(ts);
            ts->process(ts);
        }
    }
}

// set timer
static void btstack_run_loop_posix_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    gettimeofday(&a->timeout, NULL);
    a->timeout.tv_sec  +=  timeout_in_ms / 1000;
    a->timeout.tv_usec += (timeout_in_ms % 1000) * 1000;
    if (a->timeout.tv_usec  > 1000000) {
        a->timeout.tv_usec -= 1000000;
        a->timeout.tv_sec++;
    }
}

// compare timers - NULL is assumed to be before the Big Bang
// pre: 0 <= tv_usec < 1000000
static int btstack_run_loop_posix_timeval_compare(struct timeval *a, struct timeval *b){
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    if (a->tv_sec < b->tv_sec) {
        return -1;
    }
    if (a->tv_sec > b->tv_sec) {
        return 1;
    }
    
    if (a->tv_usec < b->tv_usec) {
        return -1;
    }
    if (a->tv_usec > b->tv_usec) {
        return 1;
    }
    
    return 0;
    
}

// compare timers - NULL is assumed to be before the Big Bang
// pre: 0 <= tv_usec < 1000000
static int btstack_run_loop_posix_timer_compare(btstack_timer_source_t *a, btstack_timer_source_t *b){
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return btstack_run_loop_posix_timeval_compare(&a->timeout, &b->timeout);
}

static void btstack_run_loop_posix_init(void){
    data_sources = NULL;
    timers = NULL;
    gettimeofday(&init_tv, NULL);
}

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_posix_get_time_ms(void){
    struct timeval current_tv;
    gettimeofday(&current_tv, NULL);
    return (current_tv.tv_sec  - init_tv.tv_sec)  * 1000
         + (current_tv.tv_usec - init_tv.tv_usec) / 1000;
}


static const btstack_run_loop_t btstack_run_loop_posix = {
    &btstack_run_loop_posix_init,
    &btstack_run_loop_posix_add_data_source,
    &btstack_run_loop_posix_remove_data_source,
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


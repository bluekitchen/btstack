/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

/*
 *  run_loop_embedded.c
 *
 *  For this run loop, we assume that there's no global way to wait for a list
 *  of data sources to get ready. Instead, each data source has to queried
 *  individually. Calling ds->isReady() before calling ds->process() doesn't 
 *  make sense, so we just poll each data source round robin.
 *
 */


#include <btstack/run_loop.h>
#include <btstack/linked_list.h>
#include <btstack/hal_tick.h>

#include "run_loop_private.h"
#include "debug.h"

#include <stddef.h> // NULL

// the run loop
static linked_list_t data_sources;

#ifdef EMBEDDED
static linked_list_t timers;
static uint32_t system_ticks;
#endif

/**
 * Add data_source to run_loop
 */
void embedded_add_data_source(data_source_t *ds){
    linked_list_add(&data_sources, (linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
int embedded_remove_data_source(data_source_t *ds){
    return linked_list_remove(&data_sources, (linked_item_t *) ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void embedded_add_timer(timer_source_t *ts){
#ifdef EMBEDDED
    linked_item_t *it;
    for (it = (linked_item_t *) &timers; it->next ; it = it->next){
        if (ts->timeout < ((timer_source_t *) it->next)->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (linked_item_t *) ts;
    // log_info("Added timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    // embedded_dump_timer();
#endif
}

/**
 * Remove timer from run loop
 */
int embedded_remove_timer(timer_source_t *ts){
#ifdef EMBEDDED    
    // log_info("Removed timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    return linked_list_remove(&timers, (linked_item_t *) ts);
#else
    return 0;
#endif
}

void embedded_dump_timer(){
#if 0
    linked_item_t *it;
    int i = 0;
    for (it = (linked_item_t *) timers; it ; it = it->next){
        timer_source_t *ts = (timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout.tv_sec);
    }
#endif
}

/**
 * Execute run_loop
 */
void embedded_execute() {
    data_source_t *ds;

    while (1) {

        // process data sources
        data_source_t *next;
        for (ds = (data_source_t *) data_sources; ds != NULL ; ds = next){
            next = (data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
            ds->process(ds);
        }
        
#ifdef EMBEDDED
        // process timers
        while (timers) {
            timer_source_t *ts = (timer_source_t *) timers;
            if (ts->timeout > system_ticks) break;
            run_loop_remove_timer(ts);
            ts->process(ts);
        }
#endif
        
    }
}

#ifdef EMBEDDED
static void embedded_tick_handler(void){
    system_ticks++;
}

uint32_t embedded_get_ticks(void){
    return system_ticks;
}

uint32_t embedded_ticks_for_ms(uint32_t time_in_ms){
    return delta_ms / hal_tick_get_tick_period_in_ms()
}

// set timer
void run_loop_set_timer(timer_source_t *ts, uint32_t timeout_in_ms){
    uint32_t ticks = embedded_ticks_for_ms(timeout_in_ms);
    if (ticks == 0) ticks++;
    ts->timeout = system_ticks + ticks; 
}
#endif

void embedded_init(){

    data_sources = NULL;

#ifdef EMBEDDED
    timers = NULL;
    system_ticks = 0;
    hal_tick_init();
    hal_tick_set_handler(&embedded_tick_handler);
#endif
}

run_loop_t run_loop_embedded = {
    &embedded_init,
    &embedded_add_data_source,
    &embedded_remove_data_source,
    &embedded_add_timer,
    &embedded_remove_timer,
    &embedded_execute,
    &embedded_dump_timer
};

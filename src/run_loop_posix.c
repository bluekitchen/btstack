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
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include <btstack/run_loop.h>
#include <btstack/linked_list.h>

#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

// the run loop
static linked_list_t data_sources;
static linked_list_t timers;

/**
 * Add data_source to run_loop
 */
void posix_add_data_source(data_source_t *ds){
    linked_list_add(&data_sources, (linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
int posix_remove_data_source(data_source_t *ds){
    return linked_list_remove(&data_sources, (linked_item_t *) ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void posix_add_timer(timer_source_t *ts){
    linked_item_t *it;
    for (it = (linked_item_t *) &timers; it->next ; it = it->next){
        if (run_loop_timer_compare( (timer_source_t *) it->next, ts) >= 0) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (linked_item_t *) ts;
    // printf("Added timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    // posix_dump_timer();
}

/**
 * Remove timer from run loop
 */
int posix_remove_timer(timer_source_t *ts){
    // printf("Removed timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    return linked_list_remove(&timers, (linked_item_t *) ts);
}

void posix_dump_timer(){
    linked_item_t *it;
    int i = 0;
    for (it = (linked_item_t *) timers; it ; it = it->next){
        timer_source_t *ts = (timer_source_t*) it;
        printf("timer %u, timeout %u\n", i, (unsigned int) ts->timeout.tv_sec);
    }
}

/**
 * Execute run_loop
 */
void posix_execute() {
    fd_set descriptors;
    data_source_t *ds;
    timer_source_t       *ts;
    struct timeval current_tv;
    struct timeval next_tv;
    struct timeval *timeout;
    
    while (1) {
        // collect FDs
        FD_ZERO(&descriptors);
        int highest_fd = 0;
        for (ds = (data_source_t *) data_sources; ds ; ds = (data_source_t *) ds->item.next){
            if (ds->fd) {
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
            ts = (timer_source_t *) timers;
            next_tv.tv_usec = ts->timeout.tv_usec - current_tv.tv_usec;
            next_tv.tv_sec  = ts->timeout.tv_sec  - current_tv.tv_sec;
            while (next_tv.tv_usec < 0){
                next_tv.tv_usec += 1000000;
                next_tv.tv_sec--;
            }
            if (next_tv.tv_sec < 0 || (next_tv.tv_sec == 0 && next_tv.tv_usec < 0)){
                next_tv.tv_sec  = 0; 
                next_tv.tv_usec = 0;
            }
            timeout = &next_tv;
        }
                
        // wait for ready FDs
        select( highest_fd+1 , &descriptors, NULL, NULL, timeout);
        
        // process data sources
        data_source_t *next;
        for (ds = (data_source_t *) data_sources; ds != NULL ; ds = next){
            next = (data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
            if (FD_ISSET(ds->fd, &descriptors)) {
                ds->process(ds);
            }
        }
        
        // process timers
        // pre: 0 <= tv_usec < 1000000
        while (timers) {
            gettimeofday(&current_tv, NULL);
            ts = (timer_source_t *) timers;
            if (ts->timeout.tv_sec  > current_tv.tv_sec) break;
            if (ts->timeout.tv_sec == current_tv.tv_sec && ts->timeout.tv_usec > current_tv.tv_usec) break;
            run_loop_remove_timer(ts);
            ts->process(ts);
        }
    }
}

void posix_init(){
    data_sources = NULL;
    timers = NULL;
}

const run_loop_t run_loop_posix = {
    &posix_init,
    &posix_add_data_source,
    &posix_remove_data_source,
    &posix_add_timer,
    &posix_remove_timer,
    &posix_execute,
    &posix_dump_timer
};

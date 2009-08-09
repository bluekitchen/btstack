/*
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include "run_loop.h"
#include "linked_list.h"

#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

// the run loop
static linked_list_t data_sources = NULL;
static linked_list_t timers = NULL;

// set timer
void run_loop_set_timer(timer_t *a, int timeout_in_seconds){
    gettimeofday(&a->timeout, NULL);
    a->timeout.tv_sec += timeout_in_seconds;
}

// compare timers - NULL is assumed to be before the Big Bang
int run_loop_timer_compare(timer_t *a, timer_t *b){
    
    if (!a || !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    if (a->timeout.tv_sec < b->timeout.tv_sec) {
        return -1;
    }
    if (a->timeout.tv_sec > b->timeout.tv_sec) {
        return 1;
    }

    if (a->timeout.tv_usec < b->timeout.tv_usec) {
        return -1;
    }
    if (a->timeout.tv_usec > b->timeout.tv_usec) {
        return 1;
    }
    
    return 0;
}

/**
 * Add data_source to run_loop
 */
void run_loop_add_data_source(data_source_t *ds){
    linked_list_add(&data_sources, (linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
int run_loop_remove_data_source(data_source_t *ds){
    return linked_list_remove(&data_sources, (linked_item_t *) ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void run_loop_add_timer(timer_t *ts){
    linked_item_t *it;
    for (it = (linked_item_t *) &timers; it ; it = it->next){
        if ( run_loop_timer_compare( (timer_t *) it->next, ts) >= 0) {
            ts->item.next = it->next;
            it->next = (linked_item_t *) ts;
            return;
        }
    }
}

/**
 * Remove timer from run loop
 */
int run_loop_remove_timer(timer_t *ts){
    return linked_list_remove(&timers, (linked_item_t *) ts);
}

void run_loop_timer_dump(){
    linked_item_t *it;
    int i = 0;
    for (it = (linked_item_t *) timers; it ; it = it->next){
        timer_t *ts = (timer_t*) it;
        printf("timer %u, timeout %u\n", i, ts->timeout.tv_sec);
    }
}

/**
 * Execute run_loop
 */
void run_loop_execute() {
    fd_set descriptors;
    data_source_t *ds;
    timer_t       *ts;
    
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
        // ..
        
        // wait for ready FDs
        select( highest_fd+1 , &descriptors, NULL, NULL, NULL);

        // process data sources
        data_source_t *next;
        for (ds = (data_source_t *) data_sources; ds != NULL ; ds = next){
            next = (data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
            if (FD_ISSET(ds->fd, &descriptors)) {
                ds->process(ds);
            }
        }
        
        // process timers
        // ..
    }
}




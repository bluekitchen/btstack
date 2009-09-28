/*
 *  run_loop.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include <btstack/linked_list.h>

#include <sys/time.h>

typedef struct data_source {
    linked_item_t item;
    int  fd;                                            // <-- file descriptors to watch or 0
    int  (*process)(struct data_source *ds);            // <-- do processing
} data_source_t;

typedef struct timer {
    linked_item_t item; 
    struct timeval timeout;                             // <-- next timeout
    void  (*process)(struct timer *ts);                  // <-- do processing
} timer_t;

// set timer based on current time
void run_loop_set_timer(timer_t *a, int timeout_in_seconds);

// compare timeval or timers - NULL is assumed to be before the Big Bang
int run_loop_timeval_compare(struct timeval *a, struct timeval *b);
int run_loop_timer_compare(timer_t *a, timer_t *b);

void run_loop_add_data_source(data_source_t *dataSource);     // <-- add DataSource to RunLoop
int  run_loop_remove_data_source(data_source_t *dataSource);  // <-- remove DataSource from RunLoop

void run_loop_add_timer(timer_t *timer);                // <-- add Timer to RunLoop
int  run_loop_remove_timer(timer_t *timer);             // <-- remove Timer from RunLoop

void run_loop_timer_dump();                             // debug

void run_loop_execute();                                // <-- execute configured RunLoop

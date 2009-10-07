/*
 *  run_loop.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include <btstack/linked_list.h>

#include <sys/time.h>

typedef enum {
	RUN_LOOP_POSIX = 1,
	RUN_LOOP_COCOA
} RUN_LOOP_TYPE;

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

// init must be called before any other run_loop call
void run_loop_init(RUN_LOOP_TYPE type);

// set timer based on current time
void run_loop_set_timer(timer_t *a, int timeout_in_ms);

// compare timeval or timers - NULL is assumed to be before the Big Bang
int run_loop_timeval_compare(struct timeval *a, struct timeval *b);
int run_loop_timer_compare(timer_t *a, timer_t *b);

void run_loop_add_data_source(data_source_t *dataSource);     // <-- add DataSource to RunLoop
int  run_loop_remove_data_source(data_source_t *dataSource);  // <-- remove DataSource from RunLoop

void run_loop_add_timer(timer_t *timer);                // <-- add Timer to RunLoop
int  run_loop_remove_timer(timer_t *timer);             // <-- remove Timer from RunLoop

void run_loop_execute();                                // <-- execute configured RunLoop

void run_loop_timer_dump();                             // debug


// internal use only
typedef struct {
	void (*init)();
	void (*add_data_source)(data_source_t *dataSource);
	int  (*remove_data_source)(data_source_t *dataSource);
	void (*add_timer)(timer_t *timer);
	int  (*remove_timer)(timer_t *timer); 
	void (*execute)();
	void (*dump_timer)();
} run_loop_t;

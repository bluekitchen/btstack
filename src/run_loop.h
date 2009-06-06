/*
 *  run_loop.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

typedef struct data_source {
    struct data_source *next;                           // <-- used internally by RunLoop
    int  fd;                                            // <-- file descriptors to watch or 0
    int  (*process)(struct data_source *ds, int ready); // <-- do processing, @return: more to do
} data_source_t;

typedef struct timer_source {
    struct timer_source *next;
} timer_source_t;

void run_loop_add(data_source_t *dataSource);        // <-- add DataSource to RunLoop
int  run_loop_remove(data_source_t *dataSource);     // <-- remove DataSource from RunLoop
void run_loop_execute();                          // <-- execute configured RunLoop

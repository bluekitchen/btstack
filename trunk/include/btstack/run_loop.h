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
 *  run_loop.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include <btstack/linked_list.h>

#include <sys/time.h>

#if defined __cplusplus
extern "C" {
#endif
	
typedef enum {
	RUN_LOOP_POSIX = 1,
	RUN_LOOP_COCOA,
	RUN_LOOP_EMBEDDED
} RUN_LOOP_TYPE;

typedef struct data_source {
    linked_item_t item;
    int  fd;                                 // <-- file descriptor to watch or 0
    int  (*process)(struct data_source *ds); // <-- do processing
} data_source_t;

typedef struct timer {
    linked_item_t item; 
    struct timeval timeout;                  // <-- next timeout
    void  (*process)(struct timer *ts);      // <-- do processing
} timer_source_t;

// init must be called before any other run_loop call
void run_loop_init(RUN_LOOP_TYPE type);

// add/remove data_source
void run_loop_add_data_source(data_source_t *dataSource);
int  run_loop_remove_data_source(data_source_t *dataSource);

// set timer based on current time
void run_loop_set_timer(timer_source_t *a, int timeout_in_ms);

// add/remove timer_source
void run_loop_add_timer(timer_source_t *timer); 
int  run_loop_remove_timer(timer_source_t *timer);

// execute configured run_loop
void run_loop_execute();

#if defined __cplusplus
}
#endif


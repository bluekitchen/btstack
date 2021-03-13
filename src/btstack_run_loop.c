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

#define BTSTACK_FILE__ "btstack_run_loop.c"

/*
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include "btstack_run_loop.h"

#include "btstack_debug.h"
#include "btstack_config.h"
#include "btstack_util.h"

#include <inttypes.h>

static const btstack_run_loop_t * the_run_loop = NULL;

extern const btstack_run_loop_t btstack_run_loop_embedded;

/*
 *  Portable implementation of timer and data source management as base for platform specific implementations
 */

// private data (access only by run loop implementations)
btstack_linked_list_t btstack_run_loop_base_timers;
btstack_linked_list_t btstack_run_loop_base_data_sources;

void btstack_run_loop_base_init(void){
    btstack_run_loop_base_timers = NULL;
    btstack_run_loop_base_data_sources = NULL;
}

void btstack_run_loop_base_add_data_source(btstack_data_source_t *ds){
    btstack_linked_list_add(&btstack_run_loop_base_data_sources, (btstack_linked_item_t *) ds);
}

bool btstack_run_loop_base_remove_data_source(btstack_data_source_t *ds){
    return btstack_linked_list_remove(&btstack_run_loop_base_data_sources, (btstack_linked_item_t *) ds);
}

void btstack_run_loop_base_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags |= callback_types;
}

void btstack_run_loop_base_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags &= ~callback_types;
}

bool btstack_run_loop_base_remove_timer(btstack_timer_source_t *ts){
    return btstack_linked_list_remove(&btstack_run_loop_base_timers, (btstack_linked_item_t *) ts);
}

void btstack_run_loop_base_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &btstack_run_loop_base_timers; it->next ; it = it->next){
        btstack_timer_source_t * next = (btstack_timer_source_t *) it->next;
        btstack_assert(next != ts);
        int32_t delta = btstack_time_delta(ts->timeout, next->timeout);
        if (delta < 0) break;
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
}

void btstack_run_loop_base_process_timers(uint32_t now){
    // process timers, exit when timeout is in the future
    while (btstack_run_loop_base_timers) {
        btstack_timer_source_t * ts = (btstack_timer_source_t *) btstack_run_loop_base_timers;
        int32_t delta = btstack_time_delta(ts->timeout, now);
        if (delta > 0) break;
        btstack_run_loop_base_remove_timer(ts);
        ts->process(ts);
    }
}

void btstack_run_loop_base_dump_timer(void){
#ifdef ENABLE_LOG_INFO
    btstack_linked_item_t *it;
    uint16_t i = 0;
    for (it = (btstack_linked_item_t *) btstack_run_loop_base_timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u (%p): timeout %" PRIu32 "u\n", i, ts, ts->timeout);
    }
#endif

}
/**
 * @brief Get time until first timer fires
 * @returns -1 if no timers, time until next timeout otherwise
 */
int32_t btstack_run_loop_base_get_time_until_timeout(uint32_t now){
    if (btstack_run_loop_base_timers == NULL) return -1;
    btstack_timer_source_t * ts = (btstack_timer_source_t *) btstack_run_loop_base_timers;
    uint32_t list_timeout  = ts->timeout;
    int32_t delta = btstack_time_delta(list_timeout, now);
    if (delta < 0){
        delta = 0;
    }
    return delta;
}

void btstack_run_loop_base_poll_data_sources(void){
    // poll data sources
    btstack_data_source_t *ds;
    btstack_data_source_t *next;
    for (ds = (btstack_data_source_t *) btstack_run_loop_base_data_sources; ds != NULL ; ds = next){
        next = (btstack_data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
        if (ds->flags & DATA_SOURCE_CALLBACK_POLL){
            ds->process(ds, DATA_SOURCE_CALLBACK_POLL);
        }
    }
}

/**
 * BTstack Run Loop Implementation, mainly dispatches to port-specific implementation
 */

// main implementation

void btstack_run_loop_set_timer_handler(btstack_timer_source_t *ts, void (*process)(btstack_timer_source_t *_ts)){
    ts->process = process;
};

void btstack_run_loop_set_data_source_handler(btstack_data_source_t *ds, void (*process)(btstack_data_source_t *_ds,  btstack_data_source_callback_type_t callback_type)){
    ds->process = process;
};

void btstack_run_loop_set_data_source_fd(btstack_data_source_t *ds, int fd){
    ds->source.fd = fd;
}

int btstack_run_loop_get_data_source_fd(btstack_data_source_t *ds){
    return ds->source.fd;
}

void btstack_run_loop_set_data_source_handle(btstack_data_source_t *ds, void * handle){
    ds->source.handle = handle;
}

void * btstack_run_loop_get_data_source_handle(btstack_data_source_t *ds){
    return ds->source.handle;
}

void btstack_run_loop_enable_data_source_callbacks(btstack_data_source_t *ds, uint16_t callbacks){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    the_run_loop->enable_data_source_callbacks(ds, callbacks);
}

void btstack_run_loop_disable_data_source_callbacks(btstack_data_source_t *ds, uint16_t callbacks){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    the_run_loop->disable_data_source_callbacks(ds, callbacks);
}

/**
 * Add data_source to run_loop
 */
void btstack_run_loop_add_data_source(btstack_data_source_t *ds){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    btstack_assert(ds->process != NULL);
    the_run_loop->add_data_source(ds);
}

/**
 * Remove data_source from run loop
 */
int btstack_run_loop_remove_data_source(btstack_data_source_t *ds){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->disable_data_source_callbacks != NULL);
    btstack_assert(ds->process != NULL);
    return the_run_loop->remove_data_source(ds);
}

void btstack_run_loop_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    btstack_assert(the_run_loop != NULL);
    the_run_loop->set_timer(a, timeout_in_ms);
}

/**
 * @brief Set context for this timer
 */
void btstack_run_loop_set_timer_context(btstack_timer_source_t *ts, void * context){
    ts->context = context;
}

/**
 * @brief Get context for this timer
 */
void * btstack_run_loop_get_timer_context(btstack_timer_source_t *ts){
    return ts->context;
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void btstack_run_loop_add_timer(btstack_timer_source_t *ts){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(ts->process != NULL);
    the_run_loop->add_timer(ts);
}

/**
 * Remove timer from run loop
 */
int btstack_run_loop_remove_timer(btstack_timer_source_t *ts){
    btstack_assert(the_run_loop != NULL);
    return the_run_loop->remove_timer(ts);
}

/**
 * @brief Get current time in ms
 */
uint32_t btstack_run_loop_get_time_ms(void){
    btstack_assert(the_run_loop != NULL);
    return the_run_loop->get_time_ms();
}


void btstack_run_loop_timer_dump(void){
    btstack_assert(the_run_loop != NULL);
    the_run_loop->dump_timer();
}

/**
 * Execute run_loop
 */
void btstack_run_loop_execute(void){
    btstack_assert(the_run_loop != NULL);
    the_run_loop->execute();
}

// init must be called before any other run_loop call
void btstack_run_loop_init(const btstack_run_loop_t * run_loop){
    btstack_assert(the_run_loop == NULL);
    the_run_loop = run_loop;
    the_run_loop->init();
}

void btstack_run_loop_deinit(void){
    the_run_loop = NULL;
}


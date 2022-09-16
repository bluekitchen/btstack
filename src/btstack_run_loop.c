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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
btstack_linked_list_t  btstack_run_loop_base_timers;
btstack_linked_list_t  btstack_run_loop_base_data_sources;
btstack_linked_list_t  btstack_run_loop_base_callbacks;

void btstack_run_loop_base_init(void){
    btstack_run_loop_base_timers = NULL;
    btstack_run_loop_base_data_sources = NULL;
    btstack_run_loop_base_callbacks = NULL;
}

void btstack_run_loop_base_add_data_source(btstack_data_source_t * data_source){
    btstack_linked_list_add(&btstack_run_loop_base_data_sources, (btstack_linked_item_t *) data_source);
}

bool btstack_run_loop_base_remove_data_source(btstack_data_source_t * data_source){
    return btstack_linked_list_remove(&btstack_run_loop_base_data_sources, (btstack_linked_item_t *) data_source);
}

void btstack_run_loop_base_enable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callback_types){
    data_source->flags |= callback_types;
}

void btstack_run_loop_base_disable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callback_types){
    data_source->flags &= ~callback_types;
}

bool btstack_run_loop_base_remove_timer(btstack_timer_source_t * timer){
    return btstack_linked_list_remove(&btstack_run_loop_base_timers, (btstack_linked_item_t *) timer);
}

void btstack_run_loop_base_add_timer(btstack_timer_source_t * timer){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &btstack_run_loop_base_timers; it->next ; it = it->next){
        btstack_timer_source_t * next = (btstack_timer_source_t *) it->next;

        if (next == timer){
            log_error("Timer %p already registered! Please read source code comment.", timer);
            //
            // Dear BTstack User!
            //
            // If you hit the assert below, your application code tried to add a timer to the list of
            // timers that's already in the timer list, i.e., it's already registered.
            //
            // As you've probably already modified the timer, just ignoring this might lead to unexpected
            // and hard to debug issues. Instead, we decided to raise an assert in this case to help.
            //
            // Please do a backtrace and check where you register this timer.
            // If you just want to restart it you can call btstack_run_loop_timer_remove(..) before restarting the timer.
            //
            btstack_assert(false);
        }

        int32_t delta = btstack_time_delta(timer->timeout, next->timeout);
        if (delta < 0) break;
    }
    timer->item.next = it->next;
    it->next = (btstack_linked_item_t *) timer;
}

void btstack_run_loop_base_process_timers(uint32_t now){
    // process timers, exit when timeout is in the future
    while (btstack_run_loop_base_timers) {
        btstack_timer_source_t * timer = (btstack_timer_source_t *) btstack_run_loop_base_timers;
        int32_t delta = btstack_time_delta(timer->timeout, now);
        if (delta > 0) break;
        btstack_run_loop_base_remove_timer(timer);
        timer->process(timer);
    }
}

void btstack_run_loop_base_dump_timer(void){
#ifdef ENABLE_LOG_INFO
    btstack_linked_item_t *it;
    uint16_t i = 0;
    for (it = (btstack_linked_item_t *) btstack_run_loop_base_timers; it ; it = it->next){
        btstack_timer_source_t * timer = (btstack_timer_source_t*) it;
        log_info("timer %u (%p): timeout %" PRIbtstack_time_t "\n", i, (void *) timer, timer->timeout);
    }
#endif

}
/**
 * @brief Get time until first timer fires
 * @return -1 if no timers, time until next timeout otherwise
 */
int32_t btstack_run_loop_base_get_time_until_timeout(uint32_t now){
    if (btstack_run_loop_base_timers == NULL) return -1;
    btstack_timer_source_t * timer = (btstack_timer_source_t *) btstack_run_loop_base_timers;
    uint32_t list_timeout  = timer->timeout;
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

void btstack_run_loop_base_add_callback(btstack_context_callback_registration_t * callback_registration){
    btstack_linked_list_add_tail(&btstack_run_loop_base_callbacks, (btstack_linked_item_t *) callback_registration);
}


void btstack_run_loop_base_execute_callbacks(void){
    while (1){
        btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
        if (callback_registration == NULL){
            break;
        }
        (*callback_registration->callback)(callback_registration->context);
    }
}


/**
 * BTstack Run Loop Implementation, mainly dispatches to port-specific implementation
 */

// main implementation

void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*process)(btstack_timer_source_t * _timer)){
    timer->process = process;
}

void btstack_run_loop_set_data_source_handler(btstack_data_source_t * data_source, void (*process)(btstack_data_source_t *_data_source,  btstack_data_source_callback_type_t callback_type)){
    data_source->process = process;
}

void btstack_run_loop_set_data_source_fd(btstack_data_source_t * data_source, int fd){
    data_source->source.fd = fd;
}

int btstack_run_loop_get_data_source_fd(btstack_data_source_t * data_source){
    return data_source->source.fd;
}

void btstack_run_loop_set_data_source_handle(btstack_data_source_t * data_source, void * handle){
    data_source->source.handle = handle;
}

void * btstack_run_loop_get_data_source_handle(btstack_data_source_t * data_source){
    return data_source->source.handle;
}

void btstack_run_loop_enable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    the_run_loop->enable_data_source_callbacks(data_source, callbacks);
}

void btstack_run_loop_disable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    the_run_loop->disable_data_source_callbacks(data_source, callbacks);
}

/**
 * Add data_source to run_loop
 */
void btstack_run_loop_add_data_source(btstack_data_source_t * data_source){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->enable_data_source_callbacks != NULL);
    btstack_assert(data_source->process != NULL);
    the_run_loop->add_data_source(data_source);
}

/**
 * Remove data_source from run loop
 */
int btstack_run_loop_remove_data_source(btstack_data_source_t * data_source){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->disable_data_source_callbacks != NULL);
    btstack_assert(data_source->process != NULL);
    return the_run_loop->remove_data_source(data_source);
}

void btstack_run_loop_poll_data_sources_from_irq(void){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->poll_data_sources_from_irq != NULL);
    the_run_loop->poll_data_sources_from_irq();
}

void btstack_run_loop_set_timer(btstack_timer_source_t *timer, uint32_t timeout_in_ms){
    btstack_assert(the_run_loop != NULL);
    the_run_loop->set_timer(timer, timeout_in_ms);
}

/**
 * @brief Set context for this timer
 */
void btstack_run_loop_set_timer_context(btstack_timer_source_t * timer, void * context){
    timer->context = context;
}

/**
 * @brief Get context for this timer
 */
void * btstack_run_loop_get_timer_context(btstack_timer_source_t * timer){
    return timer->context;
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void btstack_run_loop_add_timer(btstack_timer_source_t * timer){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(timer->process != NULL);
    the_run_loop->add_timer(timer);
}

/**
 * Remove timer from run loop
 */
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){
    btstack_assert(the_run_loop != NULL);
    return the_run_loop->remove_timer(timer);
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

void btstack_run_loop_trigger_exit(void){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->trigger_exit != NULL);
    the_run_loop->trigger_exit();
}

void btstack_run_loop_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    btstack_assert(the_run_loop != NULL);
    btstack_assert(the_run_loop->execute_on_main_thread != NULL);
    the_run_loop->execute_on_main_thread(callback_registration);
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


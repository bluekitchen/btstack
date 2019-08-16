/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_run_loop_base.c"

/*
 *  btstack_run_loop_base.h
 *
 *  Portable implementation of timer and data source managment as base for platform specific implementations
 */

#include "btstack_debug.h"
#include "btstack_config.h"
#include "btstack_util.h"

#include "btstack_run_loop_base.h"

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

int btstack_run_loop_base_remove_data_source(btstack_data_source_t *ds){
    return btstack_linked_list_remove(&btstack_run_loop_base_data_sources, (btstack_linked_item_t *) ds);
}

void btstack_run_loop_base_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags |= callback_types;
}

void btstack_run_loop_base_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    ds->flags &= ~callback_types;
}


int btstack_run_loop_base_remove_timer(btstack_timer_source_t *ts){
    return btstack_linked_list_remove(&btstack_run_loop_base_timers, (btstack_linked_item_t *) ts);
}

void btstack_run_loop_base_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &btstack_run_loop_base_timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        // exit if list timeout is after new timeout
        uint32_t list_timeout = ((btstack_timer_source_t *) it->next)->timeout;
        int32_t delta = btstack_time_delta(ts->timeout, list_timeout);
        if (delta < 0) break;
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
}

void  btstack_run_loop_base_process_timers(uint32_t now){
    // process timers, exit when timeout is in the future
    while (btstack_run_loop_base_timers) {
        btstack_timer_source_t * ts = (btstack_timer_source_t *) btstack_run_loop_base_timers;
        int32_t delta = btstack_time_delta(ts->timeout, now);
        if (delta > 0) break;
        btstack_run_loop_base_remove_timer(ts);
        ts->process(ts);
    }
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

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

#include <stdio.h>
#include <stdlib.h>  // exit()


#include "btstack_debug.h"
#include "btstack_config.h"

static const btstack_run_loop_t * the_run_loop = NULL;

extern const btstack_run_loop_t btstack_run_loop_embedded;

// assert run loop initialized
static void btstack_run_loop_assert(void){
    if (!the_run_loop){
        log_error("ERROR: run_loop function called before btstack_run_loop_init!");
        while(1);
    }
}


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
    btstack_run_loop_assert();
    if (the_run_loop->enable_data_source_callbacks){
        the_run_loop->enable_data_source_callbacks(ds, callbacks);
    } else {
        log_error("btstack_run_loop_remove_data_source not implemented");
    }
}

void btstack_run_loop_disable_data_source_callbacks(btstack_data_source_t *ds, uint16_t callbacks){
    btstack_run_loop_assert();
    if (the_run_loop->disable_data_source_callbacks){
        the_run_loop->disable_data_source_callbacks(ds, callbacks);
    } else {
        log_error("btstack_run_loop_disable_data_source_callbacks not implemented");
    }
}

/**
 * Add data_source to run_loop
 */
void btstack_run_loop_add_data_source(btstack_data_source_t *ds){
    btstack_run_loop_assert();
    if (the_run_loop->add_data_source){
        the_run_loop->add_data_source(ds);
    } else {
        log_error("btstack_run_loop_add_data_source not implemented");
    }
}

/**
 * Remove data_source from run loop
 */
int btstack_run_loop_remove_data_source(btstack_data_source_t *ds){
    btstack_run_loop_assert();
    if (the_run_loop->remove_data_source){
        return the_run_loop->remove_data_source(ds);
    } else {
        log_error("btstack_run_loop_remove_data_source not implemented");
        return 0;
    }
}

void btstack_run_loop_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    btstack_run_loop_assert();
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
    btstack_run_loop_assert();
    the_run_loop->add_timer(ts);
}

/**
 * Remove timer from run loop
 */
int btstack_run_loop_remove_timer(btstack_timer_source_t *ts){
    btstack_run_loop_assert();
    return the_run_loop->remove_timer(ts);
}

/**
 * @brief Get current time in ms
 */
uint32_t btstack_run_loop_get_time_ms(void){
    btstack_run_loop_assert();
    return the_run_loop->get_time_ms();
}


void btstack_run_loop_timer_dump(void){
    btstack_run_loop_assert();
    the_run_loop->dump_timer();
}

/**
 * Execute run_loop
 */
void btstack_run_loop_execute(void){
    btstack_run_loop_assert();
    the_run_loop->execute();
}

// init must be called before any other run_loop call
void btstack_run_loop_init(const btstack_run_loop_t * run_loop){
    if (the_run_loop){
        log_error("ERROR: run loop initialized twice!");
        while(1);
    }
    the_run_loop = run_loop;
    the_run_loop->init();
}


/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

//#include <errno.h>
//#include <stddef.h>
//#include <stdio.h>
//#include <string.h>

// Zephyr
#include <zephyr/kernel.h>

// BTstack
#include "btstack_debug.h"
#include "btstack_run_loop_zephyr.h"

#define SIGNAL_POLL (0xCA11)
#define SIGNAL_EXEC (0xE8EC)

#define NUM_EVENTS 10

static struct k_poll_event events[NUM_EVENTS];
static btstack_data_source_t *data_sources[NUM_EVENTS];

bool btstack_run_loop_zephyr_exit_requested = false;
bool btstack_run_loop_zephyr_data_sources_modified = false;

/**
 * Add data_source to run_loop
 */
static void btstack_run_loop_zephyr_add_data_source(btstack_data_source_t *ds){
    btstack_run_loop_zephyr_data_sources_modified = true;
    btstack_run_loop_base_add_data_source(ds);
}

/**
 * Remove data_source from run loop
 */
static bool btstack_run_loop_zephyr_remove_data_source(btstack_data_source_t *ds){
    btstack_run_loop_zephyr_data_sources_modified = true;
    return btstack_run_loop_base_remove_data_source(ds);
}

// TODO: handle 32 bit ms time overrun
static uint32_t btstack_run_loop_zephyr_get_time_ms(void){
    return  k_uptime_get_32();
}

static void btstack_run_loop_zephyr_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = k_uptime_get_32() + 1 + timeout_in_ms;
}

static struct k_poll_signal signal;
static fat_variable_t signal_variable = { .variable = &signal, .id = K_POLL_TYPE_SIGNAL };
static btstack_data_source_t signal_data_source;

static void btstack_run_loop_zephyr_signal( int value ) {
    k_poll_signal_raise(&signal, value);
}

static K_MUTEX_DEFINE(btstack_run_loop_zephyr_callbacks_mutex);

static void btstack_run_loop_zephyr_signal_handler(
        btstack_data_source_t * ds,
        btstack_data_source_callback_type_t callback_type ) {
    int signaled, result;
    k_poll_signal_check(&signal, &signaled, &result);
    k_poll_signal_reset(&signal);
    if( !signaled ) {
        return;
    }
    log_debug("%s( %x )", __func__, result );
    switch( result ) {
    case SIGNAL_POLL:
        btstack_run_loop_base_poll_data_sources();
        break;
    case SIGNAL_EXEC:
        for (;;){
            k_mutex_lock(&btstack_run_loop_zephyr_callbacks_mutex, K_FOREVER);
            btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
            k_mutex_unlock(&btstack_run_loop_zephyr_callbacks_mutex);
            if (callback_registration == NULL){
                break;
            }
            (*callback_registration->callback)(callback_registration->context);
        }
        break;
    default:
        break;
    }

}

#ifdef ENABLE_LOG_DEBUG
#define STR(x) #x
static const char *event_state_to_text[] = {
    [K_POLL_STATE_SEM_AVAILABLE]       = STR(K_POLL_STATE_SEM_AVAILABLE),
    [K_POLL_STATE_FIFO_DATA_AVAILABLE] = STR(K_POLL_STATE_FIFO_DATA_AVAILABLE),
    [K_POLL_STATE_MSGQ_DATA_AVAILABLE] = STR(K_POLL_STATE_MSGQ_DATA_AVAILABLE),
    [K_POLL_STATE_PIPE_DATA_AVAILABLE] = STR(K_POLL_STATE_PIPE_DATA_AVAILABLE),
    [K_POLL_STATE_SIGNALED]            = STR(K_POLL_STATE_SIGNALED),
};

static const char *event_type_to_text[] = {
    [K_POLL_TYPE_SEM_AVAILABLE]       = STR(K_POLL_TYPE_SEM_AVAILABLE),
    [K_POLL_TYPE_FIFO_DATA_AVAILABLE] = STR(K_POLL_TYPE_FIFO_DATA_AVAILABLE),
    [K_POLL_TYPE_MSGQ_DATA_AVAILABLE] = STR(K_POLL_TYPE_MSGQ_DATA_AVAILABLE),
    [K_POLL_TYPE_PIPE_DATA_AVAILABLE] = STR(K_POLL_TYPE_PIPE_DATA_AVAILABLE),
    [K_POLL_TYPE_SIGNAL]              = STR(K_POLL_TYPE_SIGNAL),
};
#endif

/**
 * Execute run_loop
 */
static void btstack_run_loop_zephyr_execute(void) {
    btstack_linked_list_iterator_t it;
    btstack_run_loop_zephyr_exit_requested = false;

    k_poll_signal_init( &signal );

    btstack_data_source_t *ds = &signal_data_source;
    btstack_run_loop_set_data_source_handle(ds, &signal_variable);
    btstack_run_loop_set_data_source_handler(ds, &btstack_run_loop_zephyr_signal_handler);
    btstack_run_loop_add_data_source(ds);
    btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    const uint32_t state_mask =
        K_POLL_STATE_SEM_AVAILABLE |
        K_POLL_STATE_FIFO_DATA_AVAILABLE |
        K_POLL_STATE_MSGQ_DATA_AVAILABLE |
        K_POLL_STATE_PIPE_DATA_AVAILABLE |
        K_POLL_STATE_SIGNALED;

    for (;btstack_run_loop_zephyr_exit_requested!=true;) {
        // process timers
        uint32_t now = k_uptime_get_32();
        btstack_run_loop_base_process_timers(now);

        btstack_linked_list_iterator_init(&it, &btstack_run_loop_base_data_sources);
        int nbr_events = 0;
        while( btstack_linked_list_iterator_has_next(&it) && (nbr_events<NUM_EVENTS) ) {
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            fat_variable_t *fat = (fat_variable_t*)ds->source.handle;
            int current_event = nbr_events++;
            log_debug("arm [%d] %s", current_event, event_type_to_text[fat->id]);
            k_poll_event_init( &events[current_event],
                    fat->id,
                    K_POLL_MODE_NOTIFY_ONLY,
                    fat->variable);
            data_sources[current_event] = ds;
        }
        // get time until next timer expires
        int32_t timeout_ms = btstack_run_loop_base_get_time_until_timeout(now);
        k_timeout_t timeout = K_MSEC(timeout_ms);
        if (timeout_ms < 0){
            timeout = K_FOREVER;
        }
        int rc = k_poll(events, nbr_events, timeout);
        if( btstack_run_loop_zephyr_exit_requested ) {
            return;
        }
        // handle timeouts on -EAGAIN
        if( rc != 0 ) {
            continue;
        }
        for( int i=0; i<nbr_events; ++i ) {
            uint32_t state = events[i].state;
            if((state & state_mask) > 0) {
                log_debug("trigger [%d] %s", i, event_state_to_text[state]);
                btstack_data_source_t *ds = data_sources[i];
                ds->process(ds, DATA_SOURCE_CALLBACK_READ);
            }
        }
    }
}

static void btstack_run_loop_zephyr_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    // protect list with mutex
    k_mutex_lock(&btstack_run_loop_zephyr_callbacks_mutex, K_FOREVER);
    btstack_run_loop_base_add_callback(callback_registration);
    k_mutex_unlock(&btstack_run_loop_zephyr_callbacks_mutex);
    // trigger run loop
    btstack_run_loop_zephyr_signal( SIGNAL_EXEC );
}

static void btstack_run_loop_zephyr_btstack_run_loop_init(void){
    btstack_run_loop_base_init();
}

static void btstack_run_loop_zephyr_poll_data_sources_from_irq(void) {
    btstack_run_loop_zephyr_signal( SIGNAL_POLL );
}

static void btstack_run_loop_zephyr_trigger_exit(void) {
    btstack_run_loop_zephyr_exit_requested = true;
    btstack_run_loop_zephyr_signal( SIGNAL_POLL );
}

static const btstack_run_loop_t btstack_run_loop_zephyr = {
    &btstack_run_loop_zephyr_btstack_run_loop_init,
    &btstack_run_loop_zephyr_add_data_source,
    &btstack_run_loop_zephyr_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &btstack_run_loop_zephyr_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_zephyr_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_zephyr_get_time_ms,
    &btstack_run_loop_zephyr_poll_data_sources_from_irq,
    &btstack_run_loop_zephyr_execute_on_main_thread,
    &btstack_run_loop_zephyr_trigger_exit,
};
/**
 * @brief Provide btstack_run_loop_zephyr instance for use with btstack_run_loop_init
 */
const btstack_run_loop_t * btstack_run_loop_zephyr_get_instance(void){
    return &btstack_run_loop_zephyr;
}


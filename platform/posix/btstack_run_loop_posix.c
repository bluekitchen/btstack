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

#define BTSTACK_FILE__ "btstack_run_loop_posix.c"

/*
 *  btstack_run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

// enable POSIX functions (needed for -std=c99)
#define _POSIX_C_SOURCE 200809

#include "btstack_run_loop_posix.h"

#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "btstack_linked_list.h"
#include "btstack_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

// the run loop
static int btstack_run_loop_posix_data_sources_modified;

static bool btstack_run_loop_posix_exit_requested;

// to trigger process callbacks other thread
static pthread_mutex_t       btstack_run_loop_posix_callbacks_mutex = PTHREAD_MUTEX_INITIALIZER;
static int                   btstack_run_loop_posix_process_callbacks_fd;
static btstack_data_source_t btstack_run_loop_posix_process_callbacks_ds;

// to trigger poll data sources from irq
static int                   btstack_run_loop_posix_poll_data_sources_fd;
static btstack_data_source_t btstack_run_loop_posix_poll_data_sources_ds;

// start time. tv_usec/tv_nsec = 0
#ifdef _POSIX_MONOTONIC_CLOCK
// use monotonic clock if available
static struct timespec init_ts;
#else
// fallback to gettimeofday
static struct timeval init_tv;
#endif

/**
 * Add data_source to run_loop
 */
static void btstack_run_loop_posix_add_data_source(btstack_data_source_t *ds){
    btstack_run_loop_posix_data_sources_modified = 1;
    btstack_run_loop_base_add_data_source(ds);
}

/**
 * Remove data_source from run loop
 */
static bool btstack_run_loop_posix_remove_data_source(btstack_data_source_t *ds){
    btstack_run_loop_posix_data_sources_modified = 1;
    return btstack_run_loop_base_remove_data_source(ds);
}

#ifdef _POSIX_MONOTONIC_CLOCK
/**
 * @brief Returns the timespec which represents the time(stop - start). It might be negative
 */
static void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result){
    result->tv_sec = stop->tv_sec - start->tv_sec;
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

/**
 * @brief Convert timespec to miliseconds, might overflow
 */
static uint64_t timespec_to_milliseconds(struct timespec *a){
    uint64_t ret = 0;
    uint64_t sec_val = (uint64_t)(a->tv_sec);
    uint64_t nsec_val = (uint64_t)(a->tv_nsec);
    ret = (sec_val*1000) + (nsec_val/1000000);
    return ret;
}

/**
 * @brief Returns the milisecond value of (stop - start). Might overflow
 */
static uint64_t timespec_diff_milis(struct timespec* start, struct timespec* stop){
    struct timespec diff_ts;
    timespec_diff(start, stop, &diff_ts);
    return timespec_to_milliseconds(&diff_ts);
}
#endif

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_posix_get_time_ms(void){
    uint32_t time_ms;
#ifdef _POSIX_MONOTONIC_CLOCK
    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);
    time_ms = (uint32_t) timespec_diff_milis(&init_ts, &now_ts);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_ms = (uint32_t) ((tv.tv_sec  - init_tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
#endif
    return time_ms;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_posix_execute(void) {
    fd_set descriptors_read;
    fd_set descriptors_write;
    
    btstack_linked_list_iterator_t it;
    struct timeval * timeout;
    struct timeval tv;
    uint32_t now_ms;

#ifdef _POSIX_MONOTONIC_CLOCK
    log_info("POSIX run loop with monotonic clock");
#else
    log_info("POSIX run loop using ettimeofday fallback.");
#endif

    while (btstack_run_loop_posix_exit_requested == false) {
        // collect FDs
        FD_ZERO(&descriptors_read);
        FD_ZERO(&descriptors_write);
        int highest_fd = -1;
        btstack_linked_list_iterator_init(&it, &btstack_run_loop_base_data_sources);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
            if (ds->source.fd < 0) continue;
            if (ds->flags & DATA_SOURCE_CALLBACK_READ){
                FD_SET(ds->source.fd, &descriptors_read);
                if (ds->source.fd > highest_fd) {
                    highest_fd = ds->source.fd;
                }
                log_debug("btstack_run_loop_execute adding fd %u for read", ds->source.fd);
            }
            if (ds->flags & DATA_SOURCE_CALLBACK_WRITE){
                FD_SET(ds->source.fd, &descriptors_write);
                if (ds->source.fd > highest_fd) {
                    highest_fd = ds->source.fd;
                }
                log_debug("btstack_run_loop_execute adding fd %u for write", ds->source.fd);
            }
        }

        // get next timeout
        timeout = NULL;
        now_ms = btstack_run_loop_posix_get_time_ms();
        int32_t delta_ms = btstack_run_loop_base_get_time_until_timeout(now_ms);
        if (delta_ms >= 0) {
            timeout = &tv;
            tv.tv_sec  = delta_ms / 1000;
            tv.tv_usec = (int) (delta_ms - (tv.tv_sec * 1000)) * 1000;
            log_debug("btstack_run_loop_execute next timeout in %u ms", delta_ms);
        }
                
        // wait for ready FDs
        int res = select( highest_fd+1 , &descriptors_read, &descriptors_write, NULL, timeout);
        if (res < 0){
            log_error("btstack_run_loop_posix_execute: select -> errno %u", errno);
        }
        if (res > 0){
            btstack_run_loop_posix_data_sources_modified = 0;
            btstack_linked_list_iterator_init(&it, &btstack_run_loop_base_data_sources);
            while (btstack_linked_list_iterator_has_next(&it) && !btstack_run_loop_posix_data_sources_modified){
                btstack_data_source_t *ds = (btstack_data_source_t*) btstack_linked_list_iterator_next(&it);
                log_debug("btstack_run_loop_posix_execute: check ds %p with fd %u\n", ds, ds->source.fd);
                if (FD_ISSET(ds->source.fd, &descriptors_read)) {
                    log_debug("btstack_run_loop_posix_execute: process read ds %p with fd %u\n", ds, ds->source.fd);
                    ds->process(ds, DATA_SOURCE_CALLBACK_READ);
                }
                if (btstack_run_loop_posix_data_sources_modified) break;
                if (FD_ISSET(ds->source.fd, &descriptors_write)) {
                    log_debug("btstack_run_loop_posix_execute: process write ds %p with fd %u\n", ds, ds->source.fd);
                    ds->process(ds, DATA_SOURCE_CALLBACK_WRITE);
                }
            }
        }
        log_debug("btstack_run_loop_posix_execute: after ds check\n");

        // process timers
        now_ms = btstack_run_loop_posix_get_time_ms();
        btstack_run_loop_base_process_timers(now_ms);
    }
}

static void btstack_run_loop_posix_trigger_exit(void){
    btstack_run_loop_posix_exit_requested = true;
}

// set timer
static void btstack_run_loop_posix_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    uint32_t time_ms = btstack_run_loop_posix_get_time_ms();
    a->timeout = time_ms + timeout_in_ms;
    log_debug("btstack_run_loop_posix_set_timer to %u ms (now %u, timeout %u)", a->timeout, time_ms, timeout_in_ms);
}

// trigger pipe
static void btstack_run_loop_posix_trigger_pipe(int fd){
    if (fd < 0) return;
    const uint8_t x = (uint8_t) 'x';
    ssize_t bytes_written = write(fd, &x, 1);
    UNUSED(bytes_written);
}

// poll data sources from irq

static void btstack_run_loop_posix_poll_data_sources_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(callback_type);
    uint8_t buffer[1];
    ssize_t bytes_read = read(ds->source.fd, buffer, 1);
    UNUSED(bytes_read);
    // poll data sources
    btstack_run_loop_base_poll_data_sources();
}

static void btstack_run_loop_posix_poll_data_sources_from_irq(void){
    // trigger run loop
    btstack_run_loop_posix_trigger_pipe(btstack_run_loop_posix_poll_data_sources_fd);
}

// execute on main thread from same or different thread

static void btstack_run_loop_posix_process_callbacks_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(callback_type);
    uint8_t buffer[1];
    ssize_t bytes_read = read(ds->source.fd, buffer, 1);
    UNUSED(bytes_read);
    // execute callbacks - protect list with mutex
    while (1){
        pthread_mutex_lock(&btstack_run_loop_posix_callbacks_mutex);
        btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
        pthread_mutex_unlock(&btstack_run_loop_posix_callbacks_mutex);
        if (callback_registration == NULL){
            break;
        }
        (*callback_registration->callback)(callback_registration->context);
    }
}

static void btstack_run_loop_posix_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    // protect list with mutex
    pthread_mutex_lock(&btstack_run_loop_posix_callbacks_mutex);
    btstack_run_loop_base_add_callback(callback_registration);
    pthread_mutex_unlock(&btstack_run_loop_posix_callbacks_mutex);
    // trigger run loop
    btstack_run_loop_posix_trigger_pipe(btstack_run_loop_posix_process_callbacks_fd);
}

//init

// @return fd >= 0 on success
static int btstack_run_loop_posix_register_pipe_datasource(btstack_data_source_t * data_source){
    int fildes[2]; // 0 = read,  1 = write
    int status = pipe(fildes);
    if (status != 0){
        log_error("pipe() failed");
        return -1;
    }
    data_source->source.fd = fildes[0];
    data_source->flags = DATA_SOURCE_CALLBACK_READ;
    btstack_run_loop_base_add_data_source(data_source);
    log_info("Pipe: in %u, out %u", fildes[1], fildes[0]);
    return fildes[1];
}

static void btstack_run_loop_posix_init(void){
    btstack_run_loop_base_init();
    
#ifdef _POSIX_MONOTONIC_CLOCK
    clock_gettime(CLOCK_MONOTONIC, &init_ts);
    init_ts.tv_nsec = 0;
#else
    // just assume that we started at tv_usec == 0
    gettimeofday(&init_tv, NULL);
    init_tv.tv_usec = 0;
#endif

    // setup pipe to trigger process callbacks
    btstack_run_loop_posix_process_callbacks_ds.process = &btstack_run_loop_posix_process_callbacks_handler;
    btstack_run_loop_posix_process_callbacks_fd = btstack_run_loop_posix_register_pipe_datasource(&btstack_run_loop_posix_process_callbacks_ds);

    // setup pipe to poll data sources
    btstack_run_loop_posix_poll_data_sources_ds.process = &btstack_run_loop_posix_poll_data_sources_handler;
    btstack_run_loop_posix_poll_data_sources_fd = btstack_run_loop_posix_register_pipe_datasource(&btstack_run_loop_posix_poll_data_sources_ds);
}

static const btstack_run_loop_t btstack_run_loop_posix = {
    &btstack_run_loop_posix_init,
    &btstack_run_loop_posix_add_data_source,
    &btstack_run_loop_posix_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &btstack_run_loop_posix_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_posix_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_posix_get_time_ms,
    &btstack_run_loop_posix_poll_data_sources_from_irq,
    &btstack_run_loop_posix_execute_on_main_thread,
    &btstack_run_loop_posix_trigger_exit,
};

/**
 * Provide btstack_run_loop_posix instance
 */
const btstack_run_loop_t * btstack_run_loop_posix_get_instance(void){
    return &btstack_run_loop_posix;
}


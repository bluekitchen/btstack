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

#define BTSTACK_FILE__ "btstack_run_loop_qt.c"

/*
 *  btstack_run_loop_qt.c
 */

// enable POSIX functions (needed for -std=c99)
#define _POSIX_C_SOURCE 200809

#include "btstack_run_loop_qt.h"

#include "btstack_run_loop.h"
#include "btstack_run_loop_base.h"
#include "btstack_util.h"
#include "btstack_linked_list.h"
#include "btstack_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void btstack_run_loop_qt_dump_timer(void);

// start time. tv_usec/tv_nsec = 0
#ifdef _POSIX_MONOTONIC_CLOCK
// use monotonic clock if available
static struct timespec init_ts;
#else
// fallback to gettimeofday
static struct timeval init_tv;
#endif

static BTstackRunLoopQt * btstack_run_loop_object;

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
static uint32_t btstack_run_loop_qt_get_time_ms(void){
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
static void btstack_run_loop_qt_execute(void) {
}

// set timer
static void btstack_run_loop_qt_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    uint32_t time_ms = btstack_run_loop_qt_get_time_ms();
    a->timeout = time_ms + timeout_in_ms;
    log_debug("btstack_run_loop_qt_set_timer to %u ms (now %u, timeout %u)", a->timeout, time_ms, timeout_in_ms);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_qt_add_timer(btstack_timer_source_t *ts){
    uint32_t now = btstack_run_loop_qt_get_time_ms();
    int32_t next_timeout_before = btstack_run_loop_base_get_time_until_timeout(now);
    btstack_run_loop_base_add_timer(ts);
    int32_t next_timeout_after = btstack_run_loop_base_get_time_until_timeout(now);
    if (next_timeout_after >= 0 && (next_timeout_after != next_timeout_before)){
        QTimer::singleShot((uint32_t)next_timeout_after, btstack_run_loop_object, SLOT(processTimers()));
    }
}

// BTstackRunLoopQt class implementation
void BTstackRunLoopQt::processTimers(){
    uint32_t now = btstack_run_loop_qt_get_time_ms();
    int32_t next_timeout_before = btstack_run_loop_base_get_time_until_timeout(now);
    btstack_run_loop_base_process_timers(btstack_run_loop_qt_get_time_ms());
    int32_t next_timeout_after = btstack_run_loop_base_get_time_until_timeout(now);
    if (next_timeout_after >= 0 && (next_timeout_after != next_timeout_before)){
        QTimer::singleShot((uint32_t)next_timeout_after, this, SLOT(processTimers()));
    }
}

static void btstack_run_loop_qt_init(void){

    btstack_run_loop_base_init();

    btstack_run_loop_object = new BTstackRunLoopQt();

#ifdef _POSIX_MONOTONIC_CLOCK
    clock_gettime(CLOCK_MONOTONIC, &init_ts);
    init_ts.tv_nsec = 0;
#else
    // just assume that we started at tv_usec == 0
    gettimeofday(&init_tv, NULL);
    init_tv.tv_usec = 0;
#endif
}

static void btstack_run_loop_qt_dump_timer(void){
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) btstack_run_loop_base_timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u (%p): timeout %u\n", i, ts, ts->timeout);
    }
}

static const btstack_run_loop_t btstack_run_loop_qt = {
    &btstack_run_loop_qt_init,
    &btstack_run_loop_base_add_data_source,
    &btstack_run_loop_base_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &btstack_run_loop_qt_set_timer,
    &btstack_run_loop_qt_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_qt_execute,
    &btstack_run_loop_qt_dump_timer,
    &btstack_run_loop_qt_get_time_ms,
};

/**
 * Provide btstack_run_loop_posix instance
 */
const btstack_run_loop_t * btstack_run_loop_qt_get_instance(void){
    return &btstack_run_loop_qt;
}


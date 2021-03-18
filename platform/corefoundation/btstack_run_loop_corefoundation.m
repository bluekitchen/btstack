/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "btstack_run_loop_corefoundation.c"

/*
 *  btstack_run_loop_corefoundation.c
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include "btstack_run_loop.h"
#include "btstack_debug.h"

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

static struct timeval init_tv;
static CFAbsoluteTime init_cf;
static CFRunLoopRef current_runloop;

// to trigger run loop from other thread
static int run_loop_pipe_fd;
static btstack_data_source_t run_loop_pipe_ds;
static pthread_mutex_t run_loop_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct  {
    CFSocketRef socket;
    CFRunLoopSourceRef socket_run_loop;
} btstack_corefoundation_data_source_helper_t;

static void theCFRunLoopTimerCallBack (CFRunLoopTimerRef timer,void *info){
    btstack_timer_source_t * ts = (btstack_timer_source_t*)info;
    ts->process(ts);
}

static void socketDataCallback (
						 CFSocketRef s,
						 CFSocketCallBackType callbackType,
						 CFDataRef address,
						 const void *data,
						 void *info) {
	if (!info) return;
    btstack_data_source_t * ds = (btstack_data_source_t *) info;

    if ((callbackType == kCFSocketReadCallBack) && (ds->flags & DATA_SOURCE_CALLBACK_READ)){
        // printf("btstack_run_loop_corefoundation_ds %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) ds, ds->source.fd, (int) s, (int) ds->item.next);
        ds->process(ds, DATA_SOURCE_CALLBACK_READ);
    }
    if ((callbackType == kCFSocketWriteCallBack) && (ds->flags & DATA_SOURCE_CALLBACK_WRITE)){
        // printf("btstack_run_loop_corefoundation_ds %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) ds, ds->source.fd, (int) s, (int) ds->item.next);
        ds->process(ds, DATA_SOURCE_CALLBACK_WRITE);
    }
}

static CFOptionFlags btstack_run_loop_corefoundation_option_flags_for_callback_types(uint16_t callback_types){
    uint16_t option_flags = 0;
    if (callback_types & DATA_SOURCE_CALLBACK_READ){
        option_flags |= kCFSocketReadCallBack;
    }
    if (callback_types & DATA_SOURCE_CALLBACK_WRITE){
        option_flags |= kCFSocketWriteCallBack;
    }
    return option_flags;
}

static void btstack_run_loop_corefoundation_add_data_source(btstack_data_source_t *data_source){

	// add fd as CFSocket

    // allocate memory for Core Foundation references	
    btstack_corefoundation_data_source_helper_t * references = malloc(sizeof(btstack_corefoundation_data_source_helper_t));
    if (!references){
        log_error("btstack_run_loop_corefoundation_add_data_source could not allocate btstack_corefoundation_data_source_helper_t");
        return;
    }

	// store our data_source in socket context
	CFSocketContext socketContext;
	memset(&socketContext, 0, sizeof(CFSocketContext));
	socketContext.info = data_source;

	// create CFSocket from file descriptor
    uint16_t callback_types = btstack_run_loop_corefoundation_option_flags_for_callback_types(data_source->flags);
	CFSocketRef socket = CFSocketCreateWithNative (
										  kCFAllocatorDefault,
										  data_source->source.fd,
										  callback_types,
										  socketDataCallback,
										  &socketContext
    );
    
    // configure options:
    // - don't close native fd on CFSocketInvalidate
    CFOptionFlags socket_options = CFSocketGetSocketFlags(socket);
    socket_options &= ~kCFSocketCloseOnInvalidate;
    CFSocketSetSocketFlags(socket, socket_options);
    
	// create run loop source
	CFRunLoopSourceRef socket_run_loop = CFSocketCreateRunLoopSource ( kCFAllocatorDefault, socket, 0);
    
    // store CFSocketRef and CFRunLoopSource in struct on heap
    memset(references, 0, sizeof(btstack_corefoundation_data_source_helper_t));
    references->socket = socket;
    references->socket_run_loop = socket_run_loop;

    // hack: store btstack_corefoundation_data_source_helper_t in "next" of btstack_linked_item_t
    data_source->item.next      = (void *) references;

    // add to run loop
	CFRunLoopAddSource( CFRunLoopGetCurrent(), socket_run_loop, kCFRunLoopCommonModes);
    // printf("btstack_run_loop_corefoundation_add_data_source    %x - fd %u - CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->source.fd, (int) socket, (int) socket_run_loop);
    
}

static void btstack_run_loop_corefoundation_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    btstack_corefoundation_data_source_helper_t * references = (btstack_corefoundation_data_source_helper_t *) ds->item.next;
    uint16_t option_flags = btstack_run_loop_corefoundation_option_flags_for_callback_types(callback_types);
    CFSocketEnableCallBacks(references->socket, option_flags);   
}

static void btstack_run_loop_corefoundation_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    btstack_corefoundation_data_source_helper_t * references = (btstack_corefoundation_data_source_helper_t *) ds->item.next;
    uint16_t option_flags = btstack_run_loop_corefoundation_option_flags_for_callback_types(callback_types);
    CFSocketDisableCallBacks(references->socket, option_flags);   
}

static bool  btstack_run_loop_corefoundation_remove_data_source(btstack_data_source_t *ds){
    btstack_corefoundation_data_source_helper_t * references = (btstack_corefoundation_data_source_helper_t *) ds->item.next;
    // printf("btstack_run_loop_corefoundation_remove_data_source %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->source.fd, (int) dataSource->item.next, (int) dataSource->item.user_data);
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), references->socket_run_loop, kCFRunLoopCommonModes);
    CFRelease(references->socket_run_loop);

    CFSocketInvalidate(references->socket);
    CFRelease(references->socket);

    free(references);
	return true;
}

static void btstack_run_loop_corefoundation_add_timer(btstack_timer_source_t * ts)
{
    CFAbsoluteTime fireDate = init_cf + ts->timeout;
    CFRunLoopTimerContext timerContext = {0, ts, NULL, NULL, NULL};
    CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate (kCFAllocatorDefault,fireDate,0,0,0,theCFRunLoopTimerCallBack,&timerContext);
    CFRetain(timerRef);

    // hack: store CFRunLoopTimerRef in next pointer of linked_item
    ts->item.next = (void *)timerRef;
    // printf("btstack_run_loop_corefoundation_add_timer %x -> %x now %f, then %f\n", (int) ts, (int) ts->item.next, CFAbsoluteTimeGetCurrent(),fireDate);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef, kCFRunLoopCommonModes);
}

static bool btstack_run_loop_corefoundation_remove_timer(btstack_timer_source_t * ts){
    // printf("btstack_run_loop_corefoundation_remove_timer %x -> %x\n", (int) ts, (int) ts->item.next);
	if (ts->item.next != NULL) {
    	CFRunLoopTimerInvalidate((CFRunLoopTimerRef) ts->item.next);    // also removes timer from run loops + releases it
    	CFRelease((CFRunLoopTimerRef) ts->item.next);
        return true;
	}
	return false;
}

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_corefoundation_get_time_ms(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t time_ms = ((tv.tv_sec  - init_tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
    log_debug("btstack_run_loop_corefoundation_get_time_ms: %u <- %u / %u", time_ms, (int) tv.tv_sec, (int) tv.tv_usec);
    return time_ms;
}

// set timer
static void btstack_run_loop_corefoundation_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    uint32_t time_ms = btstack_run_loop_corefoundation_get_time_ms();
    a->timeout = time_ms + timeout_in_ms;
    log_debug("btstack_run_loop_corefoundation_set_timer to %u ms (now %u, timeout %u)", a->timeout, time_ms, timeout_in_ms);
}

static void btstack_run_loop_corefoundation_pipe_process(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(callback_type);
    uint8_t buffer[1];
    (void) read(ds->source.fd, buffer, 1);
    // execute callbacks - protect list with mutex
    while (1){
        pthread_mutex_lock(&run_loop_callback_mutex);
        btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
        pthread_mutex_unlock(&run_loop_callback_mutex);
        if (callback_registration == NULL){
            break;
        }
        (*callback_registration->callback)(callback_registration->context);
    }
}

static void btstack_run_loop_corefoundation_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    // protect list with mutex
    pthread_mutex_lock(&run_loop_callback_mutex);
    btstack_run_loop_base_add_callback(callback_registration);
    pthread_mutex_unlock(&run_loop_callback_mutex);
    // trigger run loop
    if (run_loop_pipe_fd >= 0){
        const uint8_t x = (uint8_t) 'x';
        (void) write(run_loop_pipe_fd, &x, 1);
    }
}

static void btstack_run_loop_corefoundation_init(void){
    gettimeofday(&init_tv, NULL);
    // calc CFAbsoluteTime for init_tv
    // note: timeval uses unix time: seconds since Jan 1st 1970, CF uses Jan 1st 2001 as reference date
    init_cf = ((double)init_tv.tv_sec) + (((double)init_tv.tv_usec)/1000000.0) - kCFAbsoluteTimeIntervalSince1970; // unix time - since Jan 1st 1970

    // create pipe and register as data source
    int fildes[2];
    int status = pipe(fildes);
    if (status == 0 ) {
        run_loop_pipe_fd  = fildes[1];
        run_loop_pipe_ds.source.fd = fildes[0];
        run_loop_pipe_ds.flags = DATA_SOURCE_CALLBACK_READ;
        run_loop_pipe_ds.process = &btstack_run_loop_corefoundation_pipe_process;
        btstack_run_loop_base_add_data_source(&run_loop_pipe_ds);
        log_info("Pipe in %u, out %u", run_loop_pipe_fd, fildes[0]);
    } else {
        run_loop_pipe_fd  = -1;
        log_error("pipe() failed");
    }
}

static void btstack_run_loop_corefoundation_execute(void) {
    current_runloop = CFRunLoopGetCurrent();
    CFRunLoopRun();
}

static void btstack_run_loop_corefoundation_trigger_exit(void){
    if (current_runloop != NULL){
        CFRunLoopStop(current_runloop);
        current_runloop = NULL;
    }
}
static void btstack_run_loop_corefoundation_dump_timer(void){
    log_error("WARNING: btstack_run_loop_dump_timer not implemented!");
	return;
}

static const btstack_run_loop_t btstack_run_loop_corefoundation = {
    &btstack_run_loop_corefoundation_init,
    &btstack_run_loop_corefoundation_add_data_source,
    &btstack_run_loop_corefoundation_remove_data_source,
    &btstack_run_loop_corefoundation_enable_data_source_callbacks,
    &btstack_run_loop_corefoundation_disable_data_source_callbacks,
    &btstack_run_loop_corefoundation_set_timer,
    &btstack_run_loop_corefoundation_add_timer,
    &btstack_run_loop_corefoundation_remove_timer,
    &btstack_run_loop_corefoundation_execute,
    &btstack_run_loop_corefoundation_dump_timer,
    &btstack_run_loop_corefoundation_get_time_ms,
    NULL, /* poll data sources from irq */
    &btstack_run_loop_corefoundation_execute_on_main_thread,
    &btstack_run_loop_corefoundation_trigger_exit,
};

/**
 * Provide btstack_run_loop_corefoundation instance
 */
const btstack_run_loop_t * btstack_run_loop_corefoundation_get_instance(void){
    return &btstack_run_loop_corefoundation;
}

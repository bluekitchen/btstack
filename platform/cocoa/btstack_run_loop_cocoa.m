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

/*
 *  btstack_run_loop_cocoa.c
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include "btstack_run_loop.h"
#include "btstack_debug.h"

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>

static struct timeval init_tv;
static const btstack_run_loop_t btstack_run_loop_cocoa;

typedef struct  {
    CFSocketRef socket;
    CFRunLoopSourceRef socketRunLoop;
} btstack_cocoa_data_source_helper_t;

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
    btstack_data_source_t * ds = (btstack_ds_t *) info;

    if ((callbackType == kCFSocketReadCallBack) && (ds->flags & DATA_SOURCE_CALLBACK_READ)){
        // printf("btstack_run_loop_cocoa_ds %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) ds, ds->fd, (int) s, (int) ds->item.next);
        ds->process(ds, DATA_SOURCE_CALLBACK_READ);
    }
    if ((callbackType == kCFSocketWriteCallBack) && (ds->flags & DATA_SOURCE_CALLBACK_WRITE)){
        // printf("btstack_run_loop_cocoa_ds %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) ds, ds->fd, (int) s, (int) ds->item.next);
        ds->process(ds, DATA_SOURCE_CALLBACK_WRITE);
    }
}

static CFOptionFlags btstack_run_loop_cocoa_option_flags_for_callback_types(uint16_t callback_types){
    uint16_t option_flags = 0;
    if (callback_types & DATA_SOURCE_CALLBACK_READ){
        option_flags |= kCFSocketReadCallBack;
    }
    if (callback_types & DATA_SOURCE_CALLBACK_WRITE){
        option_flags |= kCFSocketWriteCallBack;
    }
    return option_flags;
}

static void btstack_run_loop_cocoa_add_data_source(btstack_data_source_t *data_source){

	// add fd as CFSocket

    // allocate memory for Core Foundation references	
    btstack_cocoa_data_source_helper_t * references = malloc(sizeof(btstack_cocoa_data_source_helper_t));
    if (!references){
        log_error("btstack_run_loop_cocoa_add_data_source could not allocate btstack_cocoa_data_source_helper_t");
        return;
    }

	// store our data_source in socket context
	CFSocketContext socketContext;
	memset(&socketContext, 0, sizeof(CFSocketContext));
	socketContext.info = data_source;

	// create CFSocket from file descriptor
    uint16_t callback_types = btstack_run_loop_cocoa_option_flags_for_callback_types(data_source->flags);
	CFSocketRef socket = CFSocketCreateWithNative (
										  kCFAllocatorDefault,
										  data_source->fd,
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
    references->socket = socket;
    references->socket_run_loop = socket_run_loop;

    // hack: store btstack_cocoa_data_source_helper_t in "next" of btstack_linked_item_t
    data_source->item.next      = (void *) references;

    // add to run loop
	CFRunLoopAddSource( CFRunLoopGetCurrent(), socket_run_loop, kCFRunLoopCommonModes);
    // printf("btstack_run_loop_cocoa_add_data_source    %x - fd %u - CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->fd, (int) socket, (int) socketRunLoop);
    
}

static void btstack_run_loop_embedded_enable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    btstack_cocoa_data_source_helper_t * references = (btstack_cocoa_data_source_helper_t *) dataSource->item.next;
    uint16_t option_flags = btstack_run_loop_cocoa_option_flags_for_callback_types(callback_types);
    CFSocketEnableCallBacks(references->socket, option_flags);   
}

static void btstack_run_loop_embedded_disable_data_source_callbacks(btstack_data_source_t * ds, uint16_t callback_types){
    btstack_cocoa_data_source_helper_t * references = (btstack_cocoa_data_source_helper_t *) dataSource->item.next;
    uint16_t option_flags = btstack_run_loop_cocoa_option_flags_for_callback_types(callback_types);
    CFSocketDisableCallBacks(references->socket, option_flags);   
}

static int  btstack_run_loop_cocoa_remove_data_source(btstack_data_source_t *dataSource){
    btstack_cocoa_data_source_helper_t * references = (btstack_cocoa_data_source_helper_t *) dataSource->item.next;
    // printf("btstack_run_loop_cocoa_remove_data_source %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->fd, (int) dataSource->item.next, (int) dataSource->item.user_data);
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), references->socketRunLoop, kCFRunLoopCommonModes);
    CFRelease(references->socketRunLoop);

    CFSocketInvalidate(references->socket);
    CFRelease(references->socket);

    free(references);
	return 0;
}

static void btstack_run_loop_cocoa_add_timer(btstack_timer_source_t * ts)
{
    // note: ts uses unix time: seconds since Jan 1st 1970, CF uses Jan 1st 2001 as reference date
    // printf("kCFAbsoluteTimeIntervalSince1970 = %f\n", kCFAbsoluteTimeIntervalSince1970);
    CFAbsoluteTime fireDate = ((double)ts->timeout.tv_sec) + (((double)ts->timeout.tv_usec)/1000000.0) - kCFAbsoluteTimeIntervalSince1970; // unix time - since Jan 1st 1970
    CFRunLoopTimerContext timerContext = {0, ts, NULL, NULL, NULL};
    CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate (kCFAllocatorDefault,fireDate,0,0,0,theCFRunLoopTimerCallBack,&timerContext);
    CFRetain(timerRef);

    // hack: store CFRunLoopTimerRef in next pointer of linked_item
    ts->item.next = (void *)timerRef;
    // printf("btstack_run_loop_cocoa_add_timer %x -> %x now %f, then %f\n", (int) ts, (int) ts->item.next, CFAbsoluteTimeGetCurrent(),fireDate);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef, kCFRunLoopCommonModes);
}

static int btstack_run_loop_cocoa_remove_timer(btstack_timer_source_t * ts){
    // printf("btstack_run_loop_cocoa_remove_timer %x -> %x\n", (int) ts, (int) ts->item.next);
	if (ts->item.next != NULL) {
    	CFRunLoopTimerInvalidate((CFRunLoopTimerRef) ts->item.next);    // also removes timer from run loops + releases it
    	CFRelease((CFRunLoopTimerRef) ts->item.next);
	}
	return 0;
}

// set timer
static void btstack_run_loop_cocoa_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
    gettimeofday(&a->timeout, NULL);
    a->timeout.tv_sec  +=  timeout_in_ms / 1000;
    a->timeout.tv_usec += (timeout_in_ms % 1000) * 1000;
    if (a->timeout.tv_usec  > 1000000) {
        a->timeout.tv_usec -= 1000000;
        a->timeout.tv_sec++;
    }
}

/**
 * @brief Queries the current time in ms since start
 */
static uint32_t btstack_run_loop_cocoa_get_time_ms(void){
    struct timeval current_tv;
    gettimeofday(&current_tv, NULL);
    return (current_tv.tv_sec  - init_tv.tv_sec)  * 1000
         + (current_tv.tv_usec - init_tv.tv_usec) / 1000;
}

static void btstack_run_loop_cocoa_init(void){
    gettimeofday(&init_tv, NULL);
}

static void btstack_run_loop_cocoa_execute(void)
{
    CFRunLoopRun();
}

static void btstack_run_loop_cocoa_dump_timer(void){
    log_error("WARNING: btstack_run_loop_dump_timer not implemented!");
	return;
}

/**
 * Provide btstack_run_loop_embedded instance
 */
const btstack_run_loop_t * btstack_run_loop_cocoa_get_instance(void){
    return &btstack_run_loop_cocoa;
}

static const btstack_run_loop_t btstack_run_loop_cocoa = {
    &btstack_run_loop_cocoa_init,
    &btstack_run_loop_cocoa_add_data_source,
    &btstack_run_loop_cocoa_remove_data_source,
    NULL,
    NULL,
    &btstack_run_loop_cocoa_set_timer,
    &btstack_run_loop_cocoa_add_timer,
    &btstack_run_loop_cocoa_remove_timer,
    &btstack_run_loop_cocoa_execute,
    &btstack_run_loop_cocoa_dump_timer,
    &btstack_run_loop_cocoa_get_time_ms,
};


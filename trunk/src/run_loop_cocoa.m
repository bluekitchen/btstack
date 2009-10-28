/*
 *  run_loop_cocoa.c
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include <btstack/btstack.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>

static void socketDataCallback (
						 CFSocketRef s,
						 CFSocketCallBackType callbackType,
						 CFDataRef address,
						 const void *data,
						 void *info)
{
	data_source_t *ds = (data_source_t *) info;
	ds->process(ds);
}

void cocoa_add_data_source(data_source_t *dataSource){

	// add fd as CF "socket"
	
	// store our dataSource in socket context
	CFSocketContext socketContext;
	bzero(&socketContext, sizeof(CFSocketContext));
	socketContext.info = dataSource;

	// create CFSocket from file descriptor
	CFSocketRef socket = CFSocketCreateWithNative (
										  kCFAllocatorDefault,
										  dataSource->fd,
										  kCFSocketReadCallBack,
										  socketDataCallback,
										  &socketContext
    );
    
	// create run loop source
	CFRunLoopSourceRef socketRunLoop = CFSocketCreateRunLoopSource ( kCFAllocatorDefault, socket, 0);
    
    // hack: store CFSocketRef in next pointer of linked_item
    dataSource->item.next = (void *) socketRunLoop;

    // add to run loop
	CFRunLoopAddSource( CFRunLoopGetCurrent(), socketRunLoop, kCFRunLoopDefaultMode);
}

int  cocoa_remove_data_source(data_source_t *dataSource){
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), (CFRunLoopSourceRef) dataSource->item.next, kCFRunLoopCommonModes);
	return 0;
}

void  cocoa_add_timer(timer_t * ts){
	// not needed yet
   fprintf(stderr, "WARNING: run_loop_add_timer not implemented yet!");
    // warning never the less
}

int  cocoa_remove_timer(timer_t * ts){
	// not needed yet
    fprintf(stderr, "WARNING: run_loop_remove_timer not implemented yet!");
    // warning never the less
	return 0;
}

void cocoa_init(){
}

void cocoa_execute(){
	// not needed yet
    fprintf(stderr, "WARNING: execute not available for RUN_LOOP_COCOA!");
    // warning never the less
	exit(10);
}

void cocoa_dump_timer(){
	// not needed yet
    fprintf(stderr, "WARNING: run_loop_dump_timer not implemented yet!");
    // warning never the less
	return;
}

const run_loop_t run_loop_cocoa = {
    &cocoa_init,
    &cocoa_add_data_source,
    &cocoa_remove_data_source,
    &cocoa_add_timer,
    &cocoa_remove_timer,
    &cocoa_execute,
    &cocoa_dump_timer
};


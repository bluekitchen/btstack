/*
 *  run_loop_cocoa.c
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include <btstack/btstack.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

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

void run_loop_add_data_source(data_source_t *dataSource){

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
	
	// create run loop source and add to run loop
	CFRunLoopSourceRef socketRunLoop = CFSocketCreateRunLoopSource ( kCFAllocatorDefault, socket, 0);
	CFRunLoopAddSource( CFRunLoopGetCurrent(), socketRunLoop, kCFRunLoopDefaultMode);
}

int  run_loop_remove_data_source(data_source_t *dataSource){
	// not needed yet
	return 0;
}


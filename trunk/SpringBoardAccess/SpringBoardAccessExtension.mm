/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 */

//
//  SpringBoardAccess.m
//
//  Created by Matthias Ringwald on 9/15/09.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <UIKit/UIKit.h>

#import "../3rdparty/substrate.h"

#include "SpringBoardAccess.h"

class SpringBoard;

@interface UIApplication (privateStatusBarIconAPI)
- (void)addStatusBarImageNamed:(id)fp8;
- (void)removeStatusBarImageNamed:(id)fp8;
@end

#define HOOK(class, name, type, args...) \
static type (*_ ## class ## $ ## name)(class *self, SEL sel, ## args); \
static type $ ## class ## $ ## name(class *self, SEL sel, ## args)

#define CALL_ORIG(class, name, args...) \
_ ## class ## $ ## name(self, sel, ## args)


CFDataRef myCallBack(CFMessagePortRef local, SInt32 msgid, CFDataRef cfData, void *info) {
	UIApplication *theApp = [UIApplication sharedApplication];

	const char *data = (const char *) CFDataGetBytePtr(cfData);
	UInt16 dataLen = CFDataGetLength(cfData);
	
	if (dataLen > 0 && data) {
		NSString * name = [NSString stringWithCString:data encoding:NSASCIIStringEncoding];
		switch (msgid){
			case SBAC_addStatusBarImage:
				[theApp addStatusBarImageNamed:name];
				break;
			case SBAC_removeStatusBarImage:
				[theApp removeStatusBarImageNamed:name];
				break;
			default:
				NSLog(@"Unknown command %u, len %u", data[0], dataLen); 
		}
	}
	return NULL;  // as stated in header, both data and returnData will be released for us after callback returns
}

//______________________________________________________________________________

HOOK(SpringBoard, applicationDidFinishLaunching$, void, id app) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    CALL_ORIG(SpringBoard, applicationDidFinishLaunching$, app);
	
	CFMessagePortRef local = CFMessagePortCreateLocal(NULL, CFSTR(SBA_MessagePortName), myCallBack, NULL, false);
	CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(NULL, local, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
	
    [pool release]; 
}
//______________________________________________________________________________

extern "C" void SpringBoardAccessInitialize(){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	NSLog(@"SpringBoardAccessInitialize called for SpringBoard!");
	
    // Setup hooks
    Class $SpringBoard(objc_getClass("SpringBoard"));
    _SpringBoard$applicationDidFinishLaunching$ =
	MSHookMessage($SpringBoard, @selector(applicationDidFinishLaunching:), &$SpringBoard$applicationDidFinishLaunching$);

    [pool release]; 
}

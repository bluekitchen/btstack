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


// minimal BluetoothManager swiped from BigBoss SBSettings Toggle
#import <Foundation/Foundation.h>
@class UIDevice;
@interface BluetoothManager : NSObject
+ (BluetoothManager *) sharedInstance;
- (void) setPowered:(BOOL)powered;
- (void) setEnabled:(BOOL)enabled;
- (BOOL) enabled;
@end
#define kAppBTServer CFSTR("com.apple.BTServer")
#define kKeyBTPowered CFSTR("defaultPoweredState")
#define kAppNetwork CFSTR("com.apple.preferences.network")
#define kKeyBTNetwork CFSTR("bluetooth-network")

int iphone_system_bt_enabled(){
	if ([[BluetoothManager sharedInstance] enabled]) {
        return 1;
    } else {
        return 0;
    }
}

void iphone_system_bt_set_enabled(int enabled)
{
	/* Get a copy of the global bluetooth server */
	BluetoothManager *bm = [BluetoothManager sharedInstance];
    if ( [bm enabled] &&  enabled) return;
    if (![bm enabled] && !enabled) return;
    
	if (enabled) {
		/* Store into preferences that bluetooth should start on system boot */
		CFPreferencesSetAppValue(kKeyBTNetwork, kCFBooleanTrue, kAppNetwork);
        
		/* Turn bluetooth on */
		[bm setPowered:YES];
	} else {
		/* Store into preferences taht bluetooth should not start on system boot */
		CFPreferencesSetAppValue(kKeyBTNetwork, kCFBooleanFalse, kAppNetwork);
        
		/* Turn bluetooth off */
		[bm setEnabled:NO];
	}
	/* Synchronize to preferences, e.g. write to disk, bluetooth settings */
	CFPreferencesAppSynchronize(kAppNetwork);
}

CFDataRef myCallBack(CFMessagePortRef local, SInt32 msgid, CFDataRef cfData, void *info) {
	UIApplication *theApp = [UIApplication sharedApplication];

	const char *data = (const char *) CFDataGetBytePtr(cfData);
	UInt16 dataLen = CFDataGetLength(cfData);
	CFDataRef returnData = NULL;
	if (!data) return NULL;

	switch (msgid){
		case SBAC_addStatusBarImage: {
			if (!dataLen) return NULL;
			NSString * name = [NSString stringWithCString:data encoding:NSASCIIStringEncoding];
			[theApp addStatusBarImageNamed:name];
			break;
		}
		case SBAC_removeStatusBarImage: {
			if (!dataLen) return NULL;
			NSString * name = [NSString stringWithCString:data encoding:NSASCIIStringEncoding];
			[theApp removeStatusBarImageNamed:name];
			break;
		}
			
		case SBAC_getBluetoothEnabled: {
			// use single byte to avoid endianess issues for the boolean
			uint8_t enabled = iphone_system_bt_enabled();
			returnData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *) &enabled, 1);
			break;
    }

		case SBAC_setBluetoothEnabled: {
			if (dataLen < 1) return NULL;
			iphone_system_bt_set_enabled( data[0]);
			break;
    }

		default:
			NSLog(@"Unknown command %u, len %u", data[0], dataLen); 
	}
	// as stated in header, both data and returnData will be released for us after callback returns
	return returnData;
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
	
	// NSLog(@"SpringBoardAccessInitialize called for SpringBoard!");
	
    // Setup hooks
    Class $SpringBoard(objc_getClass("SpringBoard"));
    _SpringBoard$applicationDidFinishLaunching$ =
	MSHookMessage($SpringBoard, @selector(applicationDidFinishLaunching:), &$SpringBoard$applicationDidFinishLaunching$);

    [pool release]; 
}

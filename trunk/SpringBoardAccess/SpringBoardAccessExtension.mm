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
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	UIApplication *theApp = [UIApplication sharedApplication];

	const char *data = (const char *) CFDataGetBytePtr(cfData);
	UInt16 dataLen = CFDataGetLength(cfData);
	
	if (dataLen > 1 && data) {
		NSString * name = [[NSString stringWithCString:&data[1] encoding:NSASCIIStringEncoding] autorelease];
		switch (data[0]){
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
	[pool release]; 
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

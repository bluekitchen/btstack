//
//  SpringBoardAccess.m
//
//  Created by Matthias Ringwald on 9/15/09.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#import "../3rdparty/substrate.h"
#import "../3rdparty/SpringBoard.h"

#define HOOK(class, name, type, args...) \
static type (*_ ## class ## $ ## name)(class *self, SEL sel, ## args); \
static type $ ## class ## $ ## name(class *self, SEL sel, ## args)

#define CALL_ORIG(class, name, args...) \
_ ## class ## $ ## name(self, sel, ## args)


CFDataRef myCallBack(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	
	
	char *message = "Thanks for calling!";
	CFDataRef returnData = CFDataCreate(NULL, (const UInt8 *) message, strlen(message)+1);
	NSLog(@"here is our received data: %s\n", CFDataGetBytePtr(data));
	
	[pool release]; 

	return returnData;  // as stated in header, both data and returnData will be released for us after callback returns
}

//______________________________________________________________________________

HOOK(SpringBoard, applicationDidFinishLaunching$, void, id app) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    CALL_ORIG(SpringBoard, applicationDidFinishLaunching$, app);
	
	CFMessagePortRef local = CFMessagePortCreateLocal(NULL, CFSTR("SpringBoardAccess"), myCallBack, NULL, false);
	CFRunLoopSourceRef source = CFMessagePortCreateRunLoopSource(NULL, local, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
	
    [pool release]; 
}
//______________________________________________________________________________

extern "C" void SpringBoardAccessInitialize(){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
    // NOTE: This library should only be loaded for SpringBoard
    NSString *identifier = [[NSBundle mainBundle] bundleIdentifier];
    if (![identifier isEqualToString:@"com.apple.springboard"])
        return;

	NSLog(@"SpringBoardAccessInitialize called for SpringBoard!");
	
    // Setup hooks
    Class $SpringBoard(objc_getClass("SpringBoard"));
    _SpringBoard$applicationDidFinishLaunching$ =
	MSHookMessage($SpringBoard, @selector(applicationDidFinishLaunching:), &$SpringBoard$applicationDidFinishLaunching$);

    [pool release]; 
}

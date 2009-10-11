//
//  BTstackCocoaAppDelegate.h
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

#import "BTInquiryViewController.h"

@interface BTstackCocoaAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
	BTInquiryViewController *inqView;
	bool inqActive;
	NSMutableArray *devices;
	uint8_t remoteNameIndex;
}

- (void) handlePacketWithType:(uint8_t) packetType data:(uint8_t*)data len:(uint16_t)len;
- (void) startInquiry;

@property (nonatomic, retain) UIWindow *window;

@end


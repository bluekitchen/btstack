//
//  BTstackCocoaAppDelegate.h
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

#import "BTInquiryViewController.h"

@interface BTstackCocoaAppDelegate : NSObject <UIApplicationDelegate,BTInquiryDelegate> {
    UIWindow *window;
	BTInquiryViewController *inqView;
}
@property (nonatomic, retain) UIWindow *window;

@end


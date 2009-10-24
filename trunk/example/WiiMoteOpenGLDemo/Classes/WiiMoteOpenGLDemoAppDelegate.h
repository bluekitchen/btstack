//
//  WiiMoteOpenGLDemoAppDelegate.h
//
//  Created by Matthias Ringwald on 8/2/09.
//

#import <UIKit/UIKit.h>
#import "BTInquiryViewController.h"

@class EAGLView;

@interface WiiMoteOpenGLDemoAppDelegate : NSObject <UIApplicationDelegate, BTInquiryDelegate> {
    UIWindow *window;
	UIViewController *glViewControl;
	BTInquiryViewController *inqViewControl;
	UINavigationController *navControl;
    EAGLView *glView;
	UILabel  *status;
}

- (void)startDemo;

@property (nonatomic, retain) UIWindow *window;
@property (nonatomic, retain) UINavigationController *navControl;
@property (nonatomic, retain) UIViewController *glViewControl;
@property (nonatomic, retain) BTInquiryViewController *inqViewControl;
@property (nonatomic, retain) EAGLView *glView;

@end


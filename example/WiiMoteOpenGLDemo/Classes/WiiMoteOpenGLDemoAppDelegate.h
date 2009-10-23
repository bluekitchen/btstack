//
//  WiiMoteOpenGLDemoAppDelegate.h
//
//  Created by Matthias Ringwald on 8/2/09.
//

#import <UIKit/UIKit.h>

@class EAGLView;

@interface WiiMoteOpenGLDemoAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
    EAGLView *glView;
	UILabel  *status;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet EAGLView *glView;
@property (nonatomic, retain) IBOutlet UILabel  *status;

@end


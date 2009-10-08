//
//  BTstackCocoaAppDelegate.h
//  BTstackCocoa
//
//  Created by Matthias Ringwald on 10/8/09.
//  Copyright Dybuster AG 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface BTstackCocoaAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end


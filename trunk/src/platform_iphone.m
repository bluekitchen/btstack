//
//  platform_iphone.m
//
//  Created by Matthias Ringwald on 8/15/09.
//

#import "platform_iphone.h"

#ifdef USE_SPRINGBOARD

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#endif

@interface UIApplication (privateStatusBarIconAPI)
- (void)addStatusBarImageNamed:(id)fp8 removeOnAbnormalExit:(BOOL)fp12;
- (void)addStatusBarImageNamed:(id)fp8;
- (void)removeStatusBarImageNamed:(id)fp8;
// iPhoneOS >= 2.2
- (void)addStatusBarImageNamed:(id)fp8 removeOnExit:(BOOL)fp12;
@end

// update SpringBoard icon
void platform_iphone_status_handler(BLUETOOTH_STATE state){
    printf("iphone state %u\n", state);
	UIApplication *theApp = [UIApplication sharedApplication];
    printf("the app %u\n", (int) theApp);
    [theApp addStatusBarImageNamed:@"BluetoothActive"];
    
    switch (state) {
        case BLUETOOTH_OFF:
            break;
        case BLUETOOTH_ON:
            break;
        case BLUETOOTH_ACTIVE:
            break;
        default:
            break;
    }
}

#endif
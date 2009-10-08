//
//  BTstackCocoaAppDelegate.m
//  BTstackCocoa
//
//  Created by Matthias Ringwald on 10/8/09.
//  Copyright Dybuster AG 2009. All rights reserved.
//

#import "BTstackCocoaAppDelegate.h"
#import "BTInquiryViewController.h"
#import "BTDevice.h"

@implementation BTstackCocoaAppDelegate

@synthesize window;


- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    // Override point for customization after application launch
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	
	// create BTInquiryView
	BTInquiryViewController *inqView = [[BTInquiryViewController alloc] init];
	[inqView setBluetoothState:kBluetoothStateInitializing];
	NSMutableArray *devices = [[NSMutableArray alloc] init];
	[inqView setDevices:devices];
	
	[window addSubview:[inqView view]];

    // show
	[window makeKeyAndVisible];
}


- (void)dealloc {
    [window release];
    [super dealloc];
}


@end

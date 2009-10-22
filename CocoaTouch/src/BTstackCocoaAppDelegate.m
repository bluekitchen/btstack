//
//  BTstackCocoaAppDelegate.m
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import "BTstackCocoaAppDelegate.h"
#import "BTDevice.h"


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <btstack/btstack.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
}

@implementation BTstackCocoaAppDelegate

@synthesize window;

- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    // Override point for customization after application launch
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	
	// create BTInquiryView
	inqView = [[BTInquiryViewController alloc] init];
	[inqView setDelegate:self];
	
	[window addSubview:[inqView view]];

    // show
	[window makeKeyAndVisible];
	
	// start Bluetooth
	run_loop_init(RUN_LOOP_COCOA);

	int res = bt_open();
	if (res){
		UIAlertView* alertView = [[UIAlertView alloc] init];
		alertView.title = @"Bluetooth not accessible!";
		alertView.message = @"Connection to BTstack failed!\n"
		"Please make sure that BTstack is installed correctly.";
		NSLog(@"Alert: %@ - %@", alertView.title, alertView.message);
		[alertView addButtonWithTitle:@"Dismiss"];
		[alertView show];
	} else {
		bt_register_packet_handler(packet_handler);
		[inqView startInquiry];
	}
}

- (void)dealloc {
    [window release];
    [super dealloc];
}

/** BTInquiryDelegate */
-(void) deviceChoosen:(BTInquiryViewController *) inqView device:(BTDevice*) device{
	NSLog(@"deviceChoosen %@", [device toString]);
}

@end

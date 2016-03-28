/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

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

#include "btstack_client.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_corefoundation.h"
#include "hci_cmd.h"

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
	btstack_run_loop_init(btstack_run_loop_corefoundation_get_instance());

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

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

#import "TestBTstackManager.h"
#import <BTstack/BTDevice.h>


@implementation TestBTstackManager

-(void) activatedBTstackManager:(BTstackManager*) manager {
	NSLog(@"activated!");
	[bt startDiscovery];
}
-(void) btstackManager:(BTstackManager*)manager activationFailed:(BTstackError)error {
	NSLog(@"activationFailed error 0x%02x!", error);
};
-(void) discoveryInquiryBTstackManager:(BTstackManager*) manager {
	NSLog(@"discoveryInquiry!");
}
-(void) discoveryStoppedBTstackManager:(BTstackManager*) manager {
	NSLog(@"discoveryStopped!");
}
-(void) btstackManager:(BTstackManager*)manager discoveryQueryRemoteName:(int)deviceIndex {
	NSLog(@"discoveryQueryRemoteName %u/%u!", deviceIndex+1, [bt numberOfDevicesFound]);
}
-(void) btstackManager:(BTstackManager*)manager deviceInfo:(BTDevice*)device {
	NSLog(@"Device Info: addr %@ name %@ COD 0x%06x", [device addressString], [device name], [device classOfDevice] ); 
}
-(BOOL) discoveryView:(BTDiscoveryViewController*)discoveryView willSelectDeviceAtIndex:(int)deviceIndex {
	if (selectedDevice) return NO;
	selectedDevice = [bt deviceAtIndex:deviceIndex];
	BTDevice *device = selectedDevice;
	NSLog(@"Device selected: addr %@ name %@ COD 0x%06x", [device addressString], [device name], [device classOfDevice] ); 
	[bt stopDiscovery];
	return NO;
}
-(void) statusCellSelectedDiscoveryView:(BTDiscoveryViewController*)discoveryView {
	if (![bt isDiscoveryActive]) {
		selectedDevice = nil;
		[bt startDiscovery];
	}
	NSLog(@"statusCellSelected!");
}
- (void)applicationDidFinishLaunching:(UIApplication *)application {	
	
	selectedDevice = nil;
	
	// create discovery controller
	discoveryView = [[BTDiscoveryViewController alloc] init];
	[discoveryView setDelegate:self];
	UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:discoveryView];
	UIWindow *window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	[window addSubview:nav.view];
	[window makeKeyAndVisible];
	
	// BTstack
	bt = [BTstackManager sharedInstance];
	[bt setDelegate:self];
	[bt addListener:self];
	[bt addListener:discoveryView];
	
	BTstackError err = [bt activate];
	if (err) NSLog(@"activate err 0x%02x!", err);
}
@end

int main(int argc, char *argv[])
{
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	int retVal = UIApplicationMain(argc, argv, nil, @"TestBTstackManager");
	[pool release];
	return retVal;
}
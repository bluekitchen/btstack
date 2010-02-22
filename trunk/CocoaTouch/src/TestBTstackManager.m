//
//  TestBTstackManager.m
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import "TestBTstackManager.h"
#import <BTstack/BTDevice.h>


@implementation TestBTstackManager

-(void) activated{
	NSLog(@"activated!");
	[bt startDiscovery];
}
-(void) activationFailed:(BTstackError)error{
	NSLog(@"activationFailed error 0x%02x!", error);
};
-(void) discoveryInquiry{
	NSLog(@"discoveryInquiry!");
	[bt storeDeviceInfo];
}
-(void) discoveryQueryRemoteName:(int)deviceIndex{
	NSLog(@"discoveryQueryRemoteName %u/%u!", deviceIndex+1, [bt numberOfDevicesFound]);
}
-(void) deviceInfo:(BTDevice*)device {
	NSLog(@"Device Info: addr %@ name %@ COD 0x%06x", [device addressString], [device name], [device classOfDevice] ); 
}
- (BOOL)willSelectDeviceAtIndex:(int)deviceIndex {
	BTDevice *device = [bt deviceAtIndex:deviceIndex];
	NSLog(@"Device selected: addr %@ name %@ COD 0x%06x", [device addressString], [device name], [device classOfDevice] ); 
	return NO;
}
-(void)statusCellSelected{
	NSLog(@"statusCellSelected!");
}
- (void)applicationDidFinishLaunching:(UIApplication *)application {	
	
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
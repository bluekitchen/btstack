//
//  TestBTstackManager.m
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import "TestBTstackManager.h"
#import <BTstack/BTDevice.h>

@implementation TestBTstackManager

-(void) doTest{
	bt = [BTstackManager sharedInstance];
	[bt setDelegate:self];
	BTstackError err = [bt activate];
	if (err) NSLog(@"activate err 0x%02x!", err);
}

-(void) activated{
	NSLog(@"activated!");
	[bt startDiscovery];
}

-(void) activationFailed:(BTstackError)error{
	NSLog(@"activationFailed error 0x%02x!", error);
};

-(void) discoveryInquiry{
	NSLog(@"discoveryInquiry!");
}
-(void) discoveryQueryRemoteName:(int)deviceIndex{
	NSLog(@"discoveryQueryRemoteName %u/%u!", deviceIndex+1, [bt numberOfDevicesFound]);
}
-(void) deviceInfo:(BTDevice*)device {
	NSLog(@"Device Info: addr %@ name %@ COD 0x%06x", [device addressString], [device name], [device classOfDevice] ); 
}

@end

int main(int argc, char *argv[])
{
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	TestBTstackManager * test = [[TestBTstackManager alloc] init];
	[test doTest];
	CFRunLoopRun();
	[test release];

	[pool release];
	return 0;
}
//
//  TestBTstackManager.m
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import "TestBTstackManager.h"

@implementation TestBTstackManager
-(id) init {
	return self;
}
-(void) test{
	bt = [BTstackManager sharedInstance];
	[bt setDelegate:self];
	BTstackError err = [bt activate];
	if (err) NSLog(@"activate err 0x%02x!", err);
}
-(void) activated{
	NSLog(@"activated!");
}
-(void) activationFailed:(BTstackError)error{
	NSLog(@"activationFailed error 0x%02x!", error);
};
@end

int main(int argc, char *argv[])
{
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	TestBTstackManager * test = [[TestBTstackManager alloc] init];
	[test test];
	CFRunLoopRun();
	[test release];

	[pool release];
	return 0;
}
//
//  WiiMoteOpenGLDemoAppDelegate.m
//
//  Created by Matthias Ringwald on 8/2/09.
//

#import "WiiMoteOpenGLDemoAppDelegate.h"
#import "EAGLView.h"
#include "../BTWiiMote.h"

// #define USE_BLUETOOTH

WiiMoteOpenGLDemoAppDelegate * theMainApp;

@implementation WiiMoteOpenGLDemoAppDelegate

@synthesize window;
@synthesize glView;
@synthesize status;

static void bt_state_cb(char *text){
	NSLog(@"BT state: %s", text);
	NSString *stringFromUTFString = [[NSString alloc] initWithUTF8String:text];
	[[theMainApp status] setText:stringFromUTFString];
}

static void bt_data_cb(uint8_t x, uint8_t y, uint8_t z){
	// NSLog(@"BT data: %u %u %u", x , y ,z);
	// [[theMainApp status] setText:[NSString stringWithFormat:@"X:%03u Y:%03u Z:%03u", x, y, z]];
	float ax = x - 128;
	float ay = y - 128;
	float az = z - 128;
	int roll  = atan2(ax, sqrt(ay*ay+az*az)) * 180 / M_PI; 
	int pitch = atan2(ay, sqrt(ax*ax+az*az)) * 180 / M_PI;

	
#if 1
	// moving average of size SIZE
#define SIZE  5
	static int pos = 0;
	static int historyRoll[SIZE];
	static int historyPitch[SIZE];
	
	historyRoll[pos] = roll;
	historyPitch[pos] = pitch;
	pos++;
	if (pos==SIZE) pos = 0;
	
	pitch = roll = 0;
	int i;
	for (i=0;i<SIZE;i++){
		roll  += historyRoll[i];
		pitch += historyPitch[i];
	}
	roll = roll / SIZE;
	pitch = pitch / SIZE;
#endif	
	// pitch += 90;
	[[theMainApp glView] setRotationX:-pitch Y:roll Z:0];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
	NSLog(@"Started");
	// create window
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	[window setBackgroundColor:[UIColor blueColor]];
	
	// add view to window
	glView = [[EAGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	[window addSubview:glView];

	//  status = [[UILabel alloc] init];
	// [status setText:@"This is my text"];
	theMainApp = self;
	
#ifdef USE_BLUETOOTH
	set_bt_state_cb(bt_state_cb);
	set_data_cb(bt_data_cb);
	start_bt();
#endif
	
	glView.animationInterval = 1.0 / 60.0;
	[glView startAnimation];

	[window makeKeyAndVisible];	 
}


- (void)applicationWillResignActive:(UIApplication *)application {
	glView.animationInterval = 1.0 / 5.0;
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
	glView.animationInterval = 1.0 / 60.0;
}


- (void)dealloc {
	[window release];
	[glView release];
	[super dealloc];
}

@end

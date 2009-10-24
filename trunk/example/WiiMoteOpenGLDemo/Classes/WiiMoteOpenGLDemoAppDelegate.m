//
//  WiiMoteOpenGLDemoAppDelegate.m
//
//  Created by Matthias Ringwald on 8/2/09.
//

#import "WiiMoteOpenGLDemoAppDelegate.h"
#import "BTDevice.h"
#import "EAGLView.h"
#import "EAGLViewController.h"
#import "BTInquiryViewController.h"

#import <btstack/btstack.h>
#import <btstack/run_loop.h>
#import <btstack/hci_cmds.h>

BTDevice *device;

WiiMoteOpenGLDemoAppDelegate * theMainApp;

@implementation WiiMoteOpenGLDemoAppDelegate

@synthesize window;
@synthesize glView;
@synthesize navControl;
@synthesize glViewControl;
@synthesize inqViewControl;

#define SIZE  5
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
	[[theMainApp glView] setRotationX:-pitch Y:roll Z:0];
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	bd_addr_t event_addr;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			if (packet[0] == 0xa1 && packet[1] == 0x31){
				bt_data_cb(packet[4], packet[5], packet[6]);
			}
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]){
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &packet[3]);
					uint16_t psm = READ_BT_16(packet, 11); 
					uint16_t source_cid = READ_BT_16(packet, 13); 
					printf("Channel successfully opened: ");
					print_bd_addr(event_addr);
					printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
						   READ_BT_16(packet, 9), psm, source_cid,  READ_BT_16(packet, 15));
					if (psm == 0x13) {
						// interupt channel openedn succesfully, now open control channel, too.
						bt_send_cmd(&l2cap_create_channel, event_addr, 0x11);
					} else {

						// request acceleration data.. probably has to be sent to control channel 0x11 instead of 0x13
						uint8_t setMode31[] = { 0x52, 0x12, 0x00, 0x31 };
						bt_send_l2cap( source_cid, setMode31, sizeof(setMode31));
						uint8_t setLEDs[] = { 0x52, 0x11, 0x10 };
						bt_send_l2cap( source_cid, setLEDs, sizeof(setLEDs));

						// start demo
						[theMainApp startDemo];
					}
					break;
					
				default:
					break;
			}
			break;
			
		default:
			break;
	}
}
- (void)startDemo {
	NSLog(@"startDemo");
	
	// stop connection icon
	[device setConnectionState:kBluetoothConnectionConnected];
	
	// push glViewControl on stack
	[[[theMainApp inqViewControl] tableView] reloadData];
	[navControl pushViewController:glViewControl animated:YES];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
	NSLog(@"Started");

#ifdef USE_BLUETOOTH
	run_loop_init(RUN_LOOP_COCOA);
	bt_open();
	bt_register_packet_handler(packet_handler);
#endif

	// create window
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	[window setBackgroundColor:[UIColor blueColor]];
	
	// create view controller
	glViewControl = [[EAGLViewController alloc] init];
	
	// create inq controller
	inqViewControl = [[BTInquiryViewController alloc] init];
	[inqViewControl setTitle:@"BTstack Device Selector"];
	[inqViewControl setAllowSelection:NO];
	
	// create nav view controller
	navControl = [[UINavigationController alloc] initWithRootViewController:inqViewControl];
	
	// add view to window
	[window addSubview:[navControl view]];

	theMainApp = self;

#ifdef USE_BLUETOOTH
	[inqViewControl setDelegate:self];
	[inqViewControl startInquiry];
#endif
	
	glView = (EAGLView *) [glViewControl view];
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

-(void) deviceChoosen:(BTInquiryViewController *) inqView device:(BTDevice*) selectedDevice{
	NSLog(@"deviceChoosen %@", [device toString]);
}

- (void) deviceDetected:(BTInquiryViewController *) inqView device:(BTDevice*) selectedDevice {
	NSLog(@"deviceDetected %@", [device toString]);
	if ([selectedDevice name] && [[selectedDevice name] caseInsensitiveCompare:@"Nintendo RVL-CNT-01"] == NSOrderedSame){
		NSLog(@"WiiMote found with address %@", [BTDevice stringForAddress:[selectedDevice address]]);
		device = selectedDevice;
		[inqViewControl stopInquiry];

		// connect to device
		[device setConnectionState:kBluetoothConnectionConnecting];
		[[[theMainApp inqViewControl] tableView] reloadData];
		bt_send_cmd(&l2cap_create_channel, [device address], 0x13);
	}
}

- (void)dealloc {
	[window release];
	[glView release];
	[super dealloc];
}

@end

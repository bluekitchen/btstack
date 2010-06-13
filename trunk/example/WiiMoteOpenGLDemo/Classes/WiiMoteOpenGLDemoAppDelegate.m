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

// quaternion rotation library
float norm(float *vector, int dim);
void normalizeVector(float *vector, int dim);
void getRotationMatrixFromVectors(float vin[3], float vout[3], float matrix[3][3]);

BTDevice *device;
uint16_t wiiMoteConHandle = 0;
WiiMoteOpenGLDemoAppDelegate * theMainApp;

@implementation WiiMoteOpenGLDemoAppDelegate

@synthesize window;
@synthesize glView;
@synthesize navControl;
@synthesize glViewControl;
@synthesize inqViewControl;

#define SIZE  5
int counter;

// define the rest position 
static float restPosition[3] = {0,0,1};
#define histSize 5
int histX[histSize];
int histY[histSize];
int histZ[histSize];

static float addToHistory(int history[histSize], int value){
	int i;
	float sum = 0;
	for (i=0; i<histSize-1;i++){
		history[i] = history[i+1];
		sum += history[i];
	}
	history[histSize-1] = value;
	return sum/histSize;
}

static void bt_data_cb(uint8_t x, uint8_t y, uint8_t z){
	
	// NSLog(@"BT data: %u %u %u", x , y ,z);
	// [[theMainApp status] setText:[NSString stringWithFormat:@"X:%03u Y:%03u Z:%03u", x, y, z]];
	float accData[3];
	accData[0] = addToHistory( histX, 1 * (x - 128));
	accData[1] = addToHistory( histY, 1 * (y - 128));
	accData[2] = addToHistory( histZ, 1 * (z - 128));

	float rotationMatrix[3][3];
	getRotationMatrixFromVectors(restPosition, accData, rotationMatrix);
	
	[[theMainApp glView] setRotationMatrix:rotationMatrix];
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
					if (packet[2] == 0) {
						// inform about new l2cap connection
						bt_flip_addr(event_addr, &packet[3]);
						uint16_t psm = READ_BT_16(packet, 11); 
						uint16_t source_cid = READ_BT_16(packet, 13); 
						wiiMoteConHandle = READ_BT_16(packet, 9);
						NSLog(@"Channel successfully opened: handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x",
							   wiiMoteConHandle, psm, source_cid,  READ_BT_16(packet, 15));
						if (psm == 0x13) {
							// interupt channel openedn succesfully, now open control channel, too.
							bt_send_cmd(&l2cap_create_channel, event_addr, 0x11);
						} else {
							// request acceleration data.. 
							uint8_t setMode31[] = { 0x52, 0x12, 0x00, 0x31 };
							bt_send_l2cap( source_cid, setMode31, sizeof(setMode31));
							uint8_t setLEDs[] = { 0x52, 0x11, 0x10 };
							bt_send_l2cap( source_cid, setLEDs, sizeof(setLEDs));

							// start demo
							[theMainApp startDemo];
						}
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
	glView = (EAGLView *) [glViewControl view];
	glView.animationInterval = 1.0 / 60.0;
	[glView startAnimation];
	
	[[[theMainApp inqViewControl] tableView] reloadData];
	[navControl pushViewController:glViewControl animated:YES];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
	NSLog(@"Started");
	bool btOK = false;
	
#ifdef USE_BLUETOOTH
	run_loop_init(RUN_LOOP_COCOA);
	if ( bt_open() ){
		UIAlertView* alertView = [[UIAlertView alloc] init];
		alertView.title = @"Bluetooth not accessible!";
		alertView.message = @"Connection to BTstack failed!\n"
		"Please make sure that BTstack is installed correctly.";
		NSLog(@"Alert: %@ - %@", alertView.title, alertView.message);
		[alertView addButtonWithTitle:@"Dismiss"];
		[alertView show];
	} else {
		bt_register_packet_handler(packet_handler);
		btOK = true;
	}
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

	if (btOK) {
		UITextView *footer = [[UITextView alloc] initWithFrame:CGRectMake(10,00,300,85)];
		footer.text = @"Please make your WiiMote discoverable by pressing the 1+2 buttons at the same time.";
		footer.textColor = [UIColor blackColor];
		footer.font = [UIFont fontWithName:@"Arial" size:18];
		// footer.textAlignment = UITextAlignmentCenter;
		footer.backgroundColor = [UIColor whiteColor];
		footer.autoresizingMask = UIViewAutoresizingFlexibleWidth;
		footer.editable = false;
		[[inqViewControl tableView] setTableFooterView:footer];
	}
	
	// create nav view controller
	navControl = [[UINavigationController alloc] initWithRootViewController:inqViewControl];
	
	// add view to window
	[window addSubview:[navControl view]];

	theMainApp = self;

	if (btOK) {
		[inqViewControl setDelegate:self];
		[inqViewControl startInquiry];
	}
	[window makeKeyAndVisible];	 
}


- (void)applicationWillResignActive:(UIApplication *)application {
	// glView.animationInterval = 1.0 / 5.0;

	if (wiiMoteConHandle) {
		bt_send_cmd(&hci_disconnect, wiiMoteConHandle, 0x13); // remote closed connection             
		wiiMoteConHandle = 0;
	}
	// bt_send_cmd(&btstack_set_power_mode, HCI_POWER_OFF );
	bt_close();

	UIAlertView* alertView = [[UIAlertView alloc] init];
	alertView.title = @"Power Management ?";
	alertView.message = @"Don't know yet, what to do when\n"
	"phone gets locked.\n"
	"Turning Bluetooth off for now. :)\n";
	NSLog(@"Alert: %@ - %@", alertView.title, alertView.message);
	// [alertView addButtonWithTitle:@"Dismiss"];
	[alertView show];
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
		[inqViewControl showConnecting:device];

		// connect to device
		[device setConnectionState:kBluetoothConnectionConnecting];
		[[[theMainApp inqViewControl] tableView] reloadData];
		bt_send_cmd(&l2cap_create_channel, [device address], 0x13);
	}
}

- (void) inquiryStopped{
	NSLog(@"inquiryStopped");
}

- (void) disconnectDevice:(BTInquiryViewController *) inqView device:(BTDevice*) selectedDevice {
}

- (void)applicationWillTerminate:(UIApplication *)application {
	// disconnect
	if (wiiMoteConHandle) {
		bt_send_cmd(&hci_disconnect, wiiMoteConHandle, 0x13); // remote closed connection             
	}
	// bt_send_cmd(&btstack_set_power_mode, HCI_POWER_OFF );

	bt_close();
}

- (void)dealloc {
	
	[window release];
	[glView release];
	[super dealloc];
}

@end

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
#import "BTstack/BTDevice.h"
#import "EAGLView.h"
#import "EAGLViewController.h"

#import <btstack/btstack.h>
#import <btstack/run_loop.h>
#import <btstack/hci_cmds.h>

@interface UIDevice (privateAPI)
-(BOOL) isWildcat;
@end

// quaternion rotation library
float norm(float *vector, int dim);
void normalizeVector(float *vector, int dim);
void getRotationMatrixFromVectors(float vin[3], float vout[3], float matrix[4][4]);
float getRotationAngle(float matrix[4][4]);
	
BTDevice *device;
uint16_t wiiMoteConHandle = 0;
uint16_t hidControl = 0;
uint16_t hidInterrupt = 0;

#define SIZE  5
int counter;

// define the rest position 
float restPosition[3] = {0,0,1};
// static float restPosition2[3] = {0,0,-1};
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

@implementation WiiMoteOpenGLDemoAppDelegate

@synthesize window;
@synthesize glView;
@synthesize navControl;
@synthesize glViewControl;

-(void) handleBtDataWithX:(uint8_t) x andY:(uint8_t) y andZ:(uint8_t) z{
	
    // NSLog(@"Data 0x%02x, 0x%02x, 0x%02x", x, y, z);
    
	float accData[3];
	accData[0] = addToHistory( histX, 1 * (x - 128));
	accData[1] = addToHistory( histY, 1 * (y - 128));
	accData[2] = addToHistory( histZ, 1 * (z - 128));
	
    [glView setRotationX:0 Y:(accData[0] * 5) Z:0];
    // NSLog(@"Set Rotation: %f", accData[0]);
    return;
    
	float rotationMatrix[4][4];

#if 0 
	// fancy stuff to fololow 
    getRotationMatrixFromVectors(restPosition, accData, rotationMatrix);

	float rotationAngle = getRotationAngle(rotationMatrix) * 180/M_PI;
	
#if 1
	if (rotationAngle >= 90){
		getRotationMatrixFromVectors(restPosition, accData, rotationMatrix);
		[glView setRotationX:0 Y:180 Z:0];
	} else {
		[glView setRotationX:0 Y:0 Z:0];
	}
#endif
#if 0
	// float frontV[3] ={ 1, 0, 0};
	float projectectFrontV[3];
	projectectFrontV[0] = rotationMatrix[0][0];
	projectectFrontV[1] = rotationMatrix[1][0];
	projectectFrontV[2] = rotationMatrix[2][0];
	float correctionZ = atan2(projectectFrontV[1], projectectFrontV[0]) * 180/M_PI;
	NSLog(@"%f, %f, %f - angle %f - dir %f, %f=> %f\n", accData[0], accData[1], accData[2], rotationAngle,
		   projectectFrontV[0], projectectFrontV[1], correctionZ);

	// if (rotationAngle >= 90){
	// [[theMainApp glView] setRotationX:0 Y:0 Z:-correctionZ];
	// }
	
#endif
#endif
	[glView setRotationMatrix:rotationMatrix];
    
}

// direct access
-(void) btstackManager:(BTstackManager*) manager
  handlePacketWithType:(uint8_t) packet_type
			forChannel:(uint16_t) channel
			   andData:(uint8_t *)packet
			   withLen:(uint16_t) size
{
	bd_addr_t event_addr;
	
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
			if (packet[0] == 0xa1 && packet[1] == 0x31){
				[self handleBtDataWithX:packet[4] andY:packet[5] andZ:packet[6]];
			}
			break;
			

		case HCI_EVENT_PACKET:
			
			switch (packet[0]){

				case HCI_EVENT_COMMAND_COMPLETE:
					if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
                        // connect to device
                        bt_send_cmd(&l2cap_create_channel, [device address], PSM_HID_CONTROL);
					}
					break;

				case HCI_EVENT_PIN_CODE_REQUEST:
					bt_flip_addr(event_addr, &packet[2]);
					if (BD_ADDR_CMP([device address], event_addr)) break;
                    
					// inform about pin code request
					NSLog(@"HCI_EVENT_PIN_CODE_REQUEST\n");
					bt_send_cmd(&hci_pin_code_request_reply, event_addr, 6,  &packet[2]); // use inverse bd_addr as PIN
					break;

				case L2CAP_EVENT_CHANNEL_OPENED:
					if (packet[2] == 0) {
						// inform about new l2cap connection
						bt_flip_addr(event_addr, &packet[3]);
						uint16_t psm = READ_BT_16(packet, 11); 
						uint16_t source_cid = READ_BT_16(packet, 13); 
                        uint16_t dest_cid   = READ_BT_16(packet, 15);
						wiiMoteConHandle = READ_BT_16(packet, 9);
						NSLog(@"Channel successfully opened: handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x",
							  wiiMoteConHandle, psm, source_cid, dest_cid);
						if (psm == PSM_HID_CONTROL) {
							// control channel openedn succesfully, now open interrupt channel, too.
                            hidControl = source_cid;
							bt_send_cmd(&l2cap_create_channel, event_addr, PSM_HID_INTERRUPT);
						} else {
							// request acceleration data.. 
                            hidInterrupt = source_cid;
							uint8_t setMode31[] = { 0xa2, 0x12, 0x00, 0x31 };
							bt_send_l2cap( hidInterrupt, setMode31, sizeof(setMode31));
							uint8_t setLEDs[] = { 0xa2, 0x11, 0x10 };
							bt_send_l2cap( hidInterrupt, setLEDs, sizeof(setLEDs));
							// start demo
							[self startDemo];
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

    // prepare glView
	glView = (EAGLView *) [glViewControl view];
	glView.animationInterval = 1.0 / 60.0;

    // push glViewControl on stack
    [navControl pushViewController:glViewControl animated:YES];
    
    // let's go
    [glView startAnimation];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {

	// create discovery controller
	discoveryView = [[BTDiscoveryViewController alloc] init];
	[discoveryView setDelegate:self];
	
    // footer text
    CGRect newFrame = CGRectMake(0.0, 0.0, discoveryView.tableView.bounds.size.width, 100);
	UIView *footerView = [[UIView alloc] initWithFrame:newFrame];

    UIDevice * uiDevice = [UIDevice currentDevice];
	float margin = ([uiDevice respondsToSelector:@selector(isWildcat)] && [uiDevice isWildcat]) ? 50 : 20;

    UITextView *footer = [[UITextView alloc] initWithFrame:CGRectMake(margin,10,discoveryView.tableView.bounds.size.width - 2 * margin ,85)];
	footer.text = @"Please make your WiiMote discoverable by pressing the little RED SYNC button.";
	footer.font = [UIFont fontWithName:@"Arial" size:18];
	footer.backgroundColor = [UIColor groupTableViewBackgroundColor];
	footer.editable = false;
	[footerView addSubview:footer];
	[[discoveryView tableView] setTableFooterView:footerView];
    
    
    // TODO fix
	// [discoveryView setAllowSelection:NO];

	// create view controller
	glViewControl = [[EAGLViewController alloc] init];

	navControl = [[UINavigationController alloc] initWithRootViewController:discoveryView];
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	[window addSubview:navControl.view];
	[window makeKeyAndVisible];

	// BTstack
	BTstackManager * bt = [BTstackManager sharedInstance];
	[bt setDelegate:self];
	[bt addListener:self];
	[bt addListener:discoveryView];

	BTstackError err = [bt activate];
	if (err) NSLog(@"activate err 0x%02x!", err);
}

// new
-(void) activatedBTstackManager:(BTstackManager*) manager {
	NSLog(@"activated!");
	[[BTstackManager sharedInstance] startDiscovery];
}

-(void) btstackManager:(BTstackManager*)manager deviceInfo:(BTDevice*)newDevice {
	NSLog(@"Device Info: addr %@ name %@ COD 0x%06x", [newDevice addressString], [newDevice name], [newDevice classOfDevice] ); 
	if ([newDevice name] && [[newDevice name] hasPrefix:@"Nintendo RVL-CNT-01"]){
		NSLog(@"WiiMote found with address %@", [newDevice addressString]);
		device = newDevice;
		[[BTstackManager sharedInstance] stopDiscovery];
	}
}

-(void) discoveryStoppedBTstackManager:(BTstackManager*) manager {
	NSLog(@"discoveryStopped!");
	bt_send_cmd(&hci_write_authentication_enable, 0);
}

- (void)dealloc {
	[window release];
	[glView release];
	[super dealloc];
}

@end

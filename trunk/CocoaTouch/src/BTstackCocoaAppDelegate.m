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

// forward packet to Objective C method
void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
	[ ((BTstackCocoaAppDelegate *)[[UIApplication sharedApplication] delegate]) handlePacketWithType:packet_type data:packet len:size];
}

@implementation BTstackCocoaAppDelegate

@synthesize window;

- (BTDevice *) getDeviceForAddress:(bd_addr_t *)addr {
	uint8_t j;
	for (j=0; j<[devices count]; j++){
		BTDevice *dev = [devices objectAtIndex:j];
		if (BD_ADDR_CMP(addr, [dev address]) == 0){
			return dev;
		}
	}
	return nil;
}

- (bool) getNextRemoteName{
	BTDevice *remoteDev = nil;
	for (remoteNameIndex = 0; remoteNameIndex < [devices count] ; remoteNameIndex++){
		BTDevice *dev = [devices objectAtIndex:remoteNameIndex];
		if (![dev name]){
			remoteDev = dev;
			break;
		}
	}
	if (remoteDev) {
		[inqView setInquiryState:kInquiryRemoteName];
		[remoteDev setConnectionState:kBluetoothConnectionRemoteName];
		bt_send_cmd(&hci_remote_name_request, [remoteDev address], [remoteDev pageScanRepetitionMode], 0, [remoteDev clockOffset] | 0x8000);
	} else  {
		[inqView setInquiryState:kInquiryInactive];
		// inquiry done.
	}
	return remoteDev;
}

- (void) startInquiry {
	if ([inqView inquiryState] != kInquiryInactive) {
		NSLog(@"Inquiry already active");
		return;
	}
	NSLog(@"Inquiry started");
	[inqView setInquiryState:kInquiryActive];
	bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, 15, 0);
}

- (void) handlePacketWithType:(uint8_t) packetType data:(uint8_t*) data len:(uint16_t) len {

	static bool inquiryDone = 0;
	bd_addr_t event_addr;
	switch (packetType) {
			
		case HCI_EVENT_PACKET:
			
			switch (data[0]){
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated
					[inqView setBluetoothState:data[2] ];
					
					// set BT state
					if (!inquiryDone && data[2] == HCI_STATE_WORKING) {
						inquiryDone = true;
						[self startInquiry];
					}
					break;

				case HCI_EVENT_INQUIRY_RESULT:
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
					{
						int numResponses = data[2];
						int i;
						for (i=0; i<numResponses;i++){
							bd_addr_t addr;
							bt_flip_addr(addr, &data[3+i*6]);
							if ([self getDeviceForAddress:&addr]) {
								NSLog(@"Device %@ already in list", [BTDevice stringForAddress:&addr]);
								continue;
							}
							BTDevice *dev = [[BTDevice alloc] init];
							[dev setAddress:&addr];
							[dev setPageScanRepetitionMode:data[3 + numResponses*6 + i]];
							[dev setClassOfDevice:READ_BT_24(data, 3 + numResponses*(6+1+1+1) + i*3)];
							[dev setClockOffset:(READ_BT_16(data, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff)];
							hexdump(data, len);
							NSLog(@"adding %@", [dev toString] );
							[devices addObject:dev];
						}
					}
					[[inqView tableView] reloadData];
					break;
					
				case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
					{
						bt_flip_addr(event_addr, &data[3]);
						BTDevice *dev = [self getDeviceForAddress:&event_addr];
						if (!dev) break;
						[dev setConnectionState:kBluetoothConnectionNotConnected];
						[dev setName:[NSString stringWithUTF8String:(const char *) &data[9]]];
						[[inqView tableView] reloadData];
						remoteNameIndex++;
						[self getNextRemoteName];
					}
					break;
					
				case L2CAP_EVENT_CHANNEL_OPENED:
					// inform about new l2cap connection
					bt_flip_addr(event_addr, &data[2]);
					uint16_t psm = READ_BT_16(data, 10); 
					uint16_t source_cid = READ_BT_16(data, 12); 
					printf("Channel successfully opened: ");
					print_bd_addr(event_addr);
					printf(", handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x\n",
						   READ_BT_16(data, 8), psm, source_cid,  READ_BT_16(data, 14));
					break;
					
				case HCI_EVENT_COMMAND_COMPLETE:
					break;
					
				default:
					// Inquiry done
					if (data[0] == HCI_EVENT_INQUIRY_COMPLETE || COMMAND_COMPLETE_EVENT(data, hci_inquiry_cancel)){
						NSLog(@"Inquiry stopped");
						if ([inqView inquiryState] == kInquiryActive){
							remoteNameIndex = 0;
							[self getNextRemoteName];
						}
						break;
					}
					
					hexdump(data, len);
					break;
			}
			
		default:
			break;
	}
}



- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    // Override point for customization after application launch
	window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	
	// create BTInquiryView
	inqView = [[BTInquiryViewController alloc] init];
	[inqView setBluetoothState:HCI_STATE_OFF];
	devices = [[NSMutableArray alloc] init];
	[inqView setDevices:devices];
	
	[window addSubview:[inqView view]];

    // show
	[window makeKeyAndVisible];
	
	// start Bluetooth
	run_loop_init(RUN_LOOP_COCOA);
	bt_open();
	bt_register_packet_handler(packet_handler);
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
}

- (void)dealloc {
    [window release];
    [super dealloc];
}


@end

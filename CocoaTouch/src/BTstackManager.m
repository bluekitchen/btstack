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

#import <BTstack/BTstackManager.h>

#import <btstack/btstack.h>
#import <btstack/BTDevice.h>
#import "../../RFCOMM/rfcomm.h"

#define BTstackManagerID @"ch.ringwald.btstack"
#define INQUIRY_INTERVAL 3

static BTstackManager * btstackManager = nil;

@interface BTstackManager (privat)
- (void)handlePacketWithType:(uint8_t) packet_type forChannel:(uint16_t) channel andData:(uint8_t *)packet withLen:(uint16_t) size;
- (void)readDeviceInfo;
- (void)storeDeviceInfo;
@end

// needed for libBTstack
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	[btstackManager handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
}

// dummy implementation of BTstackManagerDelegate protocol to avoid respondsToSelector call
@implementation NSObject (BTstackManagerDelegateDummy)
-(void) activated {};
-(void) activationFailed:(BTstackError)error {};
-(void) deactivated {};
-(BOOL) disableSystemBluetooth { return YES; }; // default: YES
-(void) deviceInfo:(BTDevice*)device {};
-(void) discoveryStopped {};
-(void) discoveryInquiry{};
-(void) discoveryQueryRemoteName:(int)deviceIndex{};
-(NSString*) pinForAddress:(bd_addr_t)addr { return @"0000"; }; // default: "0000"
-(void) l2capChannelCreatedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm asID:(uint16_t)channelID {};
-(void) l2capChannelCreateFailedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm error:(BTstackError)error{};
-(void) l2capChannelClosedForChannelID:(uint16_t)channelID{};
-(void) l2capDataReceivedForChannelID:(uint16_t)channelID withData:(uint8_t *)packet ofLen:(uint16_t)size{};
-(void) rfcommConnectionCreatedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel asID:(uint16_t)connectionID{};
-(void) rfcommConnectionCreateFailedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel error:(BTstackError)error{};
-(void) rfcommConnectionClosedForConnectionID:(uint16_t)connectionID{};
-(void) rfcommDataReceivedForConnectionID:(uint16_t)connectionID withData:(uint8_t *)packet ofLen:(uint16_t)size{};
// TODO add l2cap and rfcomm incoming events
-(void) handlePacketWithType:(uint8_t) packet_type forChannel:(uint16_t) channel andData:(uint8_t *)packet withLen:(uint16_t) size{};
@end


@implementation BTstackManager

@synthesize delegate = _delegate;

-(BTstackManager *) init {
	self = [super init];
	if (!self) return self;
	
	state = kDeactivated;
	discoveryState = kInactive;
	connectedToDaemon = NO;
	_delegate = nil;
	
	// device discovery
	discoveredDevices = [[NSMutableArray alloc] init];
	
	// read device database
	[self readDeviceInfo];
	
	// Use Cocoa run loop
	run_loop_init(RUN_LOOP_COCOA);
	
	// our packet handler
	bt_register_packet_handler(packet_handler);
	
	return self;
}

+(BTstackManager *) sharedInstance {
	if (!btstackManager) {
		btstackManager = [[BTstackManager alloc] init];
	}
	return btstackManager;
}

// Activation
-(BTstackError) activate {
	
	BTstackError err = 0;
	if (!connectedToDaemon) {
		err = bt_open();
		if (err) return BTSTACK_CONNECTION_TO_BTDAEMON_FAILED;
	}
	connectedToDaemon = YES;
	
	// check system BT
	state = kW4SysBTState;
	bt_send_cmd(&btstack_get_system_bluetooth_enabled);
	
	return err;
}

-(BTstackError) deactivate {
	if (!connectedToDaemon) return BTSTACK_CONNECTION_TO_BTDAEMON_FAILED;
	state = kW4Deactivated;
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_OFF);
	return 0;
}


// Discovery
-(BTstackError) startDiscovery {
	if (state != kActivated) return BTSTACK_NOT_ACTIVATED;
	discoveryState = kW4InquiryMode;
	bt_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
	return 0;
};
-(BTstackError) stopDiscovery{
	return 0;
};

-(int) numberOfDevicesFound{
	return [discoveredDevices count];
};

-(BTDevice*) deviceAtIndex:(int)index{
	return (BTDevice*) [discoveredDevices objectAtIndex:index];
};

-(BTDevice*) deviceForAddress:(bd_addr_t*) address{
	for (BTDevice *device in discoveredDevices){
		// NSLog(@"compare %@ to %@", [BTDevice stringForAddress:address], [device addressString]); 
		if ( BD_ADDR_CMP(address, [device address]) == 0){
			return device;
		}
	}
	return nil;
}

// Connections
-(BTstackError) createL2CAPChannelAtAddress:(bd_addr_t) address withPSM:(uint16_t)psm authenticated:(BOOL)authentication {
	return 0;
};
-(BTstackError) closeL2CAPChannelWithID:(uint16_t) channelID {
	return 0;
};
-(BTstackError) sendL2CAPPacketForChannelID:(uint16_t)channelID {
	return 0;
};

-(BTstackError) createRFCOMMConnectionAtAddress:(bd_addr_t) address withChannel:(uint16_t)psm authenticated:(BOOL)authentication {
	return 0;
};
-(BTstackError) closeRFCOMMConnectionWithID:(uint16_t) connectionID {
	return 0;
};
-(BTstackError) sendRFCOMMPacketForChannelID:(uint16_t)connectionID {
	return 0;
};

- (void) activationHandleEvent:(uint8_t *)packet withLen:(uint16_t) size {
	switch (state) {
						
		case kW4SysBTState:
		case kW4SysBTDisabled:
			
			// BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED
			if ( packet[0] == BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED){
				if (packet[2]){
					// system bt on - first time try to disable it
					if ( state == kW4SysBTState) {
						if (_delegate == nil || [_delegate disableSystemBluetooth]){
							state = kW4SysBTDisabled;
							bt_send_cmd(&btstack_set_system_bluetooth_enabled, 0);
						} else {
							state = kDeactivated;
							[_delegate activationFailed:BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH];
						}
					} else {
						state = kDeactivated;
						[_delegate activationFailed:BTSTACK_ACTIVATION_FAILED_UNKNOWN];
					}
				} else {
					state = kW4Activated;
					bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON);
				}
			}
			break;
			
		case kW4Activated:
			switch (packet[0]){
				case BTSTACK_EVENT_STATE:
					if (packet[2] == HCI_STATE_WORKING){
						state = kActivated;
						[_delegate activated];
					}
					break;
				case BTSTACK_EVENT_POWERON_FAILED:
					[_delegate activationFailed:BTSTACK_ACTIVATION_POWERON_FAILED];
					state = kDeactivated;
					break;
				default:
					break;
			}
			break;
			
		case kW4Deactivated:
			if (packet[0] == BTSTACK_EVENT_STATE){
				if (packet[2] == HCI_STATE_OFF){
					state = kDeactivated;
					[_delegate deactivated];
				}
			}
			break;
			
		default:
			break;
	}
}

-(void) discoveryRemoteName{
	BOOL found = NO;
	while ( discoveryDeviceIndex < [discoveredDevices count]){
		BTDevice *device = [discoveredDevices objectAtIndex:discoveryDeviceIndex];
		if (device.name) {
			discoveryDeviceIndex ++;
			continue;
		}
		bd_addr_t *addr = [device address];
		
		// BD_ADDR_COPY(&addr, device.address);
		// NSLog(@"Get remote name of %@", [BTDevice stringForAddress:addr]);
		bt_send_cmd(&hci_remote_name_request, addr,
					device.pageScanRepetitionMode, 0, device.clockOffset | 0x8000);
		[_delegate discoveryQueryRemoteName:discoveryDeviceIndex];
		found = YES;
		break;
	}
	if (!found) {
		// printf("Queried all devices, restart.\n");
		discoveryState = kInquiry;
		bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
		[_delegate discoveryInquiry];
	}
}

-(void) discoveryHandleEvent:(uint8_t *)packet withLen:(uint16_t) size {
	bd_addr_t addr;
	int i;
	int numResponses;
	
	switch (discoveryState) {
			
		case kInactive:
			break;
			
		case kW4InquiryMode:
			if (packet[0] == HCI_EVENT_COMMAND_COMPLETE && COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
				discoveryState = kInquiry;
				bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
				[_delegate discoveryInquiry];
			}
			break;
		
		case kInquiry:
			
			switch (packet[0]){
				case HCI_EVENT_INQUIRY_RESULT:
					numResponses = packet[2];
					for (i=0; i<numResponses ; i++){
						bt_flip_addr(addr, &packet[3+i*6]);
						// NSLog(@"found %@", [BTDevice stringForAddress:&addr]);
						BTDevice* device = [self deviceForAddress:&addr];
						if (device) continue;
						device = [[BTDevice alloc] init];
						[device setAddress:&addr];
						device.pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						device.classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1+1)   + i*3);
						device.clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff;
						device.rssi  = 0;
						[discoveredDevices addObject:device];
						
						[_delegate deviceInfo:device];
					}
					break;
					
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
					numResponses = packet[2];
					for (i=0; i<numResponses ;i++){
						bt_flip_addr(addr, &packet[3+i*6]);
						// NSLog(@"found %@", [BTDevice stringForAddress:&addr]);
						BTDevice* device = [self deviceForAddress:&addr];
						if (device) continue;
						device = [[BTDevice alloc] init];
						[device setAddress:&addr];
						device.pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						device.classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1)     + i*3);
						device.clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+3)   + i*2) & 0x7fff;
						device.rssi  =                    packet [3 + numResponses*(6+1+1+3+2) + i*1];
						[discoveredDevices addObject:device];
						
						[_delegate deviceInfo:device];
					}
					break;

				case HCI_EVENT_INQUIRY_COMPLETE:
					// printf("Inquiry scan done.\n");
					discoveryState = kRemoteName;
					discoveryDeviceIndex = 0;
					[self discoveryRemoteName];
					break;
			}
			break;
			
		case kRemoteName:
			if (packet[0] == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE){
				bt_flip_addr(addr, &packet[3]);
				// NSLog(@"Get remote name done for %@", [BTDevice stringForAddress:&addr]);
				BTDevice* device = [self deviceForAddress:&addr];
				if (device) {
					if (packet[2] == 0) {
						// get lenght: first null byte or max 248 chars
						int nameLen = 0;
						while (nameLen < 248 && packet[9+nameLen]) nameLen++;
 						device.name = [[NSString alloc] initWithBytes:&packet[9] length:nameLen encoding:NSUTF8StringEncoding];
					}
					[_delegate deviceInfo:device];
					discoveryDeviceIndex++;
					[self discoveryRemoteName];
				}
			}
			break;
				
		default:
			break;
	}
}

-(void) handlePacketWithType:(uint8_t)packet_type forChannel:(uint16_t)channel andData:(uint8_t *)packet withLen:(uint16_t) size {
	switch (state) {
			
		case kDeactivated:
			break;
		
		// Activation
		case kW4SysBTState:
		case kW4SysBTDisabled:
		case kW4Activated:
		case kW4Deactivated:
			if (packet_type == HCI_EVENT_PACKET) [self activationHandleEvent:packet withLen:size];
			break;
		
		// Discovery
		case kActivated:
			if (packet_type == HCI_EVENT_PACKET) [self discoveryHandleEvent:packet withLen:size];
			break;
			
		default:
			break;
	}

	[_delegate handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
}


-(void)readDeviceInfo {
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	NSDictionary * dict = [defaults persistentDomainForName:BTstackManagerID];
	deviceInfo = [NSMutableDictionary dictionaryWithCapacity:([dict count]+5)];
	
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *deviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[deviceEntry addEntriesFromDictionary:value];
		[deviceInfo setObject:deviceEntry forKey:key];
	}
}

-(void)storeDeviceInfo{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setPersistentDomain:deviceInfo forName:BTstackManagerID];
    [defaults synchronize];
	// NSLog(@"store prefs %@", deviceInfo );
	// NSLog(@"prefs name %@", prefsName);
	// NSLog(@"Persistence Domain names %@", [defaults persistentDomainNames]);
}

@end

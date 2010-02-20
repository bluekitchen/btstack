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
#import "../../RFCOMM/rfcomm.h"


#define BTstackManagerID @"ch.ringwald.btstack"


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
	connectedToDaemon = NO;
	_delegate = nil;
	
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
	return 0;
}

// Discovery
-(BTstackError) startDiscovery {
	return 0;
};
-(BTstackError) stopDiscovery{
	return 0;
};
-(int) numberOfDevicesFound{
	return 0;
};
-(BTDevice*) deviceAtIndex:(int)index{
	return 0;
};

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

- (void) handlePacketWithType:(uint8_t)packet_type forChannel:(uint16_t)channel andData:(uint8_t *)packet withLen:(uint16_t) size {
	switch (state) {
			
		case kDeactivated:
			break;
			
		case kW4SysBTState:
		case kW4SysBTDisabled:
			
			// BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED
			if ( packet_type == HCI_EVENT_PACKET
				&& packet[0] == BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED){
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
			if (packet_type == HCI_EVENT_PACKET){
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
			}
			break;
			
		default:
			break;
	}

	[_delegate handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
}


- (void)readDeviceInfo {
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

- (void)storeDeviceInfo{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setPersistentDomain:deviceInfo forName:BTstackManagerID];
    [defaults synchronize];
	// NSLog(@"store prefs %@", deviceInfo );
	// NSLog(@"prefs name %@", prefsName);
	// NSLog(@"Persistence Domain names %@", [defaults persistentDomainNames]);
}

@end

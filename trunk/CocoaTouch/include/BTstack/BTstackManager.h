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

#pragma once

#import <Foundation/Foundation.h>
#import <stdint.h>
#import <btstack/btstack.h>

@class BTDevice;

/*
 * Information on devices is stored in a system-wide plist and maintained by BTstackManager
 * this includes the link keys
 */

// TODO enumerate BTstackError type
typedef int BTstackError;

@protocol BTstackManagerDelegate;

@interface BTstackManager : NSObject {
	id<BTstackManagerDelegate> delegate;
}

// shared instance
+(BTstackManager *) sharedInstance;

// BTstackManagerDelegate protocol implemented by application
-(void) setDelegate:(id<BTstackManagerDelegate>) delegate;

// Activation
-(void) activate;
-(void) deactivate;

// Discovery
-(BTstackError) startDiscovery;
-(BTstackError) stopDiscovery;
-(int) numberOfDevicesFound;
-(BTDevice*) deviceAtIndex:(int)index;

// Connections
-(void) createL2CAPChannelAtAddress:(bd_addr_t) address withPSM:(uint16_t)psm authenticated:(BOOL)authentication;
-(void) closeL2CAPChannelWithID:(uint16_t) channelID;
-(void) sendL2CAPPacketForChannelID:(uint16_t)channelID;

-(void) createRFCOMMConnectionAtAddress:(bd_addr_t) address withChannel:(uint16_t)psm authenticated:(BOOL)authentication;
-(void) closeRFCOMMConnectionWithID:(uint16_t) connectionID;
-(void) sendRFCOMMPacketForChannelID:(uint16_t)connectionID;
@end


@protocol BTstackManagerDelegate
-(void) btstackManager:(BTstackManager *)btstackManager
		  handlePacketWithType:(uint8_t) packet_type
					forChannel:(uint16_t) channel
					   andData:(uint8_t *)packet
					   withLen:(uint16_t) size;

// Activation events
-(void) activated:(BTstackManager *)btstackManager;
-(void) activationFailed:(BTstackError)error btstackManager:(BTstackManager *)btstackManager ;
-(void) deactivated:(BTstackManager *)btstackManager;

// Activation callbacks
-(BOOL) disableSystemBluetooth:(BTstackManager *)btstackManager ; // default: YES

// Discovery events: general
-(void) deviceInfo:(BTDevice*)device btstackManager:(BTstackManager *)btstackManager;
-(void) discoveryStopped:(BTstackManager *)btstackManager ;

// Discovery events: UI
-(void) discoveryInquiry:(BTstackManager *)btstackManager ;
-(void) discoveryQueryRemoteName:(int)deviceIndex btstackManager:(BTstackManager *)btstackManager;

// Connection events
-(NSString*) btstackManager:(BTstackManager *)btstackManager pinForAddress:(bd_addr_t)addr;

-(void) l2capChannelCreatedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm asID:(uint16_t)channelID btstackManager:(BTstackManager *)btstackManager;
-(void) l2capChannelCreateFailedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm error:(BTstackError)error btstackManager:(BTstackManager *)btstackManager;
-(void) l2capChannelClosedForChannelID:(uint16_t)channelID btstackManager:(BTstackManager *)btstackManager;
-(void) l2capDataReceivedForChannelID:(uint16_t)channelID withData:(uint8_t *)packet andOfLen:(uint16_t) size btstackManager:(BTstackManager *)btstackManager;

-(void) rfcommConnectionCreatedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel asID:(uint16_t)connectionID btstackManager:(BTstackManager *)btstackManager;
-(void) rfcommConnectionCreateFailedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel error:(BTstackError)error btstackManager:(BTstackManager *)btstackManager;
-(void) rfcommConnectionClosedForConnectionID:(uint16_t)connectionID btstackManager:(BTstackManager *)btstackManager;
-(void) rfcommDataReceivedForConnectionID:(uint16_t)connectionID withData:(uint8_t *)packet andOfLen:(uint16_t) size btstackManager:(BTstackManager *)btstackManager;

@end


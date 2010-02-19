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

static BTstackManager * btstackManager = NULL;

@interface BTstackManager (privat)
- (void) handlePacketWithType:(uint8_t) packet_type
				   forChannel:(uint16_t) channel
					  andData:(uint8_t *)packet
					  withLen:(uint16_t) size;
@end

// needed for libBTstack
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	if (btstackManager) {
		[btstackManager handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
	}
}

@implementation BTstackManager

-(BTstackManager *) init {
	// Cocoa compatible run loop
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

-(void) setDelegate:(id<BTstackManagerDelegate>) newDelegate {
	delegate = newDelegate;
}

-(void) activate {
	bt_open();	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
}

-(void) enableRFCOMM {
	// rfcomm_register_packet_handler(packet_handler);
}

- (void) handlePacketWithType:(uint8_t)packet_type forChannel:(uint16_t)channel andData:(uint8_t *)packet withLen:(uint16_t) size {
	if (delegate) {
		[delegate handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
	}
}
@end

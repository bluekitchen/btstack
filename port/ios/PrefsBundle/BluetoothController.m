/*
 * Copyright (C) 2011 by Matthias Ringwald
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

#import "BluetoothController.h"

#pragma mark callback handler
static void btstackStoppedCallback(CFNotificationCenterRef  center,
                                   void                    *observer,
                                   CFStringRef              name,
                                   const void              *object,
                                   CFDictionaryRef          userInfo) {
    [[BluetoothController sharedInstance] connectionBroke];
}

static void bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;
    [[BluetoothController sharedInstance] eventPacketHandler:packet withSize:size];
}


static BluetoothController* sharedInstance = nil;

@implementation BluetoothController

@synthesize listener;

+(BluetoothController*) sharedInstance{
    if (!sharedInstance) {
        sharedInstance = [[self alloc] init];
    }
    return sharedInstance;
}

-(void) resetState{
    // initial state
    isConnected = NO;
    system_bluetooth = NO;
    hci_state = HCI_STATE_OFF;
    state = kIdle;
    targetType = BluetoothTypeNone;
}

-(id)init{
    
    self = [super init];
    
    listener = nil;
    
    // register for BTstack restart notficiations
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), // the notification center to use
                                    NULL,                      // an arbitrary observer-identifier
                                    btstackStoppedCallback,    // callback to call when a notification is posted
                                    CFSTR("ch.ringwald.btstack.stopped"), // notification name
                                    NULL,              // optional object identifier to filter notifications
                                    CFNotificationSuspensionBehaviorDrop); // suspension behavior
    
    // set up libBTstack
    run_loop_init(RUN_LOOP_COCOA);
    bt_register_packet_handler(bt_packet_handler);
    
    return self;
}

-(BOOL)open{

    if (!isConnected) {

        [self resetState];
        
        int err = bt_open();
        if (err){
            NSLog(@"Cannot connect to BTdaemon!");
            return NO;
        }
        // NSLog(@"Connected to BTdaemon!");
        isConnected = YES;
    }

    // get status
    bt_send_cmd(&btstack_get_state);

    return YES;
}

-(void)connectionBroke {
    // NSLog(@"BTstack stopped");
    [self resetState];
    [self open];
    [listener bluetoothStateChanged];
}

-(void)close{
    if (isConnected) {
        // NSLog(@"Disconnected from BTdaemon!");
        bt_close();
    }
    [self resetState];
}

-(BOOL)isConnected{
    return isConnected;
}

-(BluetoothType_t)targetType{
    if (isActive) {
        return targetType;
    }
    return [self bluetoothType];
}

-(BluetoothType_t) bluetoothType {
    if (hci_state != HCI_STATE_OFF) {
        return BluetoothTypeBTstack;
    }
    if (system_bluetooth) {
        return BluetoothTypeApple;
    }
    return BluetoothTypeNone;
}

-(BOOL)isActive{
    return isActive;
}

-(BOOL)canChange{
    if (isActive)     return NO;
    if (!isConnected) return NO;
    return YES;
}


-(void)bluetoothStateChanged {
    
    BluetoothType_t type = [self bluetoothType];
    
    if (state != kIdle && type != targetType) {
        return;
    }
    
    // NSLog(@"bluetoothStateChanged %u", type);
    
    state = kIdle;
    
    isActive = NO;
    [listener bluetoothStateChanged];
}

-(void)eventPacketHandler:(uint8_t*) packet withSize:(uint16_t) size {
    // NSLog(@"bt_packet_handler event: %u, state %u", packet[0], state);
    
    // update state
    switch(packet[0]){
        case BTSTACK_EVENT_STATE:
            hci_state = packet[2];
            // NSLog(@"new BTSTACK_EVENT_STATE %u", hci_state);
            break;
        case BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED:
            system_bluetooth = packet[2];
            // NSLog(@"new BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED %u", system_bluetooth);
            break;
        default:
            break;
    }
    
    switch(state){
            
        case kIdle:
            if (packet[0] == BTSTACK_EVENT_STATE) {
                if (hci_state == HCI_STATE_OFF) {
                    bt_send_cmd(&btstack_get_system_bluetooth_enabled);
                } else {
                    system_bluetooth = 0;
                }
            }
            break;
            
        case kW4SystemOffToEnableBTstack:
            if (packet[0] == BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED) {
                if (system_bluetooth == 0){
                    bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON);
                    state = kIdle;
                }
            }
            break;
            
        case kW4BTstackOffToEnableSystem:
            if (packet[0] == BTSTACK_EVENT_STATE) {
                if (hci_state == HCI_STATE_OFF) {
                    // NSLog(@"Sending set system bluetooth enable A");
                    bt_send_cmd(&btstack_set_system_bluetooth_enabled, 1);
                }
            }
            if (packet[0] == BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED) {
                if (system_bluetooth == 0){
                    // NSLog(@"Sending set system bluetooth enable B");
                    bt_send_cmd(&btstack_set_system_bluetooth_enabled, 1);
                } else {
                    // NSLog(@"Sending set system bluetooth enable DONE");
                    state = kIdle;
                }
            }
            break;
            
        case kW4Transition:
            break;
    }
    
    [self bluetoothStateChanged];
    
}

-(void)requestType:(BluetoothType_t)bluetoothType{
    // NSLog(@"bluetoothChangeRequested: old %u, new %u", [self bluetoothType], bluetoothType);

    // ignore taps during transition
    if (state != kIdle) {
        return;
    };

    switch ([self bluetoothType]){
        case BluetoothTypeNone:
            switch (bluetoothType) {
                case BluetoothTypeNone:
                    break;
                case BluetoothTypeApple:
                    state = kW4BTstackOffToEnableSystem;    // hack: turning on iOS after BTstack fails, this will make it retry
                    bt_send_cmd(&btstack_set_system_bluetooth_enabled, 1);
                    targetType = BluetoothTypeApple;
                    isActive = YES;
                    break;
                case BluetoothTypeBTstack:
                    state = kW4Transition;
                    bt_send_cmd(&btstack_set_bluetooth_enabled, 1);
                    targetType = BluetoothTypeBTstack;
                    isActive = YES;
                    break;
            }
            break;
        case BluetoothTypeApple:
            switch (bluetoothType) {
                case BluetoothTypeNone:
                    state = kW4Transition;
                    bt_send_cmd(&btstack_set_system_bluetooth_enabled, 0);
                    targetType = BluetoothTypeNone;
                    isActive = YES;
                    break;
                case BluetoothTypeApple:
                    break;
                case BluetoothTypeBTstack:
                    state = kW4SystemOffToEnableBTstack;
                    bt_send_cmd(&btstack_set_system_bluetooth_enabled, 0);
                    targetType = BluetoothTypeBTstack;
                    isActive = YES;
                    break;
            }
            break;
        case BluetoothTypeBTstack:
            switch (bluetoothType) {
                case BluetoothTypeNone:
                    state = kW4Transition;
                    bt_send_cmd(&btstack_set_bluetooth_enabled, 0);
                    targetType = BluetoothTypeNone;
                    isActive = YES;
                    break;
                case BluetoothTypeApple:
                    state = kW4BTstackOffToEnableSystem;
                    bt_send_cmd(&btstack_set_bluetooth_enabled, 0);
                    targetType = BluetoothTypeApple;
                    isActive = YES;
                    break;
                case BluetoothTypeBTstack:
                    break;
            }
            break;
    }
}


@end

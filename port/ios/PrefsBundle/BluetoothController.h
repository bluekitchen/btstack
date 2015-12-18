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

#import "btstack_client.h"

typedef enum transitionStates {
    kIdle = 1,
    kW4SystemOffToEnableBTstack,
    kW4BTstackOffToEnableSystem,
    kW4Transition
} transitionStates_t;

typedef enum BluetoothType {
    BluetoothTypeNone = 0,
    BluetoothTypeApple,
    BluetoothTypeBTstack
} BluetoothType_t;

@protocol BluetoothControllerListener
-(void)bluetoothStateChanged;
@end

@interface BluetoothController : NSObject {
    
    HCI_STATE hci_state;
    BOOL system_bluetooth;
    transitionStates_t state;
    
    BluetoothType_t targetType;
    
    BOOL isConnected;
    BOOL isActive;
    
    id<BluetoothControllerListener> listener;
}

+(BluetoothController*) sharedInstance;

-(BOOL)open;
-(void)close;
-(void)requestType:(BluetoothType_t)bluetoothType;
-(void)eventPacketHandler:(uint8_t*) packet withSize:(uint16_t) size;
-(void)connectionBroke;

-(BluetoothType_t)bluetoothType;
-(BluetoothType_t)targetType;
    
-(BOOL)isConnected;
-(BOOL)isActive;
-(BOOL)canChange;

@property (nonatomic, assign) id<BluetoothControllerListener> listener;

@end

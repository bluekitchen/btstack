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
//  BTInquiryViewController.h
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

#include "hci_cmd.h" // for HCI_STATE

#define PREFS_REMOTE_NAME  @"RemoteName"
#define PREFS_LINK_KEY     @"LinkKey"

@class BTDevice;
@protocol BTInquiryDelegate;

typedef enum {
	kInquiryInactive,
	kInquiryActive,
	kInquiryRemoteName
} InquiryState;

@interface BTInquiryViewController : UITableViewController 
{
	NSMutableArray *devices; 
	HCI_STATE bluetoothState;
	InquiryState   inquiryState;
	UIActivityIndicatorView *deviceActivity;
	UIActivityIndicatorView *bluetoothActivity;
	UIFont * deviceNameFont;
	UIFont * macAddressFont;
	id<BTInquiryDelegate> delegate;
	bool allowSelection;
	bool showIcons;
	
	// used to store link keys and remote names
	NSMutableDictionary *deviceInfo;
    
	// hacks
	bool stopRemoteNameGathering;
	bool restartInquiry;
	BTDevice *remoteNameDevice; // device for which remote name request is pending
	BTDevice *remoteDevice;     // device for which connection is pending
	BTDevice *connectedDevice;  // device to which we're connected
	bool notifyDelegateOnInquiryStopped;
	
	// 2.0 - 3.0 compatibilty
	bool onSDK20;
}

- (void) startInquiry;
- (void) stopInquiry;

- (void) showConnecting:(BTDevice *) device;
- (void) showConnected:(BTDevice *) device;

@property (nonatomic, assign) bool allowSelection;
@property (nonatomic, assign) bool showIcons;
@property (nonatomic, retain) NSMutableArray *devices;
@property (nonatomic, retain) id<BTInquiryDelegate> delegate;
@property (nonatomic, retain) NSMutableDictionary *deviceInfo;
@end

@protocol BTInquiryDelegate
- (void) deviceChoosen:(BTInquiryViewController *) inqView device:(BTDevice*) device;
- (void) deviceDetected:(BTInquiryViewController *) inqView device:(BTDevice*) device;
- (void) disconnectDevice:(BTInquiryViewController *) inqView device:(BTDevice*) device;
- (void) inquiryStopped;
@end

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

#import "BluetoothTableViewAdapter.h"
#include <notify.h>

#define BTstackID "ch.ringwald.btstack"

@implementation BluetoothTableViewAdapter
@synthesize loggingSwitch;
@synthesize offButton;

-(id) initWithTableView:(UITableView *) tableView {
	bluetoothActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[bluetoothActivity startAnimating];
    _tableView = tableView;
    self.loggingSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(0,0, 100, 20)]; // size will be ignored anyway
    [loggingSwitch addTarget:self action:@selector(loggingSwitchToggled) forControlEvents:UIControlEventValueChanged];
    self.offButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
    self.offButton.frame = CGRectMake(0.0f, 0.0f, 80.0f, 20.0f);
    // self.offButton.backgroundColor = [UIColor redColor];
    [self.offButton setTitle:@"Force Off" forState:UIControlStateNormal];  
    [offButton addTarget:self action:@selector(offTapped) forControlEvents:UIControlEventTouchUpInside];

    // get old value
    Boolean valid;
    Boolean loggingOn = CFPreferencesGetAppBooleanValue(CFSTR("Logging"), CFSTR("ch.ringwald.btstack"), &valid);
    if (!valid) loggingOn = false;
    loggingSwitch.on = loggingOn;
    return self;
}

-(void)bluetoothStateChanged{
    if ([_tableView respondsToSelector:@selector(allowsSelection)]){
        _tableView.allowsSelection = [[BluetoothController sharedInstance] canChange];
    }
    [_tableView reloadData];
}

-(void) loggingSwitchToggled {
    // NSLog(@"Logging: %u", loggingSwitch.on);
    CFPropertyListRef on;
    if (loggingSwitch.on){
        on = kCFBooleanTrue;
    } else {
        on = kCFBooleanFalse;
    }
    CFPreferencesSetValue(CFSTR("Logging"), on, CFSTR("ch.ringwald.btstack"), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(BTstackID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    // send notification
    notify_post("ch.ringwald.btstack.preferences");
}

-(void)offTapped {
    NSLog(@"BTstack 'Force off' tapped");
    [[BluetoothController sharedInstance] requestType:BluetoothTypeNone];
}

#pragma mark Table view delegate methods

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_3_0
		cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
#else
		cell = [[[UITableViewCell alloc] initWithFrame:CGRectNull reuseIdentifier:(NSString *)CellIdentifier] autorelease];
#endif
    }
    
    // Set up the cell...
	NSString *theLabel = nil;
    
    cell.accessoryType = UITableViewCellAccessoryNone;
    cell.accessoryView = nil;
    
    BluetoothType_t bluetoothType = [[BluetoothController sharedInstance] targetType];
    BOOL activity = [[BluetoothController sharedInstance] isActive];
    NSLog(@"tableView update: type %u, active %u", bluetoothType, activity);
    
    switch ([indexPath section]) {
        case 0:
            if ([[BluetoothController sharedInstance] isConnected]){
                switch (bluetoothType){
                    case BluetoothTypeBTstack:
                        theLabel = @"Bluetooth Stack: BTstack";
                        cell.accessoryView = offButton;
                        break;
                    case BluetoothTypeApple:
                        theLabel = @"Bluetooth Stack: iOS";
                        break;
                    case BluetoothTypeNone:
                        theLabel = @"Bluetooth Stack: None";
                        break;
                }
            } else {
                theLabel = @"Bluetooth Stack: Unknown";
            }
            if (activity){
                cell.accessoryView = bluetoothActivity;
            }
            break;
        case 1:
            theLabel = @"Logging";
            cell.accessoryView = loggingSwitch;
            break;
            
        default:
            break;
    }
    
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_3_0
	if (theLabel) cell.textLabel.text =  theLabel;
#else
	if (theLabel) cell.text  = theLabel;
#endif
    return cell;
}

- (NSIndexPath *)tableView:(UITableView *)tableView willSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    // avoid selecting a row
    return nil;
}

#pragma mark Table view data source methods

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 2;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
    switch (section){
        case 0:
            return @"Bluetooth Config";
        default:
            return @"BTstack Config";
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    switch (section) {
        case 0:
            if (![[BluetoothController sharedInstance] isConnected]){
                return @"Cannot connect to BTstack daemon.\n\nPlease re-install the BTstack package.";
            }
            return @"BTstack-compatible applications automatically enable BTstack on start and disable it on exit. "
                   "Otherwise, you can force BTstack off here.";
        case 1:
            return @"Turn on logging, if you experience problems with BTstack-based software and add /tmp/hci_dump.pklg to your support mail.";
        default:
            return nil;
            
    }
}

// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return 1;
}


@end

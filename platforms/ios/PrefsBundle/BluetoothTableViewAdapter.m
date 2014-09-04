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

-(id) initWithTableView:(UITableView *) tableView {
	bluetoothActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[bluetoothActivity startAnimating];
    _tableView = tableView;
    self.loggingSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(0,0, 100, 20)]; // size will be ignored anyway
    [loggingSwitch addTarget:self action:@selector(loggingSwitchToggled) forControlEvents:UIControlEventValueChanged];
    
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
    BOOL hasAccessory;
    
    BluetoothType_t bluetoothType = [[BluetoothController sharedInstance] targetType];
    BOOL activity = [[BluetoothController sharedInstance] isActive];
    // NSLog(@"tableView update: type %u, active %u", bluetoothType, activity);
    
    switch ([indexPath section]) {
        case 0:
            switch ([indexPath row]) {
                case 0:
                    theLabel = @"BTstack";
                    hasAccessory = bluetoothType == BluetoothTypeBTstack;
                    break;
                case 1:
                    theLabel = @"iOS";
                    hasAccessory = bluetoothType == BluetoothTypeApple;
                    break;
                default:
                    theLabel = @"None";
                    hasAccessory = bluetoothType == BluetoothTypeNone;
                    break;
            }
            if (hasAccessory) {
                if (activity){
                    cell.accessoryView = bluetoothActivity;
                } else {
                    cell.accessoryType = UITableViewCellAccessoryCheckmark;
                }
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
    if([indexPath section] != 0) return nil;
    
    BluetoothType_t newType;
    switch ([indexPath row]) {
        case 0:
            newType = BluetoothTypeBTstack;
            break;
        case 1:
            newType = BluetoothTypeApple;
            break;
        default:
            newType = BluetoothTypeNone;
            break;
    }
    
    [[BluetoothController sharedInstance] requestType:newType];
    [_tableView reloadData];

	return nil;
}

#pragma mark Table view data source methods

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 2;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
    switch (section){
        case 0:
            if (![[BluetoothController sharedInstance] isConnected]){
                return @"Internal BTstack Error";
            }
            return @"Active Bluetooth Stack";
        case 1:
            return @"BTstack Config";
        default:
            return @"BTstack Config";
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    switch (section) {
        case 0:
            if (![[BluetoothController sharedInstance] isConnected]){
                return @"Cannot connect to BTstack daemon.\n\nPlease re-install BTstack package and/or make "
                "sure that /Library/LaunchDaemons/ is owned by user 'root' and group 'wheel' (root:wheel).\n"
                "If you're on 5.x, pleaes install latest version of 'Corona 5.0.1 Untether' package, reboot and try again.";
            }
            return @"Enabling iOS Bluetooth after BTstack was used can take up to 30 seconds. Please be patient.";
        case 1:
            return @"Turn on logging, if you experience problems with BTstack-based software and add /tmp/hci_dump.pklg to your support mail.";
        default:
            return nil;
            
    }
}

// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    switch (section){
        case 0:
            if (![[BluetoothController sharedInstance] isConnected]){
                return 0;
            }
            return 3;
        case 1:
            return 1;
        default:
            return 1;
    }
}


@end

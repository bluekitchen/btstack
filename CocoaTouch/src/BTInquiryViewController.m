//
//  BTInquiryViewController.m
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import "BTInquiryViewController.h"
#import "BTDevice.h"

@implementation BTInquiryViewController

@synthesize devices;
@synthesize bluetoothState;
@synthesize inquiryState;

#define MOCKUP

int mock_state = 0;

- (id) init {
	self = [super initWithStyle:UITableViewStyleGrouped];
	bluetoothState = kBluetoothStateUnknown;
	inquiryState = kInquiryInactive;
	
	deviceActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[deviceActivity startAnimating];
	return self;
}

/*
- (void)viewDidLoad {
    [super viewDidLoad];

    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}
*/

/*
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
}
*/
/*
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
}
*/
/*
- (void)viewWillDisappear:(BOOL)animated {
	[super viewWillDisappear:animated];
}
*/
/*
- (void)viewDidDisappear:(BOOL)animated {
	[super viewDidDisappear:animated];
}
*/

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	return YES;
}

- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}


#pragma mark Table view methods

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
	return @"Devices";
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}


// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	int rows = 1;  // 1 for status line 
	if (bluetoothState == kBluetoothStateOn) {
		rows += [devices count];
	}
	return rows;
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
		// cell.selectionStyle = UITableViewCellSelectionStyleNone;
    }
    
    // Set up the cell...
	NSString *label = nil;
	int idx = [indexPath indexAtPosition:1];
	if (bluetoothState != kBluetoothStateOn || idx >= [devices count]) {
		if (bluetoothState == kBluetoothStateInitializing){
			label = @"Activating BTstack...";
			cell.accessoryView = deviceActivity;
		} else {
			switch (inquiryState){
				case kInquiryInactive:
					if ([devices count] > 0){
						label = @"Find more devices...";
					} else {
						label = @"Find devices...";
					}
					cell.accessoryView = nil;
					break;
				case kInquiryActive:
					label = @"Searching...";
					cell.accessoryView = deviceActivity;
					break;
				case kInquiryRemoteName:
					label = @"Query device names...";
					cell.accessoryView = deviceActivity;
					break;
			}
		}
	} else {
		BTDevice *dev = [devices objectAtIndex:idx];
		label = [dev nameOrAddress];
		switch ([dev connectionState]) {
			case kBluetoothConnectionNotConnected:
			case kBluetoothConnectionConnected:
				cell.accessoryView = nil;
				break;
			case kBluetoothConnectionConnecting:
			case kBluetoothConnectionRemoteName:
				cell.accessoryView = deviceActivity;
				break;
		}
	}
	[cell.textLabel setText:label];
    return cell;
}


- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	NSLog(@"didSelectRowAtIndexPath %@", indexPath);
    // Navigation logic may go here. Create and push another view controller.
	// AnotherViewController *anotherViewController = [[AnotherViewController alloc] initWithNibName:@"AnotherView" bundle:nil];
	// [self.navigationController pushViewController:anotherViewController];
	// [anotherViewController release];
#ifdef MOCKUP
	switch (mock_state) {

		case 0:
			bluetoothState = kBluetoothStateOn;
			[tableView reloadData];
			mock_state++;
			break;

		case 1:
			inquiryState = kInquiryActive;
			[tableView reloadData];
			mock_state++;
			
		case 2:
		case 3: {
			BTDevice * dev = [[BTDevice alloc] init];
			[dev setAddress:[NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x:", mock_state, mock_state, mock_state, mock_state, mock_state, mock_state]];
			[devices addObject:dev];
			[tableView reloadData];
			mock_state++;
			break;
		}
		case 4: {
			inquiryState = kInquiryRemoteName;
			BTDevice * dev;
			dev = [devices objectAtIndex:0];
			// [dev setConnectionState:kBluetoothConnectionRemoteName];
			dev = [devices objectAtIndex:1];
			// [dev setConnectionState:kBluetoothConnectionRemoteName];
			[tableView reloadData];
			mock_state++;
			break;
		}
		case 5: {
			BTDevice * dev;
			dev = [devices objectAtIndex:0];
			[dev setName:@"Fake Device 1"];
			[dev setConnectionState:kBluetoothConnectionNotConnected];
			[tableView reloadData];
			mock_state++;
			break;
		}
		case 6: {
			inquiryState = kInquiryInactive;
			BTDevice * dev;
			dev = [devices objectAtIndex:1];
			[dev setConnectionState:kBluetoothConnectionNotConnected];
			[dev setName:@"Fake Device 2"];
			[tableView reloadData];
			mock_state = 1;
			break;
		}
	}
#endif
}


- (void)dealloc {
    [super dealloc];
}


@end


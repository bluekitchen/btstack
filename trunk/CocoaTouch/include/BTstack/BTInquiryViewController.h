//
//  BTInquiryViewController.h
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

#include <btstack/hci_cmds.h> // for HCI_STATE

typedef enum {
	kInquiryInactive,
	kInquiryActive,
	kInquiryRemoteName
} InquiryState;

@interface BTInquiryViewController : UITableViewController {
	NSMutableArray *devices; 
	HCI_STATE bluetoothState;
	InquiryState   inquiryState;
	UIActivityIndicatorView *deviceActivity;
	UIActivityIndicatorView *bluetoothActivity;
	UIFont * deviceNameFont;
	UIFont * macAddressFont;
}
- (void) setBluetoothState:(HCI_STATE)state;
- (void) setInquiryState:(InquiryState)state;
- (InquiryState) inquiryState;
- (HCI_STATE) bluetoothState;
@property (nonatomic, retain) NSMutableArray *devices;
@end

//
//  BTInquiryViewController.h
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

typedef enum  {
	kBluetoothStateUnknown,
	kBluetoothStateOn,
	kBluetoothStateOff,
	kBluetoothStateInitializing,
	kBluetoothStateStopping
} BluetoothState;

typedef enum {
	kInquiryInactive,
	kInquiryActive,
	kInquiryRemoteName
} InquiryState;

@interface BTInquiryViewController : UITableViewController {
	NSMutableArray *devices; 
	BluetoothState bluetoothState;
	InquiryState   inquiryState;
	UIActivityIndicatorView *deviceActivity;
}
@property (nonatomic, retain) NSMutableArray *devices;
@property (nonatomic, assign) BluetoothState bluetoothState;
@property (nonatomic, assign) InquiryState   inquiryState;
@end

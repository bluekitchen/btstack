//
//  BTDevice.m
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import "BTDevice.h"

@implementation BTDevice

@synthesize name;
@synthesize address;
@synthesize classOfDevice;
@synthesize connectionState;

- (BTDevice *)init {
	name = NULL;
	address = @"00:00:00:00:00:00";
	classOfDevice = kCODInvalid;
	connectionState = kBluetoothConnectionNotConnected;
	return self;
}

- (NSString *) nameOrAddress{
	if (name) return name;
	return address;
}

- (BluetoothDeviceType) deviceType{
	switch (classOfDevice) {
		case kCODHID:
			return kBluetoothDeviceTypeHID;
		case kCODZeeMote:
			return kBluetoothDeviceTypeZeeMote;
		default:
			return kBluetoothDeviceTypeGeneric;
	}
}

- (void)dealloc {
	[name release];
	[address release];
	[super dealloc];
}

@end

//
//  BTDevice.m
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import "BTDevice.h"

@implementation BTDevice

@synthesize name;
@synthesize classOfDevice;
@synthesize connectionState;
@synthesize pageScanRepetitionMode;
@synthesize clockOffset;

- (BTDevice *)init {
	name = NULL;
	bzero(&address, 6);
	classOfDevice = kCODInvalid;
	connectionState = kBluetoothConnectionNotConnected;
	return self;
}

- (void) setAddress:(bd_addr_t *)newAddr{
	BD_ADDR_COPY( &address, newAddr);
}

- (bd_addr_t *) address{
	return &address;
}

+ (NSString *) stringForAddress:(bd_addr_t *) address {
	uint8_t * addr = (uint8_t*) address;
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[4], addr[5], addr[6]];
}

- (NSString *) nameOrAddress{
	if (name) return name;
	return [BTDevice stringForAddress:&address];
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
- (NSString *) toString{
	return [NSString stringWithFormat:@"Device addr %@ name %@ COD %x", [BTDevice stringForAddress:&address], name, classOfDevice];
}

- (void)dealloc {
	[name release];
	[super dealloc];
}

@end

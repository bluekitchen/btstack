//
//  BTDevice.h
//  BT-Keyboard
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import <Foundation/Foundation.h>

#define kCODHID      0x2540
#define kCODZeeMote   0x584
#define kCODInvalid  0xffff

typedef enum {
	kBluetoothDeviceTypeGeneric = 0,
	kBluetoothDeviceTypeHID,
	kBluetoothDeviceTypeMobilePhone,
	kBluetoothDeviceTypeSmartPhone,
	kBluetoothDeviceTypeZeeMote,
} BluetoothDeviceType;

typedef enum {
	kBluetoothConnectionNotConnected = 0,
	kBluetoothConnectionRemoteName,
	kBluetoothConnectionConnecting,
	kBluetoothConnectionConnected
} BluetoothConnectionState;

@interface BTDevice : NSObject {
	NSString * address;
	NSString * name;
	uint32_t   classOfDevice;
	BluetoothConnectionState  connectionState;
}

@property (readonly)          BluetoothDeviceType deviceType;
@property (readonly)          NSString *          nameOrAddress;
@property (nonatomic, copy)   NSString *          address;
@property (nonatomic, copy)   NSString *          name;
@property (nonatomic, assign) uint32_t            classOfDevice;
@property (nonatomic, assign) BluetoothConnectionState connectionState;

@end

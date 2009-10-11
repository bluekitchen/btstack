//
//  BTDevice.h
//  BT-Keyboard
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import <Foundation/Foundation.h>
#include <btstack/utils.h>

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
	bd_addr_t address;
	NSString * name;
	uint8_t    pageScanRepetitionMode;
	uint16_t   clockOffset;
	uint32_t   classOfDevice;
	BluetoothConnectionState  connectionState;
}

- (void) setAddress:(bd_addr_t *)addr;
- (bd_addr_t *) address;
- (NSString *) toString;
+ (NSString *) stringForAddress:(bd_addr_t *) address;

@property (readonly)          BluetoothDeviceType deviceType;
@property (readonly)          NSString *          nameOrAddress;
@property (nonatomic, copy)   NSString *          name;
@property (nonatomic, assign) uint32_t            classOfDevice;
@property (nonatomic, assign) uint16_t            clockOffset;
@property (nonatomic, assign) uint8_t             pageScanRepetitionMode;
@property (nonatomic, assign) BluetoothConnectionState connectionState;

@end

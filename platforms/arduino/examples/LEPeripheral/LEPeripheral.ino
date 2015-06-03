// LE Peripheral Example - not working yet
#include <BTstack.h>
#include <SPI.h>

/*
 * EXAMPLE_START(LEPeripheral): LE Peripheral
 */

static char characteristic_data = 'H';

void setup(void){

    Serial.begin(9600);

    // set callbacks
    BTstack.setBLEDeviceConnectedCallback(deviceConnectedCallback);
    BTstack.setBLEDeviceDisconnectedCallback(deviceDisconnectedCallback);
    BTstack.setGATTCharacteristicRead(gattReadCallback);
    BTstack.setGATTCharacteristicWrite(gattWriteCallback);

    // setup GATT Database
    BTstack.addGATTService(new UUID("B8E06067-62AD-41BA-9231-206AE80AB551"));
    BTstack.addGATTCharacteristic(new UUID("f897177b-aee8-4767-8ecc-cc694fd5fcef"), ATT_PROPERTY_READ, "This is a String!");
    BTstack.addGATTCharacteristicDynamic(new UUID("f897177b-aee8-4767-8ecc-cc694fd5fce0"), ATT_PROPERTY_READ | ATT_PROPERTY_WRITE | ATT_PROPERTY_NOTIFY, 0);

    // use dummy address to force refresh on iOS devices
    bd_addr_t dummy = { 1,2,3,4,5,6};
    BTstack.setPublicBdAddr(dummy);

    // startup Bluetooth and activate advertisements
    BTstack.setup();
    BTstack.startAdvertising();
    // BTstack.enablePacketLogger();
}


void loop(void){
    BTstack.loop();
}

void deviceConnectedCallback(BLEStatus status, BLEDevice *device) {
    switch (status){
        case BLE_STATUS_OK:
            Serial.println("Device connected!");
            break;
        default:
            break;
    }
}

void deviceDisconnectedCallback(BLEDevice * device){
    Serial.println("Disconnected.");
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param characteristic_id to be read
// @param buffer 
// @param buffer_size
uint16_t gattReadCallback(uint16_t value_handle, uint8_t * buffer, uint16_t buffer_size){
    if (buffer){
        Serial.print("gattReadCallback, value: ");
        Serial.println(characteristic_data, HEX);
        buffer[0] = characteristic_data;
    }
    return 1;
}

// ATT Client Write Callback for Dynamic Data
// @param characteristic_id to be read
// @param buffer 
// @param buffer_size
// @returns 0 if write was ok, ATT_ERROR_INVALID_OFFSET if offset is larger than max buffer
int gattWriteCallback(uint16_t value_handle, uint8_t *buffer, uint16_t buffer_size){
    characteristic_data = buffer[0];
    Serial.print("gattWriteCallback , value ");
    Serial.println(characteristic_data, HEX);
    return 0;
}


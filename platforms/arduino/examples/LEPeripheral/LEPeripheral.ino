// LE Peripheral Example - not working yet
#include <BTstack.h>
#include <SPI.h>

static uint16_t value_handle;

void setup(void){

  setup_printf(9600);

  // set callbacks
  BTstack.setBLEDeviceConnectedCallback(deviceConnectedCallback);
  BTstack.setBLEDeviceDisconnectedCallback(deviceDisconnectedCallback);
  BTstack.setGATTCharacteristicRead(gattReadCallback);
  BTstack.setGATTCharacteristicWrite(gattWriteCallback);

  // setup GATT Database
  int flags = 0;
  uint8_t * data = NULL;
  uint16_t data_len = 0;
  BTstack.addGATTService(new UUID("B8E06067-62AD-41BA-9231-206AE80AB550"));
  value_handle = BTstack.addGATTCharacteristic(new UUID("f897177b-aee8-4767-8ecc-cc694fd5fcee"), flags, "This is a String!");

  // startup Bluetooth and activate advertisements
  BTstack.setup();
  BTstack.startAdvertising();
}


void loop(void){
  BTstack.loop();
}

void deviceConnectedCallback(BLEStatus status, BLEDevice *device) {
  switch (status){
    case BLE_STATUS_OK:
      printf("Device connected!\n");
      break;
    default:
      break;
  }
}

void deviceDisconnectedCallback(BLEDevice * device){
  printf("Disconnected.\n");
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param characteristic_id to be read
// @param buffer 
// @param buffer_size
uint16_t gattReadCallback(uint16_t value_handle, uint8_t * buffer, uint16_t buffer_size){
    printf("gattReadCallback, handle %u\n", value_handle);
    return 0;
}

// ATT Client Write Callback for Dynamic Data
// @param characteristic_id to be read
// @param buffer 
// @param buffer_size
// @returns 0 if write was ok, ATT_ERROR_INVALID_OFFSET if offset is larger than max buffer
int gattWriteCallback(uint16_t value_handle, uint8_t *buffer, uint16_t buffer_size){
    printf("gattWriteCallback, handle %u\n", value_handle);
    return 0;
}


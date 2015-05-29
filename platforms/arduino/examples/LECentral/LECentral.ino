
#include <BTstack.h>
#include <SPI.h>

// 
typedef struct characteristic_summary {
    UUID         uuid;
    const char * name;
    bool         found;
    BLECharacteristic characteristic;
} characteristic_summary_t;

typedef enum characteristicIDs {
    charRX = 0,
    charTX,
    charBaud,
    charBdAddr,
    numCharacteristics  /* last one */
} characteristicIDs_t;

characteristic_summary characteristics[] = {
    { UUID("f897177b-aee8-4767-8ecc-cc694fd5fcee"), "RX"       },
    { UUID("bf45e40a-de2a-4bc8-bba0-e5d6065f1b4b"), "TX"       },
    { UUID("2fbc0f31-726a-4014-b9fe-c8be0652e982"), "Baudrate" },
    { UUID("65c228da-bad1-4f41-b55f-3d177f4e2196"), "BD ADDR"  }
};

// BLE Shield Service V2 incl. used Characteristics
UUID bleShieldServiceV2UUID("B8E06067-62AD-41BA-9231-206AE80AB550");

// Application state
BLEDevice  myBLEDevice;
BLEService myBLEService;
bool serviceFound;
bool sendCounter = false;

int counter = 0;
char counterString[20];

static timer_source_t heartbeat;

void setup(void){

    Serial.begin(9600);

    BTstack.setBLEAdvertisementCallback(advertisementCallback);
    BTstack.setBLEDeviceConnectedCallback(deviceConnectedCallback);
    BTstack.setBLEDeviceDisconnectedCallback(deviceDisconnectedCallback);
    BTstack.setGATTServiceDiscoveredCallback(gattServiceDiscovered);
    BTstack.setGATTCharacteristicDiscoveredCallback(gattCharacteristicDiscovered);
    BTstack.setGATTCharacteristicNotificationCallback(gattCharacteristicNotification);
    BTstack.setGATTCharacteristicReadCallback(gattReadCallback);
    BTstack.setGATTCharacteristicWrittenCallback(gattWrittenCallback);
    BTstack.setGATTCharacteristicSubscribedCallback(gattSubscribedCallback);

    BTstack.setup();

    BTstack.bleStartScanning();
}


void loop(void){
    BTstack.loop();

    // send counter as fast as possible
    if (sendCounter){
        sprintf(counterString, "BTstack %u\n", counter);
        int result = myBLEDevice.writeCharacteristicWithoutResponse(&characteristics[charTX].characteristic, (uint8_t*) counterString, strlen(counterString) );
        if (result == BLE_PERIPHERAL_OK){
            Serial.print("Wrote without response: ");
            Serial.println(counterString);
            counter++;
        }
    }
}

void advertisementCallback(BLEAdvertisement *bleAdvertisement) {
    Serial.print("Device discovered: ");
    Serial.print(bleAdvertisement->getBdAddr()->getAddressString());
    Serial.print(", RSSI: %d");
    Serial.println(bleAdvertisement->getRssi());
    if (bleAdvertisement->containsService(&bleShieldServiceV2UUID)) {
        Serial.println("\nBLE ShieldService V2 found!\n");
        BTstack.bleStopScanning();
        BTstack.bleConnect(bleAdvertisement, 10000);  // 10 s
    }
}

void deviceConnectedCallback(BLEStatus status, BLEDevice *device) {
    switch (status){
        case BLE_STATUS_OK:
            Serial.println("Device connected!");
            myBLEDevice = *device;
            counter = 0;
            myBLEDevice.discoverGATTServices();
            break;
        case BLE_STATUS_CONNECTION_TIMEOUT:
            Serial.println("Error while Connecting the Peripheral");
            BTstack.bleStartScanning();
            break;
        default:
            break;
    }
}

void deviceDisconnectedCallback(BLEDevice * device){
    Serial.println("Disconnected, starting over..");
    sendCounter = false;
    BTstack.bleStartScanning();
}

void gattServiceDiscovered(BLEStatus status, BLEDevice *device, BLEService *bleService) {
    switch(status){
        case BLE_STATUS_OK:
            Serial.print("Service Discovered: :");
            Serial.println(bleService->getUUID()->getUuidString());
            if (bleService->matches(&bleShieldServiceV2UUID)) {
                serviceFound = true;
                Serial.println("Our service located!");
                myBLEService = *bleService;
            }
            break;
        case BLE_STATUS_DONE:
            Serial.println("Service discovery finished");
            if (serviceFound) {
                device->discoverCharacteristicsForService(&myBLEService);
            }
            break;
        default:
            Serial.println("Service discovery error");
            break;
    }
}

void gattCharacteristicDiscovered(BLEStatus status, BLEDevice *device, BLECharacteristic *characteristic) {
    switch(status){
        case BLE_STATUS_OK:
            Serial.print("Characteristic Discovered: ");
            Serial.print(characteristic->getUUID()->getUuidString());
            Serial.print(", handle 0x");
            Serial.println(characteristic->getCharacteristic()->value_handle, HEX);
            int i;
            for (i=0;i<numCharacteristics;i++){
                if (characteristic->matches(&characteristics[i].uuid)){
                    Serial.print("Characteristic found: ");
                    Serial.println(characteristics[i].name);
                    characteristics[i].found = 1;
                    characteristics[i].characteristic = *characteristic;
                    break;
                }
            }
            break;
        case BLE_STATUS_DONE:
            Serial.print("Characteristic discovery finished, status ");
            Serial.println(status, HEX);
            if (characteristics[charRX].found) {
                device->subscribeForNotifications(&characteristics[charRX].characteristic);
            }
            break;
        default:
            Serial.println("Characteristics discovery error");
            break;
    }
}

void gattSubscribedCallback(BLEStatus status, BLEDevice * device){
    device->readCharacteristic(&characteristics[charBdAddr].characteristic);
}

void gattReadCallback(BLEStatus status, BLEDevice *device, uint8_t *value, uint16_t length) {
    Serial.print("Read callback: ");
    Serial.println((const char *)value);
    device->writeCharacteristic(&characteristics[charTX].characteristic, (uint8_t*) "Hello!", 6);
}

void gattWrittenCallback(BLEStatus status, BLEDevice *device){
    sendCounter = true;
}

void gattCharacteristicNotification(BLEDevice *device, uint16_t value_handle, uint8_t *value, uint16_t length) {
    Serial.print("Notification: ");
    Serial.println((const char *)value);
}


#include <BTstack.h>
#include <SPI.h>

void setup(void){
    Serial.begin(9600);

    BTstack.setBLEAdvertisementCallback(advertisementCallback);
    BTstack.setup();
    BTstack.bleStartScanning();
}


void loop(void){
    BTstack.loop();
}

void advertisementCallback(BLEAdvertisement *adv) {
    if (adv->isIBeacon()) {
        Serial.print("iBeacon found ");
        Serial.print(adv->getBdAddr()->getAddressString());
        Serial.print(", RSSI ");
        Serial.print(adv->getRssi());
        Serial.print(", UUID ");
        Serial.print(adv->getIBeaconUUID()->getUuidString());
        Serial.print(", MajorID ");
        Serial.print(adv->getIBeaconMajorID());
        Serial.print(", MinorID ");
        Serial.print(adv->getIBecaonMinorID());
        Serial.print(", Measured Power ");
        Serial.println(adv->getiBeaconMeasuredPower());
    } else {
        Serial.print("Device discovered: ");
        Serial.print(adv->getBdAddr()->getAddressString());
        Serial.print(", RSSI ");
        Serial.println(adv->getRssi());
    }
}


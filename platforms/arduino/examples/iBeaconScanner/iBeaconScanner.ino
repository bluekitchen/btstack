
#include <BTstack.h>
#include <SPI.h>

void setup(void){
	setup_printf(9600);

	BTstack.setBLEAdvertisementCallback(advertisementCallback);
	BTstack.setup();
	BTstack.bleStartScanning();
}


void loop(void){
	BTstack.loop();
}

void advertisementCallback(BLEAdvertisement *adv) {
	if (adv->isIBeacon()) {
		printf("iBeacon found %s, RSSI %d, UUID %s, MajorID %u, MinorID %u, Measured Power %0x\n",
		adv->getBdAddr()->getAddressString(), adv->getRssi() ,
		adv->getIBeaconUUID()->getUuidString(), adv->getIBeaconMajorID(), adv->getIBecaonMinorID(),
		adv->getiBeaconMeasuredPower());

	} else {
		printf("Device discovered: %s, RSSI: %d\n", adv->getBdAddr()->getAddressString(), adv->getRssi() );
	}
}



#include <BTstack.h>
#include <SPI.h>

// setup printf
static FILE uartout = {0} ;
static int uart_putchar (char c, FILE *stream) {
    Serial.write(c);
    return 0;
}
static void setup_printf(int baud) {
  Serial.begin(baud);
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uartout;
}  

void setup() {
	setup_printf(9600);

	BTstack.setBLEAdvertisementCallback(advertisementCallback);
	BTstack.setup();
	BTstack.bleStartScanning();
}


void loop() {
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


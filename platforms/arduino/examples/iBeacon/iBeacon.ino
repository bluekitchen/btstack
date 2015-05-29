#include <BTstack.h>
#include <stdio.h>
#include <SPI.h>

UUID uuid("E2C56DB5-DFFB-48D2-B060-D0F5A71096E0");

void setup(void){
    Serial.begin(9600);
    BTstack.iBeaconConfigure(&uuid, 4711, 2);
    BTstack.setup();
    BTstack.startAdvertising();
}

void loop(void){
    BTstack.loop();
}

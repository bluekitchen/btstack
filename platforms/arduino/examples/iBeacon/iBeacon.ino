#include <BTstack.h>
#include <stdio.h>
#include <SPI.h>

UUID uuid("E2C56DB5-DFFB-48D2-B060-D0F5A71096E0");

// retarget printf
#ifdef ENERGIA
extern "C" {
int putchar(int c) {
    Serial.write((uint8_t)c);
    return c;
}
}
static void setup_printf(void) {
  Serial.begin(9600);
}
#else
static FILE uartout = {0} ;
static int uart_putchar (char c, FILE *stream) {
    Serial.write(c);
    return 0;
}
static void setup_printf(void) {
  Serial.begin(9600);
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uartout;
}  
#endif

void setup(void){
    setup_printf();
    BTstack.iBeaconConfigure(&uuid, 4711, 2);
    BTstack.setup();
    BTstack.startAdvertising();
}

void loop(void){
    BTstack.loop();
}

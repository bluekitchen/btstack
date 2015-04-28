#include <BTstack.h>
#include <stdio.h>
#include "att_server.h"
#include <SPI.h>

// EM9301 address 0C:F3:EE:00:00:00

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
  Serial.begin(115200);
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uartout;
}  
#endif

// test profile
#include "profile.h"

// write requests
static int att_write_callback(uint16_t con_handle, uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    printf("WRITE Callback, handle 0x%04x\n", handle);

    switch(handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            printf("New value: %u\n", buffer[0]);
#ifdef PIN_LED
            if (buffer[0]){
                digitalWrite(PIN_LED, HIGH); 
            } else {
                digitalWrite(PIN_LED, LOW); 
            }
#endif
            break;
    }
    return 0;
}

void setup(){

  setup_printf();

  printf("Main::Setup()\n");
  BT.setup();

  // set up ATT
  att_set_db(profile_data);
  att_set_write_callback(att_write_callback);
}

void loop(){
  BT.loop();
}

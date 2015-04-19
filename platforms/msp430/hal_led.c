#include <btstack/hal_led.h>
#include "hal_board.h"
#include <msp430.h>


void hal_led_toggle(void){
    LED2_OUT = LED2_OUT ^ LED2_PIN;
}
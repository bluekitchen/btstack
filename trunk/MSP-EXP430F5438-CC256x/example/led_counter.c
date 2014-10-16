// *****************************************************************************
//
// led_counter demo - uses the BTstack run loop to blink an LED
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include "btstack_memory.h"

#include <btstack/run_loop.h>
#include "btstack-config.h"

#define HEARTBEAT_PERIOD_MS 1000

static int counter = 0;
static timer_source_t heartbeat;
    
static void run_loop_register_timer(timer_source_t *timer, uint16_t period){
    run_loop_set_timer(timer, period);
    run_loop_add_timer(timer);
}

static void heartbeat_handler(timer_source_t *ts){
    // increment counter
    char lineBuffer[30];
    sprintf(lineBuffer, "BTstack counter %04u\n\r", ++counter);
    printf(lineBuffer);
    
    // toggle LED
    LED_PORT_OUT = LED_PORT_OUT ^ LED_2;

    // re-register timer
    run_loop_register_timer(ts, HEARTBEAT_PERIOD_MS);
} 

static void timer_setup(){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
}

static void hw_setup(){
    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1(); 
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // init debug UART
    halUsbInit();

    // init LEDs
    LED_PORT_OUT |= LED_1 | LED_2;
    LED_PORT_DIR |= LED_1 | LED_2;
}

static void btstack_setup(){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
}

// main
int main(void)
{
    hw_setup();
    btstack_setup();
	timer_setup();
    
	printf("Run...\n\r");

    // ready - enable irq used in h4 task
    __enable_interrupt();   

 	// turn on!
    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}


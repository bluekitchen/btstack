// *****************************************************************************
//
// led_counter demo - uses the BTstack run loop to blink an LED
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430.h>

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
    LED2_OUT = LED2_OUT ^ LED2_PIN;

    // re-register timer
    run_loop_register_timer(ts, HEARTBEAT_PERIOD_MS);
} 

static void timer_setup(){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

	printf("Run...\n\r");

	timer_setup();
    

    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}


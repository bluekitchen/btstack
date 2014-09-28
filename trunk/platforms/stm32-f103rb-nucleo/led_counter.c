//*****************************************************************************
//
// led_counter demo - uses the BTstack run loop to blink an LED
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_memory.h"

#include <btstack/run_loop.h>
#include "btstack-config.h"

#define HEARTBEAT_PERIOD_MS 1000

static int counter = 0;
static timer_source_t heartbeat;
    
static void register_timer(timer_source_t *timer, uint16_t period){
    run_loop_set_timer(timer, period);
    run_loop_add_timer(timer);
}

static void heartbeat_handler(timer_source_t *ts){
    // increment counter
    char lineBuffer[30];
    sprintf(lineBuffer, "BTstack counter %04u\n", ++counter);
    printf(lineBuffer);
    
    // toggle LED
    // LED_PORT_OUT = LED_PORT_OUT ^ LED_2;

    // re-register timer
    register_timer(ts, HEARTBEAT_PERIOD_MS);
} 

// main
int btstack_main(void)
{
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    
	printf("Run...\n\r");

    // go!
    run_loop_execute();	
    
    return 0;
}


/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

// *****************************************************************************
/* EXAMPLE_START(led_counter): Hello World: blinking LED without Bluetooth
 *
 * @text The example demonstrates how to provide a periodic timer to toggle an
 * LED and send debug messages to the console as a minimal BTstack test.
 */
// *****************************************************************************



#include <stdint.h>
#include <stdio.h>

#include <btstack/run_loop.h>
#include <btstack/hal_led.h>

#define HEARTBEAT_PERIOD_MS 1000

static int counter = 0;
static timer_source_t heartbeat;
    
/* @section Periodic Timer Setup 
 *
 * @text As timers in BTstack are single shot,
 * the periodic counter is implemented by re-registering the timer source in the
 * heartbeat handler callback function. Listing LEDToggler shows heartbeat handler
 * adapted to periodically toggle an LED and print number of toggles.  
 */ 

/* LISTING_START(LEDToggler): Periodic counter */  
static void heartbeat_handler(timer_source_t *ts){     
    // increment counter
    char lineBuffer[30];
    sprintf(lineBuffer, "BTstack counter %04u\n\r", ++counter);
    puts(lineBuffer);
    
    // toggle LED
    hal_led_toggle();

    // re-register timer
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);
} 
/* LISTING_END */

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It configures the heartbeat tier and adds it to the run loop.
 */
 
/* LISTING_START(MainConfiguration): Setup heartbeat timer */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);

    printf("Running...\n\r");
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
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
/* EXAMPLE_START(led_counter): UART and timer interrupt without Bluetooth
 *
 * @text The example uses the BTstack run loop to blink an LED.
 * The example demonstrates how to setup hardware, initialize BTstack without
 * Bluetooth, provide a periodic timer to toggle an LED and print number of
 * toggles as a minimal BTstack test.
 */
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
    printf(lineBuffer);
    
    // toggle LED
    LED_PORT_OUT = LED_PORT_OUT ^ LED_2;

    // re-register timer
    run_loop_register_timer(ts, HEARTBEAT_PERIOD_MS);
} 
/* LISTING_END */

/* @section Turn On and Go
 *
 * @text Listing MainConfiguration shows main application code.
 * It is called after hardware and BTstack configuration (memory, run loop and
 * transport layer) by the platform main in
 * \path{platforms/PLATFORM/src/main.c}.
 */

/* LISTING_START(MainConfiguration): Setup heartbeat timer */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    printf("Run...\n\r");
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
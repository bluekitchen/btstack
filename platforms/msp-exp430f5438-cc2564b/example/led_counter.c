
/* EXAMPLE_START(led_counter): UART and timer interrupt without Bluetooth
 * @brief The example uses the BTstack run loop to blink an LED. The example demonstrates how to setup hardware, initialize BTstack without Bluetooth, provide a periodic timer to toggle an LED and print number of toggles as a minimal BTstack test.
 */

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

/* SNIPPET_START(LEDToggler): Periodic counter
 * @brief As timers in BTstack are single shot, the periodic counter is implemented by re-registering the timer source in the heartbeat handler callback function. Listing LEDToggler shows heartbeat handler adapted to periodically toggle an LED and print number of toggles.
 */
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
/* SNIPPET_END(LEDToggler) */

/* SNIPPET_START(RunLoopExecution): Run loop execution
 * @brief Listing RunLoopExecution shows how to setup and start the run loop. For hardware and BTstack setup, please check the source code in \path{../src/main.c} 
 */
static void timer_setup(){
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

	timer_setup();
    
	printf("Run...\n\r");

    return 0;
}
/* SNIPPET_END(RunLoopExecution) */
/* EXAMPLE_END */
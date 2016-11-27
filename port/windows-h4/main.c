#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_windows.h"
#include "hci.h"
#include "hci_dump.h"
#include "hal_led.h"
// #include "btstack_link_key_db_fs.h"
// #include "stdin_support.h"

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

int main(int argc, char * argv[]){
	printf("BTstack on windows booting up\n");

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());

    while(1){
    	printf("time ms %u\n", btstack_run_loop_get_time_ms());
    }
	return 0;
}

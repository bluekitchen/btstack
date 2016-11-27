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

int btstack_main(int argc, const char * argv[]);

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void sigint_handler(int param){

#ifndef _WIN32
    // reset anyway
    btstack_stdin_reset();
#endif
    log_info(" <= SIGINT received, shutting down..\n");   
    // hci_power_control(HCI_POWER_OFF);
    // hci_close();
    log_info("Good bye, see you.\n");    
    exit(0);
}

int main(int argc, const char * argv[]){
	printf("BTstack on windows booting up\n");

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    // setup app
    btstack_main(argc, argv);

    // go
    btstack_run_loop_execute();    

	return 0;
}

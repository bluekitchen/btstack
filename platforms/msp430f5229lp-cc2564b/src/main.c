// *****************************************************************************
//
// main.c for msp430f5529-cc256x
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430.h>

#include "bt_control_cc256x.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include <btstack/run_loop.h>
#include <btstack/hal_tick.h>
#include <btstack/hal_cpu.h>

#include "hci.h"
#include "hci_dump.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "btstack-config.h"

static void hw_setup(){

    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1(); 
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // // init debug UART
    halUsbInit();
  
    hal_tick_init();

    /// init LEDs
    LED1_DIR |= LED1_PIN;   
    LED2_DIR |= LED2_PIN;        

    // ready - enable irq used in h4 task
    __enable_interrupt();  
}

static void btstack_setup(){

    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
    
    // init HCI
    hci_transport_t    * transport = hci_transport_h4_dma_instance();
    bt_control_t       * control   = bt_control_cc256x_instance();
    hci_uart_config_t  * config    = hci_uart_config_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    
    // use eHCILL
    // bt_control_cc256x_enable_ehcill(1);    
}

int btstack_main(int argc, const char * argv[]);

// main
int main(void){

    hw_setup();

    printf("Hardware setup done\n");

    btstack_setup();
    btstack_main(0, NULL);
    // happy compiler!
    return 0;
}


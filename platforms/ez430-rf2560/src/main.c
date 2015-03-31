// *****************************************************************************
//
// spp_counter demo - it provides a SPP and sends a counter every second
//
// it doesn't use the LCD to get down to a minimal memory footpring
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "bt_control_cc256x.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "btstack-config.h"

int btstack_main(int argc, const char * argv[]);

// main
int main(void)
{
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
    bt_control_cc256x_enable_ehcill(1);
    
    // ready - enable irq used in h4 task
    __enable_interrupt();   

    btstack_main(0, NULL);

    // go!
    run_loop_execute();

    return 0;
}


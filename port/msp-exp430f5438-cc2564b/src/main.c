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
//
// spp_counter demo - it provides an SPP and sends a counter every second
//
// it doesn't use the LCD to get down to a minimal memory footprint
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "btstack_chipset_cc256x.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include "btstack_run_loop.h"

#include "hci.h"
#include "btstack_memory.h"
#include "classic/remote_device_db.h"
#include "btstack_config.h"

static void hw_setup(void){
    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1(); 
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // init debug UART
    halUsbInit();

    // init LEDs
    LED1_OUT |= LED1_PIN;
    LED1_DIR |= LED1_PIN;
    LED2_OUT |= LED2_PIN;
    LED2_DIR |= LED2_PIN;

    // ready - enable irq used in h4 task
    __enable_interrupt();  
}

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    1000000,  // main baudrate
    1,        // flow control
    NULL,
};

static void btstack_setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
    
    // init HCI
    const hci_transport_t * transport = hci_transport_h4_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, &config, remote_db);
    hci_set_chipset(btstack_chipset_cc256x_instance());
    
    // use eHCILL
    btstack_chipset_cc256x_enable_ehcill(1);    
}

int btstack_main(int argc, const char * argv[]);

// main
int main(void){

    hw_setup();

    btstack_setup();
    btstack_main(0, NULL);

    btstack_run_loop_execute();

    return 0;
}


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

#include "btstack_chipset_cc256x.h"
#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "bluetooth_company_id.h"
#include "classic/btstack_link_key_db.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"
#include "hci.h"
#include "hci_cmd.h"

int btstack_main(int argc, const char * argv[]);

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    1000000,  // main baudrate
    1,        // flow control
    NULL,
};

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("BTstack up and running.\n");
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
                uint16_t manufacturer   = little_endian_read_16(packet, 10);
                uint16_t lmp_subversion = little_endian_read_16(packet, 12);
                // assert manufacturer is TI
                if (manufacturer != BLUETOOTH_COMPANY_ID_TEXAS_INSTRUMENTS_INC){
                    printf("ERROR: Expected Bluetooth Chipset from TI but got manufacturer 0x%04x\n", manufacturer);
                    break;
                }
                // assert correct init script is used based on expected lmp_subversion
                if (lmp_subversion != btstack_chipset_cc256x_lmp_subversion()){
                    printf("Error: LMP Subversion does not match initscript! ");
                    printf("Your initscripts is for %s chipset\n", btstack_chipset_cc256x_lmp_subversion() < lmp_subversion ? "an older" : "a newer");
                    printf("Please update Makefile to include the appropriate bluetooth_init_cc256???.c file\n");
                    break;
                }
            }
            break;
        default:
            break;
    }
}

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
    LED1_OUT |= LED1_PIN;
    LED1_DIR |= LED1_PIN;
    LED2_OUT |= LED2_PIN;
    LED2_DIR |= LED2_PIN;

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
	
    // init HCI
	hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), &config);
    hci_set_link_key_db(btstack_link_key_db_memory_instance());
    hci_set_chipset(btstack_chipset_cc256x_instance());
	
    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    // ready - enable irq used in h4 task
    __enable_interrupt();   

    btstack_main(0, NULL);

    // go!
    btstack_run_loop_execute();

    return 0;
}


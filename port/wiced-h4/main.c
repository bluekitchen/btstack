/*
 * Copyright (C) 2015 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "main.c"

#include "btstack.h"
#include "btstack_chipset_bcm.h"
#include "btstack_run_loop_wiced.h"
#include "btstack_link_key_db_wiced_dct.h"
#include "le_device_db_wiced_dct.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

#include "generated_mac_address.txt"

#include "platform_bluetooth.h"
#include "wiced.h"
#include "platform/wwd_platform_interface.h"

const btstack_uart_block_t * btstack_uart_block_wiced_instance(void);

// see generated_mac_address.txt - "macaddr=02:0A:F7:3d:76:be"
static const char * wifi_mac_address = NVRAM_GENERATED_MAC_ADDRESS;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static const hci_transport_config_uart_t hci_transport_config_uart = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    1000000,    // 200000+ didn't work reliably
    1,
    NULL,
};

extern int btstack_main(void);

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
    printf("BTstack up and running.\n");
}

void application_start(void){

    /* Initialise the WICED device without WLAN */
    wiced_core_init();
    
    /* 32 kHz clock also needed for Bluetooth */
    host_platform_init_wlan_powersave_clock();

    printf("BTstack on WICED\n");

#if 0
    // init GPIOs D0-D5 for debugging - not used
    wiced_gpio_init(D0, OUTPUT_PUSH_PULL);
    wiced_gpio_init(D1, OUTPUT_PUSH_PULL);
    wiced_gpio_init(D2, OUTPUT_PUSH_PULL);
    wiced_gpio_init(D3, OUTPUT_PUSH_PULL);
    wiced_gpio_init(D4, OUTPUT_PUSH_PULL);
    wiced_gpio_init(D5, OUTPUT_PUSH_PULL);

    wiced_gpio_output_low(D0);
    wiced_gpio_output_low(D1);
    wiced_gpio_output_low(D2);
    wiced_gpio_output_low(D3);
    wiced_gpio_output_low(D4);
    wiced_gpio_output_low(D5);
#endif

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_wiced_get_instance());
    
    // enable full log output while porting
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // setup le device db storage -- not needed if used LE-only (-> start address == 0)
    le_device_db_wiced_dct_set_start_address(btstack_link_key_db_wiced_dct_get_storage_size());
    le_device_db_dump();
    
    // init HCI
    const btstack_uart_block_t * uart_driver = btstack_uart_block_wiced_instance();
    const hci_transport_t * transport = hci_transport_h4_instance(uart_driver);
    hci_init(transport, (void*) &hci_transport_config_uart);
    hci_set_link_key_db(btstack_link_key_db_wiced_dct_instance());
    hci_set_chipset(btstack_chipset_bcm_instance());

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // use WIFI Mac address + 1 for Bluetooth
    bd_addr_t dummy = { 1,2,3,4,5,6};
    sscanf_bd_addr(&wifi_mac_address[8], dummy);
    dummy[5]++;
    hci_set_bd_addr(dummy);
    
    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};
}

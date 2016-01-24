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

#include "btstack.h"
#include "btstack_control_bcm.h"
#include "btstack_run_loop_wiced.h"

#include "generated_mac_address.txt"

#include "platform_bluetooth.h"
#include "wiced.h"


// see generated_mac_address.txt - "macaddr=02:0A:F7:3d:76:be"
static const char * wifi_mac_address = NVRAM_GENERATED_MAC_ADDRESS;

static const hci_transport_config_uart_t hci_transport_config_uart = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    3000000,
    1,
    NULL,
};

extern int btstack_main(void);

static int main_sscan_bd_addr(const char * addr_string, bd_addr_t addr){
    unsigned int bd_addr_buffer[BD_ADDR_LEN];  //for sscanf, integer needed
    // reset result buffer
    memset(bd_addr_buffer, 0, sizeof(bd_addr_buffer));
    
    // parse
    int result = sscanf(addr_string, "%2x:%2x:%2x:%2x:%2x:%2x", &bd_addr_buffer[0], &bd_addr_buffer[1], &bd_addr_buffer[2],
                        &bd_addr_buffer[3], &bd_addr_buffer[4], &bd_addr_buffer[5]);

    if (result != BD_ADDR_LEN) return 0;

    // store
    int i;
    for (i = 0; i < BD_ADDR_LEN; i++) {
        addr[i] = (uint8_t) bd_addr_buffer[i];
    }
    return 1;
}

void application_start(void){

    /* Initialise the WICED device */
    wiced_init();

    printf("BTstack on WICED\n");

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_wiced_get_instance());
    
    // enable full log output while porting
    // hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // init HCI
    const hci_transport_t * transport = hci_transport_h4_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, (void*) &hci_transport_config_uart, remote_db);
    hci_set_chipset(btstack_chipset_bcm_instance());

    // use WIFI Mac address + 1 for Bluetooth
    bd_addr_t dummy = { 1,2,3,4,5,6};
    main_sscan_bd_addr(&wifi_mac_address[8], dummy);
    dummy[5]++;
    hci_set_bd_addr(dummy);
    
    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};
}

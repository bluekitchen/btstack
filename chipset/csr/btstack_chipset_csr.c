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

/*
 *  btstack_chipset_csr.c
 *
 *  Adapter to use CSR-based chipsets with BTstack
 *  SCO over HCI doesn't work over H4 connection and BTM805 module from Microchip Bluetooth Audio Developer Kit (CSR8811)
 */

#include "btstack_chipset_csr.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

#include "btstack_control.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hci_transport.h"

// minimal CSR init script to configure PSKEYs and activate them
static const uint8_t init_script[] = { 

    //  Set ANA_Freq to 26MHz
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xfe, 0x01, 0x01, 0x00, 0x00, 0x00, 0x90, 0x65,

    //  Set HCI_NOP_DISABLE
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf2, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,

#ifdef HAVE_SCO_OVER_HCI
    // Set HOSTIO_MAP_SCO_PCM to 0
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xab, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Set HOSTIO_MAP_SCO_CODEC to 0
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x00, 0x00, 0x09, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0xb0, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Set ENABLE_SCO_STREAMS to 0
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xc9, 0x22, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
#endif

    //  Set UART baudrate to 115200
    0x01, 0x00, 0xFC, 0x15, 0xc2, 0x02, 0x00, 0x0a, 0x00, 0x02, 0x00, 0x03, 0x70, 0x00, 0x00, 0xea, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xc2,

    //  WarmReset
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x03, 0x0e, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint16_t init_script_size = sizeof(init_script);

//
static uint32_t init_script_offset  = 0;
static hci_transport_config_uart_t * hci_transport_config_uart = NULL;

static void chipset_init(const void * config){
    init_script_offset = 0;
    hci_transport_config_uart = NULL;
    // check for hci_transport_config_uart_t
    if (!config) return;
    if (((hci_transport_config_t*)config)->type != HCI_TRANSPORT_CONFIG_UART) return;
    hci_transport_config_uart = (hci_transport_config_uart_t*) config;
}

// set requested baud rate
static void update_init_script_command(uint8_t *hci_cmd_buffer){
    uint16_t varid = READ_BT_16(hci_cmd_buffer, 10);
    if (varid != 0x7003) return;
    uint16_t key = READ_BT_16(hci_cmd_buffer, 14);
    log_info("csr: pskey 0x%04x", key);
    if (key != 0x01ea) return;

    // check for baud rate
    if (!hci_transport_config_uart) return;
    uint32_t baudrate = hci_transport_config_uart->baudrate_main;
    if (baudrate == 0){
        baudrate = hci_transport_config_uart->baudrate_init;
    }
    // uint32_t is stored as 2 x uint16_t with most important 16 bits first
    bt_store_16(hci_cmd_buffer, 20, baudrate >> 16);
    bt_store_16(hci_cmd_buffer, 22, baudrate &  0xffff);
}

static btstack_chipset_result_t chipset_next_command(uint8_t * hci_cmd_buffer){

    if (init_script_offset >= init_script_size) {
        return BTSTACK_CHIPSET_DONE;
    }

    // init script is stored with the HCI Command Packet Type
    init_script_offset++;
    // copy command header
    memcpy(&hci_cmd_buffer[0], (uint8_t *) &init_script[init_script_offset], 3); 
    init_script_offset += 3;
    int payload_len = hci_cmd_buffer[2];
    // copy command payload
    memcpy(&hci_cmd_buffer[3], (uint8_t *) &init_script[init_script_offset], payload_len);

    // support for on-the-fly configuration updates
    update_init_script_command(hci_cmd_buffer);

    init_script_offset += payload_len;

    // support for warm boot command
    uint16_t varid = READ_BT_16(hci_cmd_buffer, 10);
    if (varid == 0x4002){
        return BTSTACK_CHIPSET_WARMSTART_REQUIRED;
    }

    return BTSTACK_CHIPSET_VALID_COMMAND; 
}


static const btstack_chipset_t btstack_chipset_bcm = {
    "CSR",
    chipset_init,
    chipset_next_command,
    NULL, // chipset_set_baudrate_command,
    NULL, // chipset_set_bd_addr_command not supported or implemented
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_csr_instance(void){
    return &btstack_chipset_bcm;
}

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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "btstack_chipset_csr.c"

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

enum update_result {
    OK,
    SKIP,
};

// minimal CSR init script to configure PSKEYs and activate them. It uses store 0x0008 = psram.
static const uint8_t init_script[] = { 

    // 0x01fe: Set ANA_Freq to 26MHz
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xfe, 0x01, 0x01, 0x00, 0x08, 0x00, 0x90, 0x65,

    // 0x00f2: Set HCI_NOP_DISABLE
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xf2, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,

#ifdef ENABLE_SCO_OVER_HCI
    // 0x01ab: Set HOSTIO_MAP_SCO_PCM to 0
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xab, 0x01, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    // 0x01b0: Set HOSTIO_MAP_SCO_CODEC to 0
    0x00, 0xFC, 0x13, 0xc2, 0x00, 0x00, 0x09, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0xb0, 0x01, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    // 0x22c9: Set ENABLE_SCO_STREAMS to 0
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xc9, 0x22, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
#endif

    // 0x01bf: Enable RTS/CTS for BCSP (0806 -> 080e)
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00, 0x03, 0x70, 0x00, 0x00, 0xbf, 0x01, 0x01, 0x00, 0x08, 0x00, 0x0e, 0x08,

    // 0x01ea: Set UART baudrate to 115200
    0x00, 0xFC, 0x15, 0xc2, 0x02, 0x00, 0x0a, 0x00, 0x02, 0x00, 0x03, 0x70, 0x00, 0x00, 0xea, 0x01, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0xc2,

    // 0x0001: Set Bluetooth address 
    0x00, 0xFC, 0x19, 0xc2, 0x02, 0x00, 0x0A, 0x00, 0x03, 0x00, 0x03, 0x70, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x08, 0x00, 0xf3, 0x00, 0xf5, 0xf4, 0xf2, 0x00, 0xf2, 0xf1,

    //  WarmReset
    0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x03, 0x0e, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};
static const uint16_t init_script_size = sizeof(init_script);

//
static uint32_t init_script_offset  = 0;
static hci_transport_config_uart_t * hci_transport_config_uart = NULL;
static int       set_bd_addr;
static bd_addr_t bd_addr;

static void chipset_init(const void * config){
    init_script_offset = 0;
    hci_transport_config_uart = NULL;
    // check for hci_transport_config_uart_t
    if (!config) return;
    if (((hci_transport_config_t*)config)->type != HCI_TRANSPORT_CONFIG_UART) return;
    hci_transport_config_uart = (hci_transport_config_uart_t*) config;
}

// set requested baud rate
// @return 0 = OK, 1 = SKIP, 2 = ..
static enum update_result update_init_script_command(uint8_t *hci_cmd_buffer){
    uint16_t varid = little_endian_read_16(hci_cmd_buffer, 10);
    if (varid != 0x7003) return OK;
    uint16_t key = little_endian_read_16(hci_cmd_buffer, 14);
    log_info("csr: pskey 0x%04x", key);
    switch (key){
        case 0x001:
            // set bd addr command
            if (!set_bd_addr) return SKIP;
            hci_cmd_buffer[20] = bd_addr[3];
            hci_cmd_buffer[22] = bd_addr[5];
            hci_cmd_buffer[23] = bd_addr[4];
            hci_cmd_buffer[24] = bd_addr[2];
            hci_cmd_buffer[26] = bd_addr[1];
            hci_cmd_buffer[27] = bd_addr[0];
            return OK;
        case 0x01ea:
            // baud rate command
            if (!hci_transport_config_uart) return OK;
            uint32_t baudrate = hci_transport_config_uart->baudrate_main;
            if (baudrate == 0){
                baudrate = hci_transport_config_uart->baudrate_init;
            }
            // uint32_t is stored as 2 x uint16_t with most important 16 bits first
            little_endian_store_16(hci_cmd_buffer, 20, baudrate >> 16);
            little_endian_store_16(hci_cmd_buffer, 22, baudrate &  0xffff);
            break;
        default:
            break;        
    }
    return OK;
}

static btstack_chipset_result_t chipset_next_command(uint8_t * hci_cmd_buffer){

    while (true){

        if (init_script_offset >= init_script_size) {
            return BTSTACK_CHIPSET_DONE;
        }

        // copy command header
        memcpy(&hci_cmd_buffer[0], (uint8_t *) &init_script[init_script_offset], 3); 
        init_script_offset += 3;
        int payload_len = hci_cmd_buffer[2];
        // copy command payload
        memcpy(&hci_cmd_buffer[3], (uint8_t *) &init_script[init_script_offset], payload_len);

        // support for on-the-fly configuration updates
        enum update_result res = update_init_script_command(hci_cmd_buffer);
        init_script_offset += payload_len;

        // support for skipping commands
        if (res == SKIP) continue;

        // support for warm boot command
        uint16_t varid = little_endian_read_16(hci_cmd_buffer, 10);
        if (varid == 0x4002){
            return BTSTACK_CHIPSET_WARMSTART_REQUIRED;
        }

        return BTSTACK_CHIPSET_VALID_COMMAND;         
    }
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

/** 
 * Set BD ADDR before HCI POWER_ON
 * @param bd_addr
 */
void btstack_chipset_csr_set_bd_addr(bd_addr_t new_bd_addr){
    set_bd_addr = 1;
    memcpy(bd_addr, new_bd_addr, 6);
}

/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_chipset_zephyr.c"

/*
 *  btstack_chipset_zephyr.c
 *
 *  Adapter to use CSR-based chipsets with BTstack
 *  SCO over HCI doesn't work over H4 connection and BTM805 module from Microchip Bluetooth Audio Developer Kit (CSR8811)
 */

#include "btstack_chipset_zephyr.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

#include "btstack_control.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hci_transport.h"

// minimal Zpehyr init script to read static random address
static const uint8_t init_script[] = { 
    0x09, 0xfc, 0x00,
};
static const uint16_t init_script_size = sizeof(init_script);

//
static uint32_t init_script_offset  = 0;

static void chipset_init(const void * config){
    UNUSED(config);
    init_script_offset = 0;
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

        return BTSTACK_CHIPSET_VALID_COMMAND;         
    }
}


static const btstack_chipset_t btstack_chipset_zephyr = {
    "Zephyr",
    chipset_init,
    chipset_next_command,
    NULL, // chipset_set_baudrate_command,
    NULL, // chipset_set_bd_addr_command not supported or implemented
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_zephyr_instance(void){
    return &btstack_chipset_zephyr;
}

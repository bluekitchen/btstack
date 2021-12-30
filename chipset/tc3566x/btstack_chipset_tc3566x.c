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

#define BTSTACK_FILE__ "btstack_chipset_tc3566x.c"

/*
 *  btstack_chipset_tc3566x.c
 *
 *  Adapter to use Toshiba TC3566x-based chipsets with BTstack
 *  
 *  Supports:
 *  - Set BD ADDR
 *  - Set baud rate
 */

#include "btstack_config.h"
#include "btstack_chipset_tc3566x.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */
#include "hci.h"
#include "btstack_debug.h"

// should go to some common place
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

static const uint8_t baudrate_command[] = { 0x08, 0xfc, 0x11, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x14, 0x42, 0xff, 0x10, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static void chipset_set_baudrate_command(uint32_t baudrate, uint8_t *hci_cmd_buffer){
    uint16_t div1 = 0;
    uint8_t  div2 = 0;
    switch (baudrate) {
        case 115200:
            div1 = 0x001A;
            div2 = 0x60;
            break;
        case 230400:
            div1 = 0x000D;
            div2 = 0x60;
            break;
        case 460800:
            div1 = 0x0005;
            div2 = 0xA0;
            break;
        case 921600:
            div1 = 0x0003;
            div2 = 0x70;
            break;
        default:
            log_error("tc3566x_baudrate_cmd baudrate %u not supported", baudrate);
            return;
    }

    memcpy(hci_cmd_buffer, baudrate_command, sizeof(baudrate_command));
    little_endian_store_16(hci_cmd_buffer, 13, div1);
    hci_cmd_buffer[15] = div2;
}

static void chipset_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    // OGF 0x04 - Informational Parameters, OCF 0x10
    hci_cmd_buffer[0] = 0x13;
    hci_cmd_buffer[1] = 0x10;
    hci_cmd_buffer[2] = 0x06;
    reverse_bd_addr(addr, &hci_cmd_buffer[3]);
}

static const btstack_chipset_t btstack_chipset_tc3566x = {
    "tc3566x",
    NULL, // chipset_init,
    NULL, // chipset_next_command,
    chipset_set_baudrate_command,
    chipset_set_bd_addr_command,
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_tc3566x_instance(void){
    return &btstack_chipset_tc3566x;
}

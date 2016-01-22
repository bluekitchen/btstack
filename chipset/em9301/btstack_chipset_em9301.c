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
 *  btstack_chipset_em9301.c
 *
 *  Adapter to use em9301-based chipsets with BTstack
 *  
 *  Allows to set public BD ADDR
 */

#include "btstack_config.h"
#include "btstack_chipset_em9301.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */
#include "hci.h"

// should go to some common place
#define OPCODE(ogf, ocf) (ocf | ogf << 10)


static void chipset_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    bt_store_16(hci_cmd_buffer, 0, OPCODE(OGF_VENDOR, 0x02));
    hci_cmd_buffer[2] = 0x06;
    bt_flip_addr(&hci_cmd_buffer[3], addr);
}

static const btstack_chipset_t btstack_chipset_em9301 = {
    "EM9301",
    NULL, // chipset_init not used
    NULL, // chipset_next_command not used
    NULL, // chipset_set_baudrate_command not needed as we're connected via SPI
    chipset_set_bd_addr_command,
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_em9301_instance(void){
    return &btstack_chipset_em9301;
}

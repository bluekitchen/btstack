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
 *  bt_control_stlc2500d.c
 *
 *  Adapter to use stlc2500d-based chipsets with BTstack
 *  
 */

#include "btstack-config.h"
#include "bt_control_stlc2500d.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */
#include "hci.h"
#include "debug.h"

// should go to some common place
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

static int stlc2500d_baudrate_cmd(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer){
	// map baud rate to predefined settings
	int preset = 0;
	switch (baudrate){
		case 57600:
			preset = 0x0e;
			break;
        case 115200:
        	preset = 0x10;
        	break;
        case 230400:
        	preset = 0x12;
        	break;
        case 460800:
        	preset = 0x13;
        	break;
        case 921600:
        	preset = 0x14;
        	break;
        case 1843200:
        	preset = 0x16;
        	break;
        case 2000000:
        	preset = 0x19;
        	break;
        case 3000000:
        	preset = 0x1b;
        	break;
        case 4000000:
        	preset = 0x1f;
        	break;
        default:
        	log_error("stlc2500d_baudrate_cmd baudrate %u not supported", baudrate);
        	return 1;
    }
    bt_store_16(hci_cmd_buffer, 0, OPCODE(OGF_VENDOR, 0xfc));
    hci_cmd_buffer[2] = 0x01;
    hci_cmd_buffer[3] = preset;
    return 0;
}

static int stlc2500d_set_bd_addr_cmd(void * config, bd_addr_t addr, uint8_t *hci_cmd_buffer){
    bt_store_16(hci_cmd_buffer, 0, OPCODE(OGF_VENDOR, 0x22));
    hci_cmd_buffer[2] = 0x08;
    hci_cmd_buffer[3] = 254;
    hci_cmd_buffer[4] = 0x06;
    bt_flip_addr(&hci_cmd_buffer[5], addr);
    return 0;
}

// MARK: const structs 
static const bt_control_t bt_control_stlc2500d = {
	NULL,                                  // on
	NULL,                                  // off
	NULL,                                  // sleep
	NULL,                                  // wake
	NULL,                                  // valid
	NULL,                                  // name
	stlc2500d_baudrate_cmd,                // baudrate_cmd
	NULL,                                  // next_cmd
	NULL,                                  // register_for_power_notifications
    NULL,                                  // hw_error
    stlc2500d_set_bd_addr_cmd,             // set_bd_addr_cmd
};

// MARK: public API
bt_control_t *bt_control_stlc2500d_instance(void){
    return (bt_control_t*) &bt_control_stlc2500d;
}

/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  bt_control_csr.c
 *
 *  Adapter to use CSR-based chipsets with BTstack
 *  
 */

#include "bt_control_csr.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

#include "bt_control.h"

// minimal CSR init script
static const uint8_t init_script[] = { 
    //  BCCMD set ANA_Freq PSKEY to 26MHz
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x1e, 0x00, 0x03, 0x70, 0x00, 0x00, 0xfe, 0x01, 0x01, 0x00, 0x00, 0x00, 0x90, 0x65,
    //  BCCMD WarmReset
    0x01, 0x00, 0xFC, 0x13, 0xc2, 0x02, 0x00, 0x09, 0x00, 0x4d, 0x0e, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint16_t init_script_size = sizeof(init_script);

//
static uint32_t init_script_offset  = 0;
static int16_t  init_power_in_dB    = 13; // 13 dBm

static int bt_control_csr_on(void *config){
	init_script_offset = 0;
	return 0;
}

// UART Baud Rate control from: http://e2e.ti.com/support/low_power_rf/f/660/p/134850/484763.aspx
// static int csr_baudrate_cmd(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer){
//     hci_cmd_buffer[0] = 0x36;
//     hci_cmd_buffer[1] = 0xFF;
//     hci_cmd_buffer[2] = 0x04;
//     hci_cmd_buffer[3] =  baudrate        & 0xff;
//     hci_cmd_buffer[4] = (baudrate >>  8) & 0xff;
//     hci_cmd_buffer[5] = (baudrate >> 16) & 0xff;
//     hci_cmd_buffer[6] = 0;
//     return 0;
// }

static void bt_control_csr_update_command(uint8_t *hci_cmd_buffer){
}

static int bt_control_csr_next_cmd(void *config, uint8_t *hci_cmd_buffer){

    if (init_script_offset >= init_script_size) {
        return 0;
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
    bt_control_csr_update_command(hci_cmd_buffer);

    init_script_offset += payload_len;

    return 1; 
}

// MARK: const structs 

static const bt_control_t bt_control_csr = {
	bt_control_csr_on,                  // on
	NULL,                               // off
	NULL,                               // sleep
	NULL,                               // wake
	NULL,                               // valid
	NULL,                               // name
	NULL, /* csr_baudrate_cmd, */       // baudrate_cmd
	bt_control_csr_next_cmd,            // next_cmd
	NULL,                               // register_for_power_notifications
    NULL,                               // hw_error
};

static const hci_uart_config_t hci_uart_config_csr = {
    NULL,
    115200,
    0,  // 1000000,
    0
};

// MARK: public API
void bt_control_csr_set_power(int16_t power_in_dB){
    init_power_in_dB = power_in_dB;
}

bt_control_t *bt_control_csr_instance(void){
    return (bt_control_t*) &bt_control_csr;
}

hci_uart_config_t *hci_uart_config_csr_instance(void){
    return (hci_uart_config_t*) &hci_uart_config_csr;
}

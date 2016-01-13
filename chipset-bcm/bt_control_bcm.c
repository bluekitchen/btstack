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
 *  bt_control_bcm.c
 *
 *  Adapter to use Broadcom-based chipsets with BTstack
 */


#include "btstack-config.h"
#include "bt_control_bcm.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

#include "bt_control.h"
#include "debug.h"

// actual init script provided by separate bt_firmware_image.c from WICED SDK
extern const uint8_t brcm_patchram_buf[];
extern const int     brcm_patch_ram_length;
extern const char    brcm_patch_version[];

//
static uint32_t init_script_offset;
static int send_download_command;

static int bt_control_bcm_on(void *config){
    log_info("Broadcom init script %s, len %u", brcm_patch_version, brcm_patch_ram_length);
    init_script_offset = 0;
    send_download_command = 1;
    return 0;
}

// @note: Broadcom chips require higher UART clock for baud rate > 3000000 -> limit baud rate in hci.c
static int bcm_baudrate_cmd(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x18;
    hci_cmd_buffer[1] = 0xfc;
    hci_cmd_buffer[2] = 0x06;
    hci_cmd_buffer[3] = 0x00;
    hci_cmd_buffer[4] = 0x00;
    bt_store_32(hci_cmd_buffer, 5, baudrate);
    return 0;
}

// @note: bd addr has to be set after sending init script (it might just get re-set)
static int bt_control_bcm_set_bd_addr_cmd(void * config, bd_addr_t addr, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x01;
    hci_cmd_buffer[1] = 0xfc;
    hci_cmd_buffer[2] = 0x06;
    bt_flip_addr(&hci_cmd_buffer[3], addr);
    return 0;
}

static int bt_control_bcm_next_cmd(void *config, uint8_t *hci_cmd_buffer){

    // send download firmware command
    if (send_download_command){
        send_download_command = 0;
        hci_cmd_buffer[0] = 0x2e;
        hci_cmd_buffer[1] = 0xfc;
        hci_cmd_buffer[2] = 0x00;
        return 1;
    }

    if (init_script_offset >= brcm_patch_ram_length) {
        return 0;
    }

    // use memcpy with pointer
    int cmd_len = 3 + brcm_patchram_buf[init_script_offset+2];
    memcpy(&hci_cmd_buffer[0], &brcm_patchram_buf[init_script_offset], cmd_len); 
    init_script_offset += cmd_len;
    return 1; 
}

// MARK: const structs 

static const bt_control_t bt_control_bcm = {
    bt_control_bcm_on,                     // on
    NULL,                                  // off
    NULL,                                  // sleep
    NULL,                                  // wake
    NULL,                                  // valid
    NULL,                                  // name
    bcm_baudrate_cmd,                      // baudrate_cmd
    bt_control_bcm_next_cmd,               // next_cmd
    NULL,                                  // register_for_power_notifications
    NULL,                                  // hw_error
    bt_control_bcm_set_bd_addr_cmd,        // set_bd_addr_cmd
};

// MARK: public API
bt_control_t * bt_control_bcm_instance(void){
    return (bt_control_t*) &bt_control_bcm;
}

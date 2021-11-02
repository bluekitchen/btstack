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

#define BTSTACK_FILE__ "btstack_chipset_bcm_download_firmware.c"

// download firmware implementation
// requires hci_dump
// supports higher baudrate for patch upload

#include <string.h> 

#include "hci_dump.h"
#include "btstack_chipset_bcm.h"
#include "btstack_chipset_bcm_download_firmware.h"
#include "bluetooth.h"
#include "btstack_debug.h"
#include "btstack_chipset.h"

static void bcm_send_hci_baudrate(void);
static void bcm_send_next_init_script_command(void);
static void bcm_set_local_baudrate(void);
static void bcm_w4_command_complete(void);

static const btstack_uart_block_t * uart_driver;
static const btstack_chipset_t * chipset;

static uint8_t response_buffer[260];
static uint8_t command_buffer[260];

static const int hci_command_complete_len = 7;
static const uint8_t hci_reset_cmd[] = { 0x03, 0x0c, 0x00 };
// static const uint8_t hci_update_baud_rate[] = { 0x01, 0x18, 0xfc, 0x06, 0x00, 0x00,0x00, 0x00, 0x00, 0x00 };
static void (*download_complete)(int result);
static int baudrate;

static void bcm_send_prepared_command(void){
    uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
    int size = 1 + 3 + command_buffer[3];
    command_buffer[0] = 1;
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, &command_buffer[1], size-1);
    uart_driver->send_block(command_buffer, size);
}

static void bcm_send_hci_reset(void){
    if (baudrate == 0 || baudrate == 115200){
        uart_driver->set_block_received(&bcm_w4_command_complete);
    } else {
        uart_driver->set_block_received(&bcm_send_hci_baudrate);
    }
    log_info("bcm: send HCI Reset");
    uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
    memcpy(&command_buffer[1], hci_reset_cmd, sizeof(hci_reset_cmd));
    bcm_send_prepared_command();
}

static void bcm_send_hci_baudrate(void){
    hci_dump_packet(HCI_EVENT_PACKET, 0, &response_buffer[1], hci_command_complete_len-1);
    chipset->set_baudrate_command(baudrate, &command_buffer[1]);
    uart_driver->set_block_received(&bcm_set_local_baudrate);
    uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
    log_info("bcm: send baud rate command");
    bcm_send_prepared_command();
}

static void bcm_set_local_baudrate(void){
    hci_dump_packet(HCI_EVENT_PACKET, 0, &response_buffer[1], hci_command_complete_len-1);
    uart_driver->set_baudrate(baudrate);
    uart_driver->set_block_received(&bcm_w4_command_complete);
    bcm_send_next_init_script_command();
}

static void bcm_w4_command_complete(void){
    hci_dump_packet(HCI_EVENT_PACKET, 0, &response_buffer[1], hci_command_complete_len-1);
    bcm_send_next_init_script_command();
}

static void bcm_send_next_init_script_command(void){
    int res = chipset->next_command(&command_buffer[1]);
    switch (res){
        case BTSTACK_CHIPSET_VALID_COMMAND:
            bcm_send_prepared_command();
            break;
        case BTSTACK_CHIPSET_DONE:
            log_info("bcm: init script done");
            // disable init script for main startup
            btstack_chipset_bcm_enable_init_script(0);
            // reset baudreate to default
            uart_driver->set_baudrate(115200);
            // notify main
            download_complete(0);
            break;
        default:
            break;
    } 
}

/**
 * @brief Download firmware via uart_driver
 * @param uart_driver -- already initialized
 * @param done callback. 0 = Success
 */

void btstack_chipset_bcm_download_firmware_with_uart(const btstack_uart_t * the_uart_driver, int baudrate_upload, void (*done)(int result)){
    // 
    uart_driver = the_uart_driver;
    chipset     = btstack_chipset_bcm_instance();
    baudrate    = baudrate_upload; 
    download_complete = done;
    btstack_chipset_bcm_enable_init_script(1);

    int res = uart_driver->open();
    if (res) {
        log_error("uart_block init failed %u", res);
        download_complete(res);
        return;
    }

    bcm_send_hci_reset();
}
void btstack_chipset_bcm_download_firmware(const btstack_uart_block_t * the_uart_driver, int baudrate_upload, void (*done)(int result)) {
    btstack_chipset_bcm_download_firmware_with_uart((const btstack_uart_t *) the_uart_driver, baudrate_upload, done);
}

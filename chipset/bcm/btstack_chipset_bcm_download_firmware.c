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
#include <stdio.h>
#include <unistd.h>

#include "hci_dump.h"
#include "btstack_chipset_bcm.h"
#include "btstack_chipset_bcm_download_firmware.h"
#include "bluetooth.h"
#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_chipset.h"

static void bcm_send_hci_baudrate(void);
static void bcm_send_next_init_script_command(void);
static void bcm_set_local_baudrate(void);
static void bcm_w4_command_complete(void);

static const btstack_uart_block_t * uart_driver;
static const btstack_chipset_t * chipset;

static uint8_t response_buffer[260];
static uint16_t response_buffer_len;
static uint8_t command_buffer[260];

static const int hci_command_complete_len = 7;
static const int hci_command_complete_read_local_version = 15;
static const uint8_t hci_read_local_version_cmd[] = { 0x01, 0x10, 0x00};
static const uint8_t hci_reset_cmd[] =              { 0x03, 0x0c, 0x00 };
static const uint8_t hci_command_complete_reset[] = { 0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};

static void (*download_complete)(int result);

static uint32_t baudrate;

static inline void bcm_hci_dump_event(void){
    uint16_t event_len = 2 + response_buffer[2];
    hci_dump_packet(HCI_EVENT_PACKET, 0, &response_buffer[1], event_len);
}

static void bcm_send_prepared_command(void){
    command_buffer[0] = 1;
    uint16_t command_len = 3 + command_buffer[3];
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, &command_buffer[1], command_len);
    uart_driver->send_block(command_buffer, command_len + 1);
}

// select controller based on { manufacturer / lmp_subversion }
static void bcm_detect_controller(uint16_t manufacturer,
                                  uint16_t lmp_subversion) {
    const char * device_name = NULL;
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_INFINEON_TECHNOLOGIES_AG:
            switch (lmp_subversion){
                case 0x2257:
                    // CYW5557x
                    device_name = "CYW55560A1";
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (device_name == NULL){
        printf("Unknown device, please update bcm_detect_controller()\n");
        printf("in btstack/chipset/bcm/btstack_chipset_bcm_download_firmware.c\n");
    } else {
        printf("Controller: %s\n", device_name);
        btstack_chipset_bcm_set_device_name(device_name);
    }
}

// Send / Receive HCI Read Local Version Information

static void bcm_receive_command_command_complete_read_local_version(void){
    const uint8_t * packet = &response_buffer[1];
    printf("ROM version information:\n");
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    printf("- HCI Version    0x%04x\n", hci_version);
    printf("- HCI Revision   0x%04x\n", hci_revision);
    printf("- LMP Version    0x%04x\n", lmp_version);
    printf("- LMP Subversion 0x%04x\n", lmp_subversion);
    printf("- Manufacturer 0x%04x\n", manufacturer);

    bcm_detect_controller(manufacturer, lmp_subversion);

    bcm_w4_command_complete();
}

static void bcm_send_read_local_version(void){
    log_info("bcm: send HCI Read Local Version Information");
    uart_driver->set_block_received(&bcm_receive_command_command_complete_read_local_version);
    uart_driver->receive_block(response_buffer, hci_command_complete_read_local_version);
    memcpy(&command_buffer[1], hci_read_local_version_cmd, sizeof(hci_read_local_version_cmd));
    bcm_send_prepared_command();
}

// Send / Receive HCI Reset

// Although the Controller just has been reset by the user, there might still be HCI data in the UART driver
// which we'll ignore in the receive function

static void bcm_receive_command_complete_reset(void){
    response_buffer_len++;
    if (response_buffer_len == hci_command_complete_len){
        // try to match command complete for HCI Reset
        if (memcmp(response_buffer, hci_command_complete_reset, hci_command_complete_len) == 0){
            bcm_hci_dump_event();
            bcm_send_read_local_version();
            return;
        }
        memmove(&response_buffer[0], &response_buffer[1], response_buffer_len - 1);
        response_buffer_len--;
    }
    uart_driver->receive_block(&response_buffer[response_buffer_len], 1);
}

static void bcm_send_hci_reset(void){
    log_info("bcm: send HCI Reset");
    response_buffer_len = 0;
    uart_driver->set_block_received(&bcm_receive_command_complete_reset);
    uart_driver->receive_block(&response_buffer[response_buffer_len], 1);
    memcpy(&command_buffer[1], hci_reset_cmd, sizeof(hci_reset_cmd));
    bcm_send_prepared_command();
}

// Other

static void bcm_send_hci_baudrate(void){
    bcm_hci_dump_event();
    chipset->set_baudrate_command(baudrate, &command_buffer[1]);
    uart_driver->set_block_received(&bcm_set_local_baudrate);
    uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
    log_info("bcm: send baud rate command - %u", baudrate);
    bcm_send_prepared_command();
}

static void bcm_set_local_baudrate(void){
    bcm_hci_dump_event();
    uart_driver->set_baudrate(baudrate);
    uart_driver->set_block_received(&bcm_w4_command_complete);
    bcm_send_next_init_script_command();
}

static void bcm_w4_command_complete(void){
    bcm_hci_dump_event();
    uart_driver->set_block_received(&bcm_w4_command_complete);
    bcm_send_next_init_script_command();
}

static void bcm_send_next_init_script_command(void){
    int res = chipset->next_command(&command_buffer[1]);
    switch (res){
        case BTSTACK_CHIPSET_VALID_COMMAND:
            uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
            bcm_send_prepared_command();
            break;
        case BTSTACK_CHIPSET_DONE:
            log_info("bcm: init script done");
            // disable init script for main startup
            btstack_chipset_bcm_enable_init_script(0);
            // reset baudrate to default
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

    // Reset with CTS asserted (low)
    printf("Please reset Bluetooth Controller, e.g. via RESET button. Firmware download starts in:\n");
    uint8_t i;
    for (i = 3; i > 0; i--){
        printf("%u\n", i);
        sleep(1);
    }
    printf("Firmware download started\n");

    bcm_send_hci_reset();
}

void btstack_chipset_bcm_download_firmware(const btstack_uart_block_t * the_uart_driver, int baudrate_upload, void (*done)(int result)) {
    btstack_chipset_bcm_download_firmware_with_uart((const btstack_uart_t *) the_uart_driver, baudrate_upload, done);
}

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
 *  btstack_chipset_atwilc3000.c
 *
 *  Adapter to use atwilc3000-based chipsets with BTstack
 *  
 */

#define __BTSTACK_FILE__ "btstack_chipset_atwilc3000.c"

#include "btstack_config.h"
#include "btstack_chipset_atwilc3000.h"
#include "btstack_debug.h"


#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */
#include "hci.h"

#define IRAM_START 0x80000000

// HCI commands
static const uint8_t hci_reset_command[] = { 0x01, 0x03, 0x0c, 0x00 };
static const uint8_t hci_read_local_version_information_command[] = { 0x01, 0x01, 0x10, 0x00 };
static const uint8_t hci_vendor_specific_reset_command[] = { 0x01, 0x55, 0xfc, 0x00 };

// prototypes
static void atwilc3000_w4_command_complete_reset(void);
static void atwilc3000_w4_command_complete_read_local_version_information(void);
static void atwilc3000_write_memory(void);
static void atwilc3000_vendor_specific_reset(void);
static void atwilc3000_done(void);
static void atwilc3000_update_uart_params(void);
static void atwilc3000_w4_baudrate_update(void);

// globals
static void (*download_complete)(int result);
static const btstack_uart_block_t * the_uart_driver;

static int     download_count;
static uint8_t event_buffer[15];
static uint8_t command_buffer[260];
static const uint8_t * fw_data;
static uint32_t        fw_size;
static uint32_t        fw_offset;
static uint32_t        fw_baudrate;

static void atwilc3000_send_command(const uint8_t * data, uint16_t len){
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, (uint8_t *) &data[1], len - 1);
    the_uart_driver->send_block(data, len);
}

static void atwilc3000_log_event(void){
    int len = event_buffer[2] + 2;
    hci_dump_packet(HCI_EVENT_PACKET, 1, &event_buffer[1], len);
}

static void atwilc3000_start(void){
    // send HCI Reset
    the_uart_driver->set_block_received(&atwilc3000_w4_command_complete_reset);
    the_uart_driver->receive_block(&event_buffer[0], 7);
    atwilc3000_send_command(&hci_reset_command[0], sizeof(hci_reset_command));
    log_info("atwilc3000_start: wait for command complete for HCI Reset");
}

static void atwilc3000_w4_command_complete_reset(void){
    atwilc3000_log_event();
    log_info("command complete Reset");
    // static uint8_t hci_event_command_complete_reset[] = { 0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x0c };
    // TODO: check if correct event

    // send HCI Read Local Version Information
    the_uart_driver->receive_block(&event_buffer[0], 15);
    the_uart_driver->set_block_received(&atwilc3000_w4_command_complete_read_local_version_information);
    atwilc3000_send_command(&hci_read_local_version_information_command[0], sizeof(hci_read_local_version_information_command));
    log_info("atwilc3000_start: wait for command complete for HCI Read Local Version Information");
}

static void atwilc3000_w4_command_complete_read_local_version_information(void){
    atwilc3000_log_event();
    log_info("command complete Read Local Version Information");
    uint8_t firmware_version = event_buffer[7];
    log_info("Firmware version 0x%02x", firmware_version);
    if (firmware_version != 0xff){
        log_info("Firmware already loaded, download complete");
        download_complete(0);
        return;
    }
    log_info("Running from ROM, start firmware download");
    if (fw_baudrate){
        atwilc3000_update_uart_params();
    } else {
        atwilc3000_write_memory();
    }
}

static void atwilc3000_update_uart_params(void){
    command_buffer[0] = 1;
    command_buffer[1] = 0x53;
    command_buffer[2] = 0xfc;
    command_buffer[3] = 5;
    little_endian_store_32(command_buffer, 4, fw_baudrate);
    command_buffer[8] = 0;  // No flow control
    the_uart_driver->receive_block(&event_buffer[0], 7);
    the_uart_driver->set_block_received(&atwilc3000_w4_baudrate_update);
    atwilc3000_send_command(&command_buffer[0], 9);
}

static void atwilc3000_w4_baudrate_update(void){
    atwilc3000_log_event();
    the_uart_driver->set_baudrate(fw_baudrate);
    atwilc3000_write_memory();
}


static void atwilc3000_write_memory(void){
    atwilc3000_log_event();

    // done?
    if (fw_offset >= fw_size){
        log_info("DONE!!!");
        atwilc3000_vendor_specific_reset();
        return;
    }
    // bytes to write
    log_info("Write pos %u", fw_offset);
    uint16_t bytes_to_write = btstack_min((fw_size - fw_offset), (255-8));
    // setup write command
    command_buffer[0] = 1;
    command_buffer[1] = 0x52;
    command_buffer[2] = 0xfc;
    command_buffer[3] = 8; // NOTE: this is in violation of the Bluetooth Specification, but documented in the Atmel-NNNNN-ATWIL_Linux_Porting_Guide
    little_endian_store_32(command_buffer, 4, IRAM_START + fw_offset);
    little_endian_store_32(command_buffer, 8, bytes_to_write);
    memcpy(&command_buffer[12], &fw_data[fw_offset], bytes_to_write);
    //
    fw_offset += bytes_to_write;

    // send write command
    the_uart_driver->receive_block(&event_buffer[0], 7);
    the_uart_driver->set_block_received(&atwilc3000_write_memory);
    atwilc3000_send_command(&command_buffer[0], 12 + bytes_to_write);
}

static void atwilc3000_vendor_specific_reset(void){
    // send HCI Vendor Specific Reset
    // the_uart_driver->receive_block(&event_buffer[0], 7);
    // the_uart_driver->set_block_received(&atwilc3000_done);
    the_uart_driver->set_block_sent(&atwilc3000_done);
    atwilc3000_send_command(&hci_vendor_specific_reset_command[0], sizeof(hci_vendor_specific_reset_command));
}

static void atwilc3000_done(void){
    log_info("done");
    // reset baud rate
    if (fw_baudrate){
        the_uart_driver->set_baudrate(115200);
    }
    download_complete(0);
}

void btstack_chipset_atwilc3000_download_firmware(const btstack_uart_block_t * uart_driver, uint32_t baudrate, const uint8_t * da_fw_data, uint32_t da_fw_size, void (*done)(int result)){

	the_uart_driver   = uart_driver;
    download_complete = done;
    fw_data = da_fw_data;
    fw_size = da_fw_size;
    fw_offset = 0;
    fw_baudrate = baudrate;

    int res = the_uart_driver->open();

    if (res) {
    	log_error("uart_block init failed %u", res);
    	download_complete(res);
    }

    download_count = 0;
    atwilc3000_start();
}

// not used currently

static const btstack_chipset_t btstack_chipset_atwilc3000 = {
    "atwilc3000",
    NULL, // chipset_init not used
    NULL, // chipset_next_command not used
    NULL, // chipset_set_baudrate_command not needed as we're connected via SPI
    NULL, // chipset_set_bd_addr not provided
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_atwilc3000_instance(void){
    return &btstack_chipset_atwilc3000;
}


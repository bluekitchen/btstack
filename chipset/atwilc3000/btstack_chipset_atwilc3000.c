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

#define BTSTACK_FILE__ "btstack_chipset_atwilc3000.c"

/*
 *  btstack_chipset_atwilc3000.c
 *
 *  Adapter to use atwilc3000-based chipsets with BTstack
 *  
 */

#include "btstack_config.h"

#include <stddef.h>   /* NULL */
#include <stdio.h>
#include <string.h>   /* memcpy */

#include "btstack_chipset_atwilc3000.h"
#include "btstack_debug.h"
#include "hci.h"

// assert outgoing and incoming hci packet buffers can hold max hci command resp. event packet
#if HCI_OUTGOING_PACKET_BUFFER_SIZE < (HCI_CMD_HEADER_SIZE + 255)
#error "HCI_OUTGOING_PACKET_BUFFER_SIZE to small. Outgoing HCI packet buffer to small for largest HCI Command packet. Please set HCI_ACL_PAYLOAD_SIZE to 258 or higher."
#endif
#if HCI_INCOMING_PACKET_BUFFER_SIZE < (HCI_EVENT_HEADER_SIZE + 255)
#error "HCI_INCOMING_PACKET_BUFFER_SIZE to small. Incoming HCI packet buffer to small for largest HCI Event packet. Please set HCI_ACL_PAYLOAD_SIZE to 257 or higher."
#endif

// Address to load firmware
#define IRAM_START 0x80000000

// Larger blocks (e.g. 8192) cause hang
#define FIRMWARE_CHUNK_SIZE 4096

// works with 200 ms, so use 250 ms to stay on the safe side
#define ATWILC3000_RESET_TIME_MS 250

// HCI commands used for firmware upload
static const uint8_t hci_reset_command[] = { 0x01, 0x03, 0x0c, 0x00 };
static const uint8_t hci_read_local_version_information_command[] = { 0x01, 0x01, 0x10, 0x00 };
static const uint8_t hci_vendor_specific_reset_command[] = { 0x01, 0x55, 0xfc, 0x00 };

// prototypes
static void atwilc3000_configure_uart(btstack_timer_source_t * ts);
static void atwilc3000_done(void);
static void atwilc3000_update_uart_params(void);
static void atwilc3000_vendor_specific_reset(void);
static void atwilc3000_w4_baudrate_update(void);
static void atwilc3000_w4_command_complete_read_local_version_information(void);
static void atwilc3000_w4_command_complete_reset(void);
static void atwilc3000_wait_for_reset_completed(void);
static void atwilc3000_write_firmware(void);
static void atwilc3000_write_memory(void);

// globals
static void (*download_complete)(int result);
static const btstack_uart_t * the_uart_driver;
static btstack_timer_source_t reset_timer;

static int     download_count;
static uint8_t event_buffer[15];
static uint8_t command_buffer[12];
static const uint8_t * fw_data;
static uint32_t        fw_size;
static uint32_t        fw_offset;

// baudrate for firmware upload
static uint32_t        fw_baudrate;

// flow control requested
static int             fw_flowcontrol;

// flow control active
static int             atwilc3000_flowcontrol;

static void atwilc3000_set_baudrate_command(uint32_t baudrate, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x53;
    hci_cmd_buffer[1] = 0xfc;
    hci_cmd_buffer[2] = 5;
    little_endian_store_32(hci_cmd_buffer, 3, baudrate);
    hci_cmd_buffer[7] = atwilc3000_flowcontrol;    // use global state
}

static void atwilc3000_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x54;
    hci_cmd_buffer[1] = 0xFC;
    hci_cmd_buffer[2] = 0x06;
    reverse_bd_addr(addr, &hci_cmd_buffer[3]);
}

static void atwilc3000_send_command(const uint8_t * data, uint16_t len){
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, (uint8_t *) &data[1], len - 1);
    the_uart_driver->send_block(data, len);
}

static void atwilc3000_log_event(void){
    int len = event_buffer[2] + 2;
    hci_dump_packet(HCI_EVENT_PACKET, 1, &event_buffer[1], len);
}

static void atwilc3000_start(void){
    // default after power up
    atwilc3000_flowcontrol = 0;

    // send HCI Reset
    the_uart_driver->set_block_received(&atwilc3000_w4_command_complete_reset);
    the_uart_driver->receive_block(&event_buffer[0], 7);
    atwilc3000_send_command(&hci_reset_command[0], sizeof(hci_reset_command));
}

static void atwilc3000_w4_command_complete_reset(void){
    atwilc3000_log_event();
    // send HCI Read Local Version Information
    the_uart_driver->receive_block(&event_buffer[0], 15);
    the_uart_driver->set_block_received(&atwilc3000_w4_command_complete_read_local_version_information);
    atwilc3000_send_command(&hci_read_local_version_information_command[0], sizeof(hci_read_local_version_information_command));
}

static void atwilc3000_w4_command_complete_read_local_version_information(void){
    atwilc3000_log_event();
    uint8_t firmware_version = event_buffer[7];
    if (firmware_version != 0xff){
        log_info("Firmware version 0x%02x already loaded, download complete", firmware_version);
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
    atwilc3000_set_baudrate_command(fw_baudrate, &command_buffer[1]);
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
        log_info("Firmware upload complete!!!");
        atwilc3000_vendor_specific_reset();
        return;
    }

    // bytes to write
    log_info("Write pos %u", (int) fw_offset);
    uint16_t bytes_to_write = btstack_min((fw_size - fw_offset), FIRMWARE_CHUNK_SIZE);
    // setup write command
    command_buffer[0] = 1;
    command_buffer[1] = 0x52;
    command_buffer[2] = 0xfc;
    command_buffer[3] = 8; // NOTE: this is in violation of the Bluetooth Specification, but documented in the Atmel-NNNNN-ATWIL_Linux_Porting_Guide
    little_endian_store_32(command_buffer, 4, IRAM_START + fw_offset);
    little_endian_store_32(command_buffer, 8, bytes_to_write);

    // send write command - only log write command without the firmware blob
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, (uint8_t *) &command_buffer[1], 12 - 1);
    the_uart_driver->set_block_sent(&atwilc3000_write_firmware);
    the_uart_driver->send_block(&command_buffer[0], 12);
}

static void atwilc3000_write_firmware(void){
    the_uart_driver->set_block_received(&atwilc3000_write_memory);
    the_uart_driver->receive_block(&event_buffer[0], 7);

    uint16_t bytes_to_write = btstack_min((fw_size - fw_offset), FIRMWARE_CHUNK_SIZE);

    uint32_t offset = fw_offset;
    fw_offset += bytes_to_write;

    the_uart_driver->set_block_sent(NULL);
    the_uart_driver->send_block(&fw_data[offset], bytes_to_write);
}

static void atwilc3000_vendor_specific_reset(void){
    log_info("Trigger MCU reboot and wait ");
    // send HCI Vendor Specific Reset
    the_uart_driver->set_block_sent(&atwilc3000_wait_for_reset_completed);
    atwilc3000_send_command(&hci_vendor_specific_reset_command[0], sizeof(hci_vendor_specific_reset_command));
}

static void atwilc3000_wait_for_reset_completed(void){
    the_uart_driver->set_block_sent(NULL);
    btstack_run_loop_set_timer_handler(&reset_timer, &atwilc3000_configure_uart);
    btstack_run_loop_set_timer(&reset_timer, ATWILC3000_RESET_TIME_MS);
    btstack_run_loop_add_timer(&reset_timer);
}

static void atwilc3000_configure_uart(btstack_timer_source_t * ts){
    // reset baud rate if higher baud rate was requested before
    if (fw_baudrate){
        the_uart_driver->set_baudrate(HCI_DEFAULT_BAUDRATE);
    }
    // send baudrate command to enable flow control (if supported) and/or higher baud rate
    if ((fw_flowcontrol && the_uart_driver->set_flowcontrol) || (fw_baudrate != HCI_DEFAULT_BAUDRATE)){
        log_info("Send baudrate command (%u) to enable flow control", (int) fw_baudrate);
        atwilc3000_flowcontrol = fw_flowcontrol;
        command_buffer[0] = 1;
        atwilc3000_set_baudrate_command(fw_baudrate, &command_buffer[1]);
        the_uart_driver->set_block_received(&atwilc3000_done);
        the_uart_driver->receive_block(&event_buffer[0], 7);
        atwilc3000_send_command(&command_buffer[0], 9);
    } else {
        atwilc3000_done();
    }
}

static void atwilc3000_done(void){
    atwilc3000_log_event();
    // enable our flow control
    if (atwilc3000_flowcontrol){
        the_uart_driver->set_flowcontrol(atwilc3000_flowcontrol);
    }
    if (fw_baudrate){
        the_uart_driver->set_baudrate(fw_baudrate);
    }

    // done
    download_complete(0);
}

void btstack_chipset_atwilc3000_download_firmware_with_uart(const btstack_uart_t * uart_driver, uint32_t baudrate, int flowcontrol, const uint8_t * da_fw_data, uint32_t da_fw_size, void (*done)(int result)){
	the_uart_driver   = uart_driver;
    download_complete = done;
    fw_data = da_fw_data;
    fw_size = da_fw_size;
    fw_offset = 0;
    fw_baudrate = baudrate;
    fw_flowcontrol = flowcontrol;

    int res = the_uart_driver->open();
    if (res) {
    	log_error("uart_block init failed %u", res);
    	download_complete(res);
        return;
    }

    download_count = 0;
    atwilc3000_start();
}

void btstack_chipset_atwilc3000_download_firmware(const btstack_uart_block_t * uart_driver, uint32_t baudrate, int flowcontrol, const uint8_t * da_fw_data, uint32_t da_fw_size, void (*done)(int result)){
    btstack_chipset_atwilc3000_download_firmware_with_uart((const btstack_uart_t *) uart_driver, baudrate, flowcontrol, da_fw_data, da_fw_size, done);
}

static const btstack_chipset_t btstack_chipset_atwilc3000 = {
    "atwilc3000",
    NULL, // chipset_init not used
    NULL, // chipset_next_command not used
    atwilc3000_set_baudrate_command,
    atwilc3000_set_bd_addr_command,
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_atwilc3000_instance(void){
    return &btstack_chipset_atwilc3000;
}


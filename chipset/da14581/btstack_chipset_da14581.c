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

#define BTSTACK_FILE__ "btstack_chipset_da14581.c"

/*
 *  btstack_chipset_da14581.c
 *
 *  Adapter to use da1458x-based chipsets with BTstack
 *  
 */

#include "btstack_config.h"
#include "btstack_chipset_da14581.h"
#include "btstack_debug.h"


#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */
#include "hci.h"

// Firmware download protocol contstants
#define SOH         0x01
#define STX         0x02
#define ACK         0x06
#define NACK        0x15
#define CRC_INIT    0x00

// prototypes
static void da14581_w4_stx(void);
static void da14581_w4_command_sent(void);
static void da14581_w4_fw_sent(void);
static void da14581_w4_ack(void);
static void da14581_w4_crc(void);
static void da14581_w4_final_ack_sent(void);

// globals
static void (*download_complete)(int result);
static const btstack_uart_block_t * the_uart_driver;

static int     download_count;
static uint8_t response_buffer[1];
static uint8_t command_buffer[3];
static const uint8_t * fw_data;
static uint16_t  fw_size;

static void da14581_start(void){
    // start to read
    the_uart_driver->set_block_received(&da14581_w4_stx);
    the_uart_driver->receive_block(&response_buffer[0], 1);
    log_info("da14581_start: wait for 0x%02x", STX);
}

static void da14581_w4_stx(void){
    log_debug("da14581_w4_stx: read %x", response_buffer[0]);
    switch (response_buffer[0]){
        case STX:
            log_info("da14581_w4_stx: send download command");
            // setup download config message
            command_buffer[0] = SOH;
            little_endian_store_16(command_buffer, 1, fw_size);
            the_uart_driver->set_block_sent(da14581_w4_command_sent);        
            the_uart_driver->send_block(command_buffer, 3);
            break;
        default:
            // read again
            the_uart_driver->receive_block(&response_buffer[0], 1);
            break;
    }
}

static void da14581_w4_command_sent(void){
    log_info("da14581_w4_command_sent: wait for ACK 0x%02x", ACK);
    // write complete
    the_uart_driver->set_block_received(&da14581_w4_ack);
    the_uart_driver->receive_block(&response_buffer[0], 1);
}

static void da14581_w4_ack(void){
    log_info("da14581_w4_ack: read %x", response_buffer[0]);
    switch (response_buffer[0]){
        case ACK:
            // calc crc
            // send file
            log_info("da14581_w4_ack: ACK received, send firmware");
            the_uart_driver->set_block_sent(da14581_w4_fw_sent);        
            the_uart_driver->send_block(fw_data, fw_size);
            break;
        case NACK:
            // denied
            the_uart_driver->close();
            download_complete(1);
            break;
        default:
            download_count++;
            if (download_count < 10){
                // something else went wrong try again
                da14581_start();
            } else {
                // give up
                the_uart_driver->close();
                download_complete(1);
            }
            break;
    }
}

static void da14581_w4_fw_sent(void){
    // write complete
    log_info("da14581_w4_fw_sent: wait for crc");
    the_uart_driver->set_block_received(&da14581_w4_crc);
    the_uart_driver->receive_block(&response_buffer[0], 1);
}

static void da14581_w4_crc(void){
    log_info("da14581_w4_crc: read %x\n", response_buffer[0]);

    // calculate crc
    int i;
    uint8_t fcrc = CRC_INIT;
    for (i = 0; i < fw_size; i++){
        fcrc ^= fw_data[i];
    }

    // check crc
    if (fcrc != response_buffer[0]){
        log_error("da14581_w4_crc: got 0x%02x expected 0x%02x", response_buffer[0], fcrc);
        download_complete(1);
        return;
    }

    // everything's fine, send final ack
    command_buffer[0] = ACK;
    the_uart_driver->set_block_sent(&da14581_w4_final_ack_sent);
    the_uart_driver->send_block(command_buffer, 1);
}

static void da14581_w4_final_ack_sent(void){
    download_complete(0);
}

void btstack_chipset_da14581_download_firmware_with_uart(const btstack_uart_t * uart_driver, const uint8_t * da_fw_data, uint16_t da_fw_size, void (*done)(int result)){

	the_uart_driver   = uart_driver;
    download_complete = done;
    fw_data = da_fw_data;
    fw_size = da_fw_size;

    int res = the_uart_driver->open();

    if (res) {
    	log_error("uart_block init failed %u", res);
    	download_complete(res);
    }

    download_count = 0;
    da14581_start();
}

void btstack_chipset_da14581_download_firmware(const btstack_uart_block_t * uart_driver, const uint8_t * fw, uint16_t fw_size, void (*done)(int result)){
    btstack_chipset_da14581_download_firmware_with_uart(uart_driver, fw, fw_size, done);
}

// not used currently

static const btstack_chipset_t btstack_chipset_da14581 = {
    "DA14581",
    NULL, // chipset_init not used
    NULL, // chipset_next_command not used
    NULL, // chipset_set_baudrate_command not needed as we're connected via SPI
    NULL, // chipset_set_bd_addr not provided
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_da14581_instance(void){
    return &btstack_chipset_da14581;
}


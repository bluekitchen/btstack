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

#define __BTSTACK_FILE__ "main.c"

// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_link_key_db_fs.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "bluetooth_company_id.h"
#include "hci.h"
#include "hci_dump.h"
#include "stdin_support.h"

#include "btstack_chipset_bcm.h"

int btstack_main(int argc, const char * argv[]);


static hci_transport_config_uart_t transport_config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,  // main baudrate
    0,  // flow control
    NULL,
};
static btstack_uart_config_t uart_config;

static int main_argc;
static const char ** main_argv;
static const btstack_uart_block_t * uart_driver;
static const btstack_chipset_t * chipset;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void sigint_handler(int param){
    UNUSED(param);

    printf("CTRL-C - SIGINT received, shutting down..\n");   
    log_info("sigint_handler: shutting down");

    // reset anyway
    btstack_stdin_reset();

    // power down
    hci_power_control(HCI_POWER_OFF);
    hci_close();
    log_info("Good bye, see you.\n");    
    exit(0);
}

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            gap_local_bd_addr(addr);
            printf("BTstack up and running at %s\n",  bd_addr_to_str(addr));
            break;
        default:
            break;
    }
}

// download firmware implementation
#include <unistd.h>

static uint8_t response_buffer[260];
static uint8_t command_buffer[260];

static const int hci_command_complete_len = 7;
static const uint8_t hci_reset_cmd[] = { 0x01, 0x03, 0x0c, 0x00 };
// static const uint8_t hci_update_baud_rate[] = { 0x01, 0x18, 0xfc, 0x06, 0x00, 0x00,0x00, 0x00, 0x00, 0x00 };
static void (*download_complete)(int result);

static void bcm_w4_hci_command_complete(void){
    log_info("bcm: response");
    log_info_hexdump(response_buffer, hci_command_complete_len);

    int res = chipset->next_command(&command_buffer[1]);
    int size; 
    switch (res){
        case BTSTACK_CHIPSET_VALID_COMMAND:
            size = 1 + 3 + command_buffer[3];
            command_buffer[0] = 1;
            log_info("bcm: h4 command, len %u", size-1);
            log_info_hexdump(&command_buffer[1], size-1);
            uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);
            uart_driver->send_block(command_buffer, size);
            break;
        case BTSTACK_CHIPSET_DONE:
            log_info("bcm: init script done");
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

void btstack_chipset_bcm_download_firmware(const btstack_uart_block_t * the_uart_driver, void (*done)(int result)){
    // 
    uart_driver = the_uart_driver;
    download_complete = done;

    int res = uart_driver->open();
    if (res) {
        log_error("uart_block init failed %u", res);
        // download_complete(res);
        return;
    }

    // TODO: move to bstack_uart_block_posix.c
    sleep(1);

    uart_driver->set_block_received(&bcm_w4_hci_command_complete);
    uart_driver->receive_block(&response_buffer[0], hci_command_complete_len);

    log_info("bcm: send HCI Reset");
    uart_driver->send_block(hci_reset_cmd, sizeof(hci_reset_cmd));
}
// end of download firmware

static void phase2(int status);
int main(int argc, const char * argv[]){

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_STDOUT);

    // pick serial port and configure uart block driver
    transport_config.device_name = "/dev/tty.usbserial-A9OVNX5P"; // RedBear IoT pHAT breakout board

    // get BCM chipset driver
    chipset = btstack_chipset_bcm_instance();
    chipset->init(&transport_config);

    // set chipset name
    btstack_chipset_bcm_set_device_name("BCM43430A1");

    // setup UART driver
    uart_driver = btstack_uart_block_posix_instance();

    // extract UART config from transport config, but overide initial uart speed
    uart_config.baudrate    = 115200;
    uart_config.flowcontrol = transport_config.flowcontrol;
    uart_config.device_name = transport_config.device_name;
    uart_driver->init(&uart_config);

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    main_argc = argc;
    main_argv = argv;

    // phase #1 download firmware
    printf("Phase 1: Download firmware\n");

    // phase #2 start main app
    btstack_chipset_bcm_download_firmware(uart_driver, &phase2);

    // go
    btstack_run_loop_execute();    
    return 0;
}

static void phase2(int status){

    if (status){
        printf("Download firmware failed\n");
        return;
    }

    printf("Phase 2: Main app\n");

    // init HCI
    const hci_transport_t * transport = hci_transport_h5_instance(uart_driver);
    const btstack_link_key_db_t * link_key_db = btstack_link_key_db_fs_instance();
    hci_init(transport, (void*) &transport_config);
    hci_set_link_key_db(link_key_db);
    
    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup app
    btstack_main(main_argc, main_argv);
}


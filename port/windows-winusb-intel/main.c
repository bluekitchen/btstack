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
#include "btstack_run_loop_windows.h"
#include "hal_led.h"
#include "hci.h"
#include "hci_dump.h"
#include "btstack_stdin.h"
#include "btstack_chipset_intel_firmware.h"

int btstack_main(int argc, const char * argv[]);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static int main_argc;
static const char ** main_argv;
static const hci_transport_t * transport;
static int intel_firmware_loaded;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
    bd_addr_t addr;
    gap_local_bd_addr(addr);
    printf("BTstack up and running at %s\n",  bd_addr_to_str(addr));
}

static void sigint_handler(int param){
    UNUSED(param);

    printf("CTRL-C - SIGINT received, shutting down..\n");   
    log_info("sigint_handler: shutting down");

    // reset anyway
    btstack_stdin_reset();

    // power off and close only if hci was initialized before
    if (intel_firmware_loaded){
        hci_power_control( HCI_POWER_OFF);
        hci_close();
    }
    exit(0);
}

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}


static void intel_firmware_done(int result){

    printf("Done %x\n", result);

    intel_firmware_loaded = 1;

    // init HCI
    hci_init(transport, NULL);

#ifdef ENABLE_CLASSIC
    hci_set_link_key_db(btstack_link_key_db_fs_instance());
#endif    

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup app
    btstack_main(main_argc, main_argv);
}

#define USB_MAX_PATH_LEN 7
int main(int argc, const char * argv[]){

    // Prevent stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    main_argc = argc;
    main_argv = argv;

    printf("BTstack/windows-winusb booting up\n");

#if 0
    int usb_path_len = 0;
    uint8_t usb_path[USB_MAX_PATH_LEN];
    if (argc >= 3 && strcmp(argv[1], "-u") == 0){
        // parse command line options for "-u 11:22:33"
        const char * port_str = argv[2];
        printf("Specified USB Path: ");
        while (1){
            char * delimiter;
            int port = strtol(port_str, &delimiter, 16);
            usb_path[usb_path_len] = port;
            usb_path_len++;
            printf("%02x ", port);
            if (!delimiter) break;
            if (*delimiter != ':' && *delimiter != '-') break;
            port_str = delimiter+1;
        }
        printf("\n");
    }
#endif

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());
	    
    // if (usb_path_len){
    //     hci_transport_usb_set_path(usb_path_len, usb_path);
    // }

    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT

#if 1
    char pklg_path[100];
    strcpy(pklg_path, "hci_dump");
#if 0
    if (usb_path_len){
        strcat(pklg_path, "_");
        strcat(pklg_path, argv[2]);
    }
#endif
    strcat(pklg_path, ".pklg");
    printf("Packet Log: %s\n", pklg_path);
    hci_dump_open(pklg_path, HCI_DUMP_PACKETLOGGER);
#else
    hci_dump_open(NULL, HCI_DUMP_STDOUT);
#endif

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    // setup USB Transport
    transport = hci_transport_usb_instance();
    btstack_chipset_intel_download_firmware(hci_transport_usb_instance(), &intel_firmware_done);

    // go
    btstack_run_loop_execute();    

    return 0;
}

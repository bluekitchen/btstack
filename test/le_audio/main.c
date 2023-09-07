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

#define BTSTACK_FILE__ "main.c"

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
#include <limits.h>
#include <libgen.h>

#include "btstack_config.h"

#include "ble/le_device_db_tlv.h"
#include "bluetooth_company_id.h"
#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "btstack_signal.h"
#include "btstack_stdin.h"
#include "btstack_tlv_posix.h"
#include "btstack_uart.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "btstack_chipset_bcm.h"
#include "btstack_chipset_zephyr.h"

#define TLV_DB_PATH_PREFIX "/tmp/btstack_"
#define TLV_DB_PATH_POSTFIX ".tlv"
static char tlv_db_path[100];
static const btstack_tlv_t * tlv_impl;
static btstack_tlv_posix_t   tlv_context;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static bd_addr_t static_address;

// random MAC address for the device, used if nothing else is available 
static const bd_addr_t random_address = { 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01 };

// shutdown
static bool shutdown_triggered;

int btstack_main(int argc, const char * argv[]);
static void local_version_information_handler(uint8_t * packet);

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,  // main baudrate
    1,  // flow control
    NULL,
};

static void setup_tlv(bd_addr_t addr){
    printf("BTstack up and running on %s.\n", bd_addr_to_str(addr));
    btstack_strcpy(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_PREFIX);
    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), bd_addr_to_str(addr));
    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_POSTFIX);
    tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_db_path);
    btstack_tlv_set_instance(tlv_impl, &tlv_context);
#ifdef ENABLE_CLASSIC
    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv_impl, &tlv_context));
#endif
#ifdef ENABLE_BLE
    le_device_db_tlv_configure(tlv_impl, &tlv_context);
#endif
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t local_addr;
    const uint8_t *params;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    if( btstack_is_null_bd_addr(local_addr) && !btstack_is_null_bd_addr(static_address) ) {
                        memcpy(local_addr, static_address, sizeof(bd_addr_t));
                    } else if( btstack_is_null_bd_addr(local_addr ) && btstack_is_null_bd_addr(static_address) ) {
                        memcpy(local_addr, random_address, sizeof(bd_addr_t));
                        gap_random_address_set(local_addr);
                    }
                    setup_tlv(local_addr);
                    break;
                case HCI_STATE_OFF:
                    btstack_tlv_posix_deinit(&tlv_context);
                    if (!shutdown_triggered) break;
                    // reset stdin
                    btstack_stdin_reset();
                    log_info("Good bye, see you.\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)){
                case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
                    local_version_information_handler(packet);
                    break;
                case HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS:
                    params = hci_event_command_complete_get_return_parameters(packet);
                    if(params[0] != 0)
                        break;
                    if(size < 13)
                        break;
                    reverse_48(&params[2], static_address);
                    gap_random_address_set(static_address);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void trigger_shutdown(void){
    printf("CTRL-C - SIGINT received, shutting down..\n");
    log_info("sigint_handler: shutting down");
    shutdown_triggered = true;
    hci_power_control(HCI_POWER_OFF);
}

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void local_version_information_handler(uint8_t * packet){
    printf("Local version information:\n");
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    printf("- HCI Version    0x%04x\n", hci_version);
    printf("- HCI Revision   0x%04x\n", hci_revision);
    printf("- LMP Version    0x%04x\n", lmp_version);
    printf("- LMP Subversion 0x%04x\n", lmp_subversion);
    printf("- Manufacturer   0x%04x\n", manufacturer);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_PACKETCRAFT_INC:
            printf("PacketCraft HCI Controller\n");
            break;
        case BLUETOOTH_COMPANY_ID_THE_LINUX_FOUNDATION:
            printf("Zephyr HCI Controller\n");
            printf("WARNING - untested probably won't work!\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            break;
        case BLUETOOTH_COMPANY_ID_INFINEON_TECHNOLOGIES_AG:
        case BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION:
            printf("Broadcom/Cypress/Infineon Controller\n");
            config.baudrate_main = 921600;
            hci_set_chipset(btstack_chipset_bcm_instance());
            break;
        default:
            printf("Unknown manufacturer / manufacturer not supported yet.\n");
            break;
    }
}

int main(int argc, const char * argv[]){

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    // pre-select serial device
    config.device_name = "/dev/tty.usbmodemEA7EB9D612C31"; // BL654 with PTS Firmware

    // accept path from command line
    bool second_device = false;
    if (argc >= 3 && strcmp(argv[1], "-u") == 0){
        config.device_name = argv[2];
        second_device = true;
        argc -= 2;
        memmove((void *) &argv[1], &argv[3], (argc-1) * sizeof(char *));
    }
    printf("H4 device: %s\n", config.device_name);

    // log into file using HCI_DUMP_BTSNOOP format
    char *app_name = strndup( argv[0], PATH_MAX );
    char *base_name = basename( app_name );
    const char *pklg_postfix = ".btsnoop";
    char pklg_path[PATH_MAX] = "/tmp/hci_dump_";

    btstack_strcat( pklg_path, sizeof(pklg_path), base_name );
    btstack_strcat( pklg_path, sizeof(pklg_path), pklg_postfix );
    free( app_name );

    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_BTSNOOP);
    const hci_dump_t * hci_dump_impl = hci_dump_posix_fs_get_instance();
    hci_dump_init(hci_dump_impl);
    printf("Packet Log: %s\n", pklg_path);

    // init HCI
    const btstack_uart_t * uart_driver = btstack_uart_posix_instance();
	const hci_transport_t * transport = hci_transport_h4_instance_for_uart(uart_driver);
	hci_init(transport, (void*) &config);

#ifdef HAVE_PORTAUDIO
    btstack_audio_sink_set_instance(btstack_audio_portaudio_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_portaudio_source_get_instance());
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register callback for CTRL-c
    btstack_signal_register_callback(SIGINT, &trigger_shutdown);

    // setup app
    btstack_main(argc, argv);

    // go
    btstack_run_loop_execute();

    return 0;
}

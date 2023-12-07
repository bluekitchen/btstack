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

#define BTSTACK_FILE__ "main.c"

// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "btstack_config.h"

#include "bluetooth_company_id.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "hal_led.h"
#include "hci.h"
#include "hci_transport.h"
#include "hci_transport_usb.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "btstack_stdin.h"
#include "btstack_audio.h"
#include "btstack_tlv_posix.h"
#include "btstack_chipset_zephyr.h"

#define TLV_DB_PATH_PREFIX "/tmp/btstack_"
#define TLV_DB_PATH_POSTFIX ".tlv"
static char tlv_db_path[100];
static bool tlv_clear = false;
static const btstack_tlv_t * tlv_impl;
static btstack_tlv_posix_t   tlv_context;
static bd_addr_t             local_addr;

#define MAX_CMD_LINE_ITEMS  100
static int app_argc = 0;
static const char *app_argv[MAX_CMD_LINE_ITEMS] = { NULL };

int btstack_main(int argc, const char * argv[]);

static bd_addr_t static_address;
static int using_static_address;

static btstack_packet_callback_registration_t hci_event_callback_registration;

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
    printf("- Manufacturer 0x%04x\n", manufacturer);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_THE_LINUX_FOUNDATION:
            printf("Linux Foundation - assume Zephyr hci_usb example running on nRF52xx\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            break;
        default:
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t usb_path_len;
    const uint8_t * usb_path;
    uint16_t product_id;
    uint8_t i;
    const uint8_t *params;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    if (using_static_address){
                        memcpy(local_addr, static_address, 6);
                    }
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                    btstack_strcpy(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_PREFIX);
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), bd_addr_to_str(local_addr));
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_POSTFIX);
                    printf("TLV path: %s", tlv_db_path);
                    if (tlv_clear){
                        int rc = unlink(tlv_db_path);
                        if (rc == 0) {
                            printf(", reset ok");
                        } else {
                            printf(", reset failed with result = %d", rc);
                        }
                    }
                    printf("\n");
                    tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_db_path);
                    btstack_tlv_set_instance(tlv_impl, &tlv_context);
#ifdef ENABLE_CLASSIC
                    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv_impl, &tlv_context));
#endif
#ifdef ENABLE_BLE
                    le_device_db_tlv_configure(tlv_impl, &tlv_context);
#endif
                    break;
                case HCI_STATE_OFF:
                    btstack_tlv_posix_deinit(&tlv_context);
                    break;
                default:
                    break;
            }
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
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
                    using_static_address = 1;
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_TRANSPORT_USB_INFO:
            usb_path_len = hci_event_transport_usb_info_get_path_len(packet);
            usb_path = hci_event_transport_usb_info_get_path(packet);
            // print device path
            product_id = hci_event_transport_usb_info_get_product_id(packet);
            printf("USB device 0x%04x/0x%04x, path: ",
                   hci_event_transport_usb_info_get_vendor_id(packet), product_id);
            for (i=0;i<usb_path_len;i++){
                if (i) printf("-");
                printf("%02x", usb_path[i]);
            }
            printf("\n");
            break;

        default:
            break;
    }
}

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

static void btstack_log_cmd_line( int argc, const char *args[] )
{
    char buf[2048] = "command line:";
    char *ptr = buf+strlen(buf);
    size_t len = sizeof(buf);
    for( int i=0; i<argc; ++i )
    {
        int ret = snprintf(ptr, len, " %s", args[i]);
        // avoid overflow
        if((ret < 0) || ( (size_t)ret >= len ))
        {
            log_error("command line truncated!");
            break;
        }
        // terminate string
        ptr[ret] = 0;
        len -= ret;
        ptr += ret;
    }
    log_info("%s", buf);

}

static const char short_options[] = "-hu:l:c";

static const struct option long_options[] = {
    {"help",       no_argument,        NULL,   'h'},
    {"logfile",    required_argument,  NULL,   'l'},
    {"clear-tlv",  no_argument,        NULL,   'c'},
    {"usbpath",    required_argument,  NULL,   'u'},
    {0, 0, 0, 0}
};

static const char *help_options[] = {
    "print (this) help.",
    "set file to store debug output and HCI trace.",
    "clear bonding information stored in TLV.",
    "set USB path to Bluetooth Controller.",
};

static const char *option_arg_name[] = {
    "",
    "LOGFILE",
    "",
    "USBPATH",
};

static void usage(const char *name){
    unsigned int i;
    printf( "usage:\n\t%s [options]\n", name );
    printf("valid options:\n");
    for( i=0; long_options[i].name != 0; i++) {
        printf("--%-10s| -%c  %-10s\t\t%s\n", long_options[i].name, long_options[i].val, option_arg_name[i], help_options[i] );
    }
}

static void clear_argv(int idx){
    app_argv[idx] = NULL;
}

static void shift_argv(int idx){
    for( int i=idx; i<app_argc-1; ++i )
    {
        app_argv[i] = app_argv[i+1];
    }
    app_argc--;
}

static void cleanup_argv(void){
    for( int i=0; i<app_argc; ){
        if( app_argv[i] == NULL )
            shift_argv( i );
        else{
            ++i;
        }
    }
}

#define USB_MAX_PATH_LEN 7
int main(int argc, const char * argv[]){

    uint8_t usb_path[USB_MAX_PATH_LEN];
    int usb_path_len = 0;
    const char * usb_path_string = NULL;
    const char * log_file_path = NULL;

    btstack_assert( argc < MAX_CMD_LINE_ITEMS );
    app_argc = argc;
    for( int i=0; i<argc; ++i ){
        app_argv[i] = argv[i];
    }

    opterr = 0; // disable error message on unknown options
    // parse command line parameters
    while(true){
        int c = getopt_long( argc, (char* const *)argv, short_options, long_options, NULL );
        if (c < 0) {
            break;
        }
        switch (c) {
            case 'u':
                usb_path_string = optarg;
                clear_argv( optind - 1 );
                clear_argv( optind - 2 );
                break;
            case 'l':
                log_file_path = optarg;
                clear_argv( optind - 1 );
                clear_argv( optind - 2 );
                break;
            case 'c':
                tlv_clear = true;
                clear_argv( optind - 1 );
                break;
            case '?':
            case 1:
                break;
            case 'h':
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (usb_path_string != NULL){
        // parse command line options for "-u 11:22:33"
        printf("Specified USB Path: ");
        while (1){
            char * delimiter;
            int port = strtol(usb_path_string, &delimiter, 16);
            usb_path[usb_path_len] = port;
            usb_path_len++;
            printf("%02x ", port);
            if (!delimiter) break;
            if (*delimiter != ':' && *delimiter != '-') break;
            usb_path_string = delimiter+1;
        }
        printf("\n");
    }

    // remove used up command line args
    cleanup_argv();

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    if (usb_path_len){
        hci_transport_usb_set_path(usb_path_len, usb_path);
    }

    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    char pklg_path[100];
    btstack_strcpy(pklg_path, sizeof(pklg_path),  "/tmp/hci_dump");
    if (usb_path_len){
        btstack_strcat(pklg_path, sizeof(pklg_path), "_");
        btstack_strcat(pklg_path, sizeof(pklg_path), usb_path_string);
    }
    btstack_strcat(pklg_path, sizeof(pklg_path), ".pklg");

    // use path from command line if given
    if( log_file_path != NULL ){
        btstack_strcpy(pklg_path, sizeof(pklg_path), log_file_path );
    }

    // log into file using HCI_DUMP_PACKETLOGGER format
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", pklg_path);

    // let the log contain the calling parameters
    btstack_log_cmd_line( app_argc, app_argv );

    // init HCI
    hci_init(hci_transport_usb_instance(), NULL);

#ifdef HAVE_PORTAUDIO
    btstack_audio_sink_set_instance(btstack_audio_portaudio_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_portaudio_source_get_instance());
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    // setup app
    btstack_main(app_argc, app_argv);

    // go
    btstack_run_loop_execute();

    return 0;
}

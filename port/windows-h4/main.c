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

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_config.h"

#include "ble/le_device_db_tlv.h"
#include "bluetooth_company_id.h"
#include "btstack_audio.h"
#include "btstack_chipset_bcm.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_chipset_csr.h"
#include "btstack_chipset_em9301.h"
#include "btstack_chipset_stlc2500d.h"
#include "btstack_chipset_tc3566x.h"
#include "btstack_chipset_zephyr.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_windows.h"
#include "btstack_stdin.h"
#include "btstack_stdin_windows.h"
#include "btstack_tlv_windows.h"
#include "btstack_util.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "gap.h"
#include "hal_led.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_dump_windows_fs.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

int btstack_main(int argc, const char * argv[]);
static void local_version_information_handler(uint8_t * packet);
static void usage(const char *name);

static hci_transport_config_uart_t config = {
        HCI_TRANSPORT_CONFIG_UART,
        115200,
        0,  // main baudrate
        1,  // flow control
        NULL,
};

int is_bcm;

static int led_state = 0;

#define TLV_DB_PATH_PREFIX "btstack_"
#define TLV_DB_PATH_POSTFIX ".tlv"
static char tlv_db_path[100];
static bool tlv_reset;
static const btstack_tlv_t * tlv_impl;
static btstack_tlv_windows_t   tlv_context;
static bd_addr_t             local_addr;
static bd_addr_t             static_address;
static bd_addr_t             custom_address;
static bool                  airoc_download_mode;
static char                  normalized_device_name[100];
static bool shutdown_triggered;

void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void trigger_shutdown(void){
    printf("CTRL-C - SIGINT received, shutting down..\n");
    log_info("sigint_handler: shutting down");
    shutdown_triggered = true;
    hci_power_control(HCI_POWER_OFF);
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

#ifdef ENABLE_AIROC_DOWNLOAD_MODE
static void enter_download_mode(void){
    printf("Please reset Bluetooth Controller, e.g. via RESET button. Firmware download starts in:\n");
    for (uint8_t i = 3; i > 0; i--){
        printf("%u\n", i);
        Sleep(1000);
    }
    printf("Firmware download started\n");
    hci_power_control(HCI_POWER_CYCLE_COMPLETED);
}
#endif

static const char * hci_dump_type_to_string[] = {
        [HCI_DUMP_INVALID]      = "invalid",
        [HCI_DUMP_BLUEZ]        = "bluez",
        [HCI_DUMP_PACKETLOGGER] = "pklg",
        [HCI_DUMP_BTSNOOP]      = "btsnoop",
};

#define STR(x) #x
#define ENUM_TO_STRING(x) [x] = STR(x)
static const char * hci_dump_enum_to_string[] = {
        ENUM_TO_STRING(HCI_DUMP_INVALID),
        ENUM_TO_STRING(HCI_DUMP_BLUEZ),
        ENUM_TO_STRING(HCI_DUMP_PACKETLOGGER),
        ENUM_TO_STRING(HCI_DUMP_BTSNOOP),
};

static int dump_format_name_to_enum(const char *name) {
    int i;
    for (i = HCI_DUMP_INVALID; i <= HCI_DUMP_BTSNOOP; i++) {
        if (strcmp(hci_dump_type_to_string[i], name) == 0) {
            return i;
        }
    }
    return HCI_DUMP_INVALID;
}

static const char * get_file_ext(const char *filename){
    const char *dot = strrchr(filename, '.');
    if ((dot == NULL) || (dot == filename)) return NULL;
    return dot + 1;
}

static const char * path_basename(const char *path){
    const char *base_name = strrchr(path, '\\');
    const char *alt_base_name = strrchr(path, '/');
    if (base_name == NULL) return (alt_base_name != NULL) ? alt_base_name + 1 : path;
    if ((alt_base_name != NULL) && (alt_base_name > base_name)) return alt_base_name + 1;
    return base_name + 1;
}

static void copy_app_name(char * buffer, size_t buffer_size, const char * path){
    const char * app_name = path_basename(path);
    const char * ext = get_file_ext(app_name);
    btstack_strcpy(buffer, buffer_size, app_name);
    if (ext == NULL) return;
    if ((tolower((unsigned char) ext[0]) != 'e') || (tolower((unsigned char) ext[1]) != 'x') || (tolower((unsigned char) ext[2]) != 'e') || (ext[3] != '\0')) return;
    buffer[strlen(buffer) - 4] = '\0';
}

static const char * normalize_device_name(const char * device_name){
    if (device_name == NULL) return NULL;
    if (strncmp(device_name, "\\\\.\\", 4) == 0) return device_name;
    if ((toupper((unsigned char) device_name[0]) != 'C') || (toupper((unsigned char) device_name[1]) != 'O') || (toupper((unsigned char) device_name[2]) != 'M')) return device_name;
    btstack_snprintf_best_effort(normalized_device_name, sizeof(normalized_device_name), "\\\\.\\%s", device_name);
    return normalized_device_name;
}

static bool option_matches(const char *arg, const char *short_option, const char *long_option){
    return (strcmp(arg, short_option) == 0) || (strcmp(arg, long_option) == 0);
}

static const char * option_value(int argc, const char * argv[], int * arg_index, const char * option_name){
    if ((*arg_index + 1) >= argc){
        printf("Missing argument for %s\n", option_name);
        return NULL;
    }
    (*arg_index)++;
    return argv[*arg_index];
}

static void usage(const char *name){
    printf("usage:\n\t%s [options]\n", name);
    printf("valid options:\n");
    printf("--help      | -h             \t\tprint (this) help.\n");
    printf("--logfile   | -l  LOGFILE    \t\tset file to store debug output and HCI trace.\n");
    printf("--logformat | -f  btsnoop|bluez|pklg\t\tset file format to store debug output in.\n");
    printf("--reset-tlv | -r             \t\treset bonding information stored in TLV.\n");
    printf("--tty       | -u  TTY        \t\tset path to Bluetooth Controller, e.g. COM1.\n");
    printf("--bd-addr   | -m  BD_ADDR    \t\tset random static Bluetooth address.\n");
    printf("--baudrate  | -b  BAUDRATE   \t\tset initial baudrate.\n");
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
    printf("--airoc-download-mode | -d   \t\tenable AIROC Download Mode for newer CYW55xx Controller.\n");
#endif
}

int btstack_main_config(int argc, const char * argv[], hci_transport_config_uart_t * transport_config, bd_addr_t address, bool * reset_tlv, bool * airoc_download_mode_enabled){
    const char * log_file_path = NULL;
    hci_dump_format_t dump_format = HCI_DUMP_PACKETLOGGER;
    char default_log_path[200];
    int arg_index;

    btstack_assert(transport_config != NULL);

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());

    for (arg_index = 1; arg_index < argc; arg_index++){
        const char *arg = argv[arg_index];
        const char *value;
        if (option_matches(arg, "-u", "--tty")){
            value = option_value(argc, argv, &arg_index, arg);
            if (value == NULL) continue;
            transport_config->device_name = normalize_device_name(value);
            continue;
        }
        if (option_matches(arg, "-l", "--logfile")){
            value = option_value(argc, argv, &arg_index, arg);
            if (value == NULL) continue;
            log_file_path = value;
            continue;
        }
        if (option_matches(arg, "-f", "--logformat")){
            value = option_value(argc, argv, &arg_index, arg);
            if (value == NULL) continue;
            dump_format = dump_format_name_to_enum(value);
            continue;
        }
        if (option_matches(arg, "-r", "--reset-tlv")){
            if (reset_tlv != NULL){
                *reset_tlv = true;
            }
            continue;
        }
        if (option_matches(arg, "-m", "--bd-addr") || (strcmp(arg, "-a") == 0)){
            value = option_value(argc, argv, &arg_index, arg);
            if (value == NULL) continue;
            if (address != NULL){
                sscanf_bd_addr(value, address);
            }
            continue;
        }
        if (option_matches(arg, "-b", "--baudrate")){
            value = option_value(argc, argv, &arg_index, arg);
            if (value == NULL) continue;
            transport_config->baudrate_init = atoi(value);
            continue;
        }
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
        if (option_matches(arg, "-d", "--airoc-download-mode")){
            if (airoc_download_mode_enabled != NULL){
                *airoc_download_mode_enabled = true;
            }
            continue;
        }
#endif
        if (option_matches(arg, "-h", "--help")){
            usage(argv[0]);
            continue;
        }
    }

    if (log_file_path == NULL){
        char sanitized_app_name[80];
        char sanitized_device_name[80];
        const char * device_name = path_basename(transport_config->device_name);
        const char * dump_ext;
        size_t i;

        if (dump_format == HCI_DUMP_INVALID){
            dump_format = HCI_DUMP_PACKETLOGGER;
        }
        dump_ext = hci_dump_type_to_string[dump_format];
        copy_app_name(sanitized_app_name, sizeof(sanitized_app_name), argv[0]);
        btstack_strcpy(sanitized_device_name, sizeof(sanitized_device_name), device_name);
        for (i = 0; i < strlen(sanitized_device_name); i++){
            if ((sanitized_device_name[i] == '.') || (sanitized_device_name[i] == '\\') || (sanitized_device_name[i] == '/')){
                sanitized_device_name[i] = '_';
            }
        }
        btstack_snprintf_best_effort(default_log_path, sizeof(default_log_path), "hci_dump_%s_%s.%s", sanitized_app_name, sanitized_device_name, dump_ext);
        log_file_path = default_log_path;
    } else {
        const char *ext = get_file_ext(log_file_path);
        if (ext != NULL){
            dump_format = dump_format_name_to_enum(ext);
        }
        if (dump_format == HCI_DUMP_INVALID){
            dump_format = HCI_DUMP_PACKETLOGGER;
        }
    }

    hci_dump_windows_fs_open(log_file_path, dump_format);
    hci_dump_init(hci_dump_windows_fs_get_instance());
    transport_config->device_name = normalize_device_name(transport_config->device_name);
    printf("Packet Log: %s\n", log_file_path);
    printf("Log format: %s\n", hci_dump_enum_to_string[dump_format]);
    printf("Device    : \"%s\"\n", transport_config->device_name);
    printf("Baudrate  : %u\n", transport_config->baudrate_init);
    if ((reset_tlv != NULL) && *reset_tlv){
        printf("Reset tlv : true\n");
    }
    if ((address != NULL) && !btstack_is_null_bd_addr(address)){
        printf("address   : %s\n", bd_addr_to_str(address));
    }
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
    if ((airoc_download_mode_enabled != NULL) && *airoc_download_mode_enabled){
        printf("AIROC DL  : true\n");
    }
#endif
#ifdef HAVE_PORTAUDIO
    btstack_audio_sink_set_instance(btstack_audio_portaudio_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_portaudio_source_get_instance());
#endif

    return EXIT_SUCCESS;
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    const uint8_t * params;
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_POWERON_FAILED:
            printf("Terminating.\n");
            exit(EXIT_FAILURE);
            break;
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)){
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
                case HCI_STATE_REQUIRE_POWER_CYCLE:
                    enter_download_mode();
                    break;
#endif
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    if (btstack_is_null_bd_addr(local_addr) && !btstack_is_null_bd_addr(custom_address)){
                        memcpy(local_addr, custom_address, sizeof(bd_addr_t));
                    } else if (btstack_is_null_bd_addr(local_addr) && !btstack_is_null_bd_addr(static_address)){
                        memcpy(local_addr, static_address, sizeof(bd_addr_t));
                    }
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                    btstack_strcpy(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_PREFIX);
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), bd_addr_to_str_with_delimiter(local_addr, '-'));
                    btstack_strcat(tlv_db_path, sizeof(tlv_db_path), TLV_DB_PATH_POSTFIX);
                    printf("TLV path: %s", tlv_db_path);
                    if (tlv_reset){
                        int rc = remove(tlv_db_path);
                        if (rc == 0){
                            printf(", reset ok");
                        } else {
                            printf(", reset failed with result = %d", rc);
                        }
                    }
                    printf("\n");
                    tlv_impl = btstack_tlv_windows_init_instance(&tlv_context, tlv_db_path);
                    btstack_tlv_set_instance(tlv_impl, &tlv_context);
#ifdef ENABLE_CLASSIC
                    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv_impl, &tlv_context));
#endif
#ifdef ENABLE_BLE
                    le_device_db_tlv_configure(tlv_impl, &tlv_context);
#endif
                    break;
                case HCI_STATE_OFF:
                    btstack_tlv_windows_deinit(&tlv_context);
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
                    if (params[0] != 0) break;
                    if (size < 13) break;
                    reverse_48(&params[2], static_address);
                    gap_random_address_set(static_address);
                    break;
                case HCI_OPCODE_HCI_READ_LOCAL_NAME:
                    if (hci_event_command_complete_get_return_parameters(packet)[0]) break;
                    // terminate, name 248 chars
                    packet[6+248] = 0;
                    printf("Local name: %s\n", &packet[6]);
                    if (is_bcm){
                        btstack_chipset_bcm_set_device_name((const char *)&packet[6]);
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void use_fast_uart(void){
    printf("Using 921600 baud.\n");
    config.baudrate_main = 921600;
}

static void local_version_information_handler(uint8_t * packet){
    printf("Local version information:\n");
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    printf("- HCI Version  0x%04x\n", hci_version);
    printf("- HCI Revision 0x%04x\n", hci_revision);
    printf("- LMP Version  0x%04x\n", lmp_version);
    printf("- LMP Revision 0x%04x\n", lmp_subversion);
    printf("- Manufacturer 0x%04x\n", manufacturer);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO:
            printf("Cambridge Silicon Radio - CSR chipset, Build ID: %u.\n", hci_revision);
            use_fast_uart();
            hci_set_chipset(btstack_chipset_csr_instance());
            break;
        case BLUETOOTH_COMPANY_ID_TEXAS_INSTRUMENTS_INC:
            printf("Texas Instruments - CC256x compatible chipset.\n");
            if (lmp_subversion != btstack_chipset_cc256x_lmp_subversion()){
                printf("Error: LMP Subversion does not match initscript!");
                printf("Your initscripts is for %s chipset\n", btstack_chipset_cc256x_lmp_subversion() < lmp_subversion ? "an older" : "a newer");
                printf("Please update Makefile to include the appropriate bluetooth_init_cc256???.c file\n");
                exit(10);
            }
            use_fast_uart();
            hci_set_chipset(btstack_chipset_cc256x_instance());
#ifdef ENABLE_EHCILL
            printf("eHCILL enabled.\n");
#else
            printf("eHCILL disable.\n");
#endif
            break;
        case BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION:
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
            if (airoc_download_mode) {
                const char * device_name = btstack_chipset_bcm_identify_controller(lmp_subversion);
                if (device_name == NULL){
                    printf("Unknown device, please update btstack_chipset_bcm_identify_controller(...)\n");
                    printf("in btstack/chipset/bcm/btstack_chipset_bcm.c\n");
                } else {
                    printf("Identified Controller: %s\n", device_name);
                    btstack_chipset_bcm_set_device_name(device_name);
                }
            } else
#endif
            {
                printf("Broadcom - using BCM driver.\n");
                hci_set_chipset(btstack_chipset_bcm_instance());
                use_fast_uart();
                is_bcm = 1;
            }
            break;
        case BLUETOOTH_COMPANY_ID_ST_MICROELECTRONICS:
            printf("ST Microelectronics - using STLC2500d driver.\n");
            use_fast_uart();
            hci_set_chipset(btstack_chipset_stlc2500d_instance());
            break;
        case BLUETOOTH_COMPANY_ID_EM_MICROELECTRONIC_MARIN_SA:
            printf("EM Microelectronics - using EM9301 driver.\n");
            hci_set_chipset(btstack_chipset_em9301_instance());
            break;
        case BLUETOOTH_COMPANY_ID_NORDIC_SEMICONDUCTOR_ASA:
            printf("Nordic Semiconductor nRF5 chipset - using Zephyr driver.\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            break;
        case BLUETOOTH_COMPANY_ID_THE_LINUX_FOUNDATION:
            printf("Linux Foundation - assuming Zephyr running on Nordic chipset.\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            break;
        case BLUETOOTH_COMPANY_ID_TOSHIBA_CORP:
            printf("Toshiba - using TC3566x driver.\n");
            hci_set_chipset(btstack_chipset_tc3566x_instance());
            use_fast_uart();
            break;
        default:
            printf("Unknown manufacturer / manufacturer not supported yet.\n");
            break;
    }
}

int main(int argc, const char * argv[]){
    printf("BTstack on windows booting up\n");

    // pick serial port
    config.device_name = "COM1";
    btstack_main_config(argc, argv, &config, custom_address, &tlv_reset, &airoc_download_mode);

    // init HCI
    const btstack_uart_block_t * uart_driver = btstack_uart_block_windows_instance();
    const hci_transport_t * transport = hci_transport_h4_instance(uart_driver);
    hci_init(transport, (void*) &config);
#ifdef ENABLE_AIROC_DOWNLOAD_MODE
    if (airoc_download_mode) {
        printf("Use AIROC Download Mode for Broadcom/Cypress/Infineon chipset\n");
        hci_set_airoc_download_mode(true);
        hci_set_chipset(btstack_chipset_bcm_instance());
        use_fast_uart();
        is_bcm = 1;
    }
#endif
    if (!btstack_is_null_bd_addr(custom_address)){
        hci_set_bd_addr(custom_address);
    }

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup stdin to handle CTRL-c
    btstack_stdin_windows_init();
    btstack_stdin_window_register_ctrl_c_callback(&trigger_shutdown);

    // setup app
    btstack_main(argc, argv);

    // go
    btstack_run_loop_execute();

    return 0;
}

/*
* Copyright (C) 2025 BlueKitchen GmbH
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <emscripten/emscripten.h>

#include "bluetooth_company_id.h"
#include "btstack_chipset_zephyr.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_js.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "btstack_uart.h"
#include "hci_dump.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "hci_transport_h4_js.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,
    1,
    NULL,
    BTSTACK_UART_PARITY_OFF
};

static bool hci_log_enabled;
static int log_level;

static btstack_packet_callback_registration_t hci_event_callback_registration;

/** Zephyr Static Address - used on nRF5x SoCs */
static bd_addr_t zephyr_static_address;
static bd_addr_t random_address = { 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01 };

/** HAL LED */
static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void use_fast_uart(void){
    // only increase baudrate if started with default baudrate
    // to avoid issues with custom default baudrates
    if (config.baudrate_init == 115200) {
        config.baudrate_main = 921600;
        printf("Update to %u baud.\n", config.baudrate_main);
    } else {
        printf("Keep %u baud.\n", config.baudrate_init);
    }
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
    printf("- Manufacturer 0x%04x\n", manufacturer);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_NORDIC_SEMICONDUCTOR_ASA:
            printf("Nordic Semiconductor nRF5 chipset.\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            use_fast_uart();
            break;
        default:
            printf("Unknown manufacturer / manufacturer not supported yet.\n");
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    const uint8_t *params;
    static bd_addr_t local_addr;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_POWERON_FAILED:
            printf("Terminating.\n");
            exit(EXIT_FAILURE);
            break;
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    if( btstack_is_null_bd_addr(local_addr) && !btstack_is_null_bd_addr(zephyr_static_address) ) {
                        memcpy(local_addr, zephyr_static_address, sizeof(bd_addr_t));
                    } else if( btstack_is_null_bd_addr(local_addr) && btstack_is_null_bd_addr(zephyr_static_address) ) {
                        memcpy(local_addr, random_address, sizeof(bd_addr_t));
                        gap_random_address_set(local_addr);
                    }
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
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
                log_info("Zephyr read static address available");
                params = hci_event_command_complete_get_return_parameters(packet);
                if(params[0] != 0)
                    break;
                if(size < 13)
                    break;
                reverse_48(&params[2], zephyr_static_address);
                gap_random_address_set(zephyr_static_address);
                break;
            default:
                break;
            }
            break;
        default:
            break;
    }
}

EMSCRIPTEN_KEEPALIVE
void main_set_initial_baudrate(uint32_t baudrate) {
    printf("Set Baudrate %u\n", baudrate);
    config.baudrate_init = baudrate;
}

static void main_set_log_level_private(int level){
    for (int i = HCI_DUMP_LOG_LEVEL_DEBUG; i <= HCI_DUMP_LOG_LEVEL_ERROR; i++) {
        hci_dump_enable_log_level(i, i >= level);
    }
    log_level = level;
}

EMSCRIPTEN_KEEPALIVE
void main_set_log_level(int level) {
    const char * log_levels[] = { "DEBUG", "INFO", "ERROR" };
    if (log_level != level){
        printf("Set log level %u = %s\n", level, log_levels[level]);
    }
    main_set_log_level_private(level);
}

EMSCRIPTEN_KEEPALIVE
void main_set_hci_log(bool enabled) {
    if (hci_log_enabled != enabled){
        printf("HCI Log %s\n", enabled ? "enabled" : "disabled");
    }
    hci_dump_enable_packet_log(enabled);
    hci_log_enabled = enabled;
}

EMSCRIPTEN_KEEPALIVE
int main() {
    // init memory
    btstack_memory_init();

    // init run loop
    btstack_run_loop_init(btstack_run_loop_js_get_instance());

    // init HCI dump to stdout, but disable by default
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
    hci_dump_enable_packet_log(false);
    main_set_log_level_private(HCI_DUMP_LOG_LEVEL_ERROR);

    // init HCI with H4 HCI Transport over JavaScript
    hci_init(hci_transport_h4_js_instance(), (void*) &config);

    // use in-memory TLV for testing
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
    void                * btstack_tlv_context = NULL;
    btstack_tlv_set_instance(btstack_tlv_impl, btstack_tlv_context);

    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_context);
    hci_set_link_key_db(btstack_link_key_db);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, btstack_tlv_context);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    return 0;
}

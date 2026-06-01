/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#include "sdkconfig.h"

#if CONFIG_BT_ENABLED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_config.h"
#include "bluetooth_company_id.h"
#include "btstack_chipset_zephyr.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_ring_buffer.h"
#include "btstack_tlv.h"
#include "btstack_tlv_esp32.h"
#include "hci_transport_h4.h"
#include "hci_dump_embedded_stdout.h"
#include "btstack_uart_block.h"

#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"


#include "btstack_debug.h"
#include "btstack_audio.h"
#include "hci_transport_esp32_vhci.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#if defined(CONFIG_BTSTACK_EXTERNAL_UART_BT)
static hci_transport_config_uart_t config = {
    .type = HCI_TRANSPORT_CONFIG_UART,
    .device_name = "",
    .baudrate_init = 1000000,
    .baudrate_main = 0,
    .flowcontrol = BTSTACK_UART_FLOWCONTROL_ON,
    .parity = BTSTACK_UART_PARITY_OFF,
};
#endif

uint32_t esp_log_timestamp();

uint32_t hal_time_ms(void) {
    // super hacky way to get ms
    return esp_log_timestamp();
}

static btstack_packet_callback_registration_t hci_event_callback_registration;
static bd_addr_t             static_address;

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
            break;
        case BLUETOOTH_COMPANY_ID_ESPRESSIF_INCORPORATED:
            printf("Espressif SoC.\n");
            break;
        default:
            printf("Unknown manufacturer / manufacturer not supported yet.\n");
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    const uint8_t *params;

    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bd_addr_t local_addr;
                gap_local_bd_addr(local_addr);
                if( btstack_is_null_bd_addr(local_addr) && !btstack_is_null_bd_addr(static_address) ) {
                    memcpy(local_addr, static_address, sizeof(bd_addr_t));
                }
                printf("BTstack up and running at %s\n",  bd_addr_to_str(local_addr));
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)){
                case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
                    local_version_information_handler(packet);
                    break;
                case HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS:
                    printf("Zephyr read static address available\n");
                    params = hci_event_command_complete_get_return_parameters(packet);
                    if((params[0] == 0) && (size >= 13)) {
                        reverse_48(&params[2], static_address);
                        gap_random_address_set(static_address);
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

uint8_t btstack_init(void){
    // Setup memory pools and run loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_freertos_get_instance());

    // init HCI
#if defined(CONFIG_BTSTACK_INTERNAL_BT) && defined(CONFIG_BTSTACK_EXTERNAL_UART_BT)
#   warning multiple stacks not supported! (jet) selecting internal BT
#endif

#if defined(CONFIG_BTSTACK_INTERNAL_BT)
    hci_init(hci_transport_esp32_vhci_get_instance(), NULL);
#elif defined(CONFIG_BTSTACK_EXTERNAL_UART_BT)
    hci_init(hci_transport_h4_instance_for_uart(btstack_uart_block_embedded_instance()), &config);
#else
#   error missing transport definition select either CONFIG_BTSTACK_INTERNAL_BT or CONFIG_BTSTACK_EXTERNAL_UART_BT
#endif

#if defined(CONFIG_BTSTACK_HCI_DUMP_STDOUT)
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
    // setup TLV ESP32 implementation and register with system
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_esp32_get_instance();
    btstack_tlv_set_instance(btstack_tlv_impl, NULL);

#if defined(ENABLE_CLASSIC)
    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, NULL);
    hci_set_link_key_db(btstack_link_key_db);
#endif

#if defined(ENABLE_BLE)
    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, NULL);
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

#if defined(CONFIG_BTSTACK_AUDIO)
    // setup i2s audio for sink and source
    btstack_audio_sink_set_instance(btstack_audio_esp32_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_esp32_source_get_instance());
#endif

    return ERROR_CODE_SUCCESS;
}

#endif

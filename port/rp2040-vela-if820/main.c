/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define BTSTACK_FILE__ "main.c"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "btstack_audio.h"
#include "pico/stdlib.h"
#include "pico/async_context_poll.h"
#include "pico/btstack_flash_bank.h"
#include "pico/btstack_run_loop_async_context.h"

#include "tusb_config.h"

#include "ble/le_device_db_tlv.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

#if WANT_HCI_DUMP
#include "hci_dump.h"
#ifdef ENABLE_SEGGER_RTT
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif
#endif

#include "board.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    3000000,  // baudrate
    0,  // main baudrate
    1,        // flow control
    NULL,
};

static void setup_tlv(void) {
    static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;
    const hal_flash_bank_t *hal_flash_bank_impl = pico_flash_bank_instance();
    const btstack_tlv_t *btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            NULL);
    // setup global TLV
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#ifdef ENABLE_CLASSIC
    const btstack_link_key_db_t *btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
    hci_set_link_key_db(btstack_link_key_db);
#endif
#ifdef ENABLE_BLE
    // configure LE Device DB for TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#endif
}

// copy, paste & modify from rp2_common/pico_cyw43_arch/cyw43_arch_poll.c

static async_context_poll_t btstack_async_context_poll;

async_context_t *get_async_context(void) {
    if (async_context_poll_init_with_defaults(&btstack_async_context_poll))
        return &btstack_async_context_poll.core;
    return NULL;
}

// copy, paste & modify from rp2_common/pico_cyw43_driver/btstack_cyw43.c

bool btstack_init(async_context_t *context) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(context));

#if WANT_HCI_DUMP
#ifdef ENABLE_SEGGER_RTT
    hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
#endif
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), &config);

    // setup TLV storage
    setup_tlv();
    return true;
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    bd_addr_t local_addr;

    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    gap_local_bd_addr(local_addr);
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}


void btstack_main(void);

int main() {

    // Setup console
    stdio_init_all();
    stdio_usb_init();

    // Setup Test Debug pin
    gpio_init(TEST_DEBUG_PIN);
    gpio_set_dir(TEST_DEBUG_PIN, GPIO_OUT);
    gpio_put(TEST_DEBUG_PIN, 0);

#if CFG_TUD_AUDIO
    btstack_audio_sink_set_instance(btstack_audio_embedded_sink_get_instance());
#endif

    // init BTstack
    async_context_t * context = get_async_context();
    btstack_init(context);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    log_info("BTstack Port for Ezurio Vela IF820!");
    btstack_main();
    btstack_run_loop_execute();
}

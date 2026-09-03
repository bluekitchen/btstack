#define BTSTACK_FILE__ "main.c"

#include <stdio.h>

#include "pico/async_context_poll.h"
#include "pico/btstack_flash_bank.h"
#include "pico/btstack_run_loop_async_context.h"
#include "pico/stdlib.h"

#include "ble/le_device_db_tlv.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci.h"
#include "hci_transport_usb_tinyusb.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static async_context_poll_t btstack_async_context_poll;

static void setup_tlv(void) {
    static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;
    const hal_flash_bank_t *hal_flash_bank_impl = pico_flash_bank_instance();
    const btstack_tlv_t *btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
        &btstack_tlv_flash_bank_context, hal_flash_bank_impl, NULL);

    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#ifdef ENABLE_CLASSIC
    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(
        btstack_tlv_impl, &btstack_tlv_flash_bank_context));
#endif
#ifdef ENABLE_BLE
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#endif
}

static async_context_t *get_async_context(void) {
    if (async_context_poll_init_with_defaults(&btstack_async_context_poll)) {
        return &btstack_async_context_poll.core;
    }
    return NULL;
}

static bool btstack_init(async_context_t *context) {
    if (context == NULL) return false;

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(context));
    hci_init(hci_transport_usb_tinyusb_instance(), NULL);
    setup_tlv();
    return true;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE &&
        btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        bd_addr_t local_addr;
        gap_local_bd_addr(local_addr);
        printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
    }
}

void btstack_main(void);

int main(void) {
    // The native USB controller is the host for the Bluetooth dongle. UART is
    // used for the console so stdio does not compete for that controller.
    stdio_init_all();

    if (!btstack_init(get_async_context())) {
        return 1;
    }

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    log_info("BTstack RP2040 TinyUSB host port");
    btstack_main();
    btstack_run_loop_execute();
}

#define BTSTACK_FILE__ "hci_transport_usb_tinyusb.c"

#include "hci_transport_usb_tinyusb.h"

#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "tusb.h"

static btstack_data_source_t tinyusb_data_source;
static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void tinyusb_data_source_handler(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(ds);
    UNUSED(callback_type);
    tuh_task();
}

static void hci_transport_usb_tinyusb_init(const void *transport_config) {
    UNUSED(transport_config);

    tusb_rhport_init_t const rh_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUH_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL,
    };
    if (!tusb_init(BOARD_TUH_RHPORT, &rh_init)) {
        log_error("Failed to initialize TinyUSB host");
        return;
    }
    btstack_run_loop_set_data_source_handler(&tinyusb_data_source, tinyusb_data_source_handler);
    btstack_run_loop_enable_data_source_callbacks(&tinyusb_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&tinyusb_data_source);
}

static int hci_transport_usb_tinyusb_open(void) {
    // A future implementation will wait for a Bluetooth USB interface to be
    // mounted and then open its event, ACL, and optional SCO endpoints.
    log_info("TinyUSB Bluetooth HCI transport skeleton active");
    return 0;
}

static int hci_transport_usb_tinyusb_close(void) {
    btstack_run_loop_disable_data_source_callbacks(&tinyusb_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_remove_data_source(&tinyusb_data_source);
    return 0;
}

static void hci_transport_usb_tinyusb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)) {
    packet_handler = handler;
    UNUSED(packet_handler);
}

static int hci_transport_usb_tinyusb_can_send_packet_now(uint8_t packet_type) {
    UNUSED(packet_type);
    // No USB endpoints are claimed by the skeleton yet.
    return 0;
}

static int hci_transport_usb_tinyusb_send_packet(uint8_t packet_type, uint8_t *packet, int size) {
    UNUSED(packet_type);
    UNUSED(packet);
    UNUSED(size);
    return -1;
}

static const hci_transport_t hci_transport_usb_tinyusb = {
    "USB TinyUSB",
    &hci_transport_usb_tinyusb_init,
    &hci_transport_usb_tinyusb_open,
    &hci_transport_usb_tinyusb_close,
    &hci_transport_usb_tinyusb_register_packet_handler,
    &hci_transport_usb_tinyusb_can_send_packet_now,
    &hci_transport_usb_tinyusb_send_packet,
    NULL,
    NULL,
    NULL,
};

const hci_transport_t * hci_transport_usb_tinyusb_instance(void) {
    return &hci_transport_usb_tinyusb;
}

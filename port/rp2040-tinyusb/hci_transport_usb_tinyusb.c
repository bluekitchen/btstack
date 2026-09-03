#define BTSTACK_FILE__ "hci_transport_usb_tinyusb.c"

#include "hci_transport_usb_tinyusb.h"

#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "tusb.h"

static btstack_timer_source_t tinyusb_timer;
static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void tinyusb_task_handler(btstack_timer_source_t *ts) {
    UNUSED(ts);
    tuh_task();
    btstack_run_loop_set_timer(&tinyusb_timer, 1);
    btstack_run_loop_add_timer(&tinyusb_timer);
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
    btstack_run_loop_set_timer_handler(&tinyusb_timer, tinyusb_task_handler);
    btstack_run_loop_set_timer(&tinyusb_timer, 1);
    btstack_run_loop_add_timer(&tinyusb_timer);
}

static int hci_transport_usb_tinyusb_open(void) {
    // A future implementation will wait for a Bluetooth USB interface to be
    // mounted and then open its event, ACL, and optional SCO endpoints.
    log_info("TinyUSB Bluetooth HCI transport skeleton active");
    return 0;
}

static int hci_transport_usb_tinyusb_close(void) {
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

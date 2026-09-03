#define BTSTACK_FILE__ "hci_transport_usb_tinyusb.c"

#include "hci_transport_usb_tinyusb.h"

#include <string.h>

#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "tusb.h"

#define HCI_USB_INVALID_ADDRESS 0xff

// USB Bluetooth HCI Class/Subclass/Protocol
#define HCI_USB_CLASS    TUSB_CLASS_WIRELESS_CONTROLLER
#define HCI_USB_SUBCLASS 0x01
#define HCI_USB_PROTOCOL 0x01

typedef struct {
    uint8_t event_in_addr;
    uint8_t acl_in_addr;
    uint8_t acl_out_addr;
    uint8_t hci_interface_number;
    tusb_desc_endpoint_t event_in_desc;
    tusb_desc_endpoint_t acl_in_desc;
    tusb_desc_endpoint_t acl_out_desc;
} tinyusb_hci_controller_t;

static btstack_timer_source_t tinyusb_timer;
static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);
static tusb_desc_device_t tinyusb_device_descriptor;
CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN static uint8_t tinyusb_configuration_descriptor[CFG_TUH_ENUMERATION_BUFSIZE];
static tinyusb_hci_controller_t tinyusb_hci_controller;
static uint8_t tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
static uint8_t tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
static bool tinyusb_initialized;
static bool tinyusb_transport_open;

static void tinyusb_timer_handler(btstack_timer_source_t *ts) {
    UNUSED(ts);

    tuh_task();
    btstack_run_loop_set_timer(&tinyusb_timer, 1);
    btstack_run_loop_add_timer(&tinyusb_timer);
}

static void hci_transport_usb_tinyusb_init(const void *transport_config) {
    UNUSED(transport_config);
    tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_initialized = false;
    tinyusb_transport_open = false;
}

static void tinyusb_dump_hci_controller_configuration(void) {
    log_info("HCI USB configuration:");
    log_info("  Command: 0x00 (interface %u)", tinyusb_hci_controller.hci_interface_number);
    log_info("  Event:   0x%02x (transfer size %u)", tinyusb_hci_controller.event_in_addr,
             tu_edpt_packet_size(&tinyusb_hci_controller.event_in_desc));
    log_info("  ACL Out: 0x%02x (transfer size %u)", tinyusb_hci_controller.acl_out_addr,
             tu_edpt_packet_size(&tinyusb_hci_controller.acl_out_desc));
    log_info("  ACL  In: 0x%02x (transfer size %u)", tinyusb_hci_controller.acl_in_addr,
             tu_edpt_packet_size(&tinyusb_hci_controller.acl_in_desc));
}

static void tinyusb_find_hci_endpoints(const tusb_desc_configuration_t *configuration) {
    const uint8_t *descriptor_end = (const uint8_t *) configuration + tu_le16toh(configuration->wTotalLength);
    const uint8_t *descriptor = tu_desc_next(configuration);
    uint8_t interface_number = HCI_USB_INVALID_ADDRESS;
    bool hci_interface = false;

    memset(&tinyusb_hci_controller, 0, sizeof(tinyusb_hci_controller));
    tinyusb_hci_controller.hci_interface_number = HCI_USB_INVALID_ADDRESS;
    while (descriptor < descriptor_end) {
        if (tu_desc_len(descriptor) == 0) {
            break;
        }

        switch (tu_desc_type(descriptor)) {
            case TUSB_DESC_INTERFACE: {
                const tusb_desc_interface_t *interface = (const tusb_desc_interface_t *) descriptor;
                interface_number = interface->bInterfaceNumber;
                hci_interface = interface->bAlternateSetting == 0 && interface->bInterfaceClass == HCI_USB_CLASS &&
                                interface->bInterfaceSubClass == HCI_USB_SUBCLASS &&
                                interface->bInterfaceProtocol == HCI_USB_PROTOCOL;
                if (hci_interface) {
                    tinyusb_hci_controller.hci_interface_number = interface_number;
                }
                break;
            }

            case TUSB_DESC_ENDPOINT: {
                const tusb_desc_endpoint_t *endpoint = (const tusb_desc_endpoint_t *) descriptor;
                if (!hci_interface || interface_number != tinyusb_hci_controller.hci_interface_number) {
                    break;
                }

                if (tu_edpt_dir(endpoint->bEndpointAddress) == TUSB_DIR_IN &&
                    endpoint->bmAttributes.xfer == TUSB_XFER_INTERRUPT) {
                    tinyusb_hci_controller.event_in_addr = endpoint->bEndpointAddress;
                    tinyusb_hci_controller.event_in_desc = *endpoint;
                } else if (tu_edpt_dir(endpoint->bEndpointAddress) == TUSB_DIR_IN &&
                           endpoint->bmAttributes.xfer == TUSB_XFER_BULK) {
                    tinyusb_hci_controller.acl_in_addr = endpoint->bEndpointAddress;
                    tinyusb_hci_controller.acl_in_desc = *endpoint;
                } else if (tu_edpt_dir(endpoint->bEndpointAddress) == TUSB_DIR_OUT &&
                           endpoint->bmAttributes.xfer == TUSB_XFER_BULK) {
                    tinyusb_hci_controller.acl_out_addr = endpoint->bEndpointAddress;
                    tinyusb_hci_controller.acl_out_desc = *endpoint;
                }
                break;
            }

            default:
                break;
        }
        descriptor = tu_desc_next(descriptor);
    }
}

static bool tinyusb_hci_controller_usable(void) {
    return tinyusb_hci_controller.hci_interface_number != HCI_USB_INVALID_ADDRESS &&
           tinyusb_hci_controller.event_in_addr != 0 && tinyusb_hci_controller.acl_in_addr != 0 &&
           tinyusb_hci_controller.acl_out_addr != 0;
}

static void tinyusb_handle_device_descriptor(tuh_xfer_t *xfer) {
    if (xfer->result != XFER_RESULT_SUCCESS || !tinyusb_transport_open || xfer->daddr != tinyusb_candidate_daddr) {
        tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
        return;
    }

    tusb_desc_configuration_t configuration_header;
    if (tuh_descriptor_get_configuration_sync(xfer->daddr, 0, &configuration_header, sizeof(configuration_header)) !=
        XFER_RESULT_SUCCESS) {
        log_info("Unable to read configuration header for device %u", xfer->daddr);
        tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
        return;
    }

    const uint16_t configuration_length = tu_le16toh(configuration_header.wTotalLength);
    if (configuration_length < sizeof(configuration_header) ||
        configuration_length > sizeof(tinyusb_configuration_descriptor) ||
        tuh_descriptor_get_configuration_sync(xfer->daddr, 0, tinyusb_configuration_descriptor, configuration_length) !=
            XFER_RESULT_SUCCESS) {
        log_info("TinyUSB: unable to read configuration descriptor for device %u", xfer->daddr);
        tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
        return;
    }

    tinyusb_find_hci_endpoints((const tusb_desc_configuration_t *) tinyusb_configuration_descriptor);
    if (tinyusb_hci_controller_usable()) {
        tinyusb_hci_daddr = xfer->daddr;
        log_info("Bluetooth HCI controller detected at device %u (%04x:%04x)", xfer->daddr,
                 tu_le16toh(tinyusb_device_descriptor.idVendor), tu_le16toh(tinyusb_device_descriptor.idProduct));
        tinyusb_dump_hci_controller_configuration();
    } else {
        log_info("Device %u (%04x:%04x) is not a Bluetooth HCI controller", xfer->daddr,
                 tu_le16toh(tinyusb_device_descriptor.idVendor), tu_le16toh(tinyusb_device_descriptor.idProduct));
    }
    tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
}

static void tinyusb_start_hci_discovery(uint8_t daddr) {
    if (!tinyusb_transport_open || tinyusb_hci_daddr != HCI_USB_INVALID_ADDRESS ||
        tinyusb_candidate_daddr != HCI_USB_INVALID_ADDRESS) {
        return;
    }

    tinyusb_candidate_daddr = daddr;
    log_info("Device %u mounted; checking for Bluetooth HCI interface", daddr);
    if (!tuh_descriptor_get_device(daddr, &tinyusb_device_descriptor, sizeof(tinyusb_device_descriptor),
                                   tinyusb_handle_device_descriptor, 0)) {
        log_info("Unable to request device descriptor for device %u", daddr);
        tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    }
}

// Invoked by TinyUSB when a device has completed enumeration.
void tuh_mount_cb(uint8_t daddr) {
    tinyusb_start_hci_discovery(daddr);
}

// Invoked by TinyUSB when a device is unplugged or reset.
void tuh_umount_cb(uint8_t daddr) {
    if (daddr == tinyusb_candidate_daddr) {
        tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    }
    if (daddr != tinyusb_hci_daddr) {
        return;
    }

    log_info("Bluetooth HCI controller at device %u unmounted", daddr);
    tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
}

static int hci_transport_usb_tinyusb_open(void) {
    tinyusb_transport_open = true;
    tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;

    if (!tinyusb_initialized) {
        tusb_rhport_init_t const rh_init = {
            .role = TUSB_ROLE_HOST,
            .speed = TUH_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL,
        };
        if (!tusb_init(BOARD_TUH_RHPORT, &rh_init)) {
            log_error("Failed to initialize TinyUSB host");
            tinyusb_transport_open = false;
            return -1;
        }
        tinyusb_initialized = true;
    }

    btstack_run_loop_set_timer_handler(&tinyusb_timer, tinyusb_timer_handler);
    btstack_run_loop_set_timer(&tinyusb_timer, 1);
    btstack_run_loop_add_timer(&tinyusb_timer);
    log_info("Waiting for a USB Bluetooth HCI controller to mount");
    return 0;
}

static int hci_transport_usb_tinyusb_close(void) {
    tinyusb_transport_open = false;
    tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
    btstack_run_loop_remove_timer(&tinyusb_timer);
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

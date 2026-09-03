#define BTSTACK_FILE__ "hci_transport_usb_tinyusb.c"

#include "hci_transport_usb_tinyusb.h"

#include <string.h>

#include "bluetooth.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "hci.h"
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

typedef struct {
    uint8_t *packet;
    uint16_t len;
    uint16_t packet_capacity;
    uint16_t header_size;
    uint8_t packet_type;
    uint8_t endpoint_addr;
    bool transfer_active;
} tinyusb_rx_transfer_t;

static btstack_timer_source_t tinyusb_timer;
static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);
static tusb_desc_device_t tinyusb_device_descriptor;
CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN static uint8_t tinyusb_configuration_descriptor[CFG_TUH_ENUMERATION_BUFSIZE];
static tinyusb_hci_controller_t tinyusb_hci_controller;
static uint8_t tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
static uint8_t tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
static uint8_t *tinyusb_tx_packet;
static bool tinyusb_initialized;
static bool tinyusb_transport_open;

// Keep the BTstack pre-buffer hidden from TinyUSB by using pointers to the
// actual HCI packet data.
CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN static uint8_t tinyusb_acl_in_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE];
static uint8_t *tinyusb_acl_in_packet = &tinyusb_acl_in_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];
CFG_TUH_MEM_SECTION CFG_TUH_MEM_ALIGN static uint8_t tinyusb_event_in_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_EVENT_BUFFER_SIZE];
static uint8_t *tinyusb_event_in_packet = &tinyusb_event_in_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];
static tinyusb_rx_transfer_t tinyusb_acl_in_transfer;
static tinyusb_rx_transfer_t tinyusb_event_in_transfer;

static void tinyusb_timer_handler(btstack_timer_source_t *ts) {
    UNUSED(ts);

    tuh_task();
    btstack_run_loop_set_timer(&tinyusb_timer, 1);
    btstack_run_loop_add_timer(&tinyusb_timer);
}

static void tinyusb_reset_rx_transfers(void) {
    tinyusb_acl_in_transfer.len = 0;
    tinyusb_acl_in_transfer.transfer_active = false;
    tinyusb_event_in_transfer.len = 0;
    tinyusb_event_in_transfer.transfer_active = false;
}

static void hci_transport_usb_tinyusb_init(const void *transport_config) {
    UNUSED(transport_config);
    tinyusb_candidate_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_hci_daddr = HCI_USB_INVALID_ADDRESS;
    tinyusb_tx_packet = NULL;
    tinyusb_acl_in_transfer.packet = tinyusb_acl_in_packet;
    tinyusb_acl_in_transfer.len = 0;
    tinyusb_acl_in_transfer.packet_capacity = HCI_ACL_BUFFER_SIZE;
    tinyusb_acl_in_transfer.header_size = HCI_ACL_HEADER_SIZE;
    tinyusb_acl_in_transfer.packet_type = HCI_ACL_DATA_PACKET;
    tinyusb_acl_in_transfer.endpoint_addr = 0;
    tinyusb_acl_in_transfer.transfer_active = false;
    tinyusb_event_in_transfer.packet = tinyusb_event_in_packet;
    tinyusb_event_in_transfer.len = 0;
    tinyusb_event_in_transfer.packet_capacity = HCI_EVENT_BUFFER_SIZE;
    tinyusb_event_in_transfer.header_size = HCI_EVENT_HEADER_SIZE;
    tinyusb_event_in_transfer.packet_type = HCI_EVENT_PACKET;
    tinyusb_event_in_transfer.endpoint_addr = 0;
    tinyusb_event_in_transfer.transfer_active = false;
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

static void tinyusb_emit_transport_packet_sent(void) {
    static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0 };
    if (packet_handler != NULL) {
        packet_handler(HCI_EVENT_PACKET, (uint8_t *) packet_sent_event, sizeof(packet_sent_event));
    }
}

static uint16_t tinyusb_packet_expected_len(uint8_t packet_type, const uint8_t *header) {
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            return HCI_EVENT_HEADER_SIZE + header[1];
        case HCI_ACL_DATA_PACKET:
            return HCI_ACL_HEADER_SIZE + header[2] + ((uint16_t) header[3] << 8);
        default:
            return 0;
    }
}

static void tinyusb_start_rx_transfer(tinyusb_rx_transfer_t *transfer);

static void tinyusb_process_rx_data(tinyusb_rx_transfer_t *transfer, uint16_t transfer_len) {
    const uint8_t packet_type = transfer->packet_type;
    transfer->len += transfer_len;
    if (transfer->len < transfer->header_size) {
        return;
    }

    const uint16_t expected_len = tinyusb_packet_expected_len(packet_type, transfer->packet);
    if (expected_len > transfer->packet_capacity || transfer->len > expected_len) {
        log_info("HCI USB received invalid %s packet length %u (buffer capacity %u)",
                 packet_type == HCI_EVENT_PACKET ? "event" : "ACL", expected_len, transfer->packet_capacity);
        transfer->len = 0;
        return;
    }

    if (transfer->len == expected_len) {
        if (packet_handler != NULL) {
            packet_handler(packet_type, transfer->packet, expected_len);
        }
        transfer->len = 0;
    }
}

static void tinyusb_rx_transfer_complete(tuh_xfer_t *xfer) {
    tinyusb_rx_transfer_t *transfer = (tinyusb_rx_transfer_t *) xfer->user_data;
    transfer->transfer_active = false;

    if (xfer->result == XFER_RESULT_SUCCESS) {
        tinyusb_process_rx_data(transfer, (uint16_t) xfer->actual_len);
    } else {
        log_info("HCI USB receive transfer failed with result %u", xfer->result);
    }

    if (tinyusb_hci_daddr == xfer->daddr) {
        tinyusb_start_rx_transfer(transfer);
    }
}

static void tinyusb_start_rx_transfer(tinyusb_rx_transfer_t *transfer) {
    if (transfer->transfer_active || tinyusb_hci_daddr == HCI_USB_INVALID_ADDRESS) {
        return;
    }

    btstack_assert(transfer->len < transfer->packet_capacity);
    tuh_xfer_t xfer = {
        .daddr = tinyusb_hci_daddr,
        .ep_addr = transfer->endpoint_addr,
        .buflen = transfer->packet_capacity - transfer->len,
        .buffer = &transfer->packet[transfer->len],
        .complete_cb = tinyusb_rx_transfer_complete,
        .user_data = (uintptr_t) transfer,
    };
    transfer->transfer_active = tuh_edpt_xfer(&xfer);
    if (!transfer->transfer_active) {
        log_info("Unable to start HCI USB receive transfer on endpoint 0x%02x", transfer->endpoint_addr);
    }
}

static void tinyusb_tx_transfer_complete(tuh_xfer_t *xfer) {
    if (xfer->result != XFER_RESULT_SUCCESS) {
        log_info("HCI USB transmit transfer failed with result %u", xfer->result);
    }
    tinyusb_tx_packet = NULL;
    tinyusb_emit_transport_packet_sent();
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
    if (tinyusb_hci_controller_usable() && tuh_edpt_open(xfer->daddr, &tinyusb_hci_controller.event_in_desc) &&
        tuh_edpt_open(xfer->daddr, &tinyusb_hci_controller.acl_in_desc) &&
        tuh_edpt_open(xfer->daddr, &tinyusb_hci_controller.acl_out_desc)) {
        tinyusb_hci_daddr = xfer->daddr;
        tinyusb_event_in_transfer.endpoint_addr = tinyusb_hci_controller.event_in_addr;
        tinyusb_acl_in_transfer.endpoint_addr = tinyusb_hci_controller.acl_in_addr;
        log_info("Bluetooth HCI controller detected at device %u (%04x:%04x)", xfer->daddr,
                 tu_le16toh(tinyusb_device_descriptor.idVendor), tu_le16toh(tinyusb_device_descriptor.idProduct));
        tinyusb_dump_hci_controller_configuration();
        tinyusb_start_rx_transfer(&tinyusb_event_in_transfer);
        tinyusb_start_rx_transfer(&tinyusb_acl_in_transfer);
    } else if (tinyusb_hci_controller.hci_interface_number != HCI_USB_INVALID_ADDRESS) {
        log_info("Bluetooth HCI controller at device %u has unusable endpoints", xfer->daddr);
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
    tinyusb_reset_rx_transfers();
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
    tinyusb_reset_rx_transfers();
    btstack_run_loop_remove_timer(&tinyusb_timer);
    return 0;
}

static void hci_transport_usb_tinyusb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)) {
    packet_handler = handler;
    UNUSED(packet_handler);
}

static int hci_transport_usb_tinyusb_can_send_packet_now(uint8_t packet_type) {
    UNUSED(packet_type);
    return tinyusb_hci_daddr != HCI_USB_INVALID_ADDRESS && tinyusb_tx_packet == NULL;
}

static int hci_transport_usb_tinyusb_send_packet(uint8_t packet_type, uint8_t *packet, int size) {
    btstack_assert(tinyusb_hci_daddr != HCI_USB_INVALID_ADDRESS);
    btstack_assert(tinyusb_tx_packet == NULL);
    btstack_assert(size >= 0);

    tinyusb_tx_packet = packet;
    switch (packet_type) {
        case HCI_COMMAND_DATA_PACKET: {
            static tusb_control_request_t request;
            request.bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE;
            request.bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS;
            request.bmRequestType_bit.direction = TUSB_DIR_OUT;
            request.bRequest = 0;
            request.wValue = 0;
            request.wIndex = tinyusb_hci_controller.hci_interface_number;
            request.wLength = (uint16_t) size;

            tuh_xfer_t xfer = {
                .daddr = tinyusb_hci_daddr,
                .ep_addr = 0,
                .setup = &request,
                .buffer = packet,
                .complete_cb = tinyusb_tx_transfer_complete,
                .user_data = (uintptr_t) packet,
            };
            btstack_assert(tuh_control_xfer(&xfer));
            break;
        }

        case HCI_ACL_DATA_PACKET: {
            tuh_xfer_t xfer = {
                .daddr = tinyusb_hci_daddr,
                .ep_addr = tinyusb_hci_controller.acl_out_addr,
                .buflen = (uint16_t) size,
                .buffer = packet,
                .complete_cb = tinyusb_tx_transfer_complete,
                .user_data = (uintptr_t) packet,
            };
            btstack_assert(tuh_edpt_xfer(&xfer));
            break;
        }

        default:
            tinyusb_tx_packet = NULL;
            btstack_assert(false);
            break;
    }
    return 0;
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

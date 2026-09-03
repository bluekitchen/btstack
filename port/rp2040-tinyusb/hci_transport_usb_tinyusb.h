#ifndef HCI_TRANSPORT_USB_TINYUSB_H
#define HCI_TRANSPORT_USB_TINYUSB_H

#include "hci_transport.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * USB HCI transport backed by the TinyUSB host stack.
 *
 * This is currently a skeleton: it initializes and services TinyUSB, but it
 * does not yet claim Bluetooth USB interfaces or transfer HCI packets.
 */
const hci_transport_t * hci_transport_usb_tinyusb_instance(void);

#if defined(__cplusplus)
}
#endif

#endif

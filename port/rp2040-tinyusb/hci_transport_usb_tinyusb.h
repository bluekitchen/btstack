#ifndef HCI_TRANSPORT_USB_TINYUSB_H
#define HCI_TRANSPORT_USB_TINYUSB_H

#include "hci_transport.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * USB HCI transport backed by the TinyUSB host stack.
 *
 * It discovers Bluetooth USB HCI interfaces and transfers HCI command, ACL,
 * and event packets through the TinyUSB host stack.
 */
const hci_transport_t * hci_transport_usb_tinyusb_instance(void);

#if defined(__cplusplus)
}
#endif

#endif

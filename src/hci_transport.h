/*
 *  hci_transport.h
 *
 *  HCI Transport API -- allows hcid to use different transport protcols 
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */
#pragma once

#include <stdint.h>

typedef struct {
    int    (*open)(void *transport_config);
    int    (*close)();
    int    (*send_cmd_packet)(uint8_t *packet, int size);
    int    (*send_acl_packet)(uint8_t *packet, int size);
    void   (*register_event_packet_handler)(void (*handler)(uint8_t *packet, int size));
    void   (*register_acl_packet_handler)  (void (*handler)(uint8_t *packet, int size));
    int    (*get_fd)();      // <-- only used for select(..) call
    int    (*handle_data)(); // -- to be called when select( .. ) returns for the fd
    const char * (*get_transport_name)();
} hci_transport_t;

typedef struct {
    const char *device_name;
    int   baudrate;
    int   flowcontrol; // 
} hci_uart_config_t;

typedef struct {
    // unique usb device identifier
} hci_libusb_config_t;

/*
 *  hci_transport.h
 *
 *  HCI Transport API -- allows BT Daemon to use different transport protcols 
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */
#pragma once

#include <stdint.h>
#include "run_loop.h"

/* HCI packet types */

/**
 * Indicates (first byte) that this pkt to the bluetooth module is a command pkt. The
 * module will send a #HCI_EVENT_PACKET for response.
 */
#define HCI_COMMAND_DATA_PACKET	0x01

/**
 * Indicates (first byte) that this pkt is a acl pkt. Will be sent to and from the module.
 */
#define HCI_ACL_DATA_PACKET	    0x02

/**
 * Indicates (first byte) that this pkt is a sco pkt. Will be sent to and from the module.
 */
#define HCI_SCO_DATA_PACKET	    0x03

/**
 * Indicates (first byte) that this pkt is an event pkt. Only sent by the bluetooth module.
 */
#define HCI_EVENT_PACKET	    0x04

typedef struct {
    int    (*open)(void *transport_config);
    int    (*close)();
    int    (*send_cmd_packet)(uint8_t *packet, int size);
    int    (*send_acl_packet)(uint8_t *packet, int size);
    void   (*register_event_packet_handler)(void (*handler)(uint8_t *packet, int size));
    void   (*register_acl_packet_handler)  (void (*handler)(uint8_t *packet, int size));
    const char * (*get_transport_name)();
} hci_transport_t;

typedef struct {
    const char *device_name;
    uint32_t   baudrate;
    int   flowcontrol; // 
} hci_uart_config_t;

typedef struct {
    // unique usb device identifier
} hci_libusb_config_t;

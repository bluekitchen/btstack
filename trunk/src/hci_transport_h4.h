/*
 *  hci_h4_transport.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */
#pragma once

#include "hci_transport.h"

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



extern hci_transport_t hci_h4_transport;

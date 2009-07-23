/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include "hci_cmds.h"
#include "utils.h"
#include "hci_transport.h"
#include "bt_control.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>


typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

typedef enum {
    HCI_STATE_OFF = 0,
    HCI_STATE_INITIALIZING,
    HCI_STATE_WORKING,
    HCI_STATE_HALTING
} HCI_STATE;

typedef enum {
    SEND_NEGATIVE_LINK_KEY_REQUEST = 1 << 0,
    SEND_PIN_CODE_RESPONSE = 1 << 1
} hci_connection_flags_t;

typedef struct hci_connection {
    // linked list
    struct hci_connection * next;
    
    // remote side
    bd_addr_t address;
    hci_con_handle_t con_handle;
    
    // hci state machine
    hci_connection_flags_t flags;
    
} hci_connection_t;


typedef struct {
    hci_transport_t  * hci_transport;
    bt_control_t     * control;
    void             * config;
    
    uint8_t          * hci_cmd_buffer;
    hci_connection_t * connections;
    
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    uint8_t  num_acl_packets;

    /* callback to L2CAP layer */
    void (*event_packet_handler)(uint8_t *packet, uint16_t size);
    void (*acl_packet_handler)  (uint8_t *packet, uint16_t size);

    /* hci state machine */
    HCI_STATE state;
    uint8_t   substate;
    uint8_t   cmds_ready;
    
} hci_stack_t;


// set up HCI
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control);

void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size));

void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size));
    
// power control
int hci_power_control(HCI_POWER_MODE mode);

/**
 * run the hci daemon loop once
 * 
 * @return 0 or next timeout
 */
uint32_t hci_run();

// create and send hci command packets based on a template and a list of parameters
int hci_send_cmd(hci_cmd_t *cmd, ...);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

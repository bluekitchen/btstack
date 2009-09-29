/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include "hci_transport.h"
#include "bt_control.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

// packet header lenghts
#define HCI_CMD_DATA_PKT_HDR	  0x03
#define HCI_ACL_DATA_PKT_HDR	  0x04
#define HCI_SCO_DATA_PKT_HDR	  0x03
#define HCI_EVENT_PKT_HDR         0x02


// cmds for BTstack 
// get state: @returns HCI_STATE
#define BTSTACK_GET_STATE                                  0x01

// set power mode: @param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02

// set capture mode: @param on
#define BTSTACK_SET_ACL_CAPTURE_MODE                       0x03

// create l2cap channel: @param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20

// disconnect l2cap disconnect, @param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21

// 
#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)

/**
 * Connection State 
 */
typedef enum {
    SEND_NEGATIVE_LINK_KEY_REQUEST = 1 << 0,
    SEND_PIN_CODE_RESPONSE = 1 << 1
} hci_connection_flags_t;

typedef enum {
    SENT_CREATE_CONNECTION = 1,
    RECEIVED_CONNECTION_REQUEST,
    ACCEPTED_CONNECTION_REQUEST,
    REJECTED_CONNECTION_REQUEST,
    OPEN,
    SENT_DISCONNECT
} CONNECTION_STATE;

typedef enum {
    BLUETOOTH_OFF = 1,
    BLUETOOTH_ON,
    BLUETOOTH_ACTIVE
} BLUETOOTH_STATE;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // remote side
    bd_addr_t address;
    
    // module handle
    hci_con_handle_t con_handle;

    // state
    CONNECTION_STATE state;
    
    // errands
    hci_connection_flags_t flags;
    
    // timer
    timer_t timeout;
    struct timeval timestamp;

} hci_connection_t;

/**
 * main data structure
 */
typedef struct {
    // transport component with configuration
    hci_transport_t  * hci_transport;
    void             * config;
    
    // hardware power controller
    bt_control_t     * control;
    
    // list of existing baseband connections
    linked_list_t     connections;

    // single buffer for HCI Command assembly
    uint8_t          * hci_cmd_buffer;
    
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
 * run the hci control loop once
 */
void hci_run();

// create and send hci command packets based on a template and a list of parameters
int hci_send_cmd(hci_cmd_t *cmd, ...);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

// 
void hci_emit_state();
void hci_emit_connection_complete(hci_connection_t *conn);
void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
void hci_emit_nr_connections_changed();
void hci_emit_hci_open_failed();

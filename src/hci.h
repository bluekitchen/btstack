/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

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

// packet sizes
#define HCI_ACL_3DH5_SIZE         1021
#define HCI_ACL_DH5_SIZE           339

// OGFs
#define OGF_LINK_CONTROL          0x01
#define OGF_LINK_POLICY           0x02
#define OGF_CONTROLLER_BASEBAND   0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_BTSTACK 0x3d
#define OGF_VENDOR  0x3f

// cmds for BTstack 
// get state: @returns HCI_STATE
#define BTSTACK_GET_STATE                                  0x01

// set power mode: @param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02

// set capture mode: @param on
#define BTSTACK_SET_ACL_CAPTURE_MODE                       0x03

// get BTstack version
#define BTSTACK_GET_VERSION                                0x04

// get system Bluetooth state
#define BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED               0x05

// set system Bluetooth state
#define BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED               0x06

// create l2cap channel: @param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20

// disconnect l2cap disconnect, @param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21

// register l2cap service: @param psm(16), mtu (16)
#define L2CAP_REGISTER_SERVICE                             0x22

// unregister l2cap disconnect, @param psm(16)
#define L2CAP_UNREGISTER_SERVICE                           0x23

// accept connection @param bd_addr(48), dest cid (16)
#define L2CAP_ACCEPT_CONNECTION                            0x24

// decline l2cap disconnect,@param bd_addr(48), dest cid (16), reason(8)
#define L2CAP_DECLINE_CONNECTION                           0x25

// create l2cap channel: @param bd_addr(48), psm (16), mtu (16)
#define L2CAP_CREATE_CHANNEL_MTU                           0x26

// register SDP Service Record: service record (size)
#define SDP_REGISTER_SERVICE_RECORD                        0x30

// unregister SDP Service Record
#define SDP_UNREGISTER_SERVICE_RECORD                      0x31



// 
#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)

// data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x70

// data: event(8), nr_connections(8)
#define DAEMON_NR_CONNECTIONS_CHANGED                      0x71


/**
 * Connection State 
 */
typedef enum {
    RECV_LINK_KEY_REQUEST          = 0x01,
    SENT_LINK_KEY_REPLY            = 0x02,
    SENT_LINK_KEY_NEGATIVE_REQUEST = 0x04,
    RECV_LINK_KEY_NOTIFICATION     = 0x08,
    RECV_PIN_CODE_REQUEST          = 0x10,
    SENT_PIN_CODE_REPLY            = 0x20, 
    SENT_PIN_CODE_NEGATIVE_REPLY   = 0x40 
} hci_authentication_flags_t;

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
    hci_authentication_flags_t authentication_flags;
    
    // timer
    timer_source_t timeout;
    struct timeval timestamp;

    // ACL packet recombination
    uint8_t  acl_recombination_buffer[HCI_ACL_3DH5_SIZE]; // max packet: DH5 = header(4) + payload (339)
    uint16_t acl_recombination_pos;
    uint16_t acl_recombination_length;
    
    // number ACL packets sent to controller
    uint8_t num_acl_packets_sent;
    
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
    // uint8_t  total_num_cmd_packets;
    uint8_t  total_num_acl_packets;
    uint16_t acl_data_packet_length;

    /* callback to L2CAP layer */
    void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

    /* hci state machine */
    HCI_STATE state;
    uint8_t   substate;
    uint8_t   cmds_ready;
    
} hci_stack_t;

// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr);

// set up HCI
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control);
void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
    
// power control
int hci_power_control(HCI_POWER_MODE mode);

/**
 * run the hci control loop once
 */
void hci_run();

// create and send hci command packets based on a template and a list of parameters
int hci_send_cmd(const hci_cmd_t *cmd, ...);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

hci_connection_t * connection_for_handle(hci_con_handle_t con_handle);
uint8_t hci_number_outgoing_packets(hci_con_handle_t handle);
uint8_t hci_number_free_acl_slots();
int     hci_ready_to_send(hci_con_handle_t handle);
int     hci_authentication_active_for_handle(hci_con_handle_t handle);

// 
void hci_emit_state();
void hci_emit_connection_complete(hci_connection_t *conn);
void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
void hci_emit_nr_connections_changed();
void hci_emit_hci_open_failed();
void hci_emit_btstack_version();
void hci_emit_system_bluetooth_enabled(uint8_t enabled);

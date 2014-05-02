/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#ifndef __HCI_H
#define __HCI_H

#include "btstack-config.h"

#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include "hci_transport.h"
#include "bt_control.h"
#include "remote_device_db.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif
    
// packet header sizes
#define HCI_CMD_HEADER_SIZE          3
#define HCI_ACL_HEADER_SIZE   	     4
#define HCI_SCO_HEADER_SIZE  	     3
#define HCI_EVENT_HEADER_SIZE        2

// packet sizes (max payload)
#define HCI_ACL_DM1_SIZE            17
#define HCI_ACL_DH1_SIZE            27
#define HCI_ACL_2DH1_SIZE           54
#define HCI_ACL_3DH1_SIZE           83
#define HCI_ACL_DM3_SIZE           121
#define HCI_ACL_DH3_SIZE           183
#define HCI_ACL_DM5_SIZE           224
#define HCI_ACL_DH5_SIZE           339
#define HCI_ACL_2DH3_SIZE          367
#define HCI_ACL_3DH3_SIZE          552
#define HCI_ACL_2DH5_SIZE          679
#define HCI_ACL_3DH5_SIZE         1021
       
#define HCI_EVENT_PAYLOAD_SIZE     255
#define HCI_CMD_PAYLOAD_SIZE       255
    
// packet buffer sizes
// HCI_ACL_PAYLOAD_SIZE is configurable and defined in config.h
#define HCI_EVENT_BUFFER_SIZE      (HCI_EVENT_HEADER_SIZE + HCI_EVENT_PAYLOAD_SIZE)
#define HCI_CMD_BUFFER_SIZE        (HCI_CMD_HEADER_SIZE   + HCI_CMD_PAYLOAD_SIZE)
#define HCI_ACL_BUFFER_SIZE        (HCI_ACL_HEADER_SIZE   + HCI_ACL_PAYLOAD_SIZE)
    
// size of hci buffers, big enough for command, event, or acl packet without H4 packet type
// @note cmd buffer is bigger than event buffer
#ifdef HCI_PACKET_BUFFER_SIZE
    #if HCI_PACKET_BUFFER_SIZE < HCI_ACL_BUFFER_SIZE
        #error HCI_PACKET_BUFFER_SIZE must be equal or larger than HCI_ACL_BUFFER_SIZE
    #endif
    #if HCI_PACKET_BUFFER_SIZE < HCI_CMD_BUFFER_SIZE
        #error HCI_PACKET_BUFFER_SIZE must be equal or larger than HCI_CMD_BUFFER_SIZE
    #endif
#else
    #if HCI_ACL_BUFFER_SIZE > HCI_CMD_BUFFER_SIZE
        #define HCI_PACKET_BUFFER_SIZE HCI_ACL_BUFFER_SIZE
    #else
        #define HCI_PACKET_BUFFER_SIZE HCI_CMD_BUFFER_SIZE
    #endif
#endif
    
// OGFs
#define OGF_LINK_CONTROL          0x01
#define OGF_LINK_POLICY           0x02
#define OGF_CONTROLLER_BASEBAND   0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_LE_CONTROLLER 0x08
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

// enable inquiry scan for this client
#define BTSTACK_SET_DISCOVERABLE                           0x07

// set global Bluetooth state
#define BTSTACK_SET_BLUETOOTH_ENABLED                      0x08

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

// Get remote RFCOMM services
#define SDP_CLIENT_QUERY_RFCOMM_SERVICES                   0x32

// Get remote SDP services
#define SDP_CLIENT_QUERY_SERVICES                          0x33

// RFCOMM "HCI" Commands
#define RFCOMM_CREATE_CHANNEL       0x40
#define RFCOMM_DISCONNECT			0x41
#define RFCOMM_REGISTER_SERVICE     0x42
#define RFCOMM_UNREGISTER_SERVICE   0x43
#define RFCOMM_ACCEPT_CONNECTION    0x44
#define RFCOMM_DECLINE_CONNECTION   0x45
#define RFCOMM_PERSISTENT_CHANNEL   0x46
#define RFCOMM_CREATE_CHANNEL_WITH_CREDITS   0x47
#define RFCOMM_REGISTER_SERVICE_WITH_CREDITS 0x48
#define RFCOMM_GRANT_CREDITS                 0x49
    
// 
#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)

// data: event(8)
#define DAEMON_EVENT_CONNECTION_OPENED                     0x50

// data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x51

// data: event(8), nr_connections(8)
#define DAEMON_NR_CONNECTIONS_CHANGED                      0x52

// data: event(8)
#define DAEMON_EVENT_NEW_RFCOMM_CREDITS                    0x53

// data: event()
#define DAEMON_EVENT_HCI_PACKET_SENT                       0x54
    
/**
 * Connection State 
 */
typedef enum {
    AUTH_FLAGS_NONE                = 0x0000,
    RECV_LINK_KEY_REQUEST          = 0x0001,
    HANDLE_LINK_KEY_REQUEST        = 0x0002,
    SENT_LINK_KEY_REPLY            = 0x0004,
    SENT_LINK_KEY_NEGATIVE_REQUEST = 0x0008,
    RECV_LINK_KEY_NOTIFICATION     = 0x0010,
    DENY_PIN_CODE_REQUEST          = 0x0040,
    RECV_IO_CAPABILITIES_REQUEST   = 0x0080,
    SEND_IO_CAPABILITIES_REPLY     = 0x0100,
    SEND_USER_CONFIRM_REPLY        = 0x0200,
    SEND_USER_PASSKEY_REPLY        = 0x0400,

    // pairing status
    LEGACY_PAIRING_ACTIVE          = 0x2000,
    SSP_PAIRING_ACTIVE             = 0x4000,

    // connection status
    CONNECTION_ENCRYPTED           = 0x8000,
} hci_authentication_flags_t;

typedef enum {
    SEND_CREATE_CONNECTION = 0,
    SENT_CREATE_CONNECTION,
    RECEIVED_CONNECTION_REQUEST,
    ACCEPTED_CONNECTION_REQUEST,
    REJECTED_CONNECTION_REQUEST,
    OPEN,
    SENT_DISCONNECT
} CONNECTION_STATE;

typedef enum {
    BONDING_REQUEST_REMOTE_FEATURES   = 0x01,
    BONDING_RECEIVED_REMOTE_FEATURES  = 0x02,
    BONDING_REMOTE_SUPPORTS_SSP       = 0x04,
    BONDING_DISCONNECT_SECURITY_BLOCK = 0x08,
    BONDING_DISCONNECT_DEDICATED_DONE = 0x10,
    BONDING_SEND_AUTHENTICATE_REQUEST = 0x20,
    BONDING_SEND_ENCRYPTION_REQUEST   = 0x40,
    BONDING_DEDICATED                 = 0x80,
} bonding_flags_t;

typedef enum {
    BLUETOOTH_OFF = 1,
    BLUETOOTH_ON,
    BLUETOOTH_ACTIVE
} BLUETOOTH_STATE;

// le central scanning state
typedef enum {
    LE_SCAN_IDLE,
    LE_START_SCAN,
    LE_SCANNING,
    LE_STOP_SCAN,
} le_scanning_state_t;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // remote side
    bd_addr_t address;
    
    // module handle
    hci_con_handle_t con_handle;

    // le public, le random, classic
    bd_addr_type_t address_type;

    // connection state
    CONNECTION_STATE state;
    
    // bonding
    uint16_t bonding_flags;

    // requested security level
    gap_security_level_t requested_security_level;

    // 
    link_key_type_t link_key_type;

    // errands
    uint32_t authentication_flags;

    timer_source_t timeout;
    
#ifdef HAVE_TIME
    // timer
    struct timeval timestamp;
#endif
#ifdef HAVE_TICK
    uint32_t timestamp; // timeout in system ticks
#endif
    
    // ACL packet recombination - ACL Header + ACL payload
    uint8_t  acl_recombination_buffer[4 + HCI_ACL_BUFFER_SIZE];
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
    
    // bsic configuration
    const char         * local_name;
    uint32_t           class_of_device;
    bd_addr_t          local_bd_addr;
    uint8_t            ssp_enable;
    uint8_t            ssp_io_capability;
    uint8_t            ssp_authentication_requirement;
    uint8_t            ssp_auto_accept;

    // hardware power controller
    bt_control_t     * control;
    
    // list of existing baseband connections
    linked_list_t     connections;

    // single buffer for HCI Command assembly
    uint8_t   hci_packet_buffer[HCI_PACKET_BUFFER_SIZE]; // opcode (16), len(8)
    uint8_t   hci_packet_buffer_reserved;
    
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    // uint8_t  total_num_cmd_packets;
    uint8_t  total_num_acl_packets;
    uint16_t acl_data_packet_length;
    uint8_t  total_num_le_packets;
    uint16_t le_data_packet_length;

    /* local supported features */
    uint8_t local_supported_features[8];

    // usable packet types given acl_data_packet_length and HCI_ACL_BUFFER_SIZE
    uint16_t packet_types;
    
    /* callback to L2CAP layer */
    void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

    /* remote device db */
    remote_device_db_t const*remote_device_db;
    
    /* hci state machine */
    HCI_STATE state;
    uint8_t   substate;
    uint8_t   cmds_ready;
    
    uint8_t   discoverable;
    uint8_t   connectable;
    uint8_t   bondable;

    /* buffer for scan enable cmd - 0xff no change */
    uint8_t   new_scan_enable_value;
    
    // buffer for single connection decline
    uint8_t   decline_reason;
    bd_addr_t decline_addr;

    uint8_t   adv_addr_type;
    bd_addr_t adv_address;
    le_scanning_state_t le_scanning_state;
} hci_stack_t;

//*************** le client start

typedef struct le_event {
    uint8_t   type;
} le_event_t;

typedef enum {
    BLE_PERIPHERAL_OK = 0,
    BLE_PERIPHERAL_IN_WRONG_STATE,
    BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS,
    BLE_PERIPHERAL_NOT_CONNECTED,
    BLE_VALUE_TOO_LONG,
    BLE_PERIPHERAL_BUSY,
    BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED,
    BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED
} le_command_status_t;

    
typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

void le_central_register_handler(void (*le_callback)(le_event_t* event));
le_command_status_t le_central_start_scan();
le_command_status_t le_central_stop_scan();
    
//*************** le client end
    
// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr);

void hci_connectable_control(uint8_t enable);
void hci_close(void);

/**
 * run the hci control loop once
 */
void hci_run(void);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

// non-blocking UART driver needs
int hci_can_send_packet_now(uint8_t packet_type);

// same as hci_can_send_packet_now, but also checks if packet buffer is free for use
int hci_can_send_packet_now_using_packet_buffer(uint8_t packet_type);

// reserves outgoing packet buffer. @returns 1 if successful
int  hci_reserve_packet_buffer(void);
void hci_release_packet_buffer(void);

// used for internal checks in l2cap[-le].c
int hci_is_packet_buffer_reserved(void);

// get point to packet buffer
uint8_t* hci_get_outgoing_packet_buffer(void);
    
bd_addr_t * hci_local_bd_addr(void);
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle);
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t *addr, bd_addr_type_t addr_type);
uint8_t  hci_number_outgoing_packets(hci_con_handle_t handle);
uint8_t  hci_number_free_acl_slots(void);
int      hci_authentication_active_for_handle(hci_con_handle_t handle);
uint16_t hci_max_acl_data_packet_length(void);
uint16_t hci_usable_acl_packet_types(void);

// 
void hci_emit_state(void);
void hci_emit_connection_complete(hci_connection_t *conn, uint8_t status);
void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason);
void hci_emit_nr_connections_changed(void);
void hci_emit_hci_open_failed(void);
void hci_emit_btstack_version(void);
void hci_emit_system_bluetooth_enabled(uint8_t enabled);
void hci_emit_remote_name_cached(bd_addr_t *addr, device_name_t *name);
void hci_emit_discoverable_enabled(uint8_t enabled);
void hci_emit_security_level(hci_con_handle_t con_handle, gap_security_level_t level);
void hci_emit_dedicated_bonding_result(hci_connection_t * connection, uint8_t status);

// query if remote side supports SSP
// query if the local side supports SSP
int hci_local_ssp_activated();

// query if the remote side supports SSP
int hci_remote_ssp_supported(hci_con_handle_t con_handle);

// query if both sides support SSP
int hci_ssp_supported_on_both_sides(hci_con_handle_t handle);

// disable automatic l2cap disconnect for testing
void hci_disable_l2cap_timeout_check();

// disconnect because of security block
void hci_disconnect_security_block(hci_con_handle_t con_handle);

/** Embedded API **/

// Set up HCI. Needs to be called before any other function
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control, remote_device_db_t const* remote_device_db);

// Set class of device that will be set during Bluetooth init
void hci_set_class_of_device(uint32_t class_of_device);

// Registers a packet handler. Used if L2CAP is not used (rarely). 
void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));

// Requests the change of BTstack power mode.
int  hci_power_control(HCI_POWER_MODE mode);

// Allows to control if device is discoverable. OFF by default.
void hci_discoverable_control(uint8_t enable);

// Creates and sends HCI command packets based on a template and 
// a list of parameters. Will return error if outgoing data buffer 
// is occupied. 
int hci_send_cmd(const hci_cmd_t *cmd, ...);

// Deletes link key for remote device with baseband address.
void hci_drop_link_key_for_bd_addr(bd_addr_t *addr);

// Configure Secure Simple Pairing

// enable will enable SSP during init
void hci_ssp_set_enable(int enable);

// if set, BTstack will respond to io capability request using authentication requirement
void hci_ssp_set_io_capability(int ssp_io_capability);
void hci_ssp_set_authentication_requirement(int authentication_requirement);

// if set, BTstack will confirm a numberic comparion and enter '000000' if requested
void hci_ssp_set_auto_accept(int auto_accept);

// get addr type and address used in advertisement packets
void hci_le_advertisement_address(uint8_t * addr_type, bd_addr_t * addr);


#if defined __cplusplus
}
#endif

#endif // __HCI_H

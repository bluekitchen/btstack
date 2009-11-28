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
 *  hci_cmds.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>

/**
 * packet types - used in BTstack and over the H4 UART interface
 */
#define HCI_COMMAND_DATA_PACKET	0x01
#define HCI_ACL_DATA_PACKET	    0x02
#define HCI_SCO_DATA_PACKET	    0x03
#define HCI_EVENT_PACKET	    0x04

// extension for client/server communication
#define DAEMON_EVENT_PACKET     0x05

// L2CAP data
#define L2CAP_DATA_PACKET       0x06

// RFCOMM data
#define RFCOMM_DATA_PACKET       0x07


// Events from host controller to host
#define HCI_EVENT_INQUIRY_COMPLETE				           0x01
#define HCI_EVENT_INQUIRY_RESULT				           0x02
#define HCI_EVENT_CONNECTION_COMPLETE			           0x03
#define HCI_EVENT_CONNECTION_REQUEST			           0x04
#define HCI_EVENT_DISCONNECTION_COMPLETE		      	   0x05
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT            0x06
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE	           0x07
#define HCI_EVENT_ENCRIPTION_CHANGE                        0x08
#define HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE      0x09
#define HCI_EVENT_MASTER_LINK_KEY_COMPLETE                 0x0A
#define HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE  0x0B
#define HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE 0x0C
#define HCI_EVENT_QOS_SETUP_COMPLETE			           0x0D
#define HCI_EVENT_COMMAND_COMPLETE				           0x0E
#define HCI_EVENT_COMMAND_STATUS				           0x0F
#define HCI_EVENT_HARDWARE_ERROR                           0x10
#define HCI_EVENT_FLUSH_OCCURED                            0x11
#define HCI_EVENT_ROLE_CHANGE				               0x12
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS	      	   0x13
#define HCI_EVENT_MODE_CHANGE_EVENT                        0x14
#define HCI_EVENT_RETURN_LINK_KEYS                         0x15
#define HCI_EVENT_PIN_CODE_REQUEST                         0x16
#define HCI_EVENT_LINK_KEY_REQUEST                         0x17
#define HCI_EVENT_LINK_KEY_NOTIFICATION                    0x18
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                     0x1A
#define HCI_EVENT_MAX_SLOTS_CHANGED			               0x1B
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE               0x1C
#define HCI_EVENT_PACKET_TYPE_CHANGED                      0x1D
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI		      	   0x22
#define HCI_EVENT_EXTENDED_INQUIRY_RESPONSE                0x2F
#define HCI_EVENT_VENDOR_SPECIFIC				           0xFF

// last used HCI_EVENT in 2.1 is 0x3d

// events 0x50-0x5f are used internally

// events from BTstack for application/client lib
#define BTSTACK_EVENT_STATE                                0x60

// data: event(8), len(8), nr hci connections
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED               0x61

// data: none
#define BTSTACK_EVENT_POWERON_FAILED                       0x62

// data: event (8), len(8), status (8), address(48), handle (16), psm (16), source_cid(16), dest_cid (16) 
#define L2CAP_EVENT_CHANNEL_OPENED                         0x70

// data: event (8), len(8), channel (16)
#define L2CAP_EVENT_CHANNEL_CLOSED                         0x71

// data: event(8), len(8), handle(16)
#define L2CAP_EVENT_TIMEOUT_CHECK                          0x72

// last HCI error is 0x3d
// l2cap errors - enumeration by the command that created them
#define L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD 0x60
#define L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED 0x61
#define L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST 0x62

#define L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL        0x63
#define L2CAP_CONNECTION_RESPONSE_RESULT_PENDING           0x64
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM       0x65
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY  0x66
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES 0x65

#define L2CAP_CONFIG_RESPONSE_RESULT_SUCCESSFUL            0x66
#define L2CAP_CONFIG_RESPONSE_RESULT_UNACCEPTABLE_PARAMS   0x67
#define L2CAP_CONFIG_RESPONSE_RESULT_REJECTED              0x68
#define L2CAP_CONFIG_RESPONSE_RESULT_UNKNOWN_OPTIONS       0x69

/**
 * Default INQ Mode
 */
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)
/**
 *  Hardware state of Bluetooth controller 
 */
typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

/**
 * State of BTstack 
 */
typedef enum {
    HCI_STATE_OFF = 0,
    HCI_STATE_INITIALIZING,
    HCI_STATE_WORKING,
    HCI_STATE_HALTING
} HCI_STATE;

/** 
 * compact HCI Command packet description
 */
 typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;


// HCI Commands - see hci_cmds.c for info on parameters
extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_inquiry_cancel;
extern hci_cmd_t hci_link_key_request_negative_reply;
extern hci_cmd_t hci_pin_code_request_reply;
extern hci_cmd_t hci_authentication_requested;
extern hci_cmd_t hci_set_event_mask;
extern hci_cmd_t hci_reset;
extern hci_cmd_t hci_create_connection;
extern hci_cmd_t hci_disconnect;
extern hci_cmd_t hci_host_buffer_size;
extern hci_cmd_t hci_write_authentication_enable;
extern hci_cmd_t hci_write_local_name;
extern hci_cmd_t hci_write_page_timeout;
extern hci_cmd_t hci_write_class_of_device;
extern hci_cmd_t hci_remote_name_request;
extern hci_cmd_t hci_remote_name_request_cancel;
extern hci_cmd_t hci_read_bd_addr;
extern hci_cmd_t hci_delete_stored_link_key;
extern hci_cmd_t hci_write_scan_enable;
extern hci_cmd_t hci_accept_connection_request;
extern hci_cmd_t hci_write_inquiry_mode;
extern hci_cmd_t hci_write_extended_inquiry_response;
extern hci_cmd_t hci_write_simple_pairing_mode;

// BTSTACK client/server commands - see hci.c for info on parameters
extern hci_cmd_t btstack_get_state;
extern hci_cmd_t btstack_set_power_mode;
extern hci_cmd_t btstack_set_acl_capture_mode;
// L2CAP client/server commands - see l2cap.c for info on parameters
extern hci_cmd_t l2cap_create_channel;
extern hci_cmd_t l2cap_disconnect;

/*
 *  hci_cmds.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

// packet header lenghts
#define HCI_CMD_DATA_PKT_HDR	  0x03
#define HCI_ACL_DATA_PKT_HDR	  0x04
#define HCI_SCO_DATA_PKT_HDR	  0x03
#define HCI_EVENT_PKT_HDR         0x02

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

// get HCI CMD OGF
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

// OGFs
#define OGF_LINK_CONTROL 0x01
#define OGF_CONTROLLER_BASEBAND 0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_BTSTACK 0x3d
#define OGF_VENDOR  0x3f

// cmds for BTstack 

// get state: @returns HCI_STATE
#define HCI_BTSTACK_GET_STATE                              0x01

// set power mode: @param HCI_POWER_MODE
#define HCI_BTSTACK_SET_POWER_MODE                         0x02

// create l2cap channel: @param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x03

// disconnect l2cap disconnect, @param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x04

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
#define NECTEVENT_ION_PACKET_TYPE_CHANGED                  0x1D
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI		      	   0x22
#define HCI_EVENT_VENDOR_SPECIFIC				           0xFF

// events from BTstack for application/client lib
#define HCI_EVENT_BTSTACK_WORKING                          0x80
#define HCI_EVENT_BTSTACK_STATE                            0x81

// data: event (8), len(8), address(48), handle (16), psm (16), source_cid(16), dest_cid (16) 
#define HCI_EVENT_L2CAP_CHANNEL_OPENED                     0x82

// data: event (8), len(8), channel (16)
#define HCI_EVENT_L2CAP_CHANNEL_CLOSED                     0x83

// data: event(8), len(8), handle(16)
#define HCI_EVENT_L2CAP_TIMEOUT_CHECK                      0x84


#define COMMAND_COMPLETE_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_COMPLETE && READ_BT_16(event,3) == cmd.opcode)

#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)

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

// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, va_list argptr);


// HCI Commands - see hci_cmds.c for info on parameters
extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_inquiry_cancel;
extern hci_cmd_t hci_link_key_request_negative_reply;
extern hci_cmd_t hci_pin_code_request_reply;
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
extern hci_cmd_t l2cap_create_channel;
extern hci_cmd_t l2cap_disconnect;

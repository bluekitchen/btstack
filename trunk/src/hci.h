/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "hci_transport.h"
#include "bt_control.h"

// helper for BT little endian format
#define READ_BT_16( buffer, pos) (buffer[pos] | (buffer[pos+1] << 8))
#define READ_BT_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16))
#define READ_BT_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16) | (((uint32_t) buffer[pos+3])) << 24)

// packet header lengh
#define HCI_CMD_DATA_PKT_HDR	  0x03
#define HCI_ACL_DATA_PKT_HDR	  0x04
#define HCI_SCO_DATA_PKT_HDR	  0x03
#define HCI_EVENT_PKT_HDR         0x02

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
#define BTSTACK_EVENT_HCI_WORKING                          0x80

#define COMMAND_COMPLETE_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_COMPLETE && READ_BT_16(event,3) == cmd.opcode)


/**
 * Default INQ Mode
 */
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)

/**
 * @brief Length of a bluetooth device address.
 */
#define BD_ADDR_LEN 6
typedef uint8_t bd_addr_t[BD_ADDR_LEN];

/**
 * @brief The link key type
 */
#define LINK_KEY_LEN 16
typedef uint8_t link_key_t[LINK_KEY_LEN]; 

/**
 * @brief hci connection handle type
 */
typedef uint16_t hci_con_handle_t;

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

typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;

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
    void (*event_packet_handler)(uint8_t *packet, int size);
    void (*acl_packet_handler)  (uint8_t *packet, int size);

    /* hci state machine */
    HCI_STATE state;
    uint8_t   substate;
    uint8_t   cmds_ready;
    
} hci_stack_t;


// set up HCI
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control);

void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, int size));

void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size));
    
// power control
int hci_power_control(HCI_POWER_MODE mode);

/**
 * run the hci daemon loop once
 * 
 * @return 0 or next timeout
 */
uint32_t hci_run();

//
void hexdump(void *data, int size);

// create and send hci command packet based on a template and a list of parameters
int hci_send_cmd(hci_cmd_t *cmd, ...);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

// helper
extern void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
extern void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);
extern void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

// HCI Commands - see hci.c for info on parameters
extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_inquiry_cancel;
extern hci_cmd_t hci_link_key_request_negative_reply;
extern hci_cmd_t hci_pin_code_request_reply;
extern hci_cmd_t hci_reset;
extern hci_cmd_t hci_create_connection;
extern hci_cmd_t hci_host_buffer_size;
extern hci_cmd_t hci_write_authentication_enable;
extern hci_cmd_t hci_write_page_timeout;
extern hci_cmd_t hci_read_bd_addr;


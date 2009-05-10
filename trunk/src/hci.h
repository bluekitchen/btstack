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


// helper for BT little endian format
#define READ_BT_16( buffer, pos) (buffer[pos] | (buffer[pos+1] << 8))
#define READ_BT_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16))
#define READ_BT_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16) | (((uint32_t) buffer[pos+3])) << 24)


// packet headers
#define HCI_CMD_DATA_PKT_HDR	  0x03
#define HCI_ACL_DATA_PKT_HDR	  0x04
#define HCI_SCO_DATA_PKT_HDR	  0x03
#define HCI_EVENT_PKT_HDR         0x02

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


typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;


// set up HCI
void hci_init(hci_transport_t *transport, void *config);

// power control
int hci_power_control(HCI_POWER_MODE mode);

// run the hci daemon loop
void hci_run();

//
void hexdump(uint8_t *data, int size);

// create and send hci command packet based on a template and a list of parameters
int hci_send_cmd(hci_cmd_t *cmd, ...);

extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_reset;
extern hci_cmd_t hci_create_connection;
extern hci_cmd_t hci_host_buffer_size;
extern hci_cmd_t hci_write_page_timeout;
    
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)

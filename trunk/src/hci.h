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

typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;

typedef uint8_t bd_addr_t[6];

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

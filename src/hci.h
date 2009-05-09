/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include <stdint.h>

#include "hci_transport.h"

typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

typedef struct {
    uint8_t  ogf;
    uint16_t ocf;
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

// create hci command packet based on a template and a list of parameters
void hci_create_cmd_packet(uint8_t *buffer, uint8_t *cmd_len, hci_cmd_t *cmd, ...);

int hci_send_cmd_packet(uint8_t *buffer, int size);

extern hci_cmd_t hci_inquiry;
extern hci_cmd_t hci_reset;
    
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)

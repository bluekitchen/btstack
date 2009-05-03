/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include "hci_transport.h"

typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON 
} HCI_POWER_MODE;

// set up HCI
void hci_init(hci_transport_t *transport, void *config);

// power control
int hci_power_control(HCI_POWER_MODE mode);

// run the hci daemon loop
void hci_run();


/*
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include "hci_transport.h"

// run the hci daemon loop
void hci_run(hci_transport_t *transport, void *config);


/*
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include <unistd.h>

#include "hci.h"

static hci_transport_t *hci_transport;

void hci_init(hci_transport_t *transport, void *config){
    
    // reference to use transport layer implementation
    hci_transport = transport;
    
    // open unix socket
    
    // wait for connections
    
    // enter loop
    
    // handle events
}

int hci_power_control(HCI_POWER_MODE power_mode){
    return 0;
}

int hci_send_cmd(char *buffer, int size){
    return hci_transport->send_cmd_packet(buffer, size);
}

void hci_run(){
    while (1) {
        //  construct file descriptor set to wait for
        //  select
        
        // for each ready file in FD - call handle_data
        sleep(1);
    }
}
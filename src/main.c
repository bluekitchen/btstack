/*
 *  main.c
 * 
 *  Simple tests
 * 
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <stdio.h>
#include <stdlib.h>

#include "hci.h"
#include "hci_h4_transport.h"

int main (int argc, const char * argv[]) {
    
    // 
    if (argc <= 1){
        printf("HCI Daemon tester. Specify device name for Ericsson ROK 101 007\n");
        exit(1);
    }
    
    // H4 UART
    hci_transport_t * transport = &hci_h4_transport;

    // Ancient Ericsson ROK 101 007 (ca. 2001)
    hci_uart_config_t config;
    config.device_name = argv[1];
    config.baudrate    = 57600;
    config.flowcontrol = 1;

    // Go
    hci_run(transport, &config);
    
    return 0;
}

/*
 *  main.c
 * 
 *  Simple tests
 * 
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>

#include "hci.h"
#include "hci_transport_h4.h"

static hci_transport_t * transport;
static hci_uart_config_t config;


#if 0
static void *hci_daemon_thread(void *arg){
    printf("HCI Daemon started\n");
    hci_run(transport, &config);
    return NULL;
}
#endif


int main (int argc, const char * argv[]) {
    
    // 
    if (argc <= 1){
        printf("HCI Daemon tester. Specify device name for Ericsson ROK 101 007\n");
        exit(1);
    }
    
    // H4 UART
    transport = &hci_h4_transport;

    // Ancient Ericsson ROK 101 007 (ca. 2001)
    config.device_name = argv[1];
    config.baudrate    = 57600;
    config.flowcontrol = 1;

#if 0
    // create and start HCI daemon thread
    pthread_t new_thread;
    pthread_create(&new_thread, NULL, hci_daemon_thread, NULL);

    // open UNIX domain socket
    while(1){
        sleep(1);
        printf(".\n");
    }
    
#endif

    hci_init(transport, &config);
    hci_power_control(HCI_POWER_ON);
    // 
    // register callback
    //
    hci_run();    
    return 0;
}

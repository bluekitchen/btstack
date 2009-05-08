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

void hexdump(uint8_t *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", data[i]);
    }
    printf("\n");
}

void event_handler(uint8_t *packet, int size){
    printf("Event received, size %u\n", size );
    hexdump( packet, size);
    
    // 
    if (packet[3] == 3 && packet[4] == 12){
        // reset done, send inq
        uint8_t inq[] = {  0x01, 0x04, 0x05, 0x33,  0x8b,  0x9e, 0x0a, 0x14};
        transport->send_cmd_packet( inq, sizeof(inq) );
    }
}


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

    // hci_init(transport, &config);
    // hci_power_control(HCI_POWER_ON);
    
    // open low-level device
    transport->open(&config);
    
    // 
    // register callbacks
    //
    // hci_run();    
    transport->register_event_packet_handler(&event_handler);

    // get fd for select call
    int transport_fd = transport->get_fd();
    
    // send hci reset
    uint8_t reset[] = { 0x03, 0x0c, 0x00 };
    transport->send_cmd_packet( reset, sizeof(reset) );
    
    // 
    fd_set descriptors;
    FD_ZERO(&descriptors);
    while (1){
        FD_SET(transport_fd, &descriptors);
        // int ready = 
        select( transport_fd+1, &descriptors, NULL, NULL, NULL);
        // printf("Ready: %d, isset() = %u\n", ready, FD_ISSET(transport_fd, &descriptors));
        transport->handle_data();
    }
    
    return 0;
}

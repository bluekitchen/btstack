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
static uint8_t buffer [200];

#define COMMAND_COMPLETE_EVENT(event,cmd) ( event[0] == 0x0e && (event[3] | (event[4] << 8)) == cmd.opcode)

#if 0
// reset done, send host buffer size
hci_create_cmd_packet( buffer, &len, &hci_host_buffer_size, 400, 255, 1, 0, 0);
hci_send_cmd_packet( buffer, len );
// reset done, send inq
hci_create_cmd_packet( buffer, &len, &hci_inquiry, HCI_INQUIRY_LAP, 30, 0);
hci_send_cmd_packet( buffer, len );
#endif
void event_handler(uint8_t *packet, int size){
    uint8_t len;
    // printf("Event type: %x, opcode: %x, other %x\n", packet[0], packet[3] | packet[4] << 8);
    if ( COMMAND_COMPLETE_EVENT(packet, hci_reset) ) {
        // reset done, write page timeout
        hci_create_cmd_packet( buffer, &len, &hci_write_page_timeout, 0x6000); // ca. 15 sec
        hci_send_cmd_packet( buffer, len );
    }

    if ( COMMAND_COMPLETE_EVENT(packet, hci_host_buffer_size) ) {
    }

    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_page_timeout) ) {
        // hci_host_buffer_size done, send connect
        // bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };
        bd_addr_t addr = { 0x00, 0x16, 0xcb, 0x09, 0x94, 0xa9};
        hci_create_cmd_packet( buffer, &len, &hci_create_connection, &addr, 0x18, 0, 0, 0, 0);
        hci_send_cmd_packet( buffer, len );
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
    
    hci_init(transport, &config);
    hci_power_control(HCI_POWER_ON);
    
    // open low-level device
    transport->open(&config);
    
    // 
    // register callbacks
    //
    transport->register_event_packet_handler(&event_handler);

    // get fd for select call
    int transport_fd = transport->get_fd();
    
    // send hci reset
    uint8_t len;
    hci_create_cmd_packet( buffer, &len, &hci_reset);
    hci_send_cmd_packet( buffer, len );

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

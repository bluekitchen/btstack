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

#include "hci.h"
#include "hci_transport_h4.h"
#include "hci_dump.h"
#include "bt_control_iphone.h"

#include "l2cap.h"

static hci_transport_t * transport;
static hci_uart_config_t config;

hci_con_handle_t con_handle= 0;
uint16_t dest_cid;

void event_handler(uint8_t *packet, int size){
    // printf("Event type: %x, opcode: %x, other %x\n", packet[0], packet[3] | packet[4] << 8);

#if 0
    // bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
    // bd_addr_t addr = { 0x00, 0x16, 0xcb, 0x09, 0x94, 0xa9};  // MacBook Pro

    // bt stack activated, set authentication enabled
    if (packet[0] == BTSTACK_EVENT_HCI_WORKING) {
        hci_send_cmd(&hci_write_authentication_enable, 1);
        // hci_send_cmd(&hci_host_buffer_size, 400, 255, 1, 0, 0);
    }
    
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
        // hci_write_authentication_enable done, send connect
        hci_send_cmd(&hci_create_connection, &addr, 0x18, 0, 0, 0, 0);
    }

    if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
        printf("Please enter PIN 1234 on remote device\n");
    }
    
    // connection established -> start L2CAP conection
    if (packet[0] == HCI_EVENT_CONNECTION_COMPLETE){
        if (packet[2] == 0){
            // get new connection handle
            con_handle = READ_BT_16(packet, 3);
            
            // request l2cap connection
            printf("> CONNECTION REQUEST\n");
            l2cap_send_signaling_packet(con_handle, CONNECTION_REQUEST, sig_seq_nr++, 0x13, local_cid);
        }
    }
#else
    if (packet[0] == BTSTACK_EVENT_HCI_WORKING) {
        hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, 10, 0);
    }
    if (packet[0] == HCI_EVENT_INQUIRY_COMPLETE){
        // power cycle
        hci_power_control( HCI_POWER_OFF );
        // 
        printf ("Power off!\n");
        sleep(10);
        printf ("Restart!\n");
        hci_power_control( HCI_POWER_ON );
    }
#endif
    
}

void acl_handler(uint8_t *packet, int size){
    uint16_t source_cid;
    uint16_t result;
    uint8_t config_options[] = { 1, 2, 150, 0}; // mtu = 48 
    // connection response 
    if (packet[8] == CONNECTION_RESPONSE){
        dest_cid = READ_BT_16(packet, 12);
        source_cid = READ_BT_16(packet, 14);
        result = READ_BT_16(packet, 16);
        uint16_t status = READ_BT_16(packet, 18);
        printf("< CONNECTION_RESPONSE: id %u, dest cid %u, src cid %u, result %u, status %u\n", packet[9], dest_cid, source_cid, result, status); 
        if (result == 0){
            printf("> CONFIGURE_REQUEST: id %u\n", sig_seq_nr);
            l2cap_send_signaling_packet(con_handle, CONFIGURE_REQUEST, sig_seq_nr++, dest_cid, 0, 4, &config_options);
        }
    } 
    else if (packet[8] == CONFIGURE_RESPONSE){
        source_cid = READ_BT_16(packet, 12);
        uint16_t flags = READ_BT_16(packet, 14);
        result = READ_BT_16(packet, 16);
        printf("< CONFIGURE_RESPONSE: id %u, src cid %u, flags %u, result %u!!!\n", packet[9], source_cid, flags, result);
        hexdump(&packet[18], size-18);
    }
    else if (packet[8] == CONFIGURE_REQUEST){
        printf("< CONFIGURE_REQUEST: id %u\n", packet[9]);
        hexdump(&packet[16], size-16);
        printf("> CONFIGURE_RESPONSE: id %u\n", packet[9]);
        l2cap_send_signaling_packet(con_handle, CONFIGURE_RESPONSE, packet[9], local_cid, 0, 0, size - 16, &packet[16]);
    }
    else {
        printf("Unknown ACL ^^^ \n");
    }
}

int main (int argc, const char * argv[]) {
    
    bt_control_t * control = NULL;
    
#if 0
    // 
    if (argc <= 1){
        printf("HCI Daemon tester. Specify device name for Ericsson ROK 101 007\n");
        exit(1);
    }

    // Ancient Ericsson ROK 101 007 (ca. 2001)
    config.device_name = argv[1];
    config.baudrate    = 57600;
    config.flowcontrol = 1;
#else 
    // iPhone
    config.device_name = "/dev/tty.bluetooth";
    config.baudrate    = 115200;
//    config.baudrate    = 230400;  // ok
//    config.baudrate    = 460800;  // ok
//    config.baudrate    = 921600;  // ok
//    config.baudrate    = 2400000; // does not work (yet)
    config.flowcontrol = 1;
    control = &bt_control_iphone;
#endif
    
    // use logger: format HCI_DUMP_PACKETLOGGER or HCI_DUMP_BLUEZ
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    
    // H4 UART
    transport = &hci_transport_h4;

    // init HCI
    hci_init(transport, &config, control);

    // 
    // register callbacks
    //
    hci_register_event_packet_handler(&event_handler);
    hci_register_acl_packet_handler(&acl_handler);

    // init L2CAP
    l2cap_init();
         
    // 
    fd_set descriptors;
    FD_ZERO(&descriptors);
    while (1){
        // handle HCI
        hci_run();
        
        // get fd for select call
        int transport_fd = transport->get_fd();
        FD_SET(transport_fd, &descriptors);
        // int ready = 
        select( transport_fd+1, &descriptors, NULL, NULL, NULL);
        // printf("Ready: %d, isset() = %u\n", ready, FD_ISSET(transport_fd, &descriptors));

        // handle incoming data from BT module
        transport->handle_data();
    }
    
    return 0;
}

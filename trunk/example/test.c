/*
 *  test.c
 *  project
 *
 *  Created by Matthias Ringwald on 7/14/09.
 *  Copyright 2009 Dybuster AG. All rights reserved.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "../src/btstack.h"
#include "../src/linked_list.h"
#include "../src/run_loop.h"
#include "../src/socket_server.h"
#include "../src/hci.h"
#include "../src/l2cap.h"

hci_con_handle_t con_handle_out = 0;
hci_con_handle_t con_handle_in = 0;
uint16_t dest_cid;

#define BT_HID
// #define POWER_CYCLE_TEST
// #define MITM


void event_handler(uint8_t *packet, int size){
    // printf("Event type: %x, opcode: %x, other %x\n", packet[0], packet[3] | packet[4] << 8);
    
#if defined(BT_HID) || defined(MITM)
    bd_addr_t addr = {0x00, 0x03, 0xc9, 0x3d, 0x77, 0x43 };  // Think Outside Keyboard
    
    // bt stack activated, get started
    if (packet[0] == HCI_EVENT_BTSTACK_WORKING || packet[0] == HCI_EVENT_BTSTACK_STATE) {
        bt_send_cmd(&hci_write_class_of_device, 0x7A020C);  // used on iPhone
    }
    
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_class_of_device) ) {
        bt_send_cmd(&hci_write_local_name, "BT in the Middle");
    }
    
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_local_name) ) {
#if 1
        bt_send_cmd(&hci_write_authentication_enable, 1);
    }
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable) ) {
#endif
#if 0
        bt_send_cmd(&hci_write_inquiry_mode, 2);
    }
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
#endif
#if 0
        bt_send_cmd(&hci_set_event_mask, 0xffffffff, 0x1fffffff);
    }
    
    uint8_t eir[] = {
        0x0d, 0x09, 0x4d, 0x69, 0x6c, 0x61, 0x73, 0x20, 0x69, 0x50, 0x68, 0x6f, 0x6e, 0x65, 0x0f, 0x02,
        0x00, 0x12, 0x1f, 0x11, 0x07, 0x11, 0x2f, 0x11, 0x0a, 0x11, 0x16, 0x11, 0x15, 0x11, 0x27, 0xff,
        0x00, 0x4c, 0x02, 0x24, 0x02, 0x86, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    if ( COMMAND_COMPLETE_EVENT(packet, hci_set_event_mask) ) {
#endif
#if 0
        bt_send_cmd(&hci_write_extended_inquiry_response, 0, eir);
    }
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_extended_inquiry_response) ) {
#endif
#if 0
        bt_send_cmd(&hci_write_simple_pairing_mode, 1);
    }
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_simple_pairing_mode) ) {
#endif
#if 0
        bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, 15, 0);
    }
    if (packet[0] == HCI_EVENT_INQUIRY_COMPLETE)
#endif
#if 1
        bt_send_cmd(&hci_create_connection, &addr, 0x18, 0, 0, 0, 0);
#endif
}
if (packet[0] == HCI_EVENT_CONNECTION_REQUEST){
    bt_flip_addr(addr, &packet[2]); 
    bt_send_cmd(&hci_accept_connection_request, &addr, 1);
}

if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
    printf("Please enter PIN 1234 on remote device\n");
}

// connection established -> start L2CAP conection
if (packet[0] == HCI_EVENT_CONNECTION_COMPLETE){
    if (packet[2] == 0){
        // get new connection handle
        if (! con_handle_out) {
            con_handle_out = READ_BT_16(packet, 3);
        } else {
            con_handle_in  = READ_BT_16(packet, 3);
        }
#ifndef MITM
        // request l2cap connection
        printf("> CONNECTION REQUEST\n");
        bt_send_l2cap_signaling_packet(con_handle_out, CONNECTION_REQUEST, sig_seq_nr++, 0x13, local_cid);
#else
        printf("Connected to target device, please start!\n");
#endif
        
    }
}
#endif 

#ifdef POWER_CYCLE_TEST
if (packet[0] == HCI_EVENT_BTSTACK_WORKING) {
    bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, 10, 0);
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
    
#ifdef MITM
    // forward packet on "other" connection
    hci_con_handle_t incoming_hdl = READ_BT_16(packet, 0) & 0xfff;
    hci_con_handle_t outgoing_hdl = 0;
#if 0
    if  (incoming_hdl == con_handle_in){
        outgoing_hdl = con_handle_out;
    } else if (incoming_hdl == con_handle_out){
        outgoing_hdl = con_handle_in;
    } else {
        printf("Incoming acl packet on handle %u\n!", incoming_hdl);
    }
#else
    outgoing_hdl = incoming_hdl;
#endif
    if (outgoing_hdl){
        bt_store_16( packet, 0, (READ_BT_16(packet, 0) & 0xf000) | outgoing_hdl);
        bt_send_acl_packet(packet, size);
    }
    
#else
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
            bt_send_l2cap_signaling_packet(con_handle_out, CONFIGURE_REQUEST, sig_seq_nr++, dest_cid, 0, 4, &config_options);
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
        bt_send_l2cap_signaling_packet(con_handle_out, CONFIGURE_RESPONSE, packet[9], local_cid, 0, 0, size - 16, &packet[16]);
    }
    else {
        printf("Unknown ACL ^^^ \n");
    }
#endif
}

int main (int argc, const char * argv[]){
	bt_open();
	bt_register_event_packet_handler(event_handler);
	bt_register_acl_packet_handler  (acl_handler);
	bt_send_cmd(&hci_get_btstack_state);
	run_loop_execute();
	bt_close();
}
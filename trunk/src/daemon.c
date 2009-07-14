/*
 *  daemon.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack background daemon
 *
 */

#include "../config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "linked_list.h"
#include "run_loop.h"
#include "socket_server.h"

#ifdef USE_BLUETOOL
#include "bt_control_iphone.h"
#endif

#ifdef HAVE_TRANSPORT_H4
#include "hci_transport_h4.h"
#endif

#ifdef HAVE_TRANSPORT_USB
#include <libusb-1.0/libusb.h>
#include "hci_transport_usb.h"
#endif

static hci_transport_t * transport;
static hci_uart_config_t config;

int main (int argc, const char * argv[]){
    
    bt_control_t * control = NULL;

#ifdef HAVE_TRANSPORT_H4
    transport = hci_transport_h4_instance();
    config.device_name = UART_DEVICE;
    config.baudrate    = UART_SPEED;
    config.flowcontrol = 1;
#endif

#ifdef HAVE_TRANSPORT_USB
    transport = hci_transport_usb_instance();
#endif

#ifdef USE_BLUETOOL
    control = &bt_control_iphone;
#endif

    // @TODO allow configuration per HCI CMD
    
    // use logger: format HCI_DUMP_PACKETLOGGER or HCI_DUMP_BLUEZ
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    
    // init HCI
    hci_init(transport, &config, control);
    
    // init L2CAP
    l2cap_init();

    // 
    // register callbacks
    //
    hci_register_event_packet_handler(&socket_server_send_event_all);
    hci_register_acl_packet_handler(&socket_server_send_acl_all);
    
    // @TODO allow control per HCI CMD
    // turn on 
    hci_power_control(HCI_POWER_ON);
    
    // @TODO make choice of socket server configurable (TCP and/or Unix Domain Socket)
    // @TODO make port and/or socket configurable per config.h
    
    // create server
    socket_server_create_tcp(1919);
    
    // go!
    run_loop_execute();

    return 0;
}
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
#include "socket_connection.h"

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

static int btstack_command_handler(connection_t *connection, uint8_t *packet, uint16_t size){
    // BTstack Commands
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 1, packet, size);
    bd_addr_t addr;
    uint16_t cid;
    uint16_t psm;
    uint8_t reason;
    // BTstack internal commands - 16 Bit OpCode, 8 Bit ParamLen, Params...
    switch (READ_CMD_OCF(packet)){
        case HCI_BTSTACK_GET_STATE:
            hci_emit_state();
            break;
        case HCI_BTSTACK_SET_POWER_MODE:
            hci_power_control(packet[3]);
            break;
        case L2CAP_CREATE_CHANNEL:
            bt_flip_addr(addr, &packet[3]);
            psm = READ_BT_16(packet, 9);
            l2cap_create_channel_internal( connection, addr, psm );
            break;
        case L2CAP_DISCONNECT:
            cid = READ_BT_16(packet, 3);
            reason = packet[5];
            l2cap_disconnect_internal(cid, reason);
            break;
        default:
            //@TODO: log into hci dump as vendor specific "event"
            printf("Error: command %u not implemented\n:", READ_CMD_OCF(packet));
            break;
    }
    return 0;
}

static int daemon_client_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            if (READ_CMD_OGF(data) != OGF_BTSTACK) { 
                // HCI Command
                hci_send_cmd_packet(data, length);
            } else {
                // BTstack command
                btstack_command_handler(connection, data, length);
            }
            break;
        case HCI_ACL_DATA_PACKET:
            // process l2cap packet...
            channel = READ_BT_16(data, 0);
            l2cap_send_internal(channel, data, length);
            break;
    }
    return 0;
}

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

    // @TODO: allow configuration per HCI CMD
    
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    // hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    hci_dump_open(NULL, HCI_DUMP_STDOUT);
                  
    // init HCI
    hci_init(transport, &config, control);
    
    // init L2CAP
    l2cap_init();
            
    // @TODO: make choice of socket server configurable (TCP and/or Unix Domain Socket)
    // @TODO: make port and/or socket configurable per config.h
    
    // create server
    socket_connection_create_tcp(BTSTACK_PORT);
    socket_connection_register_packet_callback(daemon_client_handler);
    
    // go!
    run_loop_execute();

    return 0;
}
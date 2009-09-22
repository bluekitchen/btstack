/*
 *  daemon.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack background daemon
 *
 */

#include "../config.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "linked_list.h"
#include "run_loop.h"
#include "socket_connection.h"

#ifdef USE_BLUETOOL
#include "bt_control_iphone.h"
#endif

#ifdef USE_SPRINGBOARD
#include "platform_iphone.h"
#endif

#ifdef HAVE_TRANSPORT_H4
#include "hci_transport_h4.h"
#endif

#ifdef HAVE_TRANSPORT_USB
#include <libusb-1.0/libusb.h>
#include "hci_transport_usb.h"
#endif

#define DAEMON_NO_CONNECTION_TIMEOUT 60

static hci_transport_t * transport;
static hci_uart_config_t config;

static timer_t timeout;

static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state){
    printf("Bluetooth status: %u\n", state);
};
static void (*bluetooth_status_handler)(BLUETOOTH_STATE state) = dummy_bluetooth_status_handler;

static void daemon_no_connections_timeout(){
#ifdef USE_LAUNCHD
    printf("No connection for %u seconds -> POWER OFF and quit\n", DAEMON_NO_CONNECTION_TIMEOUT);
    hci_power_control( HCI_POWER_OFF);
    exit(0);
#else
    printf("No connection for %u seconds -> POWER OFF\n", DAEMON_NO_CONNECTION_TIMEOUT);
    hci_power_control( HCI_POWER_OFF);
#endif
}

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
        case HCI_BTSTACK_SET_ACL_CAPTURE_MODE:
            if (packet[3]) {
                l2cap_set_capture_connection(connection);
            } else {
                l2cap_set_capture_connection(NULL);
            }
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
            fprintf(stderr, "Error: command %u not implemented\n:", READ_CMD_OCF(packet));
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
            hci_send_acl_packet(data, length);
            break;
        case L2CAP_DATA_PACKET:
            // process l2cap packet...
            l2cap_send_internal(channel, data, length);
            break;
        case DAEMON_EVENT_PACKET:
            switch (data[0]) {
                case DAEMON_CONNECTION_CLOSED:
                    l2cap_close_channels_for_connection(connection);
                    break;
                case DAEMON_NR_CONNECTIONS_CHANGED:
                    printf("Nr Connections changed, new %u\n", data[1]);
                    if (data[1]) {
                        run_loop_remove_timer(&timeout);
                    } else {
                        run_loop_set_timer(&timeout, DAEMON_NO_CONNECTION_TIMEOUT);
                        run_loop_add_timer(&timeout);
                    }
                default:
                    break;
            }
            break;
    }
    return 0;
}

static void daemon_event_handler(uint8_t *packet, uint16_t size){
    // handle state event
    if (packet[0] == HCI_EVENT_BTSTACK_WORKING){
        bluetooth_status_handler(BLUETOOTH_ON);
    }
    if (packet[0] == HCI_EVENT_BTSTACK_STATE){
        if (packet[2] == HCI_STATE_WORKING) {
            bluetooth_status_handler(BLUETOOTH_ON);
        }
        if (packet[2] == HCI_STATE_OFF) {
            bluetooth_status_handler(BLUETOOTH_OFF);
        }
    }
    if (packet[0] == HCI_EVENT_NR_CONNECTIONS_CHANGED){
        if (packet[2]) {
            bluetooth_status_handler(BLUETOOTH_ACTIVE);
        } else {
            bluetooth_status_handler(BLUETOOTH_ON);
        }
    }
    
    // forward event to clients
    socket_connection_send_packet_all(HCI_EVENT_PACKET, 0, packet, size);
}

static void daemon_sigint_handler(int param){
    printf(" <= SIGINT received, shutting down..\n");    
    hci_power_control( HCI_POWER_OFF);
    printf("Good bye, see you.\n");    
    exit(0);
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
    
#ifdef USE_SPRINGBOARD
    bluetooth_status_handler = platform_iphone_status_handler;
#endif
    
    // @TODO: allow configuration per HCI CMD
    
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    // hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    hci_dump_open(NULL, HCI_DUMP_STDOUT);
                  
    // init HCI
    hci_init(transport, &config, control);
    
    // init L2CAP
    l2cap_init();
    l2cap_register_event_packet_handler(daemon_event_handler);
    timeout.process = daemon_no_connections_timeout;
    
    // @TODO: make choice of socket server configurable (TCP and/or Unix Domain Socket)
    
    // create server
    // socket_connection_create_tcp(BTSTACK_PORT);
    socket_connection_create_unix(BTSTACK_UNIX);

    socket_connection_register_packet_callback(daemon_client_handler);

    // handle CTRL-c
    signal(SIGINT, daemon_sigint_handler);
    // handle SIGTERM - suggested for launchd
    signal(SIGTERM, daemon_sigint_handler);
    // make stderr unbuffered
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
    printf("BTdaemon started - stdout\n");
    fprintf(stderr,"BTdaemon started - stderr\n");
    // go!
    run_loop_execute();

    return 0;
}
/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

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

#include <getopt.h>

#include <btstack/btstack.h>
#include <btstack/linked_list.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "l2cap.h"
#include "sdp.h"
#include "socket_connection.h"

#ifdef USE_BLUETOOL
#include "bt_control_iphone.h"
#endif

#ifdef USE_SPRINGBOARD
#include "platform_iphone.h"
#endif

#ifdef HAVE_TRANSPORT_USB
#include <libusb-1.0/libusb.h>
#endif

#define DAEMON_NO_CONNECTION_TIMEOUT 20000

#define HANDLE_POWER_NOTIFICATIONS


static hci_transport_t * transport;
static hci_uart_config_t config;

static timer_source_t timeout;
static uint8_t timeout_active = 0;

static int num_client_connections = 0;

static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state);
static void (*bluetooth_status_handler)(BLUETOOTH_STATE state) = dummy_bluetooth_status_handler;



#ifdef HANDLE_POWER_NOTIFICATIONS

#include <CoreFoundation/CoreFoundation.h>

// minimal IOKit
#include <Availability.h>
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_2_0
// compile issue fix with 4.2 headers
#undef NSEC_PER_USEC
#undef USEC_PER_SEC
#undef NSEC_PER_SEC
// end of fix
#include <mach/mach.h>
#define IOKIT
#include <device/device_types.h>

// costants
#define sys_iokit                          err_system(0x38)
#define sub_iokit_common                   err_sub(0)

#define iokit_common_msg(message)          (UInt32)(sys_iokit|sub_iokit_common|message)

#define kIOMessageCanDevicePowerOff        iokit_common_msg(0x200)
#define kIOMessageDeviceWillPowerOff       iokit_common_msg(0x210)
#define kIOMessageDeviceWillNotPowerOff    iokit_common_msg(0x220)
#define kIOMessageDeviceHasPoweredOn       iokit_common_msg(0x230)
#define kIOMessageCanSystemPowerOff        iokit_common_msg(0x240)
#define kIOMessageSystemWillPowerOff       iokit_common_msg(0x250)
#define kIOMessageSystemWillNotPowerOff    iokit_common_msg(0x260)
#define kIOMessageCanSystemSleep           iokit_common_msg(0x270)
#define kIOMessageSystemWillSleep          iokit_common_msg(0x280)
#define kIOMessageSystemWillNotSleep       iokit_common_msg(0x290)
#define kIOMessageSystemHasPoweredOn       iokit_common_msg(0x300)
#define kIOMessageSystemWillRestart        iokit_common_msg(0x310)
#define kIOMessageSystemWillPowerOn        iokit_common_msg(0x320)

// types
typedef io_object_t io_connect_t;
typedef io_object_t	io_service_t;
typedef	kern_return_t		IOReturn;

typedef struct IONotificationPort * IONotificationPortRef;
typedef void
(*IOServiceInterestCallback)(
							 void *			refcon,
							 io_service_t	service,
							 uint32_t		messageType,
							 void *			messageArgument );
io_connect_t IORegisterForSystemPower (void * refcon,
									   IONotificationPortRef * thePortRef,
									   IOServiceInterestCallback callback,
									   io_object_t * notifier );
CFRunLoopSourceRef IONotificationPortGetRunLoopSource(IONotificationPortRef	notify );
IOReturn IOAllowPowerChange ( io_connect_t kernelPort, long notificationID );
IOReturn IOCancelPowerChange ( io_connect_t kernelPort, long notificationID );

io_connect_t root_port;
io_object_t notifier;
io_connect_t  root_port; // a reference to the Root Power Domain IOService

void MySleepCallBack( void * refCon, io_service_t service, natural_t messageType, void * messageArgument ) {
    printf( "messageType %08lx, arg %08lx\n", (long unsigned int)messageType, (long unsigned int)messageArgument);
    switch ( messageType ) {
        case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
			 Applications have a chance to prevent sleep by calling IOCancelPowerChange.
			 Most applications should not prevent idle sleep.
			 
			 Power Management waits up to 30 seconds for you to either allow or deny idle sleep.
			 If you don't acknowledge this power change by calling either IOAllowPowerChange
			 or IOCancelPowerChange, the system will wait 30 seconds then go to sleep.
			 */
			
            // Uncomment to cancel idle sleep
			// IOCancelPowerChange( root_port, (long)messageArgument );
            // we will allow idle sleep
            IOAllowPowerChange( root_port, (long)messageArgument );
			break;
			
        case kIOMessageSystemWillSleep:
            /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
			 IOCancelPowerChange to acknowledge this message, sleep will be
			 delayed by 30 seconds.
			 
			 NOTE: If you call IOCancelPowerChange to deny sleep it returns kIOReturnSuccess,
			 however the system WILL still go to sleep. 
			 */
			
            // let's sleep
            hci_power_control(HCI_POWER_SLEEP);
            
            // power control only starts falling asleep, count on the 30 second delay 
            // IOAllowPowerChange( root_port, (long)messageArgument );
            break;
			
        case kIOMessageSystemWillPowerOn:
            
            //System has started the wake up process...
            
            // assume that all clients use Bluetooth -> if connection, start Bluetooth
            if (num_client_connections) {
                hci_power_control(HCI_POWER_ON);
            }
            break;
			
        case kIOMessageSystemHasPoweredOn:
            //System has finished waking up...
			break;
			
        default:
            break;
			
    }
}
#endif
#endif

static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state){
    printf("Bluetooth status: %u\n", state);
};

static void daemon_no_connections_timeout(){
    printf("No connection for %u seconds -> POWER OFF\n", DAEMON_NO_CONNECTION_TIMEOUT/1000);
    hci_power_control( HCI_POWER_OFF);
}

static int btstack_command_handler(connection_t *connection, uint8_t *packet, uint16_t size){
    // BTstack Commands
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 1, packet, size);
    bd_addr_t addr;
    uint16_t cid;
    uint16_t psm;
    uint16_t mtu;
    uint8_t  reason;
    uint32_t service_record_handle;
    
    // BTstack internal commands - 16 Bit OpCode, 8 Bit ParamLen, Params...
    switch (READ_CMD_OCF(packet)){
        case BTSTACK_GET_STATE:
            hci_emit_state();
            break;
        case BTSTACK_SET_POWER_MODE:
            hci_power_control(packet[3]);
            break;
        case BTSTACK_GET_VERSION:
            hci_emit_btstack_version();
            break;   
#ifdef USE_BLUETOOL
        case BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED:
            iphone_system_bt_set_enabled(packet[3]);
            // fall through .. :)
        case BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED:
            hci_emit_system_bluetooth_enabled(iphone_system_bt_enabled());
            break;
#else
        case BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED:
        case BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED:
            hci_emit_system_bluetooth_enabled(0);
            break;
#endif
        case L2CAP_CREATE_CHANNEL_MTU:
            bt_flip_addr(addr, &packet[3]);
            psm = READ_BT_16(packet, 9);
            mtu = READ_BT_16(packet, 11);
            l2cap_create_channel_internal( connection, NULL, addr, psm, mtu);
            break;
        case L2CAP_CREATE_CHANNEL:
            bt_flip_addr(addr, &packet[3]);
            psm = READ_BT_16(packet, 9);
            l2cap_create_channel_internal( connection, NULL, addr, psm, 150);   // until r865
            break;
        case L2CAP_DISCONNECT:
            cid = READ_BT_16(packet, 3);
            reason = packet[5];
            l2cap_disconnect_internal(cid, reason);
            break;
        case L2CAP_REGISTER_SERVICE:
            psm = READ_BT_16(packet, 3);
            mtu = READ_BT_16(packet, 5);
            l2cap_register_service_internal(connection, NULL, psm, mtu);
            break;
        case L2CAP_UNREGISTER_SERVICE:
            psm = READ_BT_16(packet, 3);
            l2cap_unregister_service_internal(connection, psm);
            break;
        case L2CAP_ACCEPT_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            l2cap_accept_connection_internal(cid);
            break;
        case L2CAP_DECLINE_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            reason = packet[7];
            l2cap_decline_connection_internal(cid, reason);
            break;
        case SDP_REGISTER_SERVICE_RECORD:
            printf("SDP_REGISTER_SERVICE_RECORD size %u\n", size);
            sdp_register_service_internal(connection, &packet[3]);
            break;
        case SDP_UNREGISTER_SERVICE_RECORD:
            service_record_handle = READ_BT_32(packet, 3);
            sdp_unregister_service_internal(connection, service_record_handle);
            break;
        default:
            //@TODO: log into hci dump as vendor specific "event"
            fprintf(stderr, "Error: command %u not implemented\n:", READ_CMD_OCF(packet));
            break;
    }
    return 0;
}

static int daemon_client_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length){
    
    int err = 0;
    
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
            err = hci_send_acl_packet(data, length);
            break;
        case L2CAP_DATA_PACKET:
            // process l2cap packet...
            err = l2cap_send_internal(channel, data, length);
            break;
        case DAEMON_EVENT_PACKET:
            switch (data[0]) {
                case DAEMON_EVENT_CONNECTION_CLOSED:
                    sdp_unregister_services_for_connection(connection);
                    l2cap_close_connection(connection);
                    break;
                case DAEMON_NR_CONNECTIONS_CHANGED:
                    num_client_connections = data[1];
                    printf("Nr Connections changed, new %u\n",num_client_connections);
                    if (timeout_active) {
                        run_loop_remove_timer(&timeout);
                        timeout_active = 0;
                    }
                    if (!num_client_connections) {
                        run_loop_set_timer(&timeout, DAEMON_NO_CONNECTION_TIMEOUT);
                        run_loop_add_timer(&timeout);
                        timeout_active = 1;
                    }
                default:
                    break;
            }
            break;
    }
    if (err) {
        printf("Daemon Handler: err %d\n", err);
    }
    return err;
}

static void deamon_status_event_handler(uint8_t *packet, uint16_t size){
    
    // local cache
    static HCI_STATE hci_state = HCI_STATE_OFF;
    static int num_connections = 0;
    
    uint8_t update_status = 0;
    
    // handle state event
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            hci_state = packet[2];
            printf("New state: %u\n", hci_state);
            update_status = 1;
            break;
        case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
            num_connections = packet[2];
            printf("New nr connections: %u\n", num_connections);
            update_status = 1;
            break;
        default:
            break;
    }
    
    // choose full bluetooth state 
    if (update_status) {
        if (hci_state != HCI_STATE_WORKING) {
            bluetooth_status_handler(BLUETOOTH_OFF);
        } else {
            if (num_connections) {
                bluetooth_status_handler(BLUETOOTH_ACTIVE);
            } else {
                bluetooth_status_handler(BLUETOOTH_ON);
            }
        }
    }
}

static void daemon_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type == HCI_EVENT_PACKET) {
        deamon_status_event_handler(packet, size);
        if (packet[0] == HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS) {
            socket_connection_retry_parked();
        }
    }
    if (connection) {
        socket_connection_send_packet(connection, packet_type, channel, packet, size);
    } else {
        socket_connection_send_packet_all(packet_type, channel, packet, size);
    }
}

static void daemon_sigint_handler(int param){
    printf(" <= SIGINT received, shutting down..\n");    
    hci_power_control( HCI_POWER_OFF);
    hci_close();
    printf("Good bye, see you.\n");    
    exit(0);
}

static void daemon_sigpipe_handler(int param){
    printf(" <= SIGPIPE received.. trying to ignore..\n");    
}

static void usage(const char * name) {
    printf("%s, BTstack background daemon\n", name);
    printf("usage: %s [-h|--help] [--tcp]\n", name);
    printf("    -h|--help  display this usage\n");
    printf("    --tcp      use TCP server socket instead of local unix socket\n");
}

int main (int argc,  char * const * argv){
    
    static int tcp_flag = 0;
    
    while (1) {
        static struct option long_options[] = {
            { "tcp", no_argument, &tcp_flag, 1 },
            { "help", no_argument, 0, 0 },
            { 0,0,0,0 } // This is a filler for -1
        };
        
        int c;
        int option_index = -1;
        
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        
        if (c == -1) break; // no more option
        
        // treat long parameter first
        if (option_index == -1) {
            switch (c) {
                case '?':
                case 'h':
                    usage(argv[0]);
                    return 0;
                    break;
            }
        } else {
            switch (option_index) {
                case 1:
                    usage(argv[0]);
                    return 0;
                    break;
            }
        }
    }
    
    // make stderr/stdout unbuffered
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
    printf("BTdaemon started - stdout\n");
    fprintf(stderr,"BTdaemon started - stderr\n");

    // handle CTRL-c
    signal(SIGINT, daemon_sigint_handler);
    // handle SIGTERM - suggested for launchd
    signal(SIGTERM, daemon_sigint_handler);
    // handle SIGPIPE
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGPIPE, &act, NULL);
    
    
    bt_control_t * control = NULL;
    remote_device_db_t * remote_device_db = NULL;
    
#ifdef HAVE_TRANSPORT_H4
    transport = hci_transport_h4_instance();
    config.device_name = UART_DEVICE;
    config.baudrate    = UART_SPEED;
    config.flowcontrol = 1;
    // config.flowcontrol = 0;  // external Bluetooth!
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
    
#ifdef REMOTE_DEVICE_DB
    remote_device_db = &REMOTE_DEVICE_DB;
#endif
    
#ifdef USE_COCOA_RUN_LOOP 
    run_loop_init(RUN_LOOP_POSIX);
#else
    run_loop_init(RUN_LOOP_COCOA);
#endif
    
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    hci_dump_set_max_packets(1000);
    // hci_dump_open(NULL, HCI_DUMP_STDOUT);
                  
    // init HCI
    hci_init(transport, &config, control, remote_device_db);
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(daemon_packet_handler);
    timeout.process = daemon_no_connections_timeout;

#ifdef HAVE_SDP
    // init SDP
    sdp_init();
    // sdp_test();
#endif
    
#ifdef USE_LAUNCHD
    socket_connection_create_launchd();
#else
    // create server
    if (tcp_flag) {
        socket_connection_create_tcp(BTSTACK_PORT);
    } else {
        socket_connection_create_unix(BTSTACK_UNIX);
    }
#endif
    socket_connection_register_packet_callback(daemon_client_handler);
    
#ifdef HANDLE_POWER_NOTIFICATIONS
    IONotificationPortRef  notifyPortRef;  // notification port allocated by IORegisterForSystemPower
    io_object_t            notifierObject; // notifier object, used to deregister later
    void*                  refCon = NULL;  // this parameter is passed to the callback
	
    // register to receive system sleep notifications
    root_port = IORegisterForSystemPower( refCon, &notifyPortRef, MySleepCallBack, &notifierObject );
    if (!root_port)
    {
        printf("IORegisterForSystemPower failed\n");
        return 1;
    }
	
    // add the notification port to the application runloop
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes); 
#endif
    
    // go!
    run_loop_execute();
    return 0;
}
/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "panu_demo.c"

/*
 * panu_demo.c
 * Author: Ole Reinhardt <ole.reinhardt@kernelconcepts.de>
 */

/* EXAMPLE_START(panu_demo): PANU Demo
 *
 * @text This example implements both a PANU client and a server. In server mode, it 
 * sets up a BNEP server and registers a PANU SDP record and waits for incoming connections.
 * In client mode, it connects to a remote device, does an SDP Query to identify the PANU
 * service and initiates a BNEP connection.
 */

#include "btstack_config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if_arp.h>

#ifdef __APPLE__
#include <net/if.h>
#include <net/if_types.h>

#include <netinet/if_ether.h>
#include <netinet/in.h>
#endif

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __linux
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include "btstack.h"

static int record_id = -1;
static uint16_t bnep_l2cap_psm      = 0;
static uint32_t bnep_remote_uuid    = 0;
static uint16_t bnep_version        = 0;
static uint16_t bnep_cid            = 0;

static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

//static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
// static bd_addr_t remote = {0xE0,0x06,0xE6,0xBB,0x95,0x79}; // Ole Thinkpad
static bd_addr_t remote = {0x84,0x38,0x35,0x65,0xD1,0x15};  // MacBook 2013 

static int  tap_fd = -1;
static uint8_t network_buffer[BNEP_MTU_MIN];
static size_t  network_buffer_len = 0;

#ifdef __APPLE__
// tuntaposx provides fixed set of tapX devices
static const char * tap_dev = "/dev/tap0";
static char tap_dev_name[16] = "tap0";
#endif

#ifdef __linux
// Linux uses single control device to bring up tunX or tapX interface
static const char * tap_dev = "/dev/net/tun";
static char tap_dev_name[16] = "bnep%d";
#endif


static btstack_data_source_t tap_dev_ds;
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP and BNEP are initialized and a BNEP service, for server mode,
 * is registered, before the Bluetooth stack gets started, as shown in Listing PanuSetup.
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void panu_setup(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Initialize L2CAP 
    l2cap_init();

    // Initialise BNEP
    bnep_init();
    // Minimum L2CAP MTU for bnep is 1691 bytes
    bnep_register_service(packet_handler, BLUETOOTH_SERVICE_CLASS_PANU, 1691);  
}
/* LISTING_END */

/* @section TUN / TAP interface routines 
 *
 * @text This example requires a TUN/TAP interface to connect the Bluetooth network interface
 * with the native system. It has been tested on Linux and OS X, but should work on any
 * system that provides TUN/TAP with minor modifications.
 * 
 * On Linux, TUN/TAP is available by default. On OS X, tuntaposx from
 * http://tuntaposx.sourceforge.net needs to be installed.
 *
 * The *tap_alloc* function sets up a virtual network interface with the given Bluetooth Address.
 * It is rather low-level as it sets up and configures a network interface.
 */ 

static int tap_alloc(char *dev, bd_addr_t bd_addr)
{
    struct ifreq ifr;
    int fd_dev;
    int fd_socket;

    if( (fd_dev = open(tap_dev, O_RDWR)) < 0 ) {
        fprintf(stderr, "TAP: Error opening %s: %s\n", tap_dev, strerror(errno));
        return -1;
    }

#ifdef __linux
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 
    if( *dev ) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    int err;
    if( (err = ioctl(fd_dev, TUNSETIFF, (void *) &ifr)) < 0 ) {
        fprintf(stderr, "TAP: Error setting device name: %s\n", strerror(errno));
        close(fd_dev);
        return -1;
    }  
    strcpy(dev, ifr.ifr_name);
#endif
#ifdef __APPLE__
    dev = tap_dev_name;
#endif    

    fd_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (fd_socket < 0) {
        close(fd_dev);
		fprintf(stderr, "TAP: Error opening netlink socket: %s\n", strerror(errno));
		return -1;
	}

    // Configure the MAC address of the newly created bnep(x) 
    // device to the local bd_address
    memset (&ifr, 0, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, dev);
#ifdef __linux
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data, bd_addr, sizeof(bd_addr_t));
	if (ioctl(fd_socket, SIOCSIFHWADDR, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
        fprintf(stderr, "TAP: Error setting hw addr: %s\n", strerror(errno));
        exit(1);
		return -1;
	}
#endif
#ifdef __APPLE__
    ifr.ifr_addr.sa_len = ETHER_ADDR_LEN;
    ifr.ifr_addr.sa_family = AF_LINK;
    (void)memcpy(ifr.ifr_addr.sa_data, bd_addr, ETHER_ADDR_LEN);
    if (ioctl(fd_socket, SIOCSIFLLADDR, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
        fprintf(stderr, "TAP: Error setting hw addr: %s\n", strerror(errno));
        exit(1);
        return -1;
}
#endif    

    // Bring the interface up
	if (ioctl(fd_socket, SIOCGIFFLAGS, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
		fprintf(stderr, "TAP: Error reading interface flags: %s\n", strerror(errno));
		return -1;
	}

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;

		if (ioctl(fd_socket, SIOCSIFFLAGS, &ifr) == -1) {
            close(fd_dev);
            close(fd_socket);
            fprintf(stderr, "TAP: Error set IFF_UP: %s\n", strerror(errno));
            return -1;
		}
	}

	close(fd_socket);
    
    return fd_dev;
}

/*
 * @text Listing processTapData shows how a packet is received from the TAP network interface
 * and forwarded over the BNEP connection.
 * 
 * After successfully reading a network packet, the call to
 * the *bnep_can_send_packet_now* function checks, if BTstack can forward
 * a network packet now. If that's not possible, the received data stays
 * in the network buffer and the data source elements is removed from the
 * run loop. The *process_tap_dev_data* function will not be called until
 * the data source is registered again. This provides a basic flow control.
 */

/* LISTING_START(processTapData): Process incoming network packets */
static void process_tap_dev_data(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) 
{
    UNUSED(ds);
    UNUSED(callback_type);

    ssize_t len;
    len = read(ds->fd, network_buffer, sizeof(network_buffer));
    if (len <= 0){
        fprintf(stderr, "TAP: Error while reading: %s\n", strerror(errno));
        return;
    }

    network_buffer_len = len;
    if (bnep_can_send_packet_now(bnep_cid)) {
        bnep_send(bnep_cid, network_buffer, network_buffer_len);
        network_buffer_len = 0;
    } else {
        // park the current network packet
        btstack_run_loop_remove_data_source(&tap_dev_ds);
    }
    return;
}
/* LISTING_END */

// PANU client routines 
static char * get_string_from_data_element(uint8_t * element){
    de_size_t de_size = de_get_size_type(element);
    int pos     = de_get_header_size(element);
    int len = 0;
    switch (de_size){
        case DE_SIZE_VAR_8:
            len = element[1];
            break;
        case DE_SIZE_VAR_16:
            len = big_endian_read_16(element, 1);
            break;
        default:
            break;
    }
    char * str = (char*)malloc(len+1);
    memcpy(str, &element[pos], len);
    str[len] ='\0';
    return str; 
}


/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    char *str;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                printf("SDP Record: Nr: %d\n", record_id);
            }

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {

                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                            if (de_get_element_type(attribute_value) != DE_DES) break;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                                uint8_t * element = des_iterator_get_element(&des_list_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint32_t uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_SERVICE_CLASS_PANU:
                                    case BLUETOOTH_SERVICE_CLASS_NAP:
                                    case BLUETOOTH_SERVICE_CLASS_GN:
                                        printf("SDP Attribute 0x%04x: BNEP PAN protocol UUID: %04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        bnep_remote_uuid = uuid;
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case 0x0100:
                        case 0x0101:
                            str = get_string_from_data_element(attribute_value);
                            printf("SDP Attribute: 0x%04x: %s\n", sdp_event_query_attribute_byte_get_attribute_id(packet), str);
                            free(str);
                            break;
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
                                printf("SDP Attribute: 0x%04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet));

                                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_BNEP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                                printf("l2cap_psm 0x%04x, bnep_version 0x%04x\n", bnep_l2cap_psm, bnep_version);

                                /* Create BNEP connection */
                                bnep_connect(packet_handler, remote, bnep_l2cap_psm, BLUETOOTH_SERVICE_CLASS_PANU, bnep_remote_uuid);
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            fprintf(stderr, "General query done with status %d.\n", sdp_event_query_complete_get_status(packet));

            break;
    }
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HCI Events.
 */


/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
/* LISTING_PAUSE */
    UNUSED(channel);

    int       rc;
    uint8_t   event;
    bd_addr_t event_addr;
    bd_addr_t local_addr;
    uint16_t  uuid_source;
    uint16_t  uuid_dest;
    uint16_t  mtu;    
  
    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP BNEP query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("Start SDP BNEP query.\n");
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote, BLUETOOTH_PROTOCOL_BNEP);
                    }
                    break;

                /* LISTING_PAUSE */
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06u'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                /* LISTING_RESUME */

                /* @text BNEP_EVENT_CHANNEL_OPENED is received after a BNEP connection was established or 
                 * or when the connection fails. The status field returns the error code.
                 * 
                 * The TAP network interface is then configured. A data source is set up and registered with the 
                 * run loop to receive Ethernet packets from the TAP interface.
                 *
                 * The event contains both the source and destination UUIDs, as well as the MTU for this connection and
                 * the BNEP Channel ID, which is used for sending Ethernet packets over BNEP.
                 */  
				case BNEP_EVENT_CHANNEL_OPENED:
                    if (bnep_event_channel_opened_get_status(packet)) {
                        printf("BNEP channel open failed, status %02x\n", bnep_event_channel_opened_get_status(packet));
                    } else {
                        bnep_cid    = bnep_event_channel_opened_get_bnep_cid(packet);
                        uuid_source = bnep_event_channel_opened_get_source_uuid(packet);
                        uuid_dest   = bnep_event_channel_opened_get_destination_uuid(packet);
                        mtu         = bnep_event_channel_opened_get_mtu(packet);
                        //bt_flip_addr(event_addr, &packet[9]); 
                        memcpy(&event_addr, &packet[11], sizeof(bd_addr_t));
                        printf("BNEP connection open succeeded to %s source UUID 0x%04x dest UUID: 0x%04x, max frame size %u\n", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
                        /* Create the tap interface */
                        gap_local_bd_addr(local_addr);
                        tap_fd = tap_alloc(tap_dev_name, local_addr);
                        if (tap_fd < 0) {
                            printf("Creating BNEP tap device failed: %s\n", strerror(errno));
                        } else {
                            printf("BNEP device \"%s\" allocated.\n", tap_dev_name);
                            /* Create and register a new runloop data source */
                            btstack_run_loop_set_data_source_fd(&tap_dev_ds, tap_fd);
                            btstack_run_loop_set_data_source_handler(&tap_dev_ds, &process_tap_dev_data);
                            btstack_run_loop_add_data_source(&tap_dev_ds);
                        }
                    }
					break;
                
                /* @text If there is a timeout during the connection setup, BNEP_EVENT_CHANNEL_TIMEOUT will be received
                 * and the BNEP connection  will be closed
                 */     
                case BNEP_EVENT_CHANNEL_TIMEOUT:
                    printf("BNEP channel timeout! Channel will be closed\n");
                    break;

                /* @text BNEP_EVENT_CHANNEL_CLOSED is received when the connection gets closed.
                 */
                case BNEP_EVENT_CHANNEL_CLOSED:
                    printf("BNEP channel closed\n");
                    btstack_run_loop_remove_data_source(&tap_dev_ds);
                    if (tap_fd > 0) {
                        close(tap_fd);
                        tap_fd = -1;
                    }
                    break;

                /* @text BNEP_EVENT_CAN_SEND_NOW indicates that a new packet can be send. This triggers the retry of a 
                 * parked network packet. If this succeeds, the data source element is added to the run loop again.
                 */
                case BNEP_EVENT_CAN_SEND_NOW:
                    // Check for parked network packets and send it out now 
                    if (network_buffer_len > 0) {
                        bnep_send(bnep_cid, network_buffer, network_buffer_len);
                        network_buffer_len = 0;
                        // Re-add the tap device data source
                        btstack_run_loop_add_data_source(&tap_dev_ds);
                    }
                    
                    break;
                    
                default:
                    break;
            }
            break;

        /* @text Ethernet packets from the remote device are received in the packet handler with type BNEP_DATA_PACKET.
         * It is forwarded to the TAP interface.
         */
        case BNEP_DATA_PACKET:
            // Write out the ethernet frame to the tap device 
            if (tap_fd > 0) {
                rc = write(tap_fd, packet, size);
                if (rc < 0) {
                    fprintf(stderr, "TAP: Could not write to TAP device: %s\n", strerror(errno));
                } else 
                if (rc != size) {
                    fprintf(stderr, "TAP: Package written only partially %d of %d bytes\n", rc, size);
                }
            }
            break;            
            
        default:
            break;
    }
}
/* LISTING_END */


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    printf("Client HCI init done\n");

    panu_setup();
    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/* EXAMPLE_END */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */


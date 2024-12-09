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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "panu_demo.c"

/*
 * panu_demo.c
 * Author: Ole Reinhardt <ole.reinhardt@kernelconcepts.de>
 */

/* EXAMPLE_START(panu_demo): BNEP/PANU (Linux only)
 *
 * @text This example implements both a PANU client and a server. In server mode, it 
 * sets up a BNEP server and registers a PANU SDP record and waits for incoming connections.
 * In client mode, it connects to a remote device, does an SDP Query to identify the PANU
 * service and initiates a BNEP connection.
 *
 * Note: currently supported only on Linux and provides a TAP network interface which you
 *       can configure yourself.
 *
 * To enable client mode, uncomment ENABLE_PANU_CLIENT below.
 */

#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

// #define ENABLE_PANU_CLIENT

// network types
#define NETWORK_TYPE_IPv4       0x0800
#define NETWORK_TYPE_ARP        0x0806
#define NETWORK_TYPE_IPv6       0x86DD

static uint16_t bnep_cid            = 0;

#ifdef ENABLE_PANU_CLIENT

static int record_id = -1;

static uint16_t bnep_version        = 0;
static uint32_t bnep_remote_uuid    = 0;
static uint16_t bnep_l2cap_psm      = 0;

static uint16_t sdp_bnep_l2cap_psm      = 0;
static uint16_t sdp_bnep_version        = 0;
static uint32_t sdp_bnep_remote_uuid    = 0;

static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);
#endif

// MBP 2016
static const char * remote_addr_string = "78:4F:43:8C:B2:5D";
// Wiko Sunny static const char * remote_addr_string = "A0:4C:5B:0F:B2:42";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;

// outgoing network packet
static const uint8_t * network_buffer;
static uint16_t network_buffer_len;

static uint8_t panu_sdp_record[220];

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP and BNEP are initialized and a BNEP service, for server mode,
 * is registered, before the Bluetooth stack gets started, as shown in Listing PanuSetup.
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#ifdef ENABLE_PANU_CLIENT
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#endif
static void network_send_packet_callback(const uint8_t * packet, uint16_t size);

static void panu_setup(void){

    // Initialize L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // init SDP, create record for PANU and register with SDP
    sdp_init();
    memset(panu_sdp_record, 0, sizeof(panu_sdp_record));
    uint16_t network_packet_types[] = { NETWORK_TYPE_IPv4, NETWORK_TYPE_ARP, 0};    // 0 as end of list

    // Initialise BNEP
    bnep_init();
    // Minimum L2CAP MTU for bnep is 1691 bytes
#ifdef ENABLE_PANU_CLIENT
    bnep_register_service(packet_handler, BLUETOOTH_SERVICE_CLASS_PANU, 1691);
    // PANU
    pan_create_panu_sdp_record(panu_sdp_record, sdp_create_service_record_handle(), network_packet_types, NULL, NULL, BNEP_SECURITY_NONE);
#else
    bnep_register_service(packet_handler, BLUETOOTH_SERVICE_CLASS_NAP, 1691);
    // NAP Network Access Type: Other, 1 MB/s
    pan_create_nap_sdp_record(panu_sdp_record, sdp_create_service_record_handle(), network_packet_types, NULL, NULL, BNEP_SECURITY_NONE, PAN_NET_ACCESS_TYPE_OTHER, 1000000, NULL, NULL);
#endif
    btstack_assert(de_get_len( panu_sdp_record) <= sizeof(panu_sdp_record));
    sdp_register_service(panu_sdp_record);

    // Initialize network interface
    btstack_network_init(&network_send_packet_callback);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
}
/* LISTING_END */

#ifdef ENABLE_PANU_CLIENT

// PANU client routines 
static void get_string_from_data_element(uint8_t * element, uint16_t buffer_size, char * buffer_data){
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
    uint16_t bytes_to_copy = btstack_min(buffer_size-1,len);
    memcpy(buffer_data, &element[pos], bytes_to_copy);
    buffer_data[bytes_to_copy] ='\0';
}


/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */
static void handle_sdp_client_record_complete(void){

    printf("SDP BNEP Record complete\n");

    // accept first entry or if we foudn a NAP and only have a PANU yet
    if ((bnep_remote_uuid == 0) || (sdp_bnep_remote_uuid == BLUETOOTH_SERVICE_CLASS_NAP && bnep_remote_uuid == BLUETOOTH_SERVICE_CLASS_PANU)){
        bnep_l2cap_psm   = sdp_bnep_l2cap_psm;
        bnep_remote_uuid = sdp_bnep_remote_uuid;
        bnep_version     = sdp_bnep_version;
    }
}

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    char str[20];

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
                handle_sdp_client_record_complete();
                // next record started
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
                                        sdp_bnep_remote_uuid = uuid;
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case 0x0100:
                        case 0x0101:
                            get_string_from_data_element(attribute_value, sizeof(str), str);
                            printf("SDP Attribute: 0x%04x: %s\n", sdp_event_query_attribute_byte_get_attribute_id(packet), str);
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
                                    
                                    if (!element) continue;
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    des_iterator_next(&prot_it);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_bnep_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_BNEP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_bnep_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                                printf("Summary: uuid 0x%04x, l2cap_psm 0x%04x, bnep_version 0x%04x\n", sdp_bnep_remote_uuid, sdp_bnep_l2cap_psm, sdp_bnep_version);

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
            handle_sdp_client_record_complete();
            fprintf(stderr, "Query complete, status 0%02x, bnep psm 0x%04x.\n", sdp_event_query_complete_get_status(packet), bnep_l2cap_psm);
            if (bnep_l2cap_psm){
                /* Create BNEP connection */
                bnep_connect(packet_handler, remote_addr, bnep_l2cap_psm, BLUETOOTH_SERVICE_CLASS_PANU, bnep_remote_uuid);
            } else {
                fprintf(stderr, "No BNEP service found\n");
            }

            break;
    }
}
#endif

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
#ifdef ENABLE_PANU_CLIENT
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP BNEP query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("Start SDP BNEP query for remote PAN Network Access Point (NAP).\n");
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_NAP);
                    }
                    break;
#endif
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
                        printf("BNEP channel open failed, status 0x%02x\n", bnep_event_channel_opened_get_status(packet));
                    } else {
                        bnep_cid    = bnep_event_channel_opened_get_bnep_cid(packet);
                        uuid_source = bnep_event_channel_opened_get_source_uuid(packet);
                        uuid_dest   = bnep_event_channel_opened_get_destination_uuid(packet);
                        mtu         = bnep_event_channel_opened_get_mtu(packet);
                        bnep_event_channel_opened_get_remote_address(packet, event_addr);
                        printf("BNEP connection open succeeded to %s source UUID 0x%04x dest UUID: 0x%04x, max frame size %u\n", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);

                        /* Setup network interface */
                        gap_local_bd_addr(local_addr);
                        btstack_network_up(local_addr);
                        printf("Network Interface %s activated\n", btstack_network_get_name());
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
                    btstack_network_down();
                    break;

                /* @text BNEP_EVENT_CAN_SEND_NOW indicates that a new packet can be send. This triggers the send of a 
                 * stored network packet. The tap datas source can be enabled again
                 */
                case BNEP_EVENT_CAN_SEND_NOW:
                    if (network_buffer_len > 0) {
                        bnep_send(bnep_cid, (uint8_t*) network_buffer, network_buffer_len);
                        network_buffer_len = 0;
                        btstack_network_packet_sent();
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
            // Write out the ethernet frame to the network interface
            btstack_network_process_packet(packet, size);
            break;            
            
        default:
            break;
    }
}
/* LISTING_END */

/*
 * @section Network packet handler
 * 
 * @text A pointer to the network packet is stored and a BNEP_EVENT_CAN_SEND_NOW requested
 */

/* LISTING_START(networkPacketHandler): Network Packet Handler */
static void network_send_packet_callback(const uint8_t * packet, uint16_t size){
    network_buffer = packet;
    network_buffer_len = size;
    bnep_request_can_send_now_event(bnep_cid);
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    panu_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);

    gap_discoverable_control(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
#ifdef ENABLE_PANU_CLIENT
    gap_set_local_name("PANU Client 00:00:00:00:00:00");
#else
    gap_set_local_name("NAP Server 00:00:00:00:00:00");
#endif
    gap_set_class_of_device(0x020300); // Service Class "Networking", Major Device Class "LAN / NAT"

    // Turn on the device
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/* EXAMPLE_END */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */


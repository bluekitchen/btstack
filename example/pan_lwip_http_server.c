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

#define __BTSTACK_FILE__ "pan_lwip_http_server.c"

// *****************************************************************************
/* EXAMPLE_START(pan_lwip_http_server): PAN - HTTP Server using lwIP
 *
 * @text Bluetooth PAN is mainly used for Internet Tethering, where e.g. a mobile
 * phone provides internet connection to a laptop or a tablet.
 *
 * Instead of regular internet access, it's also possible to provide a Web app on a 
 * Bluetooth device, e.g. for configuration or maintenance. For some device,
 * this can be a more effective way to provide an interface compared to dedicated
 * smartphone applications (for Android and iOS).
 *
 * Before iOS 11, accessing an HTTP server via Bluetooth PAN was not supported on
 * the iPhone, but on iPod and iPad. With iOS 11, this works as expected.
 *
 * After pairing your device, please open the URL http://192.168.7.1 in your web
 * browser.

 */
 // *****************************************************************************

#include <stdio.h>

#include "btstack_config.h"
#include "bnep_lwip.h"
#include "btstack.h"

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/apps/httpd.h"
#include "dhserver.h"

// network types
#define NETWORK_TYPE_IPv4       0x0800
#define NETWORK_TYPE_ARP        0x0806
#define NETWORK_TYPE_IPv6       0x86DD

static uint8_t pan_sdp_record[220];

static btstack_packet_callback_registration_t hci_event_callback_registration;

/* 
 * @section Packet Handler
 *
 * @text All BNEP events are handled in the platform/bnep_lwip.c BNEP-LWIP Adapter.
 *       Here, we only print status information and handle pairing requests.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    /* LISTING_PAUSE */
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_addr;
  
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {            

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Auto accept\n");
                    hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
                    break;

                /* @text BNEP_EVENT_CHANNEL_OPENED is received after a BNEP connection was established or 
                 * or when the connection fails. The status field returns the error code.
                 */  
                case BNEP_EVENT_CHANNEL_OPENED:
                    if (bnep_event_channel_opened_get_status(packet)) {
                        printf("BNEP channel open failed, status %02x\n", bnep_event_channel_opened_get_status(packet));
                    } else {
                        uint16_t uuid_source = bnep_event_channel_opened_get_source_uuid(packet);
                        uint16_t uuid_dest   = bnep_event_channel_opened_get_destination_uuid(packet);
                        uint16_t mtu         = bnep_event_channel_opened_get_mtu(packet);
                        bnep_event_channel_opened_get_remote_address(packet, event_addr);
                        printf("BNEP connection open succeeded to %s source UUID 0x%04x dest UUID: 0x%04x, max frame size %u\n", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
                        printf("Please open 'http://192.168.7.1' in your web browser: \n");
                    }
                    break;
                
                /* @text BNEP_EVENT_CHANNEL_CLOSED is received when the connection gets closed.
                 */
                case BNEP_EVENT_CHANNEL_CLOSED:
                    printf("BNEP channel closed\n");
                    break;

                default:
                    break;
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

/* @section PAN BNEP Setup
 *
 * @text Listing PanBnepSetup shows the setup of the PAN setup
 */
 
/* LISTING_START(PanBnepSetup): Configure GAP, register PAN NAP servier, register PAN NA SDP record. */
static void pan_bnep_setup(void){

    // Discoverable
    // Set local name with a template Bluetooth address, that will be automatically
    // replaced with a actual address once it is available, i.e. when BTstack boots
    // up and starts talking to a Bluetooth module.
    gap_set_local_name("PAN HTTP 00:00:00:00:00:00");
    gap_discoverable_control(1);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Initialize L2CAP
    l2cap_init();

    // Initialize BNEP
    bnep_init();

    // Init SDP
    sdp_init();
    memset(pan_sdp_record, 0, sizeof(pan_sdp_record));
    uint16_t network_packet_types[] = { NETWORK_TYPE_IPv4, NETWORK_TYPE_ARP, 0};    // 0 as end of list

    // NAP Network Access Type: Other, 1 MB/s
    pan_create_nap_sdp_record(pan_sdp_record, sdp_create_service_record_handle(), network_packet_types, NULL, NULL, BNEP_SECURITY_NONE, PAN_NET_ACCESS_TYPE_OTHER, 1000000, NULL, NULL);
    sdp_register_service(pan_sdp_record);
    printf("SDP service record size: %u\n", de_get_len((uint8_t*) pan_sdp_record));

    // Init BNEP lwIP Adapter
    bnep_lwip_init();

    // Setup NAP Service via BENP lwIP adapter
    bnep_lwip_register_service(BLUETOOTH_SERVICE_CLASS_NAP, 1691);

    // register callback - to print state
    bnep_lwip_register_packet_handler(packet_handler);
}
/* LISTING_END */


/* @section DHCP Server Configuration
 *
 * @text Listing DhcpSetup shows the DHCP Server configuration for network 192.168.7.0/8
 */
 
/* LISTING_START(DhcpSetup): Setup DHCP Server with 3 entries for 192.168.7.0/8. */

#define NUM_DHCP_ENTRY 3

static dhcp_entry_t entries[NUM_DHCP_ENTRY] =
{
    /* mac    ip address        subnet mask        lease time */
    { {0}, {192, 168, 7, 2}, {255, 255, 255, 0}, 24 * 60 * 60 },
    { {0}, {192, 168, 7, 3}, {255, 255, 255, 0}, 24 * 60 * 60 },
    { {0}, {192, 168, 7, 4}, {255, 255, 255, 0}, 24 * 60 * 60 }
};

static dhcp_config_t dhcp_config =
{
    {192, 168, 7, 1}, 67, /* server address, port */
    {0, 0, 0, 0},         /* dns server */
    NULL,                /* dns suffix */
    NUM_DHCP_ENTRY,       /* num entry */
    entries               /* entries */
};
/* LISTING_END */


/* @section DHCP Server Setup
 *
 * @text Listing LwipSetup shows the setup of the lwIP network stack and starts the DHCP Server
 */
 
/* LISTING_START(LwipSetup): Report lwIP version, init lwIP, start DHCP Serverr */

static void network_setup(void){
    printf("lwIP version: " LWIP_VERSION_STRING "\n");

#if BYTE_ORDER == LITTLE_ENDIAN
    // big endian detection not supported by build configuration
    if (btstack_is_big_endian()){
        printf("lwIP configured for little endian, but running on big endian. Please set BYTE_ORDER to BIG_ENDIAN in lwiopts.h\n");
        while (1);
    }
#endif

    // init lwIP stack
#if NO_SYS
    lwip_init();
#else
    tcpip_init(NULL, NULL);
#endif

    // start DHCP Server
    dhserv_init(&dhcp_config);

    // start HTTP Server
    httpd_init();
}
/* LISTING_END */


/* @section Main
 *
 * @text Setup the lwIP network and PAN NAP
 */
 
/* LISTING_START(Main): Setup lwIP network and PAN NAP */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    // setup lwIP, HTTP, DHCP
    network_setup();

    // setup Classic PAN NAP Service
    pan_bnep_setup();

    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */

/* EXAMPLE_END */

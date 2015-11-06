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

// *****************************************************************************
/* EXAMPLE_START(gatt_browser): GATT Client - Discovering primary services and their characteristics
 * 
 * @text This example shows how to use the GATT Client
 * API to discover primary services and their characteristics of the first found
 * device that is advertising its services.
 *
 * The logic is divided between the HCI and GATT client packet handlers.
 * The HCI packet handler is responsible for finding a remote device, 
 * connecting to it, and for starting the first GATT client query.
 * Then, the GATT client packet handler receives all primary services and
 * requests the characteristics of the last one to keep the example short.
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include <btstack/sdp_util.h>

#include "btstack-config.h"

#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"
#include "gatt_client.h"
#include "sm.h"

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} advertising_report_t;

static bd_addr_t cmdline_addr = { };
static int cmdline_addr_found = 0;

uint16_t gc_handle;
static le_service_t services[40];
static int service_count = 0;
static int service_index = 0;


/* @section GATT client setup
 *
 * @text In the setup phase, a GATT client must register the HCI and GATT client
 * packet handlers, as shown in Listing GATTClientSetup.
 * Additionally, the security manager can be setup, if signed writes, or
 * encrypted, or authenticated connection are required, to access the
 * characteristics, as explained in Section on [SMP](../protocols/#sec:smpProtocols).
 */

/* LISTING_START(GATTClientSetup): Setting up GATT client */
static uint16_t gc_id;

// Handles connect, disconnect, and advertising report events,  
// starts the GATT client, and sends the first query.
static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// Handles GATT client query results, sends queries and the 
// GAP disconnect command when the querying is done.
void handle_gatt_client_event(le_event_t * event);

static void gatt_client_setup(void){
    // Initialize L2CAP and register HCI event handler
    l2cap_init();
    l2cap_register_packet_handler(&handle_hci_event);

    // Initialize GATT client 
    gatt_client_init();
    // Register handler for GATT client events
    gc_id = gatt_client_register_packet_handler(&handle_gatt_client_event);

    // Optinoally, Setup security manager
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
}
/* LISTING_END */

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    if (uuid16){
        printf("%04x",uuid16);
    } else {
        printUUID128(uuid128);
    }
}

static void dump_advertising_report(advertising_report_t * e){
    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type,
           e->address_type, bd_addr_to_str(e->address), e->rssi, e->length);
    printf_hexdump(e->data, e->length);
    
}

static void dump_characteristic(le_characteristic_t * characteristic){
    printf("    * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid ",
                            characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, characteristic->properties);
    printUUID(characteristic->uuid128, characteristic->uuid16);
    printf("\n");
}

static void dump_service(le_service_t * service){
    printf("    * service: [0x%04x-0x%04x], uuid ", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
    printf("\n");
}

static void fill_advertising_report_from_packet(advertising_report_t * report, uint8_t *packet){
    int pos = 2;
    report->event_type = packet[pos++];
    report->address_type = packet[pos++];
    bt_flip_addr(report->address, &packet[pos]);
    pos += 6;
    report->rssi = packet[pos++];
    report->length = packet[pos++];
    report->data = &packet[pos];
    pos += report->length;
    dump_advertising_report(report);
}


/* @section HCI packet handler
 * 
 * @text The HCI packet handler has to start the scanning, 
 * to find the first advertising device, to stop scanning, to connect
 * to and later to disconnect from it, to start the GATT client upon
 * the connection is completed, and to send the first query - in this
 * case the gatt_client_discover_primary_services() is called, see 
 * Listing GATTBrowserHCIPacketHandler.  
 */

/* LISTING_START(GATTBrowserHCIPacketHandler): Connecting and disconnecting from the GATT client */
static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    advertising_report_t report;
    
    uint8_t event = packet[0];
    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (packet[2] != HCI_STATE_WORKING) break;
            if (cmdline_addr_found){
                printf("Trying to connect to %s\n", bd_addr_to_str(cmdline_addr));
                le_central_connect(cmdline_addr, 0);
                break;
            }
            printf("BTstack activated, start scanning!\n");
            le_central_set_scan_parameters(0,0x0030, 0x0030);
            le_central_start_scan();
            break;
        case GAP_LE_ADVERTISING_REPORT:
            fill_advertising_report_from_packet(&report, packet);
            // stop scanning, and connect to the device
            le_central_stop_scan();
            le_central_connect(report.address,report.address_type);
            break;
        case HCI_EVENT_LE_META:
            // wait for connection complete
            if (packet[2] !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            gc_handle = READ_BT_16(packet, 4);
            // query primary services
            gatt_client_discover_primary_services(gc_id, gc_handle);
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("\nGATT browser - DISCONNECTED\n");
            exit(0);
            break;
        default:
            break;
    }
}
/* LISTING_END */

/* @section GATT Client event handler
 *
 * @text Query results and further queries are handled by the GATT client packet
 * handler, as shown in Listing GATTBrowserQueryHandler. Here, upon
 * receiving the primary services, the
 * gatt_client_discover_characteristics_for_service() query for the last
 * received service is sent. After receiving the characteristics for the service,
 * gap_disconnect is called to terminate the connection. Upon
 * disconnect, the HCI packet handler receives the disconnect complete event.  
 */

/* LISTING_START(GATTBrowserQueryHandler): Handling of the GATT client queries */
static int search_services = 1;

void handle_gatt_client_event(le_event_t * event){
    le_service_t service;
    le_characteristic_t characteristic;
    switch(event->type){
        case GATT_SERVICE_QUERY_RESULT:
            service = ((le_service_event_t *) event)->service;
            dump_service(&service);
            services[service_count++] = service;
            break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
            characteristic = ((le_characteristic_event_t *) event)->characteristic;
            dump_characteristic(&characteristic);
            break;
        case GATT_QUERY_COMPLETE:
            if (search_services){
                // GATT_QUERY_COMPLETE of search services 
                service_index = 0;
                printf("\nGATT browser - CHARACTERISTIC for SERVICE ");
                printUUID128(service.uuid128); printf("\n");
                search_services = 0;
                gatt_client_discover_characteristics_for_service(gc_id, gc_handle, &services[service_index]);
            } else {
                // GATT_QUERY_COMPLETE of search characteristics
                if (service_index < service_count) {
                    service = services[service_index++];
                    printf("\nGATT browser - CHARACTERISTIC for SERVICE ");
                    printUUID128(service.uuid128);
                    printf(", [0x%04x-0x%04x]\n", service.start_group_handle, service.end_group_handle);
                    
                    gatt_client_discover_characteristics_for_service(gc_id, gc_handle, &service);
                    break;
                }
                service_index = 0;
                gap_disconnect(gc_handle); 
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

static void usage(const char *name){
	fprintf(stderr, "\nUsage: %s [-a|--address aa:bb:cc:dd:ee:ff]\n", name);
	fprintf(stderr, "If no argument is provided, GATT browser will start scanning and connect to the first found device.\nTo connect to a specific device use argument [-a].\n\n");
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    int arg = 1;
    cmdline_addr_found = 0;
    
    while (arg < argc) {
		if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
			arg++;
			cmdline_addr_found = sscan_bd_addr((uint8_t *)argv[arg], cmdline_addr);
            arg++;
            continue;
        }
        usage(argv[0]);
        return 0;
	}

    gatt_client_setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}

/* EXAMPLE_END */



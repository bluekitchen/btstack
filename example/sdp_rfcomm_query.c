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

#define BTSTACK_FILE__ "sdp_rfcomm_query.c"
 
// *****************************************************************************
/* EXAMPLE_START(sdp_rfcomm_query): SDP Client - Query RFCOMM SDP record
 *
 * @text The example shows how the SDP Client is used to get all RFCOMM service
 * records from a remote device. It extracts the remote RFCOMM Server Channel, 
 * which are needed to connect to a remote RFCOMM service.
 */
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "hci_cmd.h"
#include "btstack_run_loop.h"

#include "btstack.h"

#define NUM_SERVICES 10

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};

static struct {
    uint8_t channel_nr;
    char    service_name[SDP_SERVICE_NAME_LEN+1];
} services[NUM_SERVICES];

static uint8_t  service_index = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;

static void handle_start_sdp_client_query(void * context){
    UNUSED(context);
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, remote, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;
                (void) sdp_client_register_query_callback(&handle_sdp_client_query_request);
            }
            break;
        default:
            break;
    }
}

static void store_found_service(const char * name, uint8_t port){
    printf("APP: Service name: '%s', RFCOMM port %u\n", name, port);
    if (service_index < NUM_SERVICES){
        services[service_index].channel_nr = port;
        strncpy(services[service_index].service_name, (char*) name, SDP_SERVICE_NAME_LEN);
        services[service_index].service_name[SDP_SERVICE_NAME_LEN] = 0;
        service_index++;
    } else {
        printf("APP: list full - ignore\n");
        return;
    }
}

static void report_found_services(void){
    printf("\n *** Client query response done. ");
    if (service_index == 0){
        printf("No service found.\n\n");
    } else {
        printf("Found following %d services:\n", service_index);
    }
    int i;
    for (i=0; i<service_index; i++){
        printf("     Service name %s, RFCOMM port %u\n", services[i].service_name, services[i].channel_nr);
    }    
    printf(" ***\n\n");
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            store_found_service(sdp_event_query_rfcomm_service_get_name(packet), 
                                sdp_event_query_rfcomm_service_get_rfcomm_channel(packet));
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                printf("SDP query failed 0x%02x\n", sdp_event_query_complete_get_status(packet));
                break;
            } 
            printf("SDP query done.\n");
            report_found_services();
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // init L2CAP
    l2cap_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}

/* EXAMPLE_END */
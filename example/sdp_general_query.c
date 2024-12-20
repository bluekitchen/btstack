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

#define BTSTACK_FILE__ "sdp_general_query.c"
 
// *****************************************************************************
/* EXAMPLE_START(sdp_general_query): SDP Client - Query Remote SDP Records
 *
 * @text The example shows how the SDP Client is used to get a list of 
 * service records on a remote device. 
 */
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"


#ifndef HAVE_POSIX_FILE_IO
// Jambox static const char * remote_addr_string = "00:21:3C:AC:F7:38";
// Mac static const char * remote_addr_string = "F0:18:98:60:3E:E5";
// iPhone 5S: 
static const char * remote_addr_string = "6C:72:E7:10:22:EE";
#endif

static bd_addr_t remote_addr;

static int record_id = -1;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* @section SDP Client Setup
 *
 * @text SDP is based on L2CAP. To receive SDP query events you must register a
 * callback, i.e. query handler, with the SPD parser, as shown in 
 * Listing SDPClientInit. Via this handler, the SDP client will receive the following events:
 * - SDP_EVENT_QUERY_ATTRIBUTE_VALUE containing the results of the query in chunks,
 * - SDP_EVENT_QUERY_COMPLETE indicating the end of the query and the status
 */

/* LISTING_START(SDPClientInit): SDP client setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void sdp_general_query_init(void){
    // init L2CAP
    l2cap_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
}
/* LISTING_END */

/* @section SDP Client Query 
 *
 * @text To trigger an SDP query to get the a list of service records on a remote device,
 * you need to call sdp_client_query_uuid16() with the remote address and the
 * UUID of the public browse group, as shown in Listing SDPQueryUUID. 
 * In this example we used fixed address of the remote device shown in Listing Remote.
 * Please update it with the address of a device in your vicinity, e.g., one reported
 * by the GAP Inquiry example in the previous section.
 */ 

/* LISTING_START(SDPQueryUUID): Querying a list of service records on a remote device. */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("Connecting to %s\n", bd_addr_to_str(remote_addr));
                sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_PROTOCOL_L2CAP);
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

static void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        printf("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

/* @section Handling SDP Client Query Results 
 *
 * @text The SDP Client returns the results of the query in chunks. Each result
 * packet contains the record ID, the Attribute ID, and a chunk of the Attribute
 * value.
 * In this example, we append new chunks for the same Attribute ID in a large buffer,
 * see Listing HandleSDPQUeryResult.
 *
 * To save memory, it's also possible to process these chunks directly by a custom stream parser,
 * similar to the way XML files are parsed by a SAX parser. Have a look at *src/sdp_client_rfcomm.c*
 * which retrieves the RFCOMM channel number and the service name.
 */

/* LISTING_START(HandleSDPQUeryResult): Handling query result chunks. */
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // handle new record
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id){
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                printf("\n---\nRecord nr. %u\n", record_id);
            }

            assertBuffer(sdp_event_query_attribute_byte_get_attribute_length(packet));

            attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
            if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)){
               printf("Attribute 0x%04x: ", sdp_event_query_attribute_byte_get_attribute_id(packet));
               de_dump_data_element(attribute_value);
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                printf("SDP query failed 0x%02x\n", sdp_event_query_complete_get_status(packet));
                break;
            } 
            printf("SDP query done.\n");
            break;
        default:
            break;
    }
}
/* LISTING_END */

#ifdef HAVE_POSIX_FILE_IO
static void usage(const char *name){
    printf("\nUsage: %s -a|--address aa:bb:cc:dd:ee:ff\n", name);
    printf("Use argument -a to connect to a specific device and dump the result of SDP query for L2CAP services.\n\n");
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

#ifdef HAVE_POSIX_FILE_IO
    int remote_addr_found = 0;
    if (argc == 3) {
        if(!strcmp(argv[1], "-a") || !strcmp(argv[1], "--address")){
            remote_addr_found = sscanf_bd_addr(argv[2], remote_addr);
        }
    }
    if (!remote_addr_found){
        usage(argv[0]);
        return 1;
    }
#else
    (void)argc;
    (void)argv;
    sscanf_bd_addr(remote_addr_string, remote_addr);
#endif

    sdp_general_query_init();
    // turn on!
    hci_power_control(HCI_POWER_ON);
            
    return 0;
}

/* EXAMPLE_END */

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

#define __BTSTACK_FILE__ "sdp_general_query.c"
 
// *****************************************************************************
/* EXAMPLE_START(sdp_general_query): Dump remote SDP Records
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

int record_id = -1;
int attribute_id = -1;

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

static void sdp_client_init(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
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

/* LISTING_START(Remote): Address of remote device in big-endian order */
static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
/* LISTING_END */

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
                sdp_client_query_uuid16(&handle_sdp_client_query_result, remote, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
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

    switch (packet[0]){
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
            printf("General query done with status %d.\n\n", sdp_event_query_complete_get_status(packet));
            break;
    }
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;
    
    printf("Client HCI init done\r\n");
    
    sdp_client_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);
            
    return 0;
}

/* EXAMPLE_END */

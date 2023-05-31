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

#define BTSTACK_FILE__ "sdp_bnep_query.c"
 
// *****************************************************************************
/* EXAMPLE_START(sdp_bnep_query): SDP Client - Query BNEP SDP record
 *
 * @text The example shows how the SDP Client is used to get all BNEP service
 * records from a remote device. It extracts the remote BNEP PAN protocol 
 * UUID and the L2CAP PSM, which are needed to connect to a remote BNEP service.
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

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        printf("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

/* @section SDP Client Setup
 *
 * @text As with the previous example, you must register a
 * callback, i.e. query handler, with the SPD parser, as shown in 
 * Listing SDPClientInit. Via this handler, the SDP client will receive events:
 * - SDP_EVENT_QUERY_ATTRIBUTE_VALUE containing the results of the query in chunks,
 * - SDP_EVENT_QUERY_COMPLETE reporting the status and the end of the query. 
 */

/* LISTING_START(SDPClientInit): SDP client setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void sdp_bnep_qeury_init(void){
    // init L2CAP
    l2cap_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
}
/* LISTING_END */


/* @section SDP Client Query 
 * To trigger an SDP query to get the a list of service records on a remote device,
 * you need to call sdp_client_query_uuid16() with the remote address and the
 * BNEP protocol UUID, as shown in Listing SDPQueryUUID. 
 * In this example we again used fixed address of the remote device. Please update
 * for your environment.
 */ 

/* LISTING_START(SDPQueryUUID): Querying the a list of service records on a remote device. */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started 
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("Start SDP BNEP query.\n");
                sdp_client_query_uuid16(&handle_sdp_client_query_result, remote, BLUETOOTH_PROTOCOL_BNEP);
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

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


/* @section Handling SDP Client Query Result 
 *
 * @text The SDP Client returns the result of the query in chunks. Each result
 * packet contains the record ID, the Attribute ID, and a chunk of the Attribute
 * value, see Listing HandleSDPQUeryResult. Here, we show how to parse the
 * Service Class ID List and Protocol Descriptor List, as they contain 
 * the BNEP Protocol UUID and L2CAP PSM respectively.
 */

/* LISTING_START(HandleSDPQUeryResult): Extracting BNEP Protcol UUID and L2CAP PSM */
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    /* LISTING_PAUSE */
    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    char str[20];

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

                /* LISTING_RESUME */
                /* @text The Service Class ID List is a Data Element Sequence (DES) of UUIDs. 
                 * The BNEP PAN protocol UUID is within this list.
                 */

                switch(sdp_event_query_attribute_byte_get_attribute_id(packet)){
                    // 0x0001 "Service Class ID List"
                    case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                        if (de_get_element_type(attribute_value) != DE_DES) break;
                        for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                            uint8_t * element = des_iterator_get_element(&des_list_it);
                            if (de_get_element_type(element) != DE_UUID) continue;
                            uint32_t uuid = de_get_uuid32(element);
                            switch (uuid){
                                case BLUETOOTH_SERVICE_CLASS_PANU:
                                case BLUETOOTH_SERVICE_CLASS_NAP:
                                case BLUETOOTH_SERVICE_CLASS_GN:
                                    printf(" ** Attribute 0x%04x: BNEP PAN protocol UUID: %04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet), (int) uuid);
                                    break;
                                default:
                                    break;
                            }
                        }
                        break;
                    /* LISTING_PAUSE */
                    // 0x0100 "Service Name"
                    case 0x0100:
                    // 0x0101 "Service Description"
                    case 0x0101:
                        get_string_from_data_element(attribute_value, sizeof(str), str);
                        printf(" ** Attribute 0x%04x: %s\n", sdp_event_query_attribute_byte_get_attribute_id(packet), str);
                        break;
                    
                    /* LISTING_RESUME */
                    /* @text The Protocol Descriptor List is DES 
                     * which contains one DES for each protocol. For PAN serivces, it contains
                     * a DES with the L2CAP Protocol UUID and a PSM,
                     * and another DES with the BNEP UUID and the the BNEP version.
                     */
                    case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:{
                            printf(" ** Attribute 0x%04x: ", sdp_event_query_attribute_byte_get_attribute_id(packet));
                            
                            uint16_t l2cap_psm = 0;
                            uint16_t bnep_version = 0;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                                if (des_iterator_get_type(&des_list_it) != DE_DES) continue;
                                uint8_t * des_element = des_iterator_get_element(&des_list_it);
                                des_iterator_init(&prot_it, des_element);
                                uint8_t * element = des_iterator_get_element(&prot_it);
                                
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint32_t uuid = de_get_uuid32(element);
                                des_iterator_next(&prot_it);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &l2cap_psm);
                                        break;
                                    case BLUETOOTH_PROTOCOL_BNEP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_version);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            printf("l2cap_psm 0x%04x, bnep_version 0x%04x\n", l2cap_psm, bnep_version);
                        }
                        break;
                    /* LISTING_PAUSE */
                    default:
                        break;
                }
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)){
                printf("SDP query failed, status 0x%02x\n", sdp_event_query_complete_get_status(packet));
                break;
            } 
            printf("SDP query done.\n");
            break;
        default:
            break;
    }
    /* LISTING_RESUME */
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    printf("Client HCI init done\r\n");

    sdp_bnep_qeury_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);
            
    return 0;
}

/* EXAMPLE_END */

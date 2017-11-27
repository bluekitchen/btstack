/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "hid_host_demo.c"

/*
 * hid_host_demo.c
 */

/* EXAMPLE_START(hid_host_demo): HID Host Demo
 *
 * @text This example implements an HID Host. For now, it connnects to a fixed device, queries the HID SDP
 * record and opens the HID Control + Interrupt channels
 */

#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 200

// SDP
static uint8_t            hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t           hid_descriptor_len;

static uint16_t           hid_control_psm;
static uint16_t           hid_interrupt_psm;

static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;

// L2CAP
static uint16_t            l2cap_hid_control_cid;
static uint16_t            l2cap_hid_interrupt_cid;

// MBP 2016
static const char * remote_addr_string = "F4-0F-24-3B-1B-E1";
// iMpulse static const char * remote_addr_string = "64:6E:6C:C1:AA:B5";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;


/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP is initialized 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Initialize L2CAP 
    l2cap_init();
}
/* LISTING_END */

/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        des_iterator_next(&prot_it);
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_control_psm);
                                        printf("HID Control PSM: 0x%004x\n", (int) hid_control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_interrupt_psm);
                                            printf("HID Interrupt PSM: 0x%004x\n", (int) hid_interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            hid_descriptor_len = sdp_event_query_attribute_byte_get_attribute_length(packet);
                            memcpy(hid_descriptor, attribute_value, hid_descriptor_len);
                            printf("HID Descriptor:\n");
                            printf_hexdump(hid_descriptor, hid_descriptor_len);
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
            if (!hid_control_psm) {
                printf("HID Control PSM missing\n");
                break;
            }
            if (!hid_interrupt_psm) {
                printf("HID Interrupt PSM missing\n");
                break;
            }
            printf("Setup HID\n");
            status = l2cap_create_channel(packet_handler, remote_addr, hid_control_psm, 48, &l2cap_hid_control_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
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
    uint8_t   event;
    bd_addr_t event_addr;
    uint8_t   status;
    uint16_t  l2cap_cid;

    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("Start SDP HID query for remote HID Device.\n");
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
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

                case L2CAP_EVENT_CHANNEL_OPENED: 
                    status = packet[2];
                    if (status){
                        printf("L2CAP Connection failed: 0x%02x\n", status);
                        break;
                    }
                    l2cap_cid  = little_endian_read_16(packet, 13);
                    if (!l2cap_cid) break;
                    if (l2cap_cid == l2cap_hid_control_cid){
                        status = l2cap_create_channel(packet_handler, remote_addr, hid_interrupt_psm, 48, &l2cap_hid_interrupt_cid);
                        if (status){
                            printf("Connecting to HID Control failed: 0x%02x\n", status);
                            break;
                        }
                    }                        
                    if (l2cap_cid == l2cap_hid_interrupt_cid){
                        printf("HID Connection established\n");
                    }
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            // for now, just dump incoming data
            if (channel == l2cap_hid_interrupt_cid){
                printf("HID Interrupt: ");
            } else if (channel == l2cap_hid_control_cid){
                printf("HID Control: ");
            } else {
                break;
            }
            printf_hexdump(packet, size);
        default:
            break;
    }
}
/* LISTING_END */

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);

    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

/* EXAMPLE_END */

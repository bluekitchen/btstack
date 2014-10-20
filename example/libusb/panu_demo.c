/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * panu_demo.c
 * Copyright (C) 2014 Ole Reinhardt <ole.reinhardt@kernelconcepts.de>
 * 
 */

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdp_parser.h"
#include "sdp_client.h"
#include "sdp_query_util.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_parser.h"
#include "des_iterator.h"

int record_id = -1;
int attribute_id = -1;
uint16_t public_browse_group = 0x1002;
uint16_t bnep_protocol_id    = 0x000f;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);

//static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static bd_addr_t remote = {0xE0,0x06,0xE6,0xBB,0x95,0x79};



char * get_string_from_data_element(uint8_t * element){
    de_size_t de_size = de_get_size_type(element);
    int pos     = de_get_header_size(element);
    int len = 0;
    switch (de_size){
        case DE_SIZE_VAR_8:
            len = element[1];
            break;
        case DE_SIZE_VAR_16:
            len = READ_NET_16(element, 1);
            break;
        default:
            break;
    }
    char * str = (char*)malloc(len+1);
    memcpy(str, &element[pos], len);
    str[len] ='\0';
    return str; 
}


/* SDP parser callback */
static void handle_sdp_client_query_result(sdp_query_event_t *event)
{
    sdp_query_attribute_value_event_t *value_event;
    sdp_query_complete_event_t        *complete_event;

    switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
            value_event = (sdp_query_attribute_value_event_t*) event;
            
            /* Handle new SDP record */
            if (value_event->record_id != record_id) {
                record_id = value_event->record_id;
                printf("SDP Record: Nr: %d\n", record_id);
            }

            if (value_event->attribute_length <= attribute_value_buffer_size) {
                attribute_value[value_event->data_offset] = value_event->data;
                
                if ((uint16_t)(value_event->data_offset+1) == value_event->attribute_length) {

                    switch(value_event->attribute_id){
                        case 0x0100:
                        case 0x0101:
                            printf("SDP Attribute: 0x%04x: %s\n", value_event->attribute_id, get_string_from_data_element(attribute_value));
                            break;
                        case 0x004: {
                                des_iterator_t des_list_it;
                                uint16_t       l2cap_psm = 0;
                                uint16_t       bnep_version = 0;

                                printf("SDP Attribute: 0x%04x\n", value_event->attribute_id);

                                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    des_iterator_t prot_it;
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint16_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_element_get_uuid16(element);
                                    switch (uuid){
                                        case 0x0100:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &l2cap_psm);
                                            break;
                                        case 0x000f:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                                printf("l2cap_psm 0x%04x, bnep_version 0x%04x\n", l2cap_psm, bnep_version);
                                printf("  -> row data (length %d): ", value_event->attribute_length);
                                hexdumpf(attribute_value, value_event->attribute_length);
                                printf("\n");

                                /* Create BNEP connection */
                                bnep_connect(NULL, &remote, l2cap_psm, BNEP_UUID_GN);
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, value_event->attribute_length);
            }
            break;
            
        case SDP_QUERY_COMPLETE:
            complete_event = (sdp_query_complete_event_t*) event;
            fprintf(stderr, "General query done with status %d.\n", complete_event->status);

            break;
    }
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    uint8_t event;
    bd_addr_t event_addr;
    uint16_t  uuid_source;
    uint16_t  uuid_dest;
    uint16_t  mtu;    
    
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    
    event = packet[0];
    
    switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (event) {            
                case BTSTACK_EVENT_STATE:
                    /* BT Stack activated, get started */ 
                    if (packet[2] == HCI_STATE_WORKING) {
                        /* Send a general query for BNEP Protocol ID */
                        printf("Start SDP BNEP query.\n");
                        sdp_general_query_for_uuid(remote, bnep_protocol_id);
                    }
                    break;

                case HCI_EVENT_COMMAND_COMPLETE:
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                        bt_flip_addr(event_addr, &packet[6]);
                        printf("BD-ADDR: %s\n", bd_addr_to_str(event_addr));
                        break;
                    }
                    break;

                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06u'\n", READ_BT_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case BNEP_EVENT_INCOMING_CONNECTION:
					// data: event(8), len(8), status (8), bnep source uuid (16), bnep destination uuid (16), remote_address (48)
                    uuid_source = READ_BT_16(packet, 3);
                    uuid_dest   = READ_BT_16(packet, 5);
                    mtu         = READ_BT_16(packet, 7);
					bt_flip_addr(event_addr, &packet[9]);
					printf("BNEP connection from %s source UUID 0x%04x dest UUID: 0x%04x, max frame size: %u", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
					break;
					
				case BNEP_EVENT_OPEN_CHANNEL_COMPLETE:
                    if (packet[2]) {
                        printf("BNEP channel open failed, status %02x\n", packet[2]);
                    } else {
                        // data: event(8), len(8), status (8), bnep source uuid (16), bnep destination uuid (16), remote_address (48)
                        uuid_source = READ_BT_16(packet, 3);
                        uuid_dest   = READ_BT_16(packet, 5);
                        mtu         = READ_BT_16(packet, 7);
                        bt_flip_addr(event_addr, &packet[9]); 
                        printf("BNEP connection open succeeded to %s source UUID 0x%04x dest UUID: 0x%04x, max frame size %u", bd_addr_to_str(event_addr), uuid_source, uuid_dest, mtu);
                    }
					break;
                    
                case BNEP_EVENT_CHANNEL_CLOSED:
                    printf("BNEP channel closed\n");
                    break;

                    
                default:
                    break;
            }
            
        default:
            break;
    }
}

static void btstack_setup(void)
{
    hci_transport_t    *transport;
    hci_uart_config_t  *config;
    bt_control_t       *control;
    remote_device_db_t *remote_db;

    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);

    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
   
    transport = hci_transport_usb_instance();
    config    = NULL;
    control   = NULL;    
    remote_db = (remote_device_db_t *) &remote_device_db_memory;
    
    hci_init(transport, config, control, remote_db);

    printf("Client HCI init done\n");
    
    /* Initialize L2CAP */
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    /* Initialise BNEP */
    bnep_init();
    bnep_register_packet_handler(packet_handler);
//    bnep_register_service(NULL, BNEP_UUID_PANU, 1691);  /* Minimum L2CAP MTU for bnep is 1691 bytes */

    /* Turn on the device */
    hci_power_control(HCI_POWER_ON);
}

int main(int argc, char **argv)
{
    /* Initialise SDP */
    sdp_parser_init();
    sdp_parser_register_callback(handle_sdp_client_query_result);

    /* Setup BT-Stack */
    btstack_setup();

    /* Start mainloop */
    run_loop_execute(); 
    return 0;
}

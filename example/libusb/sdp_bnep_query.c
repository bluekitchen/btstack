
// *****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

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
#include "pan.h"

int record_id = -1;
int attribute_id = -1;
uint16_t public_browse_group =  0x1002;
uint16_t bnep_protocol_id = 0x000f;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};

static void handle_sdp_client_query_result(sdp_query_event_t * event);

static void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        printf("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                printf("Start SDP BNEP query.\n");
                sdp_general_query_for_uuid(remote, bnep_protocol_id);
            }
            break;
        default:
            break;
    }
}

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



static void handle_sdp_client_query_result(sdp_query_event_t * event){
    sdp_query_attribute_value_event_t * ve;
    sdp_query_complete_event_t * ce;
    des_iterator_t des_list_it;
    des_iterator_t prot_it;

    switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
            ve = (sdp_query_attribute_value_event_t*) event;
            
            // handle new record
            if (ve->record_id != record_id){
                record_id = ve->record_id;
                printf("\n---\nRecord nr. %u\n", record_id);
            }

            assertBuffer(ve->attribute_length);

            attribute_value[ve->data_offset] = ve->data;
            if ((uint16_t)(ve->data_offset+1) == ve->attribute_length){
                switch(ve->attribute_id){
                    case SDP_ServiceClassIDList:
                        if (de_get_element_type(attribute_value) != DE_DES) break;
                        for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                            uint8_t * element = des_iterator_get_element(&des_list_it);
                            if (de_get_element_type(element) != DE_UUID) continue;
                            uint16_t uuid = de_element_get_uuid16(element);
                            switch (uuid){
                                case PANU_UUID:
                                case NAP_UUID:
                                case GN_UUID:
                                    printf(" ** Attribute 0x%04x: BNEP PAN protocol UUID: %04x\n", ve->attribute_id, uuid);
                                    break;
                                default:
                                    break;
                            }
                        }
                        break;
                    case 0x0100:
                    case 0x0101:
                        printf(" ** Attribute 0x%04x: %s\n", ve->attribute_id, get_string_from_data_element(attribute_value));
                        break;
                    case 0x004:{
                            printf(" ** Attribute 0x%04x: ", ve->attribute_id);
                            
                            uint16_t l2cap_psm = 0;
                            uint16_t bnep_version = 0;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
                                if (des_iterator_get_type(&des_list_it) != DE_DES) continue;
                                uint8_t * des_element = des_iterator_get_element(&des_list_it);
                                des_iterator_init(&prot_it, des_element);
                                uint8_t * element = des_iterator_get_element(&prot_it);
                                
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint16_t uuid = de_element_get_uuid16(element);
                                switch (uuid){
                                    case 0x100:
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
                            // printf("  -> row data (length %d): ", ve->attribute_length);
                            // hexdumpf(attribute_value, ve->attribute_length);
                            // printf("\n");
                            
                        }
                        break;
                    default:
                        break;
                }

                
            }
            break;
        case SDP_QUERY_COMPLETE:
            ce = (sdp_query_complete_event_t*) event;
            printf("General query done with status %d.\n\n", ce->status);

            break;
    }
}

 static void btstack_setup(){
    /// GET STARTED ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
    
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
   
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config = NULL;
    bt_control_t       * control   = NULL;    

    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    printf("Client HCI init done\r\n");
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);
}

int main(void){
    sdp_parser_init();
    sdp_parser_register_callback(handle_sdp_client_query_result);
            
    btstack_setup();
    // go!
    run_loop_execute(); 
    return 0;
}

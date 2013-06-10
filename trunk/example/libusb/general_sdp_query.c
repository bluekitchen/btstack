
//*****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
//*****************************************************************************

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdp_parser.h"
#include "sdp_client.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};

static void handle_general_sdp_parser_event(sdp_parser_event_t * event);

static const uint8_t des_attributeIDList[]       = {0x35, 0x05, 0x0A, 0x00, 0x01, 0xff, 0xff};  // Arribute: 0x0001 - 0xffff
static const uint8_t des_serviceSearchPattern[] =  {0x35, 0x03, 0x19, 0x10, 0x02};

static void general_query(bd_addr_t remote_dev){
    sdp_parser_init();
    sdp_parser_register_callback(handle_general_sdp_parser_event);
    sdp_client_query(remote, (uint8_t*)&des_serviceSearchPattern, (uint8_t*)&des_attributeIDList[0]);
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                general_query(remote);
            }
            break;
        default:
            break;
    }
}

static void btstack_setup(){
    /// GET STARTED ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
    
    hci_dump_open("/tmp/hci_dump_sdp_client.pklg", HCI_DUMP_PACKETLOGGER);
   
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


uint16_t attribute_id = -1;
uint16_t record_id = -1;
uint8_t * attribute_value = NULL;
int       attribute_value_buffer_size = 0;

void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        attribute_value_buffer_size *= 2;
        attribute_value = (uint8_t *) realloc(attribute_value, attribute_value_buffer_size);
    }
}

static void handle_general_sdp_parser_event(sdp_parser_event_t * event){
    sdp_parser_attribute_value_event_t * ve;
    sdp_parser_complete_event_t * ce;

    switch (event->type){
        case SDP_PARSER_ATTRIBUTE_VALUE:
            ve = (sdp_parser_attribute_value_event_t*) event;
            
            // handle new record
            if (ve->record_id != record_id){
                record_id = ve->record_id;
                printf("\n---\nRecord nr. %u\n", record_id);
            }

            assertBuffer(ve->attribute_length);

            attribute_value[ve->data_offset] = ve->data;
            if ((uint16_t)(ve->data_offset+1) == ve->attribute_length){
               printf("Attribute 0x%04x: ", ve->attribute_id);
               de_dump_data_element(attribute_value);
            }
            break;
        case SDP_PARSER_COMPLETE:
            ce = (sdp_parser_complete_event_t*) event;
            printf("General query done with status %d.\n\n", ce->status);
            break;
    }
}

int main(void){
    attribute_value_buffer_size = 1000;
    attribute_value = (uint8_t*) malloc(attribute_value_buffer_size);
    record_id = -1;
    sdp_parser_init();
    sdp_parser_register_callback(handle_general_sdp_parser_event);

    btstack_setup();
    // go!
    run_loop_execute(); 
    return 0;
}

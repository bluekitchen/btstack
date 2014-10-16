
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
#include <msp430x54x.h>

#include "sdp_parser.h"
#include "sdp_client.h"
#include "sdp_query_util.h"

#include "bt_control_cc256x.h"
#include "hal_adc.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_lcd.h"
#include "hal_usb.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static const uint8_t des_attributeIDList[]       = {0x35, 0x05, 0x0A, 0x00, 0x01, 0xff, 0xff};  // Arribute: 0x0001 - 0xffff
static const uint8_t des_serviceSearchPattern[] =  {0x35, 0x03, 0x19, 0x00, 0x00};

uint16_t attribute_id = -1;
uint16_t record_id = -1;
int      attribute_value_buffer_size = 1000;
uint8_t  attribute_value[1000];

static void handle_general_sdp_parser_event(sdp_query_event_t * event);

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                sdp_general_query_for_uuid(remote, 0x1002);
            }
            break;
        default:
            break;
    }
}

static void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        printf("Buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

static void handle_general_sdp_parser_event(sdp_query_event_t * event){
    sdp_query_attribute_value_event_t * ve;
    sdp_query_complete_event_t * ce;

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
               printf("Attribute 0x%04x: ", ve->attribute_id);
               de_dump_data_element(attribute_value);
            }
            break;
        case SDP_QUERY_COMPLETE:
            ce = (sdp_query_complete_event_t*) event;
            printf("General query done with status %d.\n\n", ce->status);
            break;
    }
}


static void hw_setup(){
    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1(); 
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // init debug UART
    halUsbInit();

    // init LEDs
    LED_PORT_OUT |= LED_1 | LED_2;
    LED_PORT_DIR |= LED_1 | LED_2;

    // ready - enable irq used in h4 task
    __enable_interrupt();  
}

static void btstack_setup(){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
    
    // init HCI
    hci_transport_t    * transport = hci_transport_h4_dma_instance();
    bt_control_t       * control   = bt_control_cc256x_instance();
    hci_uart_config_t  * config    = hci_uart_config_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    
    // use eHCILL
    bt_control_cc256x_enable_ehcill(1);
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
}


int main(void){
    sdp_parser_init();
    sdp_parser_register_callback(handle_general_sdp_parser_event);

    hw_setup();
    btstack_setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    // go!
    run_loop_execute(); 
    return 0;
}

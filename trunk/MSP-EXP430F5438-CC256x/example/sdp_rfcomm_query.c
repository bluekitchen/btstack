
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

#include "sdp_query_rfcomm.h"

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
#include "bt_control_cc256x.h"

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};

static uint8_t  service_index = 0;
static uint8_t  channel_nr[10];
static char*    service_name[10];


static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                sdp_query_rfcomm_channel_and_name_for_uuid(remote, 0x1002);
            }
            break;
        default:
            break;
    }
}


void store_found_service(uint8_t * name, uint8_t port){
    printf("APP: Service name: '%s', RFCOMM port %u\n", name, port);
    channel_nr[service_index] = port;
    service_name[service_index] = (char*) malloc(SDP_SERVICE_NAME_LEN+1);
    strncpy(service_name[service_index], (char*) name, SDP_SERVICE_NAME_LEN);
    service_name[service_index][SDP_SERVICE_NAME_LEN] = 0;
    service_index++;
}

void report_found_services(){
    printf("\n *** Client query response done. ");
    if (service_index == 0){
        printf("No service found.\n\n");
    } else {
        printf("Found following %d services:\n", service_index);
    }
    int i;
    for (i=0; i<service_index; i++){
        printf("     Service name %s, RFCOMM port %u\n", service_name[i], channel_nr[i]);
    }    
    printf(" ***\n\n");
}

void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
            
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            store_found_service(ve->service_name, ve->channel_nr);
            break;
        case SDP_QUERY_COMPLETE:
            report_found_services();
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
    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);

    hw_setup();
    btstack_setup();
    
    printf("Run...\n\r");

    // turn on!
    hci_power_control(HCI_POWER_ON);
    // go!
    run_loop_execute(); 
    return 0;
}

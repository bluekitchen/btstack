// *****************************************************************************
//
// spp_counter demo - it provides an SPP and sends a counter every second
//
// it doesn't use the LCD to get down to a minimal memory footprint
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "rfcomm.h"
#include "sdp.h"
#include "btstack-config.h"

#define HEARTBEAT_PERIOD_MS 1000

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[150];
static timer_source_t heartbeat;
    
static int real_counter = 0;
static int counter_to_send = 0;

enum STATE {INIT, W4_CONNECTION, W4_CHANNEL_COMPLETE, ACTIVE} ;
enum STATE state = INIT;

static void send_packet(void){
    if (real_counter <= counter_to_send) return;
                
    char lineBuffer[30];
    sprintf(lineBuffer, "BTstack counter %04u\n\r", counter_to_send);
    printf(lineBuffer);
    
    int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));
    if (err) return;
    
    counter_to_send++;
}

// Bluetooth logic
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    uint8_t event = packet[0];

    // handle events, ignore data
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started - set local name
            if (packet[2] == HCI_STATE_WORKING) {
                hci_send_cmd(&hci_write_local_name, "BTstack SPP Counter");
            }
            break;
        
        case HCI_EVENT_COMMAND_COMPLETE:
            if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                bt_flip_addr(event_addr, &packet[6]);
                printf("BD-ADDR: %s\n\r", bd_addr_to_str(event_addr));
                break;
            }
            if (COMMAND_COMPLETE_EVENT(packet, hci_write_local_name)){
                hci_discoverable_control(1);
                break;
            }
            break;

        case HCI_EVENT_LINK_KEY_REQUEST:
            // deny link key request
            printf("Link key request\n\r");
            bt_flip_addr(event_addr, &packet[2]);
            hci_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
            break;
            
        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            printf("Pin code request - using '0000'\n\r");
            bt_flip_addr(event_addr, &packet[2]);
            hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
            break;
        
        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            bt_flip_addr(event_addr, &packet[2]); 
            rfcomm_channel_nr = packet[8];
            rfcomm_channel_id = READ_BT_16(packet, 9);
            printf("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(event_addr));
            rfcomm_accept_connection_internal(rfcomm_channel_id);
            break;
            
        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n\r", packet[2]);
            } else {
                rfcomm_channel_id = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
            }
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
        case RFCOMM_EVENT_CREDITS:
            if (rfcomm_can_send_packet_now(rfcomm_channel_id)) send_packet();
            break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_channel_id = 0;
            break;
        
        default:
            break;
    }

}

static void run_loop_register_timer(timer_source_t *timer, uint16_t period){
    run_loop_set_timer(timer, period);
    run_loop_add_timer(timer);
}


static void timer_handler(timer_source_t *ts){
    real_counter++;
    // re-register timer
    run_loop_register_timer(ts, HEARTBEAT_PERIOD_MS);
} 

// main
int btstack_main(void);
int btstack_main(void){
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
    
    // init RFCOMM
    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, 1, "SPP Counter");
    printf("SDP service buffer size: %u\n\r", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);

    // set one-shot timer
    heartbeat.process = &timer_handler;
    run_loop_register_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    
    printf("Run...\n\r");

    // turn on!
    hci_power_control(HCI_POWER_ON);

    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}


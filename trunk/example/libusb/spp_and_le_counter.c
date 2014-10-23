// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "rfcomm.h"

#include "sdp.h"
#include "spp_and_le_counter.h"

#include "att.h"
#include "att_server.h"
#include "central_device_db.h"
#include "gap_le.h"
#include "sm.h"

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[150];
static int       le_notification_enabled;

// THE Couner
static int  counter = 0;
static char counter_string[30];
static int  counter_string_len;

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x0b, 0x09, 'L', 'E', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r', 
};
uint8_t adv_data_len = sizeof(adv_data);

enum {
    SET_ADVERTISEMENT_PARAMS = 1 << 0,
    SET_ADVERTISEMENT_DATA   = 1 << 1,
    ENABLE_ADVERTISEMENTS    = 1 << 2,
};
static uint16_t todos = 0;


static void gap_run(){

    if (!hci_can_send_command_packet_now()) return;

    if (todos & SET_ADVERTISEMENT_DATA){
        printf("GAP_RUN: set advertisement data\n");
        todos &= ~SET_ADVERTISEMENT_DATA;
        hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type = 0;   // default
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x0030;
        uint16_t adv_int_max = 0x0030;
        hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, 0, 0, &null_addr, 0x07, 0x00);
        return;
    }    

    if (todos & ENABLE_ADVERTISEMENTS){
        printf("GAP_RUN: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        return;
    }
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
                        todos = SET_ADVERTISEMENT_PARAMS | SET_ADVERTISEMENT_DATA | ENABLE_ADVERTISEMENTS;
                        gap_run();
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

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    todos = ENABLE_ADVERTISEMENTS;
                    le_notification_enabled = 0;
                    gap_run();
                    break;

                case RFCOMM_EVENT_INCOMING_CONNECTION:
					// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
					bt_flip_addr(event_addr, &packet[2]); 
					rfcomm_channel_nr = packet[8];
					rfcomm_channel_id = READ_BT_16(packet, 9);
					printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection_internal(rfcomm_channel_id);
					break;
					
				case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
					// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
					if (packet[2]) {
						printf("RFCOMM channel open failed, status %u\n", packet[2]);
					} else {
						rfcomm_channel_id = READ_BT_16(packet, 12);
						mtu = READ_BT_16(packet, 14);
						printf("\nRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
					}
					break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
			}
            break;
                        
        case RFCOMM_DATA_PACKET:
            printf("RCV: '");
            for (i=0;i<size;i++){
                putchar(packet[i]);
            }
            printf("'\n");
            break;

        default:
            break;
	}
    gap_run();
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(uint16_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    if (att_handle == ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE){
        if (buffer){
            memcpy(buffer, &counter_string[offset], counter_string_len - offset);
        }
        return counter_string_len - offset;
    }
    return 0;
}

// write requests
static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    // printf("WRITE Callback, handle %04x, mode %u, offset %u, data: ", handle, transaction_mode, offset);
    // printf_hexdump(buffer, buffer_size);
    if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    le_notification_enabled = READ_BT_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    return 0;
}
static void  heartbeat_handler(struct timer *ts){

    counter++;
    counter_string_len = sprintf(counter_string, "BTstack counter %04u\n", counter);

    if (rfcomm_channel_id){
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)) {
            int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) counter_string, counter_string_len);
            if (err) {
                log_error("rfcomm_send_internal -> error 0X%02x", err);
            } else {
                printf("%s", counter_string);
            }
        }
    }

    if (le_notification_enabled) {
        att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 

void setup(void){
	/// GET STARTED with BTstack ///
	btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
	    
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
	hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t * config = NULL;
	bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
	hci_init(transport, config, control, remote_db);
	hci_discoverable_control(1);

	l2cap_init();
	l2cap_register_packet_handler(packet_handler);

	rfcomm_init();
	rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
	memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    // service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    // sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    // printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    // sdp_register_service_internal(NULL, service_record_item);
    sdp_create_spp_service( spp_service_buffer, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    printf("SDP service record size: %u\n", de_get_len(spp_service_buffer));
    sdp_register_service_internal(NULL, spp_service_buffer);

    hci_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // setup central device db
    central_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_dump_attributes();
}

// main == setup
int main(void)
{
    setup();

    // set one-shot timer
    timer_source_t heartbeat;
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	
    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}


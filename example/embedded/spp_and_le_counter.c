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

// *****************************************************************************
/* EXAMPLE_START(spp_and_le_counter): Dual mode example
 * 
 * @text The SPP and LE Counter example combines the Bluetooth Classic SPP Counter
 * and the Bluetooth LE Counter into a single application.
 *
 * In this Section, we only point out the differences to the individual examples
 * and how how the stack is configured.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
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
#include "le_device_db.h"
#include "gap_le.h"
#include "sm.h"

#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000

static uint16_t  rfcomm_channel_id;
static uint32_t   spp_service_buffer[150/4]; // implicit alignment to 4-byte memory address
static int       le_notification_enabled;

// THE Couner
static timer_source_t heartbeat;
static int  counter = 0;
static char counter_string[30];
static int  counter_string_len;

/*
 * @section Advertisements 
 *
 * @text The Flags attribute in the Advertisement Data indicates if a device is in dual-mode or not.
 * Flag 0x02 indicates LE General Discoverable, Dual-Mode device. See Listing advertisements.
 */
/* LISTING_START(advertisements): Advertisement data: Flag 0x02 indicates a dual mode device */
const uint8_t adv_data[] = {
    // Flags: General Discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x0b, 0x09, 'L', 'E', ' ', 'C', 'o', 'u', 'n', 't', 'e', 'r', 
};
/* LISTING_END */
uint8_t adv_data_len = sizeof(adv_data);


/* 
 * @section Packet Handler
 * 
 * @text The packet handler of the combined example is just the combination of the individual packet handlers.
 */

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", READ_BT_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    le_notification_enabled = 0;
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
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(uint16_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    if (att_handle == ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE){
        if (buffer){
            log_info("att_read_callback for Characteristic *FF11*, value %s", counter_string);
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

/*
 * @section Heartbeat Handler
 * 
 * @text Similar to the packet handler, the heartbeat handler is the combination of the individual ones.
 * After updating the counter, it sends an RFCOMM packet if an RFCOMM connection is active,
 * and an LE notification if the remote side has requested notifications.
 */

 /* LISTING_START(heartbeat): Combined Heartbeat handler */
static void  heartbeat_handler(struct timer *ts){

    counter++;
    counter_string_len = sprintf(counter_string, "BTstack counter %04u\n", counter);
    // log_info("%s", counter_string);

    if (rfcomm_channel_id){
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)){
            int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) counter_string, counter_string_len);
            if (err) {
                log_error("rfcomm_send_internal -> error 0X%02x", err);
            }
        }
    }

    if (le_notification_enabled) {
        int err = att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
        if (err){
            log_error("att_server_notify -> error 0X%02x", err);
        }
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 
/* LISTING_END */

/*
 * @section Main Application Setup
 *
 * @text As with the packet and the heartbeat handlers, the combined app setup contains the code from the individual example setups.
 */

/* LISTING_START(MainConfiguration): Init L2CAP RFCOMM SDO SM ATT Server and start heartbeat timer */
int btstack_main(void);
int btstack_main(void)
{
    hci_discoverable_control(1);

    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 0xffff);

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
/* LISTING_PAUSE */
#ifdef EMBEDDED
/* LISTING_RESUME */
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    printf("SDP service buffer size: %u\n", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);
/* LISTING_PAUSE */
#else
    sdp_create_spp_service( (uint8_t*)spp_service_buffer, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    printf("SDP service record size: %u\n", de_get_len((uint8_t*)spp_service_buffer));
    sdp_register_service_internal(NULL, (uint8_t*)spp_service_buffer);
#endif
/* LISTING_RESUME */

    hci_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

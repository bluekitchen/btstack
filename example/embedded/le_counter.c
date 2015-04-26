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
/* EXAMPLE_START(le_counter): LE Peripheral - Heartbeat Counter over GATT
 *
 * @text All newer operating systems provide GATT Client functionality.
 * The LE Counter examples demonstrates how to specify a minimal GATT Database
 * with a custom GATT Service and a custom Characteristic that sends periodic
 * notifications. 
 */
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

#include "le_counter.h"

#include "att.h"
#include "att_server.h"
#include "le_device_db.h"
#include "gap_le.h"
#include "sm.h"

#define HEARTBEAT_PERIOD_MS 1000

/* LISTING_START(notificationFlag): Notification Flag */
static int  le_notification_enabled;
/* LISTING_END */

/*
 * @section Managing LE Advertisements
 *
 * @text To be discoverable, LE Advertisements are enabled using direct HCI Commands.
 * As there's no guarantee that a HCI command can always be sent, the gap_run function
 * is used to send each command as requested by the $todos$ variable.
 * First, the advertisement data is set, then the advertisement params incl. the 
 * advertisement interval is set. Finally, advertisements are enabled. See Listing gapRun.
 *
 * In this example, the Advertisement contains the Flags attribute and the device name.
 * The flag 0x06 indicates: LE General Discoverable Mode and BR/EDR not supported.
 */

/* LISTING_START(gapRun): Handle GAP Tasks */
const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x06, 
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
/* LISTING_END */

/* 
 * @section Packet Handler
 *
 * @text The packet handler is only used to trigger advertisements after BTstack is ready and after disconnects.
 * See Listing packetHandler. 
 * Advertisemetns are not automatically re-enabled after a connection was closed although it was active before.
 *
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    todos = ENABLE_ADVERTISEMENTS;
                    le_notification_enabled = 0;
                    gap_run();
                    break;
                
                default:
                    break;
			}
            break;

        default:
            break;
	}
    gap_run();
}
/* LISTING_END */

/*
 * @section Hearbeat Handler
 *
 * @text The hearbeat handler updates the value of the single Characteristic provided in this example
 * and sends a notification for this characteristic if enabled. See Listing heartbeat.
 */

 /* LISTING_START(heartbeat): Hearbeat Handler */
static timer_source_t heartbeat;
static int  counter = 0;
static char counter_string[30];
static int  counter_string_len;

static void  heartbeat_handler(struct timer *ts){
    counter++;
    counter_string_len = sprintf(counter_string, "BTstack counter %04u\n", counter);
    puts(counter_string);

    if (le_notification_enabled) {
        att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
    }
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 
/* LISTING_END */


/*
 * @section ATT Read
 *
 * The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value lenght. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */

/* LISTING_START(attRead): ATT Read */

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
/* LISTING_END */


/*
 * @section ATT Write
 *
 * @text The only valid att write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the att_handle matches the client configuration handle, the new configuration value is stored and used
 * in the hearbeat handler to decide if a new value should be sent. See Listing attWrite
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    le_notification_enabled = READ_BT_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    return 0;
}
/* LISTING_END */


/* @section Main app setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_counter.gatt$. Finally, it configures the heartbeat handler and
 * boots the Bluetooth stack.
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP SM ATT Server and start heartbeat timer */
int btstack_main(void);
int btstack_main(void)
{
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
    att_dump_attributes();
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(&heartbeat);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */

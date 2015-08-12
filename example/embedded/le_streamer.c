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
/* EXAMPLE_START(le_streamer): LE Peripheral - Stream data over GATT
 *
 * @text All newer operating systems provide GATT Client functionality.
 * This example shows how to get a maximal throughput via BLE:
 * - send whenever possible
 * - use the max ATT MTU
 *
 * In theory, we should also update the connection parameters, but we already get
 * a connection interval of 30 ms and there's no public way to use a shorter 
 * interval with iOS (if we're not implementing an HID device)
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

#include "le_streamer.h"

#include "att.h"
#include "att_server.h"
#include "le_device_db.h"
#include "gap_le.h"
#include "sm.h"

static int   le_notification_enabled;
static void  packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static int   att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static void  streamer(void);

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x06, 
    // Name
    0x0c, 0x09, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r', 
};
const uint8_t adv_data_len = sizeof(adv_data);

static int  counter = 'A';
static char test_data[200];
static int  test_data_len;

static uint16_t conn_handle;

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager, and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_streamer.gatt$. Finally, it configures the advertisements 
 * and boots the Bluetooth stack. 
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP, SM, ATT Server, and enable advertisements */

static void le_streamer_setup(void){
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, att_write_callback);    
    att_server_register_packet_handler(packet_handler);
    
    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);
}
/* LISTING_END */

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */
#define REPORT_INTERVAL_MS 3000
static uint32_t test_data_sent;
static uint32_t test_data_start;

static void test_reset(void){
    test_data_start = run_loop_get_time_ms();
    test_data_sent = 0;
}

static void test_track_sent(int bytes_sent){
    test_data_sent += test_data_len;
    // evaluate
    uint32_t now = run_loop_get_time_ms();
    uint32_t time_passed = now - test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    int bytes_per_second = test_data_sent * 1000 / time_passed;
    printf("%u bytes sent-> %u.%03u kB/s\n", test_data_sent, bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    test_data_start = now;
    test_data_sent  = 0;
}
/* LISTING_END(tracking): Tracking throughput */

/* 
 * @section Packet Handler
 *
 * @text The packet handler is used to stop the notifications and reset the MTU on connect
 * It would also be a good place to request the connection parameter update as indicated 
 * in the commented code block.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    int mtu;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    le_notification_enabled = 0;
                    break;
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            test_data_len = ATT_DEFAULT_MTU - 3;
                            conn_handle = READ_BT_16(packet, 4);
                            // min con interval 20 ms 
                            // gap_request_connection_parameter_update(conn_handle, 0x10, 0x18, 0, 0x0048);
                            // printf("Connected, requesting conn param update for handle 0x%04x\n", conn_handle);
                            break;
                    }
                    break;  
                case ATT_MTU_EXCHANGE_COMPLETE:
                    mtu = READ_BT_16(packet, 4) - 3;
                    printf("ATT MTU = %u\n", mtu);
                    test_data_len = mtu - 3;
                    break;
            }
    }
    // try sending whenever something happens
    streamer();
}

/* LISTING_END */
/*
 * @section Streamer
 *
 * @text The streamer function checks if notifications are enabled and if a notification can be sent now.
 * It creates some test data - a single letter that gets increased every time - and tracks the data sent.
 */

 /* LISTING_START(streamer): Streaming code */
static void streamer(void){
    // check if we can send
    if (!le_notification_enabled) return;
    if (!att_server_can_send()) return;

    // create test data
    int i;
    counter++;
    if (counter > 'Z') counter = 'A';
    for (i=0;i<sizeof(test_data);i++){
        test_data[i] = counter;
    }

    // send
    att_server_notify(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) test_data, test_data_len);

    // track
    test_track_sent(test_data_len);
} 
/* LISTING_END */

/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(uint16_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    le_notification_enabled = READ_BT_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    test_reset();
    return 0;
}
/* LISTING_END */

int btstack_main(void);
int btstack_main(void)
{
    le_streamer_setup();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

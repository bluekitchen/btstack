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

#include "btstack.h"
#include "le_streamer.h"

static int   le_notification_enabled;
static void  packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static int   att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static void  streamer(void);
static btstack_packet_callback_registration_t hci_event_callback_registration;

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x0c, 0x09, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r', 
};
const uint8_t adv_data_len = sizeof(adv_data);

static int  counter = 'A';
static char test_data[200];
static int  test_data_len;

static hci_con_handle_t connection_handle;

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager, and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_streamer.gatt$. Finally, it configures the advertisements 
 * and boots the Bluetooth stack. 
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP, SM, ATT Server, and enable advertisements */

static void le_streamer_setup(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

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
    test_data_start = btstack_run_loop_get_time_ms();
    test_data_sent = 0;
}

static void test_track_sent(int bytes_sent){
    test_data_sent += test_data_len;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
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
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    le_notification_enabled = 0;
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            test_data_len = ATT_DEFAULT_MTU - 3;
                            connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                            // min con interval 20 ms 
                            // gap_request_connection_parameter_update(connection_handle, 0x10, 0x18, 0, 0x0048);
                            // printf("Connected, requesting conn param update for handle 0x%04x\n", connection_handle);
                            break;
                    }
                    break;  
                case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
                    mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
                    printf("ATT MTU = %u\n", mtu);
                    test_data_len = mtu - 3;
                    break;
                case ATT_EVENT_CAN_SEND_NOW:
                    streamer();
                    break;
            }
    }
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

    // create test data
    counter++;
    if (counter > 'Z') counter = 'A';
    memset(test_data, counter, sizeof(test_data));

    // send
    att_server_notify(connection_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) test_data, test_data_len);

    // track
    test_track_sent(test_data_len);

    // request next send event
    att_server_request_can_send_now_event(connection_handle);
} 
/* LISTING_END */

/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored.
 * If notifications get enabled, an ATT_EVENT_CAN_SEND_NOW is requested. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    // printf("att_write_callback att_handle %04x, transaction mode %u\n", att_handle, transaction_mode);
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE) return 0;
    switch(att_handle){
        case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
            le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            printf("Notifications enabled %u\n", le_notification_enabled); 
            if (le_notification_enabled){
                att_server_request_can_send_now_event(connection_handle);
            }
            test_reset();
            break;
        case ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE:
            printf("Write to ...FF12...: ");
            printf_hexdump(buffer, buffer_size);
            break;       
    }
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

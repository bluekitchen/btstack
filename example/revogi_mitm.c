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

#include "revogi_mitm.h"
#include "btstack.h"

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_counter.gatt$. 
 * Additionally, it enables the Battery Service Server with the current battery level.
 * Finally, it configures the advertisements 
 * and the heartbeat handler and boots the Bluetooth stack. 
 * In this example, the Advertisement contains the Flags attribute and the device name.
 * The flag 0x06 indicates: LE General Discoverable Mode and BR/EDR not supported.
 */
 
/* LISTING_START(MainConfiguration): Init L2CAP SM ATT Server and start heartbeat timer */
static int  le_notification_enabled;
static btstack_packet_callback_registration_t hci_event_callback_registration;
// static hci_con_handle_t con_handle;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Service 16-bit: FFF0
    0x03, 0x02, 0xF0, 0xFF,
    // Manufacturer specific data (aa bb - address 78:A5:04:82:1B:A4)
    0x09, 0xFF, 0x04, 0x1E, 0x78, 0xA5, 0x04, 0x82, 0x1B, 0xA4,    
    // Name
    0x08, 0x09, 'D', 'E', 'L', 'I', 'G', 'H', 'T',  
};
const uint8_t adv_data_len = sizeof(adv_data);

static void le_counter_setup(void){

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    
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
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - stop the counter after a disconnect
 *        - send a notification when the requested ATT_EVENT_CAN_SEND_NOW is received
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    le_notification_enabled = 0;
                    break;
                case ATT_EVENT_CAN_SEND_NOW:
                    // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
                    break;
            }
            break;
    }
}
/* LISTING_END */

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */

/* LISTING_START(attRead): ATT Read */

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    switch (att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            printf("FFF1 Read\n");
            break;
        case ATT_CHARACTERISTIC_FFF2__VALUE_HANDLE:
            printf("FFF2 Read\n");
            break;
        case ATT_CHARACTERISTIC_FFF3_01_VALUE_HANDLE:
            printf("FFF3 Read\n");
            break;
        case ATT_CHARACTERISTIC_FFF4_01_VALUE_HANDLE:
            printf("FFF4 Read\n");
            break;
        case ATT_CHARACTERISTIC_FFF5_01_VALUE_HANDLE:
            printf("FFF5 Read\n");
            break;
        case ATT_CHARACTERISTIC_FFF6_01_VALUE_HANDLE:
            printf("FFF6 Read\n");
            break;
    }
    return 0;
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
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    uint8_t fff3_set[] = { 0x0F, 0x0D, 0x03, 0x00};
    switch (att_handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            printf("FFF1 Write: ");
            printf_hexdump(buffer, buffer_size);
            break;
        case ATT_CHARACTERISTIC_FFF2__VALUE_HANDLE:
            printf("FFF2 Write: ");
            printf_hexdump(buffer, buffer_size);
            break;
        case ATT_CHARACTERISTIC_FFF3_01_VALUE_HANDLE:
            if (memcmp(buffer, fff3_set, sizeof(fff3_set)) == 0){
                printf("Set: R: %02x, G: %02x, B: %02x, Level: %02x\n", buffer[4], buffer[5], buffer[6], buffer[7]);
            } else {
                printf("FFF3 Write: ");
                printf_hexdump(buffer, buffer_size);
            }
            break;
        case ATT_CHARACTERISTIC_FFF4_01_VALUE_HANDLE:
            printf("FFF4 Write: ");
            printf_hexdump(buffer, buffer_size);
            break;
        case ATT_CHARACTERISTIC_FFF5_01_VALUE_HANDLE:
            printf("FFF5 Write: ");
            printf_hexdump(buffer, buffer_size);
            break;
        case ATT_CHARACTERISTIC_FFF6_01_VALUE_HANDLE:
            printf("FFF6 Write: ");
            printf_hexdump(buffer, buffer_size);
            break;
    }
    // if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE) return 0;
    // le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    // con_handle = connection_handle;
    return 0;
}
/* LISTING_END */

int btstack_main(void);
int btstack_main(void)
{
    le_counter_setup();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

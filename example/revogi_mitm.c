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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "revogi_mitm.h"
#include "btstack.h"

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

enum { 
    FFF1_CHARACTERISTIC,
    FFF2_CHARACTERISTIC,
    FFF3_CHARACTERISTIC,
    FFF4_CHARACTERISTIC,
    FFF5_CHARACTERISTIC,
    FFF6_CHARACTERISTIC,
    //
    FFF0_CHARACTERSITICS
};

typedef enum {
    IDLE,
    W4_OUTGOING_CONNECTED,
    W4_QUERY_SERVICE_COMPLETED,
    W4_QUERY_CHARACTERISTICS_COMPLETED,
    CONNECTED,
} state_t;

#define HEARTBEAT_PERIOD_MS 100

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

static btstack_packet_callback_registration_t hci_event_callback_registration;
static hci_con_handle_t bulb_con_handle;
static gatt_client_service_t fff0_service;
static gatt_client_characteristic_t fff0_characteristics[FFF0_CHARACTERSITICS];
static btstack_timer_source_t heartbeat;
static bd_addr_t bulb_addr;
static const char * bulb_addr_string = "78:A5:04:82:1B:A4";
static state_t state = IDLE;
static uint8_t message_buffer[27];
static uint8_t color_wheel_pos;
static int color_wheel_active = 0;

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    if (uuid16){
        printf("%04x",uuid16);
    } else {
        printf("%s", uuid128_to_str(uuid128));
    }
}

static void dump_service(gatt_client_service_t * service){
    printf("    * service: [0x%04x-0x%04x], uuid ", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
    printf("\n");
}

static void dump_characteristic(gatt_client_characteristic_t * characteristic){
    printf("    * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid ",
                            characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, characteristic->properties);
    printUUID(characteristic->uuid128, characteristic->uuid16);
    printf("\n");
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
static void color_wheel(uint8_t wheel_pos, uint8_t * red, uint8_t * green, uint8_t * blue) {
    if (wheel_pos < 85) {
        *red = wheel_pos * 3;
        *green = 255 - wheel_pos * 3;
        *blue = 0;
    } else if(wheel_pos < 170) {
        wheel_pos -= 85;
        *red = 255 - wheel_pos * 3;
        *green = 0;
        *blue = wheel_pos * 3;
    } else {
        wheel_pos -= 170;
        *red = 0;
        *green = wheel_pos * 3;
        *blue = 255 - wheel_pos * 3;
    }
}

static void set_rgb_and_brightness(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness){

    // values 0 - 200
    brightness = btstack_min(brightness, 200);

    // setup message
    message_buffer[0] = 0x0F;
    message_buffer[1] = 0x0D;
    message_buffer[2] = 0x03;
    message_buffer[3] = 0x00;

    message_buffer[4] = red;
    message_buffer[5] = green;
    message_buffer[6] = blue;
    message_buffer[7] = brightness;

    message_buffer[8]  = 0x02;
    message_buffer[9]  = 0xD1;
    message_buffer[10] = 0x01;
    message_buffer[11] = 0x66;

    message_buffer[15] = 0xFF;
    message_buffer[16] = 0xFF;

    uint8_t crc = 1;
    int i;
    for(i = 2; i < 14; i++){
        crc += message_buffer[i];
    }
    message_buffer[14] = crc;

    gatt_client_write_value_of_characteristic_without_response(bulb_con_handle,
        fff0_characteristics[FFF3_CHARACTERISTIC].value_handle, 17, message_buffer);
}

static void heartbeat_handler(struct btstack_timer_source *ts){

    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);

    static int counter = 0;
    counter++;
    if (counter == 20){
        printf("(beat)\n");
        counter = 0;
    }

    if (!bulb_con_handle) return;

    color_wheel_pos += 5;   // 255 / 5 = ~ 50 = 5s * 10 Hz 
    uint8_t red, green, blue;
    color_wheel(color_wheel_pos, &red, &green, &blue);
    set_rgb_and_brightness(red, green, blue, 200);

} 

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    gatt_client_characteristic_t characteristic;
    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            gatt_event_service_query_result_get_service(packet, &fff0_service);
            dump_service(&fff0_service);
            break;
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            memcpy(&fff0_characteristics[characteristic.uuid16 - 0xfff1], &characteristic, sizeof(gatt_client_characteristic_t));
            dump_characteristic(&characteristic);
            break;
        case GATT_EVENT_QUERY_COMPLETE:
            switch (state){
                case W4_QUERY_SERVICE_COMPLETED:
                    state = W4_QUERY_CHARACTERISTICS_COMPLETED;
                    gatt_client_discover_characteristics_for_service(handle_gatt_client_event, bulb_con_handle, &fff0_service);
                    break;
                case W4_QUERY_CHARACTERISTICS_COMPLETED:
                    state = CONNECTED;
                    printf("Bulb connected, starting color wheel\n");
                    color_wheel_active = 0;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    sscanf_bd_addr(bulb_addr_string, bulb_addr);
                    printf("BTstack activated, start connecting...\n");
                    state = W4_OUTGOING_CONNECTED;
                    // gap_connect(bulb_addr, BD_ADDR_TYPE_LE_PUBLIC);
                    break;
                case HCI_EVENT_LE_META:
                    // wait for connection complete
                    if (hci_event_le_meta_get_subevent_code(packet) != HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
                    if (state == W4_OUTGOING_CONNECTED) {
                        printf("Connected, query services for FFF0...\n");
                        bulb_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                        gap_update_connection_parameters(bulb_con_handle, 80, 80, 20, 500);
                        // query primary services
                        state = W4_QUERY_SERVICE_COMPLETED;
                        gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, bulb_con_handle, 0xfff0);
                    } else {
                        gap_request_connection_parameter_update(hci_subevent_le_connection_complete_get_connection_handle(packet), 80, 80, 20, 500);                   
                    }
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    if (hci_event_disconnection_complete_get_connection_handle(packet) == bulb_con_handle){
                        printf("Bulb disconnected\n");
                        bulb_con_handle = 0;
                        color_wheel_active = 0;
                    }
                    break;
                case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
                    if (state != CONNECTED) break;
                    if (btstack_event_nr_connections_changed_get_number_connections(packet) > 1){
                        printf("Client(s) connected, stopping color wheel\n");
                        color_wheel_active = 0;
                    } else {
                        printf("All clients disconnected, starting color wheel\n");
                        color_wheel_active = 1;
                    }
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
#if 0
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
#endif
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
            if (state != CONNECTED) break;
            memcpy(message_buffer, buffer, buffer_size);
            gatt_client_write_value_of_characteristic_without_response(bulb_con_handle,
                fff0_characteristics[FFF3_CHARACTERISTIC].value_handle, buffer_size, message_buffer);
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

    // Initialize GATT client 
    gatt_client_init();

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 10);
    btstack_run_loop_add_timer(&heartbeat);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

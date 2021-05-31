/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "gatt_heart_rate_client.c"

// *****************************************************************************
/* EXAMPLE_START(gatt_heart_rate_client): GATT Heart Rate Sensor Client 
 *
 * @text Connects for Heart Rate Sensor and reports measurements.
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

// prototypes
static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

typedef enum {
    TC_OFF,
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_SERVICE_RESULT,
    TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC,
    TC_W4_ENABLE_NOTIFICATIONS_COMPLETE,
    TC_W4_SENSOR_LOCATION_CHARACTERISTIC,
    TC_W4_SENSOR_LOCATION,
    TC_CONNECTED
} gc_state_t;

static const char * sensor_contact_string[] = {
    "not supported",
    "not supported",
    "no/poor contact",
    "good contact"
};

static bd_addr_t cmdline_addr;
static int cmdline_addr_found = 0;

// addr and type of device with correct name
static bd_addr_t      le_streamer_addr;
static bd_addr_type_t le_streamer_addr_type;

static hci_con_handle_t connection_handle;

static gatt_client_service_t        heart_rate_service;
static gatt_client_characteristic_t body_sensor_location_characteristic;
static gatt_client_characteristic_t heart_rate_measurement_characteristic;

static uint8_t body_sensor_location;

static gatt_client_notification_t notification_listener;
static int listener_registered;

static gc_state_t state = TC_OFF;
static btstack_packet_callback_registration_t hci_event_callback_registration;


// returns 1 if name is found in advertisement
static int advertisement_report_contains_uuid16(uint16_t uuid16, uint8_t * advertisement_report){
    // get advertisement from report event
    const uint8_t * adv_data = gap_event_advertising_report_get_data(advertisement_report);
    uint16_t        adv_len  = gap_event_advertising_report_get_data_length(advertisement_report);

    // iterate over advertisement data
    ad_context_t context;
    int found = 0;
    for (ad_iterator_init(&context, adv_len, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t data_size    = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        int i;
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
                // compare common prefix
                for (i=0; i<data_size;i+=2){
                    if (little_endian_read_16(data, i) == uuid16) {
                        found = 1;
                        break;
                    }
                }
                break;
            default:
                break;
        }
    }
    return found;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint16_t heart_rate;
    uint8_t  sensor_contact;
    uint8_t  att_status;

    switch(state){
        case TC_W4_SERVICE_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &heart_rate_service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS){
                        printf("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    state = TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC;
                    printf("Search for Heart Rate Measurement characteristic.\n");
                    gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &heart_rate_service, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT);
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &heart_rate_measurement_characteristic);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", att_status);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    // register handler for notifications
                    listener_registered = 1;
                    gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &heart_rate_measurement_characteristic);
                    // enable notifications
                    printf("Enable Notify on Heart Rate Measurements characteristic.\n");
                    state = TC_W4_ENABLE_NOTIFICATIONS_COMPLETE;
                    gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
                        &heart_rate_measurement_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_ENABLE_NOTIFICATIONS_COMPLETE:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_QUERY_COMPLETE:
                    printf("Notifications enabled, ATT status %02x\n", gatt_event_query_complete_get_att_status(packet));

                    state = TC_W4_SENSOR_LOCATION_CHARACTERISTIC;
                    printf("Search for Sensor Location characteristic.\n");
                    gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &heart_rate_service, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
                    break;
                default:
                    break;
            }
            break;


        case TC_W4_SENSOR_LOCATION_CHARACTERISTIC:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &body_sensor_location_characteristic);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
                        state = TC_CONNECTED;
                        break;  
                    } 
                    if (body_sensor_location_characteristic.value_handle == 0){
                        printf("Sensor Location characteristic not available.\n");
                        state = TC_CONNECTED;
                        break;
                    }
                    state = TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC;
                    printf("Read Body Sensor Location.\n");
                    state = TC_W4_SENSOR_LOCATION;
                    gatt_client_read_value_of_characteristic(handle_gatt_client_event, connection_handle, &body_sensor_location_characteristic);
                    break;
                default:
                    break;
            }
            break;


        case TC_W4_SENSOR_LOCATION:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
                    body_sensor_location = gatt_event_characteristic_value_query_result_get_value(packet)[0];
                    printf("Sensor Location: %u\n", body_sensor_location);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    state = TC_CONNECTED;
                    break;
                default:
                    break;
            }
            break;

        case TC_CONNECTED:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_NOTIFICATION:
                    if (gatt_event_notification_get_value(packet)[0] & 1){
                        heart_rate = little_endian_read_16(gatt_event_notification_get_value(packet), 1);
                    } else {
                        heart_rate = gatt_event_notification_get_value(packet)[1];
                    }
                    sensor_contact = (gatt_event_notification_get_value(packet)[0] >> 1) & 3;
                    printf("Heart Rate: %3u, Sensor Contact: %s\n", heart_rate, sensor_contact_string[sensor_contact]);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    break;
                case GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE:
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

// Either connect to remote specified on command line or start scan for device with Hear Rate Service in advertisement
static void gatt_heart_rate_client_start(void){
    if (cmdline_addr_found){
        printf("Connect to %s\n", bd_addr_to_str(cmdline_addr));
        state = TC_W4_CONNECT;
        gap_connect(cmdline_addr, 0);
    } else {
        printf("Start scanning!\n");
        state = TC_W4_SCAN_RESULT;
        gap_set_scan_parameters(0,0x0030, 0x0030);
        gap_start_scan();
    }
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    uint16_t conn_interval;
    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                gatt_heart_rate_client_start();
            } else {
                state = TC_OFF;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            if (state != TC_W4_SCAN_RESULT) return;
            // check name in advertisement
            if (!advertisement_report_contains_uuid16(ORG_BLUETOOTH_SERVICE_HEART_RATE, packet)) return;
            // store address and type
            gap_event_advertising_report_get_address(packet, le_streamer_addr);
            le_streamer_addr_type = gap_event_advertising_report_get_address_type(packet);
            // stop scanning, and connect to the device
            state = TC_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(le_streamer_addr));
            gap_connect(le_streamer_addr,le_streamer_addr_type);
            break;
        case HCI_EVENT_LE_META:
            // wait for connection complete
            if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            if (state != TC_W4_CONNECT) return;
            connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
            // print connection parameters (without using float operations)
            conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
            printf("Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
            printf("Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));  
            // initialize gatt client context with handle, and add it to the list of active clients
            // query primary services
            printf("Search for Heart Rate service.\n");
            state = TC_W4_SERVICE_RESULT;
            gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_HEART_RATE);
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // unregister listener
            connection_handle = HCI_CON_HANDLE_INVALID;
            if (listener_registered){
                listener_registered = 0;
                gatt_client_stop_listening_for_characteristic_value_updates(&notification_listener);
            }
            if (cmdline_addr_found){
                printf("Disconnected %s\n", bd_addr_to_str(cmdline_addr));
                return;
            }
            printf("Disconnected %s\n", bd_addr_to_str(le_streamer_addr));
            if (state == TC_OFF) break;
            gatt_heart_rate_client_start();
            break;
        default:
            break;
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void usage(const char *name){
    fprintf(stderr, "Usage: %s [-a|--address aa:bb:cc:dd:ee:ff]\n", name);
    fprintf(stderr, "If no argument is provided, GATT Heart Rate Client will start scanning and connect to the first device named 'LE Streamer'.\n");
    fprintf(stderr, "To connect to a specific device use argument [-a].\n\n");
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

#ifdef HAVE_BTSTACK_STDIN
    int arg = 1;
    cmdline_addr_found = 0;
    
    while (arg < argc) {
        if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
            arg++;
            cmdline_addr_found = sscanf_bd_addr(argv[arg], cmdline_addr);
            arg++;
            if (!cmdline_addr_found) exit(1);
            continue;
        }
        usage(argv[0]);
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif
    l2cap_init();

    gatt_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */

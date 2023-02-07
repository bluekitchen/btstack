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
/* EXAMPLE_START(gatt_heart_rate_client): Connects to Heart Rate Sensor and reports measurements */
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
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT,
    TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC,
    TC_W4_ENABLE_NOTIFICATIONS_COMPLETE,
    TC_W4_SENSOR_LOCATION_CHARACTERISTIC,
    TC_W4_SENSOR_LOCATION,
    TC_W4_WRITE_CHARACTERISTIC,
    TC_CONNECTED,
    TC_W4_DISCONNECT
} gc_state_t;

// typedef enum {
//     GATT_ 0x2a37
// } gatt_client_characteristic_uuid_t;

static const char * sensor_contact_string[] = {
    "not supported",
    "not supported",
    "no/poor contact",
    "good contact"
};

#ifdef HAVE_BTSTACK_STDIN
// pts:         
static const char * device_addr_string = "00:1B:DC:07:32:EF";
#endif
static bd_addr_t device_addr;
static bd_addr_t cmdline_addr = { };
static int cmdline_addr_found = 0;

// addr and type of device with correct name
static bd_addr_t      le_streamer_addr;
static bd_addr_type_t le_streamer_addr_type;

static hci_con_handle_t connection_handle;

// static gatt_client_service_t        heart_rate_service;
// static gatt_client_characteristic_t body_sensor_location_characteristic;
// static gatt_client_characteristic_t heart_rate_measurement_characteristic;

static gatt_client_service_t        service;
static gatt_client_characteristic_t characteristic;
static gatt_client_characteristic_descriptor_t descriptor;

// static uint8_t body_sensor_location;

static gatt_client_notification_t notification_listener;
static int listener_registered;

static gc_state_t state = TC_OFF;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

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

    switch(state){
        case TC_W4_SERVICE_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &service);
                    printf("GATT Service:\n  start handle 0x%02x, end handle 0x%02x\n", service.start_group_handle, service.end_group_handle);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("SERVICE_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_CHARACTERISTIC_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
                    printf("GATT characteristic:\n   attribute handle 0x%02x, properties 0x%02x,   handle 0x%02x, uuid 0x%02x\n", 
                        characteristic.start_handle, characteristic.properties, characteristic.value_handle, characteristic.uuid16);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    break;
                default:
                    break;
            }
            break;
        
        case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
                    gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &descriptor);
                    printf("GATT descriptors:\n   handle 0x%02x, uuid 0x%02x\n", 
                        descriptor.handle, descriptor.uuid16);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    if (packet[4] != 0){
                        printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
                        gap_disconnect(connection_handle);
                        break;  
                    } 
                    break;
                default:
                    break;
            }
            break;
        
            
        case TC_W4_SENSOR_LOCATION:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:{
                    int value_len = gatt_event_characteristic_value_query_result_get_value_length(packet);
                    const uint8_t * value = gatt_event_characteristic_value_query_result_get_value(packet);
                    printf_hexdump(value, value_len);
                    
                    switch (characteristic.uuid16){
                        case ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT:{
                            break;
                    }
                    case ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION:
                            // https://www.bluetooth.com/specifications/gatt/services
                            // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.body_sensor_location.xml
                            printf("Body sensor location 0x%02x\n", value[0]);
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST:
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING:
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING:
                            break;
                        default:
                            break;

                    }
                    state = TC_CONNECTED;
                    break;
                }
                default:
                    break;
            }
            break;

        case TC_W4_ENABLE_NOTIFICATIONS_COMPLETE:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_QUERY_COMPLETE:
                    printf("Notifications enabled, ATT status %02x\n", gatt_event_query_complete_get_att_status(packet));
                    state = TC_CONNECTED;
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_WRITE_CHARACTERISTIC:
            printf_hexdump(packet, size);
            break;

        // case TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC:
        //     switch(hci_event_packet_get_type(packet)){
        //         case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        //             gatt_event_characteristic_query_result_get_characteristic(packet, &heart_rate_measurement_characteristic);
        //             printf("GATT characteristic:\n   start handle 0x%02x, value handle 0x%02x, end handle 0x%02x\n", 
        //                 heart_rate_measurement_characteristic.start_handle, heart_rate_measurement_characteristic.value_handle, heart_rate_measurement_characteristic.end_handle);
        //             break;
        //         case GATT_EVENT_QUERY_COMPLETE:
        //             if (packet[4] != 0){
        //                 printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
        //                 gap_disconnect(connection_handle);
        //                 break;  
        //             } 
        //             // register handler for notifications
        //             listener_registered = 1;
        //             gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &heart_rate_measurement_characteristic);
        //             // enable notifications
        //             printf("Enable Notify on Heart Rate Measurements characteristic.\n");
        //             state = TC_W4_ENABLE_NOTIFICATIONS_COMPLETE;
        //             gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
        //                 &heart_rate_measurement_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
        //             break;
        //         default:
        //             break;
        //     }
        //     break;

        // 


        // case TC_W4_SENSOR_LOCATION_CHARACTERISTIC:
        //     switch(hci_event_packet_get_type(packet)){
        //         case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        //             gatt_event_characteristic_query_result_get_characteristic(packet, &body_sensor_location_characteristic);
        //             printf("GATT Heart Rate Service body_sensor_location_characteristic:\n  start handle 0x%02x, value handle 0x%02x, end handle 0x%02x\n", 
        //                 body_sensor_location_characteristic.start_handle, body_sensor_location_characteristic.value_handle, body_sensor_location_characteristic.end_handle);
                    
        //             break;
        //         case GATT_EVENT_QUERY_COMPLETE:
        //             if (packet[4] != 0){
        //                 printf("CHARACTERISTIC_QUERY_RESULT - Error status %x.\n", packet[4]);
        //                 state = TC_CONNECTED;
        //                 break;  
        //             } 
        //             state = TC_W4_HEART_RATE_MEASUREMENT_CHARACTERISTIC;
        //             printf("Read Body Sensor Location.\n");
        //             state = TC_W4_SENSOR_LOCATION;
        //             gatt_client_read_value_of_characteristic(handle_gatt_client_event, connection_handle, &body_sensor_location_characteristic);
        //             break;
        //         default:
        //             break;
        //     }
        //     break;


        case TC_CONNECTED:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_NOTIFICATION:{
                    uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
                    uint16_t value_length = gatt_event_notification_get_value_length(packet);
                    const uint8_t * value = gatt_event_notification_get_value(packet);
                    printf("Notification handle 0x%04x, value len %d, value: ", value_handle, value_length);
                    printf_hexdump(value, value_length);
                    int pos = 0;
                            
                    switch (characteristic.uuid16){
                        case ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT:{
                            uint8_t flags = gatt_event_notification_get_value(packet)[pos++];
                            uint16_t heart_rate;
                            uint8_t  sensor_contact;
                            uint8_t  energy_expended;
                            uint8_t  rr_interval;

                            if (flags & 1){
                                heart_rate = little_endian_read_16(value, pos);
                                pos += 2;
                            } else {
                                heart_rate = value[pos++];
                            }
                            printf("Heart Rate: %3u bpm\n", heart_rate);
                            sensor_contact = (flags >> 1) & 3;
                            if (sensor_contact){
                                printf("Sensor Contact: %s\n", sensor_contact_string[sensor_contact]);
                            }
                            energy_expended = (flags >> 3) & 1;
                            if (energy_expended){
                                printf("Energy expended status: %u\n", little_endian_read_16(value, pos));
                                pos += 2;    
                            }  
                            rr_interval = (flags >> 4) & 1;
                            if (rr_interval){
                                for (;pos < value_length; pos+=2){
                                    printf("RR interval status: %u\n", little_endian_read_16(value, pos));    
                                }
                            }                           
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
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
    bd_addr_t addr;
    uint16_t conn_interval;
    uint8_t event = hci_event_packet_get_type(packet);
    switch (event) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
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
            // printf("Search for Heart Rate service.\n");
            // state = TC_W4_SERVICE_RESULT;
            // gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_HEART_RATE);
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

#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- GATT Heart Rate Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("e      - create connection to addr %s\n", device_addr_string);
    printf("E      - disconnect\n");
    printf("a      - start scanning\n");
    printf("\n");
    printf("P      - search all primary services\n");
    printf("C      - search all characteristics of last found primary service\n");
    printf("D      - search all characteristic descriptors of last found characteristic\n");
    printf("\n");
    printf("H      - search GATT Heart rate service\n");
    printf("1      - search HEART_RATE_MEASUREMENT characteristic\n");
    printf("2      - search BODY_SENSOR_LOCATION characteristic\n");
    printf("\n");
    printf("I      - search GATT Device Information service\n");
    printf("3      - search for 0x2a2a device info characteristic\n");
    printf("4      - search for 0x2a25 device info characteristic\n");
    printf("5      - search for 0x2a29 device info characteristic\n");
    printf(" \n");
    printf("r      - read request for last found characteristic\n");
    printf("n      - register notification for last found characteristic\n");
    printf(" \n");
    printf("6      - write HR control point: reset energy expended\n");
    printf(" \n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'e':
            printf("Connect to %s\n", device_addr_string);
            state = TC_W4_CONNECT;
            status = gap_connect(device_addr, 0);
            break;
        case 'E':
            printf("Disconnect from %s\n", device_addr_string);
            state = TC_W4_DISCONNECT;
            status = gap_disconnect(connection_handle);
            break;
        case 'a':
            printf("Start scanning!\n");
            state = TC_W4_SCAN_RESULT;
            gap_set_scan_parameters(0,0x0030, 0x0030);
            gap_start_scan();
            break;
        case 'P':
            printf("Search all primary services\n");
            state = TC_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services(handle_gatt_client_event, connection_handle);
            break;
        case 'C':
            printf("Search all characteristics\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service(handle_gatt_client_event, connection_handle, &service);
            break;
        case 'D':
            printf("Search all characteristic descriptors\n");
            state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT;
            status = gatt_client_discover_characteristic_descriptors(handle_gatt_client_event, connection_handle, &characteristic);
            break;
        case 'H':
            printf("Search GATT Heart rate service\n");
            state = TC_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_HEART_RATE);
            break;

        case '1':
            printf("Search for HEART_RATE_MEASUREMENT characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT);
            break;
        case '2':
            printf("Search for BODY_SENSOR_LOCATION characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION);
            break;

        case 'I':
            printf("Search GATT Device Information service\n");
            state = TC_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION);
            break;
        case '3':
            printf("Search for 0x2a2a device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST);
            break;
        case '4':
            printf("Search for 0x2a25 device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING);
            break;
        case '5':
            printf("Search for 0x2a29 device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING);
            break;

        case 'r':
            printf("Read request for characteristic 0x%4x, value handle 0x%4x.\n", characteristic.uuid16, characteristic.value_handle);
            state = TC_W4_SENSOR_LOCATION;
            status = gatt_client_read_value_of_characteristic(handle_gatt_client_event, connection_handle, &characteristic);
            break;            
        case 'n':
            printf("Register notification handler for characteristic 0x%4x.\n", characteristic.uuid16);
            listener_registered = 1;
            gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &characteristic);
            printf("Request Notify on characteristic 0x%4x.\n", characteristic.uuid16);
            state = TC_W4_ENABLE_NOTIFICATIONS_COMPLETE;
            status = gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
                &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);

            break;     

        case '6':
            printf("Search for HR control point characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_CONTROL_POINT);
            break;     
        case '7':{
            uint8_t value[] = {0x01};
            printf("Register notification handler for characteristic 0x%4x.\n", characteristic.uuid16);
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, characteristic.value_handle, sizeof(value), &value[0]);
            break;
        }   
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("AVDTP Sink cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
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

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    gatt_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */

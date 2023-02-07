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

#define BTSTACK_FILE__ "csc_client_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#include "btstack_config.h"

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
    TC_W4_ENABLE_INDICATIONS_COMPLETE,
    TC_W4_SENSOR_LOCATION_CHARACTERISTIC,
    TC_W4_SENSOR_LOCATION,
    TC_W4_WRITE_CHARACTERISTIC,
    TC_CONNECTED,
    TC_W4_DISCONNECT
} gc_state_t;

static const char * sensor_location_string[] = {
    "other",
    "top of shoe",
    "in shoe",
    "hip",
    "front wheel",
    "left crank",
    "right crank",
    "left pedal",
    "right pedal",
    "front hub",
    "rear dropout",
    "chainstay",
    "rear wheel",
    "rear hub",
    "chest",
    "spider",
    "chain ring",
    "reserved"
};

static const char * sensor_loc2str(cycling_speed_and_cadence_sensor_location_t index){
    if (index < CSC_SERVICE_SENSOR_LOCATION_RESERVED){
        return sensor_location_string[index];
    }
    return sensor_location_string[CSC_SERVICE_SENSOR_LOCATION_RESERVED];
}

#define MAX_NUM_MEASUREMENTS 20
#ifdef HAVE_BTSTACK_STDIN
// pts:         
static const char * device_addr_string = "00:1B:DC:07:32:EF";
#endif
static bd_addr_t cmdline_addr = { };
static int cmdline_addr_found = 0;

// addr and type of device with correct name
static bd_addr_t      csc_server_addr;
static bd_addr_type_t csc_server_addr_type;

static hci_con_handle_t connection_handle;

static gatt_client_service_t        service;
static gatt_client_characteristic_t characteristic;
static gatt_client_characteristic_t measurement_characteristic;
static gatt_client_characteristic_t control_point_characteristic;

static gatt_client_characteristic_descriptor_t descriptor;

static gatt_client_notification_t notification_listener;
static int notification_listener_registered;

static gatt_client_notification_t indication_listener;
static int indication_listener_registered;

static gc_state_t state = TC_OFF;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static uint32_t cumulative_wheel_revolutions[MAX_NUM_MEASUREMENTS];
static uint16_t last_wheel_event_time[MAX_NUM_MEASUREMENTS]; // Unit has a resolution of 1/1024s
static uint16_t cumulative_crank_revolutions[MAX_NUM_MEASUREMENTS];
static uint16_t last_crank_event_time[MAX_NUM_MEASUREMENTS]; // Unit has a resolution of 1/1024s
static int wm_index = 0;
static int cm_index = 0;

static btstack_timer_source_t indication_timer;

static int wheel_circumference_cm = 210;
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

#define INDICATION_TIMEOUT_MS 30000

static void csc_client_indication_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    // avdtp_connection_t * connection = (avdtp_connection_t *) btstack_run_loop_get_timer_context(timer);
    printf("Indication not received\n");
}

static void csc_client_indication_timer_start(void){
    btstack_run_loop_remove_timer(&indication_timer);
    btstack_run_loop_set_timer_handler(&indication_timer, csc_client_indication_timeout_handler);
    // btstack_run_loop_set_timer_context(&indication_timer, connection);
    btstack_run_loop_set_timer(&indication_timer, INDICATION_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&indication_timer);
}

static void csc_client_indication_timer_stop(void){
    btstack_run_loop_remove_timer(&indication_timer);
} 


static float csc_client_calculate_instantaneous_speed_km_per_h(uint32_t * wheel_rotation, uint16_t * time){
    if (time[1] == time[0]) return 0;
    int16_t delta_time = time[1] - time[0];
    int32_t delta_wheel_rotation = wheel_rotation[1] - wheel_rotation[0];
    return  delta_wheel_rotation * wheel_circumference_cm / (float)delta_time * 1024 * 3600 / 100000;
}

static float csc_client_calculate_instantaneous_cadence_rpm(uint16_t * crank_revolution, uint16_t * time){
    if (time[1] == time[0]) return 0;
    int16_t delta_time = time[1] - time[0];
    int16_t delta_crank_revolution = crank_revolution[1] - crank_revolution[0];
    return  delta_crank_revolution / (float)delta_time * 60 * 1024;
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
                    switch(characteristic.uuid16){
                        case ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT:
                            measurement_characteristic = characteristic;
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_SC_CONTROL_POINT:
                            control_point_characteristic = characteristic;
                            break;
                        default:
                            break;
                    }
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
                    case ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION:
                            // https://www.bluetooth.com/specifications/gatt/services
                            // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.body_sensor_location.xml
                            printf("Body sensor location 0x%02x, name %s\n", value[0], sensor_loc2str(value[0]));
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
        case TC_W4_ENABLE_INDICATIONS_COMPLETE:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_QUERY_COMPLETE:
                    printf("Indications enabled, ATT status %02x\n", gatt_event_query_complete_get_att_status(packet));
                    state = TC_CONNECTED;
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_WRITE_CHARACTERISTIC:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_QUERY_COMPLETE:
                    printf("Command done, ATT status %02x\n", gatt_event_query_complete_get_att_status(packet));
                    if (gatt_event_query_complete_get_att_status(packet) == ATT_ERROR_SUCCESS){
                        csc_client_indication_timer_start();
                    }
                    state = TC_CONNECTED;
                    break;
                default:
                    break;
            }
            break;


        case TC_CONNECTED:
            switch(hci_event_packet_get_type(packet)){
                case GATT_EVENT_INDICATION:{
                    uint16_t value_length = gatt_event_notification_get_value_length(packet);
                    const uint8_t * value = gatt_event_notification_get_value(packet);
                    printf("Indication value len %d, value: ", value_length);
                    printf_hexdump(value, value_length);
                    csc_client_indication_timer_stop();
                    break;
                }
                case GATT_EVENT_NOTIFICATION:{
                    // uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
                    // uint16_t value_length = gatt_event_notification_get_value_length(packet);
                    const uint8_t * value = gatt_event_notification_get_value(packet);
                    // printf("Notification handle 0x%04x, value len %d, value: ", value_handle, value_length);
                    // printf_hexdump(value, value_length);
                    int pos = 0;
                    uint8_t wheel_revolution_data_supported = 0;
                    uint8_t crank_revolution_data_supported = 0;

                    switch (characteristic.uuid16){
                        case ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT:{
                            uint8_t flags = value[pos++];
                            printf("flags 0x%02x\n", flags);
                            
                            if (flags & (1 << CSC_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED)){
                                wheel_revolution_data_supported = 1;
                            }
                            if (flags & (1 << CSC_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED)){
                                crank_revolution_data_supported = 1;
                            }
                            
                            if (wheel_revolution_data_supported){
                                cumulative_wheel_revolutions[wm_index] = little_endian_read_32(value, pos);
                                pos += 4;
                                last_wheel_event_time[wm_index] = little_endian_read_16(value, pos);
                                pos += 2;
                                printf("wheel_revolution_data (0x%04x) = %d, time (0x%02x) = %f s\n", cumulative_wheel_revolutions[wm_index], cumulative_wheel_revolutions[wm_index], 
                                    last_wheel_event_time[wm_index], last_wheel_event_time[wm_index]/1024.0);
                                if (wm_index > 0){
                                    // Speed = (Difference in two successive Cumulative Wheel Revolution values * Wheel Circumference) / (Difference in two successive Last Wheel Event Time values)
                                    printf("instantaneous speed %f km/h\n", csc_client_calculate_instantaneous_speed_km_per_h(&cumulative_wheel_revolutions[wm_index-1], &last_wheel_event_time[wm_index-1]));
                                }
                                wm_index++;
                            }

                            if (crank_revolution_data_supported){
                                cumulative_crank_revolutions[cm_index] = little_endian_read_16(value, pos);
                                pos += 2;
                                last_crank_event_time[cm_index] = little_endian_read_16(value, pos);
                                pos += 2;
                                printf("crank_revolution_data (0x%04x) = %d, time (0x%02x) = %f s\n", cumulative_crank_revolutions[cm_index], cumulative_crank_revolutions[cm_index],
                                    last_crank_event_time[cm_index], last_crank_event_time[cm_index]/1024.0);

                                if (cm_index > 0){
                                    printf("instantaneous cadence %f rpm\n", 
                                        csc_client_calculate_instantaneous_cadence_rpm(&cumulative_crank_revolutions[cm_index-1], &last_crank_event_time[cm_index-1]));
                                }
                                cm_index++;
                            }
                            
                            break;
                        }
                        case ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE:
                            printf("ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE\n");
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION:{
                            cycling_speed_and_cadence_sensor_location_t sensor_location = value[pos++];
                            printf("Sensor location 0x%2x, name %s\n", sensor_location, sensor_loc2str(sensor_location));
                            break;
                        }
                        default:
                            printf("uuid 0x%2x\n", characteristic.uuid16);
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
                printf("btstack is powered on\n");;
            } else {
                state = TC_OFF;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            if (state != TC_W4_SCAN_RESULT) return;
            // check name in advertisement
            if (!advertisement_report_contains_uuid16(ORG_BLUETOOTH_SERVICE_HEART_RATE, packet)) return;
            // store address and type
            gap_event_advertising_report_get_address(packet, csc_server_addr);
            csc_server_addr_type = gap_event_advertising_report_get_address_type(packet);
            // stop scanning, and connect to the device
            state = TC_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(csc_server_addr));
            gap_connect(csc_server_addr,csc_server_addr_type);
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
            state = TC_CONNECTED;
            // TODO: find better way to re-register listeners. Re-register notification listener if Characteristic is known
            notification_listener_registered = 0;
            if (measurement_characteristic.uuid16 != 0){
                gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &measurement_characteristic);
                notification_listener_registered = 1;
            }
            
            indication_listener_registered = 0;
            if (control_point_characteristic.uuid16 != 0){
                gatt_client_listen_for_characteristic_value_updates(&indication_listener, handle_gatt_client_event, connection_handle, &control_point_characteristic);
                indication_listener_registered = 1;
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // unregister listener
            connection_handle = HCI_CON_HANDLE_INVALID;
            if (notification_listener_registered){
                notification_listener_registered = 0;
                gatt_client_stop_listening_for_characteristic_value_updates(&notification_listener);
            }
            if (indication_listener_registered){
                indication_listener_registered = 0;
                gatt_client_stop_listening_for_characteristic_value_updates(&indication_listener);
            }
            printf("Disconnected %s\n", bd_addr_to_str(csc_server_addr));
            wm_index = 0;
            cm_index = 0;
            if (state == TC_OFF) break;
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
    printf("b      - start pairing\n");
    printf("a      - start scanning\n");
    printf("\n");
    printf("P      - search all primary services\n");
    printf("C      - search all characteristics of last found primary service\n");
    printf("D      - search all characteristic descriptors of last found characteristic\n");
    printf("\n");
    printf("H      - search CSC service\n");
    printf("1      - search CSC_MEASUREMENT characteristic\n");
    printf("2      - search CSC FEATURE characteristic\n");
    printf("3      - search SENSOR_LOCATION characteristic\n");
    printf("4      - search CONTROL_POINT characteristic\n");
    printf("\n");
    printf("I      - search GATT Device Information service\n");
    printf("x      - search for 0x2a2a device info characteristic\n");
    printf("y      - search for 0x2a25 device info characteristic\n");
    printf("z      - search for 0x2a29 device info characteristic\n");
    printf(" \n");
    printf("F      - search CSC feature\n");
    printf("f      - search for 0x0075 handle\n");
    printf(" \n");
    
    printf("r      - read request for last found characteristic\n");
    printf("n      - register notification for last found characteristic\n");
    printf("i      - register indication for last found characteristic\n");
    printf(" \n");

    printf("j      - set cumulative wheel revolutions to 0 \n");
    printf("k      - set cumulative wheel revolutions to 0xFFFF \n");
    printf("o      - request supported sensor locations\n");
    printf("p      - update sensor location to right pedal\n");
    printf(" \n");
    
    printf("J      - write unsupported opcode \n");
    printf("K      - write invalid parameter \n");

    printf(" \n");
    printf(" \n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static uint8_t csc_client_set_cumulative_wheel_revolutions(uint32_t wheel_revolutions){
    uint8_t event[20];
    int pos = 0;
    event[pos++] = CSC_OPCODE_SET_CUMULATIVE_VALUE; // opcode;
    little_endian_store_32(event, pos, wheel_revolutions);
    pos += 4;
    return gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, control_point_characteristic.value_handle, pos, event);
}

static uint8_t csc_client_update_sensor_location(cycling_speed_and_cadence_sensor_location_t sensor_location){
    uint8_t event[20];
    int pos = 0;
    event[pos++] = CSC_OPCODE_UPDATE_SENSOR_LOCATION; // opcode;
    event[pos++] = sensor_location;
    return gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, control_point_characteristic.value_handle, pos, event);
}

static uint8_t csc_client_request_supported_sensor_locations(){
    uint8_t event[20];
    int pos = 0;
    event[pos++] = CSC_OPCODE_REQUEST_SUPPORTED_SENSOR_LOCATIONS; // opcode;
    return gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, control_point_characteristic.value_handle, pos, event);
}


static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'e':
            printf("Connect to %s\n", device_addr_string);
            state = TC_W4_CONNECT;
            status = gap_connect(csc_server_addr, 0);
            break;
        case 'E':
            printf("Disconnect from %s\n", device_addr_string);
            state = TC_W4_DISCONNECT;
            status = gap_disconnect(connection_handle);
            break;
        case 'b':
            printf("Start pairing\n");
            sm_request_pairing(connection_handle);
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
            printf("Search CSC service\n");
            state = TC_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_CYCLING_SPEED_AND_CADENCE);
            break;

        case '1':
            printf("Search for CSC_MEASUREMENT characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT);
            break;
        case '2':
            printf("Search for CSC_FEATURE characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE);
            break;
        case '3':
            printf("Search for SENSOR_LOCATION characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION);
            break;
        case '4':
            printf("Search for CONTROL_POINT characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_SC_CONTROL_POINT);
            break;

        case 'I':
            printf("Search GATT Device Information service\n");
            state = TC_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, connection_handle, ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION);
            break;
        case 'x':
            printf("Search for 0x2a2a device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST);
            break;
        case 'y':
            printf("Search for 0x2a25 device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING);
            break;
        case 'z':
            printf("Search for 0x2a29 device info characteristic.\n");
            state = TC_W4_CHARACTERISTIC_RESULT;
            status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING);
            break;

        // case 'x':
        //     printf("Search for 0x2a2a device info characteristic.\n");
        //     state = TC_W4_CHARACTERISTIC_RESULT;
        //     status = gatt_client_discover_characteristics_for_service_by_uuid16(handle_gatt_client_event, connection_handle, &service, ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST);
        //     break;

        case 'r':
            printf("Read request for characteristic 0x%4x, value handle 0x%4x.\n", characteristic.uuid16, characteristic.value_handle);
            state = TC_W4_SENSOR_LOCATION;
            status = gatt_client_read_value_of_characteristic(handle_gatt_client_event, connection_handle, &characteristic);
            break;            
        case 'n':
            printf("Register notification handler for characteristic 0x%4x.\n", characteristic.uuid16);
            gatt_client_listen_for_characteristic_value_updates(&notification_listener, handle_gatt_client_event, connection_handle, &characteristic);
            printf("Request Notify on characteristic 0x%4x.\n", characteristic.uuid16);
            state = TC_W4_ENABLE_NOTIFICATIONS_COMPLETE;
            status = gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
                &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
            if (status == ERROR_CODE_SUCCESS){
                notification_listener_registered = 1;
            }
            break;     

        case 'i':
            printf("Register indication handler for characteristic 0x%4x.\n", characteristic.uuid16);
            gatt_client_listen_for_characteristic_value_updates(&indication_listener, handle_gatt_client_event, connection_handle, &characteristic);
            printf("Request Indicate on characteristic 0x%4x.\n", characteristic.uuid16);
            state = TC_W4_ENABLE_INDICATIONS_COMPLETE;
            status = gatt_client_write_client_characteristic_configuration(handle_gatt_client_event, connection_handle,
                &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
            if (status == ERROR_CODE_SUCCESS){
                indication_listener_registered = 1;
            }
            break;
        
        case 'j':
            printf("set cumulative value to 0\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = csc_client_set_cumulative_wheel_revolutions(0);
            break;
        case 'k':
            printf("set cumulative value to 0xFFFF\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = csc_client_set_cumulative_wheel_revolutions(0xFFFF);
            break;
        case 'o':
            printf("request supported sensor locations\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = csc_client_request_supported_sensor_locations();
            break;
        case 'p':
            printf("update sensor location to right pedal\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = csc_client_update_sensor_location(CSC_SERVICE_SENSOR_LOCATION_RIGHT_PEDAL);
            break;

        case 'J':{
            uint8_t value[] = {10};
            printf("write unsupported opcode\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle, control_point_characteristic.value_handle, 1, value);
            break;
        }
        case 'K':
            printf("write invalid param\n");
            state = TC_W4_WRITE_CHARACTERISTIC;
            status = csc_client_update_sensor_location(CSC_SERVICE_SENSOR_LOCATION_RESERVED);
            break;
    
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("GATT cmd \'%c\' failed, status 0x%02x\n", cmd, status);
        state = TC_CONNECTED;
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

    le_device_db_init();
    
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, csc_server_addr);
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */

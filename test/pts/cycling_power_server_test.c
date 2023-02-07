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

#define BTSTACK_FILE__ "cycling_power_server_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cycling_power_server_test.h"
#include "btstack_config.h"
#include "btstack.h"

// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml
// cycling / cycling power sensor
static const uint16_t appearance = (18 << 6) | 4;
static uint16_t con_handle;

static uint16_t event_time_s = 0;
static uint16_t force_magnitude_newton = 0;
static uint16_t torque_magnitude_newton_m = 0;
static uint16_t angle_deg = 0;

static uint8_t broadcast_adv[31];
static uint16_t adv_int_min = 0x0030;
static uint16_t adv_int_max = 0x0030;
static btstack_packet_callback_registration_t hci_event_callback_registration;


const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // Name
    0x0E, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'C', 'y', 'c', 'l', 'i', 'n', 'g', '_', 'P', 'o', 'w', 'e', 'r',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_CYCLING_POWER & 0xff, ORG_BLUETOOTH_SERVICE_CYCLING_POWER >> 8,
    // Appearance
    3, BLUETOOTH_DATA_TYPE_APPEARANCE, appearance & 0xff, appearance >> 8
};

const uint8_t adv_data_len = sizeof(adv_data);

static cycling_power_sensor_location_t supported_sensor_locations[] = {
    CP_SENSOR_LOCATION_TOP_OF_SHOE,
    CP_SENSOR_LOCATION_IN_SHOE,
    CP_SENSOR_LOCATION_HIP,
    CP_SENSOR_LOCATION_FRONT_WHEEL,
    CP_SENSOR_LOCATION_LEFT_CRANK,
    CP_SENSOR_LOCATION_RIGHT_CRANK
};
static uint16_t num_supported_sensor_locations = 6;
static gatt_date_time_t calibration_date = {2018, 1, 1, 13, 40, 50};
static cycling_power_sensor_measurement_context_t measurement_type;
static int enhanced_calibration = 0;
static uint16_t manufacturer_company_id = 0xf0f0; //  SIG assigned numbers
static uint8_t numb_manufacturer_specific_data = CYCLING_POWER_MANUFACTURER_SPECIFIC_DATA_MAX_SIZE;
static uint8_t manufacturer_specific_data[] = { 
    0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22,
    0x33, 0x33, 0x33, 0x33,
    0x44, 0x44, 0x44, 0x44 
};

#ifdef HAVE_BTSTACK_STDIN
// static char * measurement_flag_str[] = {
//     "1   Pedal Power Balance",
//     "2   Pedal Power Balance Reference", // Unknown/Left
//     "2   Accumulated Torque",            // Wheel Based/Crank
//     "2   Accumulated Torque Source",     // Wheel Based/Crank
//     "4 2 Wheel Revolution Data",
//     "2 2 Crank Revolution Data",
//     "2 2 Extreme Force Magnitudes",
//     "2 2 Extreme Torque Magnitudes",
//     "3   Extreme Angles",
//     "2   Top Dead Spot Angle",
//     "2   Bottom Dead Spot Angle",
//     "2   Accumulated Energy",
//     "Offset Compensation Indicator"
// };

// static char buffer[80];

// static char * measurement_flag2str(cycling_power_measurement_flag_t flag, uint8_t value){
//     if (flag >= CP_MEASUREMENT_FLAG_RESERVED) return "Reserved";

//     strcpy(buffer, measurement_flag_str[flag]);
//     int pos = strlen(measurement_flag_str[flag]);
//     // printf(" copy %d\n", pos);
//     switch (flag){
//         case CP_MEASUREMENT_FLAG_PEDAL_POWER_BALANCE_REFERENCE:
//             if (value == 0){
//                 strcpy(buffer + pos, ": Unknown");
//             } else {
//                 strcpy(buffer + pos, ": Left");
//             }
//             break;
//         case CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_PRESENT:
//             if (value == 0){
//                 strcpy(buffer + pos, ": Wheel Based");
//             } else {
//                 strcpy(buffer + pos, ": Crank Based");
//             }
//             break;
//         case CP_MEASUREMENT_FLAG_ACCUMULATED_TORQUE_SOURCE:
//             if (value == 0){
//                 strcpy(buffer + pos, ": Wheel Based");
//             } else {
//                 strcpy(buffer + pos, ": Crank Based");
//             }
//             break;
//         default:
//             if (value == 0){                                
//                 strcpy(buffer + pos, ": NOT SUPPORTED");
//             }
//             break;
//     }
//     return &buffer[0];
// }

static void dump_feature_flags(uint32_t feature_flags){
    int i;
    printf("feature flags: \n");
    for (i = 0; i < CP_FEATURE_FLAG_RESERVED; i++){
        printf("%02d ", i);
    }
    printf("\n");
    for (i = 0; i < CP_FEATURE_FLAG_RESERVED; i++){
        uint8_t value = (feature_flags & (1 << i)) != 0;
        printf("%2d ", value);
    }
    printf("\n");
}

static void dump_measurement_flags(uint16_t measurement_flags){
    int i;
    printf("measurement flags: \n");
    // for (i = 0; i < CP_MEASUREMENT_FLAG_RESERVED; i++){
    //     printf("%02d ", i);
    // }
    // printf("\n");
    for (i = 0; i < CP_MEASUREMENT_FLAG_RESERVED; i++){
        uint8_t value = (measurement_flags & (1 << i)) != 0;
        printf("%2d ", value);
    }
    printf("\n");
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Cycling Power Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("u    - push update\n");
    printf("\n");
    printf("t    - add positive torque\n");
    printf("T    - add negative torque\n");
    printf("w    - add wheel revolution\n");
    printf("c    - add crank revolution\n");
    printf("e    - add energy\n");
    printf("\n");
    printf("p    - set instantaneous power\n");
    printf("b    - set power balance percentage\n");
    printf("m    - set force magnitude\n");
    printf("M    - set torque magnitude\n");
    printf("a    - set angle\n");
    printf("x    - set top dead spot angle\n");
    printf("y    - set bottom dead spot angle\n");
    printf("R    - reset values\n");
    printf("z    - stop calibration\n");
    printf("Z    - incorrect calibration position\n");
    printf("Y    - Invalid param\n");
    printf("X    - start calibration\n");
    
    printf("\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void cps_reset_values(void){
    event_time_s = 0;
    force_magnitude_newton = 0;
    torque_magnitude_newton_m = 0;
    angle_deg = 0;
}



static void stdin_process(char cmd){
    switch (cmd){
        case 'C':
            printf("reset all values\n");
            cps_reset_values();
            break;
        case 'u':{
            printf("push update\n");
            cycling_power_service_server_update_values();
            event_time_s++;
            break;
        }
        case 'r':
            printf("add positive torque\n");
            cycling_power_service_server_add_torque(100);
            break;
        case 'R':
            printf("add negative torque\n");
            cycling_power_service_server_add_torque(-100);
            break;
        case 'w':
            printf("add wheel revolution\n");
            cycling_power_service_server_add_wheel_revolution(10, event_time_s);
            break;
        case 'W':
            printf("reverse wheel revolution\n");
            cycling_power_service_server_add_wheel_revolution(-10, event_time_s);
            break;

        case 'c':
            printf("add crank revolution\n");
            cycling_power_service_server_add_crank_revolution(10, event_time_s);
            break;
        case 'e':
            printf("add energy\n");
            cycling_power_service_add_energy(100);
            break;

        case 'p':
            printf("set instantaneous power\n");
            cycling_power_service_server_set_instantaneous_power(100);
            break;
        case 'b':
            printf("set power balance percentage\n");
            cycling_power_service_server_set_pedal_power_balance(50);
            break;
        case 'm':
            force_magnitude_newton += 10;
            printf("set force magnitude\n");
            cycling_power_service_server_set_force_magnitude(force_magnitude_newton-5, force_magnitude_newton+5);
            break;
        case 'M':
            torque_magnitude_newton_m += 10;
            printf("set torque magnitude\n");
            cycling_power_service_server_set_torque_magnitude(torque_magnitude_newton_m-5, torque_magnitude_newton_m+5);
            break;
        case 'a':
            angle_deg += 10;
            printf("set angle\n");
            cycling_power_service_server_set_angle(angle_deg-5, angle_deg+5);
            break;
        case 'x':
            printf("set top dead spot angle\n");
            cycling_power_service_server_set_top_dead_spot_angle(180); 
            break;
        case 'y':
            printf("set bottom dead spot angle\n");
            cycling_power_service_server_set_bottom_dead_spot_angle(20); 
            break;
        case 'Y':
            printf("Invalid parameter\n");
            uint16_t calibrated_value = 0;
            if (enhanced_calibration){
                cycling_power_server_enhanced_calibration_done(3, calibrated_value, 
                    manufacturer_company_id, numb_manufacturer_specific_data, manufacturer_specific_data);
            } else {
                cycling_power_server_calibration_done(2, calibrated_value); 
            }
            break;
        case 'z':{
            printf("stop calibration\n");
            switch (measurement_type){
                case CP_SENSOR_MEASUREMENT_CONTEXT_FORCE:
                    calibrated_value = force_magnitude_newton;
                    break;
                case CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE:
                    calibrated_value = torque_magnitude_newton_m;
                    break;
                default:
                    printf("wrong measurement type\n");
                    break;
            }

            if (enhanced_calibration){
                printf(" enhanced calibration on\n");
                cycling_power_server_enhanced_calibration_done(measurement_type, calibrated_value, 
                    manufacturer_company_id, numb_manufacturer_specific_data, manufacturer_specific_data);
            } else {
                // printf("cycling_power_server_calibration_done, data %d \n", calibrated_value);
                cycling_power_server_calibration_done(measurement_type, calibrated_value); 
            }
            break;
        }
        case 'Z':
            printf("stop calibration, incorrect calibration position\n");
            if (enhanced_calibration){
                printf(" enhanced calibration on\n");
                cycling_power_server_enhanced_calibration_done(measurement_type, CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION, 
                    manufacturer_company_id, numb_manufacturer_specific_data, manufacturer_specific_data);
            } else {
                cycling_power_server_calibration_done(measurement_type, CP_CALIBRATION_STATUS_INCORRECT_CALIBRATION_POSITION); 
            }

            break;
        case 't':
            printf("disconnect \n");
            // gap_advertisements_enable(0);
            gap_disconnect(con_handle);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}
#endif


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int pos;
    bd_addr_t null_addr;
    
    if (packet_type != HCI_EVENT_PACKET) return;
 
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    break;
                default:
                    break;
            }
            break;

        case HCI_EVENT_GATTSERVICE_META:
            switch (packet[2]){
                case GATTSERVICE_SUBEVENT_CYCLING_POWER_START_CALIBRATION:
                    measurement_type = gattservice_subevent_cycling_power_start_calibration_get_measurement_type(packet);
                    enhanced_calibration =  gattservice_subevent_cycling_power_start_calibration_get_is_enhanced(packet);
                    break;

                case GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_START:
                    printf("start broadcast\n");
                    // set ADV_NONCONN_IND
                    pos = cycling_power_get_measurement_adv(adv_int_max, &broadcast_adv[0], sizeof(broadcast_adv));
                    memset(null_addr, 0, 6);
                    gap_advertisements_set_params(adv_int_min, adv_int_max, 0x03, 0, null_addr, 0x07, 0x00);
                    gap_advertisements_set_data(pos, (uint8_t*) broadcast_adv);
                    gap_advertisements_enable(1);
                    break;
                case GATTSERVICE_SUBEVENT_CYCLING_POWER_BROADCAST_STOP:
                    printf("stop broadcast\n");
                    gap_advertisements_enable(0);
                    break;

                default:
                    break;
            }
            break;
        default:
            break;
    }
        
    // uint8_t event = hci_event_packet_get_type(packet);
}

int btstack_main(void);
int btstack_main(void){
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    

    // setup heart rate service
    // cycling_power_service_server_init(0x2FFFFF, 0x1F, CP_PEDAL_POWER_BALANCE_REFERENCE_LEFT);

    uint32_t feature_flags = 0;   
    feature_flags |= (1 << CP_FEATURE_FLAG_PEDAL_POWER_BALANCE_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_ACCUMULATED_TORQUE_SUPPORTED);
    // feature_flags |= (1 << CP_FEATURE_FLAG_WHEEL_REVOLUTION_DATA_SUPPORTED);
    // feature_flags |= (1 << CP_FEATURE_FLAG_CRANK_REVOLUTION_DATA_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_EXTREME_ANGLES_SUPPORTED);
    // feature_flags |= (1 << CP_FEATURE_FLAG_TOP_AND_BOTTOM_DEAD_SPOT_ANGLE_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_ACCUMULATED_ENERGY_SUPPORTED);
    // feature_flags |= (1 << CP_FEATURE_FLAG_OFFSET_COMPENSATION_INDICATOR_SUPPORTED);
    // feature_flags |= (1 << CP_FEATURE_FLAG_OFFSET_COMPENSATION_SUPPORTED);
    
    feature_flags |= (1 << CP_FEATURE_FLAG_EXTREME_MAGNITUDES_SUPPORTED);
    feature_flags |= (CP_SENSOR_MEASUREMENT_CONTEXT_FORCE << CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT);
    // feature_flags |= (CP_SENSOR_MEASUREMENT_CONTEXT_TORQUE << CP_FEATURE_FLAG_SENSOR_MEASUREMENT_CONTEXT);

    // feature_flags |= (1 << CP_FEATURE_FLAG_INSTANTANEOUS_MEASUREMENT_DIRECTION_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_MULTIPLE_SENSOR_LOCATIONS_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_CRANK_LENGTH_ADJUSTMENT_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_CHAIN_LENGTH_ADJUSTMENT_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_CHAIN_WEIGHT_ADJUSTMENT_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_SPAN_LENGTH_ADJUSTMENT_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_FACTORY_CALIBRATION_DATE_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_OFFSET_COMPENSATION_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_ENHANCED_OFFSET_COMPENSATION_SUPPORTED);
    feature_flags |= (1 << CP_FEATURE_FLAG_CYCLING_POWER_MEASUREMENT_CHARACTERISTIC_CONTENT_MASKING_SUPPORTED);


    printf(" num_supported_sensor_locations %lu\n", sizeof(num_supported_sensor_locations));

    cycling_power_service_server_init(feature_flags, CP_PEDAL_POWER_BALANCE_REFERENCE_LEFT, CP_TORQUE_SOURCE_WHEEL, 
        &supported_sensor_locations[0], num_supported_sensor_locations, supported_sensor_locations[0]);
    cycling_power_service_server_packet_handler(packet_handler);

    cycling_power_service_server_set_factory_calibration_date(calibration_date);

    uint16_t measurement_flags = cycling_power_service_measurement_flags();
    dump_feature_flags(feature_flags);
    dump_measurement_flags(measurement_flags);
    // dump_measurement_flags_as_str(measurement_flags);

    cycling_power_service_server_add_torque(100);
    cycling_power_service_server_add_torque(-100);
    cycling_power_service_server_add_wheel_revolution(10, event_time_s);
    cycling_power_service_server_add_crank_revolution(10, event_time_s);
    cycling_power_service_add_energy(100);

    cycling_power_service_server_set_instantaneous_power(100);
    cycling_power_service_server_set_pedal_power_balance(50);
    force_magnitude_newton += 10;
    cycling_power_service_server_set_force_magnitude(force_magnitude_newton-5, force_magnitude_newton+5);
    torque_magnitude_newton_m += 10;
    cycling_power_service_server_set_torque_magnitude(torque_magnitude_newton_m-5, torque_magnitude_newton_m+5);
    angle_deg += 10;
    cycling_power_service_server_set_angle(angle_deg-5, angle_deg+5);
    cycling_power_service_server_set_top_dead_spot_angle(180); 
    cycling_power_service_server_set_bottom_dead_spot_angle(20); 
    cycling_power_service_add_energy(100);
    int16_t values[] = {12, -50, 100};
    cycling_power_service_server_set_torque_magnitude_values(3, values);
    cycling_power_service_server_set_force_magnitude_values(3, values);
    cycling_power_service_server_set_instantaneous_measurement_direction(CP_INSTANTANEOUS_MEASUREMENT_DIRECTION_TANGENTIAL_COMPONENT);
    // cycling_power_service_server_set_first_crank_measurement_angle(first_crank_measurement_angle_deg);

    // setup advertisements

    uint8_t adv_type = 0;   // AFV_IND
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

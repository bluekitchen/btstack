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

#define BTSTACK_FILE__ "csc_server_test.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csc_server_test.h"
#include "btstack.h"

#include "btstack_config.h"

#define WHEEL_REVOLUTION_DATA_SUPPORTED 1
#define CRANK_REVOLUTION_DATA_SUPPORTED 1
#define MULTIPLE_SENSOR_LOCATIONS_SUPPORTED 1

// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml
// cycling / speed and cadence sensor
static const uint16_t appearance = (18 << 6) | 5;

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x0B, 0x09, 'C', 'S', 'C', ' ', 'S', 'e', 'r', 'v', 'e','r',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_CYCLING_SPEED_AND_CADENCE & 0xff, ORG_BLUETOOTH_SERVICE_CYCLING_SPEED_AND_CADENCE >> 8,
    // Appearance
    3, BLUETOOTH_DATA_TYPE_APPEARANCE, appearance & 0xff, appearance >> 8,
};

const uint8_t adv_data_len = sizeof(adv_data);

static int32_t  wheel_revolutions = 0;
static uint16_t last_wheel_event_time = 0;
static uint16_t crank_revolutions = 0;
static uint16_t last_crank_event_time = 0;

#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth CSCS Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("f     - update forward wheel revolution\n");
    printf("r     - update reverse wheel revolution\n");
    printf("c     - update crank revolution\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    last_wheel_event_time += 10;
    last_crank_event_time += 10;

    switch (cmd){
        case '\n':
        case '\r':
            break;
        case 'f':
            wheel_revolutions = 10;
            crank_revolutions = 10;
            cycling_speed_and_cadence_service_server_update_values(wheel_revolutions, last_wheel_event_time, crank_revolutions, last_crank_event_time);
            break;
        case 'r':
            wheel_revolutions = -10;
            crank_revolutions = 10;
            cycling_speed_and_cadence_service_server_update_values(wheel_revolutions, last_wheel_event_time, crank_revolutions, last_crank_event_time);
            break;
        case 'c':
            wheel_revolutions = 0;
            crank_revolutions = 0;
            cycling_speed_and_cadence_service_server_update_values(wheel_revolutions, last_wheel_event_time, crank_revolutions, last_crank_event_time);
            break;
        default:
            show_usage();
            break;
    }
}
#endif

int btstack_main(void);
int btstack_main(void){
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    

    // setup heart rate service
    cycling_speed_and_cadence_service_server_init(CSC_SERVICE_SENSOR_LOCATION_TOP_OF_SHOE, 
        MULTIPLE_SENSOR_LOCATIONS_SUPPORTED, WHEEL_REVOLUTION_DATA_SUPPORTED, CRANK_REVOLUTION_DATA_SUPPORTED);
    
    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
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

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

#define BTSTACK_FILE__ "le_counter.c"

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

#include "hrp_server_test.h"
#include "btstack.h"
#include "ble/gatt-service/heart_rate_service_server.h"

#define HEARTBEAT_PERIOD_MS 1000
#define ENERGY_EXPENDED_SUPPORTED 1

static btstack_timer_source_t heartbeat;

static uint16_t heart_rate = 100;
static int rr_interval_count = 2;
static uint16_t rr_intervals[] = {0x55, 0x66};

static void heartbeat_handler(struct btstack_timer_source *ts);

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, 0x01, 0x06, 
    // Name
    0x12, 0x09, 'H', 'e', 'a', 'r', 't', ' ', 'R', 'a', 't', 'e', ' ', 'S', 'e', 'r', 'v', 'e','r',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HEART_RATE & 0xff, ORG_BLUETOOTH_SERVICE_HEART_RATE >> 8
};
const uint8_t adv_data_len = sizeof(adv_data);

static void heartbeat_handler(struct btstack_timer_source *ts){

    // simulate increase of energy spent 
    // heart_rate_service_server_update_heart_rate_values(heart_rate, HEART_RATE_SERVICE_SENSOR_CONTACT_HAVE_CONTACT, rr_interval_count, &rr_intervals[0]);
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
} 

#ifdef HAVE_BTSTACK_STDIN

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HRP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c   - notify heart rate measurement, no contact with sensor\n");
    printf("e   - add 50kJ energy expended\n");
    printf("n   - notify heart rate measurement, contact with sensor\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}


static void stdin_process(char cmd){
    switch (cmd){
        case 'n': 
            printf("Notify heart rate measurement, contact with sensor\n");
            heart_rate_service_server_update_heart_rate_values(heart_rate, HEART_RATE_SERVICE_SENSOR_CONTACT_HAVE_CONTACT, rr_interval_count, &rr_intervals[0]);
            break;
        case 'c': 
            printf("Notify heart rate measurement, no contact with sensor\n");
            heart_rate_service_server_update_heart_rate_values(heart_rate, HEART_RATE_SERVICE_SENSOR_CONTACT_NO_CONTACT, rr_interval_count, &rr_intervals[0]);
            break;
        case 'e': 
            printf("Add 50kJ energy expended\n");
            heart_rate_service_add_energy_expended(50);
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
    heart_rate_service_server_init(HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_HAND, ENERGY_EXPENDED_SUPPORTED);
    printf("Heart rate service: body sensor location 0x%02x\n", HEART_RATE_SERVICE_BODY_SENSOR_LOCATION_HAND);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // setup simulated heartbeat measurement
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);


#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */

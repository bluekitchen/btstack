/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// ANCS Client Demo
//
// TODO: query full text upon notification using control point
// TODO: present notifications in human readable form
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <termios.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>

#include "debug.h"
#include "btstack_memory.h"

#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "att.h"
#include "att_server.h"
#include "central_device_db.h"
#include "gap_le.h"
#include "gatt_client.h"
#include "sm.h"

// ancs client profile
#include "ancs_client.h"

#ifdef HAVE_UART_CSR
#include "bt_control_csr.h"
static hci_uart_config_t hci_uart_config_csr8811 = {
    "/dev/tty.usbserial-A40081HW",
    115200,
    0, // 1000000,
    1
};
#endif


typedef enum ancs_chunk_parser_state {
    W4_ATTRIBUTE_ID,
    W4_ATTRIBUTE_LEN,
    W4_ATTRIBUTE_COMPLETE,
} ancs_chunk_parser_state_t;

typedef enum {
    TC_IDLE,
    TC_W4_ENCRYPTED_CONNECTION,
    TC_W4_SERVICE_RESULT,
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_DATA_SOURCE_SUBSCRIBED,
    TC_W4_NOTIFICATION_SOURCE_SUBSCRIBED,
    TC_SUBSCRIBED,
    TC_W4_DISCONNECT
} tc_state_t;

typedef enum {
    SET_ADVERTISEMENT_PARAMS = 1 << 0,
    SET_ADVERTISEMENT_DATA   = 1 << 1,
    ENABLE_ADVERTISEMENTS    = 1 << 2,
} todo_t;

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x05, 0x09, 'A', 'N', 'C', 'S', 
    // Service Solicitation, 128-bit UUIDs - ANCS (little endian)
    0x11,0x15,0xD0,0x00,0x2D,0x12,0x1E,0x4B,0x0F,0xA4,0x99,0x4E,0xCE,0xB5,0x31,0xF4,0x05,0x79
};
uint8_t adv_data_len = sizeof(adv_data);

const char * ancs_attribute_names[] = { 
    "AppIdentifier",
    "IDTitle",
    "IDSubtitle",
    "IDMessage",
    "IDMessageSize",
    "IDDate"
};

const uint8_t ancs_service_uuid[] =             {0x79,0x05,0xF4,0x31,0xB5,0xCE,0x4E,0x99,0xA4,0x0F,0x4B,0x1E,0x12,0x2D,0x00,0xD0};
const uint8_t ancs_notification_source_uuid[] = {0x9F,0xBF,0x12,0x0D,0x63,0x01,0x42,0xD9,0x8C,0x58,0x25,0xE6,0x99,0xA2,0x1D,0xBD};
const uint8_t ancs_control_point_uuid[] =       {0x69,0xD1,0xD8,0xF3,0x45,0xE1,0x49,0xA8,0x98,0x21,0x9B,0xBD,0xFD,0xAA,0xD9,0xD9};
const uint8_t ancs_data_source_uuid[] =         {0x22,0xEA,0xC6,0xE9,0x24,0xD6,0x4B,0xB5,0xBE,0x44,0xB3,0x6A,0xCE,0x7C,0x7B,0xFB};

static todo_t todos = 0;
static uint32_t ancs_notification_uid;
static uint16_t handle;
static gatt_client_t ancs_client_context;
static int ancs_service_found;
static le_service_t  ancs_service;
static le_characteristic_t ancs_notification_source_characteristic;
static le_characteristic_t ancs_control_point_characteristic;
static le_characteristic_t ancs_data_source_characteristic;
static int ancs_characteristcs;
static tc_state_t tc_state = TC_IDLE;

static ancs_chunk_parser_state_t chunk_parser_state;
static uint8_t  ancs_notification_buffer[50];
static uint16_t ancs_bytes_received;
static uint16_t ancs_bytes_needed;
static uint8_t  ancs_attribute_id;
static uint16_t ancs_attribute_len;

static void app_run();

void print_attribute(){
    ancs_notification_buffer[ancs_bytes_received] = 0;
    printf("%14s: %s\n", ancs_attribute_names[ancs_attribute_id], ancs_notification_buffer);
}

void ancs_chunk_parser_init(){
    chunk_parser_state = W4_ATTRIBUTE_ID;
    ancs_bytes_received = 0;
    ancs_bytes_needed = 6;
}

void ancs_chunk_parser_handle_byte(uint8_t data){
    ancs_notification_buffer[ancs_bytes_received++] = data;
    if (ancs_bytes_received < ancs_bytes_needed) return;
    switch (chunk_parser_state){
        case W4_ATTRIBUTE_ID:
            ancs_attribute_id   = ancs_notification_buffer[ancs_bytes_received-1];
            ancs_bytes_received = 0;
            ancs_bytes_needed   = 2;
            chunk_parser_state  = W4_ATTRIBUTE_LEN;
            break;
        case W4_ATTRIBUTE_LEN:
            ancs_attribute_len  = READ_BT_16(ancs_notification_buffer, ancs_bytes_received-2);
            ancs_bytes_received = 0;
            ancs_bytes_needed   = ancs_attribute_len;
            if (ancs_attribute_len == 0) {
                ancs_bytes_needed   = 1;
                chunk_parser_state  = W4_ATTRIBUTE_ID;
                break;
            }
            chunk_parser_state  = W4_ATTRIBUTE_COMPLETE;
            break;
        case W4_ATTRIBUTE_COMPLETE:
            print_attribute();
            ancs_bytes_received = 0;
            ancs_bytes_needed   = 1;
            chunk_parser_state  = W4_ATTRIBUTE_ID;
            break;
    }
}

void handle_gatt_client_event(le_event_t * event){
    le_characteristic_t characteristic;
    le_characteristic_value_event_t * value_event;
    switch(tc_state){
        case TC_W4_SERVICE_RESULT:
            switch(event->type){
                case GATT_SERVICE_QUERY_RESULT:
                    ancs_service = ((le_service_event_t *) event)->service;
                    ancs_service_found = 1;
                    break;
                case GATT_QUERY_COMPLETE:
                    if (!ancs_service_found){
                        printf("ANCS Service not found");
                        tc_state = TC_IDLE;
                        break;
                    }
                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
                    printf("ANCS Client - Discover characteristics for ANCS SERVICE \n");
                    gatt_client_discover_characteristics_for_service(&ancs_client_context, &ancs_service);
                    break;
                default:
                    break;
            }
            break;
            
        case TC_W4_CHARACTERISTIC_RESULT:
            switch(event->type){
                case GATT_CHARACTERISTIC_QUERY_RESULT:
                    characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    if (memcmp(characteristic.uuid128, ancs_notification_source_uuid, 16) == 0){
                        printf("ANCS Notification Source Characterisic found\n");
                        ancs_notification_source_characteristic = characteristic;
                        ancs_characteristcs++;
                        break;                        
                    }
                    if (memcmp(characteristic.uuid128, ancs_control_point_uuid, 16) == 0){
                        printf("ANCS Control Point found\n");
                        ancs_control_point_characteristic = characteristic;
                        ancs_characteristcs++;
                        break;                        
                    }
                    if (memcmp(characteristic.uuid128, ancs_data_source_uuid, 16) == 0){
                        printf("ANCS Data Source Characterisic found\n");
                        ancs_data_source_characteristic = characteristic;
                        ancs_characteristcs++;
                        break;                        
                    }
                    break;
                case GATT_QUERY_COMPLETE:
                    printf("ANCS Characteristcs count %u\n", ancs_characteristcs);
                    tc_state = TC_W4_NOTIFICATION_SOURCE_SUBSCRIBED;
                    gatt_client_write_client_characteristic_configuration(&ancs_client_context, &ancs_notification_source_characteristic,
                        GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                default:
                    break;
            }
            break;
        case TC_W4_NOTIFICATION_SOURCE_SUBSCRIBED:
            switch(event->type){
                case GATT_QUERY_COMPLETE:
                    printf("ANCS Notification Source subscribed\n");
                    tc_state = TC_W4_DATA_SOURCE_SUBSCRIBED;
                    gatt_client_write_client_characteristic_configuration(&ancs_client_context, &ancs_data_source_characteristic,
                        GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                default:
                    break;
            }
            break;
        case TC_W4_DATA_SOURCE_SUBSCRIBED:
            switch(event->type){
                case GATT_QUERY_COMPLETE:
                    printf("ANCS Data Source subscribed\n");
                    tc_state = TC_SUBSCRIBED;
                    break;
                default:
                    break;
            }
            break;
        case TC_SUBSCRIBED:
            if ( event->type != GATT_NOTIFICATION && event->type != GATT_INDICATION ) break;
            value_event = (le_characteristic_value_event_t *) event;
            if (value_event->value_handle == ancs_data_source_characteristic.value_handle){
                int i;
                for (i=0;i<value_event->blob_length;i++) {
                    ancs_chunk_parser_handle_byte(value_event->blob[i]);
                }
            } else if (value_event->value_handle == ancs_notification_source_characteristic.value_handle){
                ancs_notification_uid = READ_BT_32(value_event->blob, 4);
                printf("Notification received: EventID %02x, EventFlags %02x, CategoryID %02x, CategoryCount %u, UID %04x\n",
                    value_event->blob[0], value_event->blob[1], value_event->blob[2], value_event->blob[3], ancs_notification_uid);
                static uint8_t get_notification_attributes[] = {0, 0,0,0,0,  0,  1,32,0,  2,32,0, 3,32,0, 4, 5};
                bt_store_32(get_notification_attributes, 1, ancs_notification_uid);
                ancs_notification_uid = 0;
                ancs_chunk_parser_init();
                gatt_client_write_value_of_characteristic(&ancs_client_context, ancs_control_point_characteristic.value_handle, 
                    sizeof(get_notification_attributes), get_notification_attributes);
            } else {
                printf("Unknown Source: ");
                printf_hexdump(value_event->blob , value_event->blob_length);
            }
            break;
        default:
            break;
    }    
    app_run();
}

static void app_run(){

    if (!hci_can_send_command_packet_now()) return;

    if (todos & SET_ADVERTISEMENT_DATA){
        printf("app_run: set advertisement data\n");
        todos &= ~SET_ADVERTISEMENT_DATA;
        hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type = 0;   // default
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x0030;
        uint16_t adv_int_max = 0x0030;
        hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, 0, 0, &null_addr, 0x07, 0x00);
        return;
    }    

    if (todos & ENABLE_ADVERTISEMENTS){
        printf("app_run: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        return;
    }
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    int connection_encrypted;
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        todos = SET_ADVERTISEMENT_PARAMS | SET_ADVERTISEMENT_DATA | ENABLE_ADVERTISEMENTS;
                        app_run();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = READ_BT_16(packet, 4);
                            printf("Connection handle 0x%04x\n", handle);

                            // we need to be paired to enable notifications
                            tc_state = TC_W4_ENCRYPTED_CONNECTION;
                            sm_send_security_request();
                            break;

                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                    if (handle != READ_BT_16(packet, 3)) break;
                    connection_encrypted = packet[5];
                    log_info("Encryption state change: %u", connection_encrypted);
                    if (!connection_encrypted) break;
                    if (tc_state != TC_W4_ENCRYPTED_CONNECTION) break;

                    // let's start
                    printf("\nANCS Client - CONNECTED, discover ANCS service\n");
                    tc_state = TC_W4_SERVICE_RESULT;
                    gatt_client_start(&ancs_client_context, handle);
                    gatt_client_discover_primary_services_by_uuid128(&ancs_client_context, ancs_service_uuid);
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    todos = ENABLE_ADVERTISEMENTS;
                    break;
                    
                case ATT_HANDLE_VALUE_INDICATION_COMPLETE:
                    printf("ATT_HANDLE_VALUE_INDICATION_COMPLETE status %u\n", packet[2]);
                    break;

                default:
                    break;
            }
    }
    app_run();
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
#ifdef HAVE_UART_CSR
    hci_transport_t    * transport = hci_transport_h4_instance();
    hci_uart_config_t  * config    = &hci_uart_config_csr8811;
    bt_control_t       * control   = bt_control_csr_instance();
#else
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
#endif
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);

    // set up l2cap_le
    l2cap_init();
    
    // setup central device db
    central_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING ); 

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    
    att_server_register_packet_handler(app_packet_handler);

    // setup GATT client
    gatt_client_init();
    gatt_client_register_packet_handler(&handle_gatt_client_event);
}

int main(void)
{
    printf("BTstack ANCS Client starting up...\n");

    setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}

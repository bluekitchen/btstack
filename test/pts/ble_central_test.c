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
//
// BLE Central PTS Test
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "le_device_db.h"
#include "stdin_support.h"
#include "ad_parser.h"

// test profile
#include "ble_central_test.h"

// Non standard IXIT
#define PTS_USES_RECONNECTION_ADDRESS_FOR_ITSELF
#define PTS_UUID128_REPRESENTATION

typedef enum {
    CENTRAL_IDLE,
    CENTRAL_W4_NAME_QUERY_COMPLETE,
    CENTRAL_W4_NAME_VALUE,
    CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE,
    CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE,
    CENTRAL_W4_SIGNED_WRITE_QUERY_COMPLETE,
    CENTRAL_W4_PRIMARY_SERVICES,
    CENTRAL_ENTER_START_HANDLE_4_DISCOVER_CHARACTERISTICS,
    CENTRAL_ENTER_END_HANDLE_4_DISCOVER_CHARACTERISTICS,
    CENTRAL_W4_CHARACTERISTICS,
    CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_HANDLE,
    CENTRAL_ENTER_HANDLE_4_READ_CHARACTERISTIC_VALUE_BY_UUID,
    CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID,
    CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE,
    CENTRAL_W4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE,
    CENTRAL_W4_READ_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE,
    CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE,
    CENTRAL_W4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE,
    CENTRAL_W4_READ_MULTIPLE_CHARACTERISTIC_VALUES,
    CENTRAL_W4_WRITE_WITHOUT_RESPONSE,
    CENTRAL_W4_WRITE_CHARACTERICISTIC_VALUE,
    CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_VALUE,
    CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_VALUE,
    CENTRAL_W4_RELIABLE_WRITE,
    CENTRAL_W4_WRITE_CHARACTERISTIC_DESCRIPTOR,
    CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR,
    CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR,
    CENTRAL_W4_SIGNED_WRITE,
    CENTRAL_GPA_ENTER_UUID,
    CENTRAL_GPA_ENTER_START_HANDLE,
    CENTRAL_GPA_ENTER_END_HANDLE,
    CENTRAL_GPA_W4_RESPONSE,
    CENTRAL_GPA_W4_RESPONSE2,
    CENTRAL_GPA_W4_RESPONSE3,
    CENTRAL_GPA_W4_RESPONSE4,
} central_state_t;

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} advertising_report_t;

static const uint8_t gpa_format_type_len[] = {
    /* 0x00 */
    1,1,1,1,1,
    /* 0x05 */
    2,2,
    /* 0x07 */
    3,4,6,8,16,
    /* 0x0c */
    1,2,2,3,4,6,8,16,
    /* 0x14 */
    4,8,2,4,4
};

static uint8_t test_irk[] =  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static int gap_privacy = 0;
static int gap_bondable = 1;
static char gap_device_name[20];
static int gap_connectable = 0;

static char * sm_io_capabilities = NULL;
static int sm_mitm_protection = 0;
static int sm_have_oob_data = 0;
static uint8_t * sm_oob_data_A = (uint8_t *) "0123456789012345"; // = { 0x30...0x39, 0x30..0x35}
static uint8_t * sm_oob_data_B = (uint8_t *) "3333333333333333"; // = { 0x30...0x39, 0x30..0x35}
static int sm_min_key_size = 7;
static uint8_t pts_privacy_flag;

static int peer_addr_type;
static bd_addr_t peer_address;
static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;
static int ui_uint16 = 0;
static int ui_uint16_request = 0;
static int ui_uint16_pos = 0;
static int ui_uuid16 = 0;
static int ui_uuid128_request = 0;
static int ui_uuid128_pos     = 0;
static uint8_t ui_uuid128[16];
static int      ui_handles_count;
static int      ui_handles_index;
static uint16_t ui_handles[10];
static uint16_t ui_attribute_handle;
static int      ui_attribute_offset;
static int      ui_value_request = 0;
static uint8_t  ui_value_data[50];
static int      ui_value_pos = 0;
static uint16_t ui_start_handle;
static uint16_t ui_end_handle;
static uint8_t ui_presentation_format[7];
static uint16_t ui_aggregate_handle;
static uint16_t handle = 0;
static uint16_t gc_id;

static bd_addr_t public_pts_address = {0x00, 0x1B, 0xDC, 0x07, 0x32, 0xef};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;
static int       reconnection_address_set = 0;
static bd_addr_t our_private_address;

static uint16_t pts_signed_write_characteristic_uuid = 0xb00d;
static uint16_t pts_signed_write_characteristic_handle = 0x00b1;
static uint8_t signed_write_value[] = { 0x12 };
static int le_device_db_index;
static sm_key_t signing_csrk;

static central_state_t central_state = CENTRAL_IDLE;
static le_characteristic_t gap_name_characteristic;
static le_characteristic_t gap_reconnection_address_characteristic;
static le_characteristic_t gap_peripheral_privacy_flag_characteristic;
static le_characteristic_t signed_write_characteristic;

static void show_usage();
///

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    if (uuid16){
        printf("%04x",uuid16);
    } else {
        printUUID128(uuid128);
    }
}

static const char * att_errors[] = {
    "OK",
    "Invalid Handle",
    "Read Not Permitted",
    "Write Not Permitted",
    "Invalid PDU",
    "Insufficient Authentication",
    "Request No Supported",
    "Invalid Offset",
    "Insufficient Authorization",
    "Prepare Queue Full",
    "Attribute Not Found",
    "Attribute Not Long",
    "Insufficient Encryption Size",
    "Invalid Attribute Value Length",
    "Unlikely Error",
    "Insufficient Encryption",
    "Unsupported Group Type",
    "Insufficient Resource"
};
static const char * att_error_reserved = "Reserved";
static const char * att_error_application = "Application Error";
static const char * att_error_common_error = "Common Profile and Service Error Codes";
static const char * att_error_timeout = "Timeout";

static const char * att_error_string_for_code(uint8_t code){
    if (code >= 0xe0) return att_error_common_error;
    if (code >= 0xa0) return att_error_reserved;
    if (code >= 0x80) return att_error_application;
    if (code == 0x7f) return att_error_timeout;
    if (code >= 0x12) return att_error_reserved;
    return att_errors[code];
}

const char * ad_event_types[] = {
    "Connectable undirected advertising",
    "Connectable directed advertising",
    "Scannable undirected advertising",
    "Non connectable undirected advertising",
    "Scan Response"
};

static void handle_advertising_event(uint8_t * packet, int size){
    // filter PTS
    bd_addr_t addr;
    bt_flip_addr(addr, &packet[4]);

    // always request address resolution
    sm_address_resolution_lookup(packet[3], addr);

    // ignore advertisement from devices other than pts
    // if (memcmp(addr, current_pts_address, 6)) return;

    printf("Advertisement: %s - %s, ", bd_addr_to_str(addr), ad_event_types[packet[2]]);
    int adv_size = packet[11];
    uint8_t * adv_data = &packet[12];

    // check flags
    ad_context_t context;
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        // uint8_t size      = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        switch (data_type){
            case 1: // AD_FLAGS
                if (*data & 1) printf("LE Limited Discoverable Mode, ");
                if (*data & 2) printf("LE General Discoverable Mode, ");
                break;
            default:
                break;
        }
    }

    // dump data
    printf("Data: ");
    printf_hexdump(adv_data, adv_size);
}

static uint8_t gap_adv_type(void){
    // if (gap_scannable) return 0x02;
    // if (gap_directed_connectable) return 0x01;
    if (!gap_connectable) return 0x03;
    return 0x00;
}

static void update_advertisment_params(void){
    uint8_t adv_type = gap_adv_type();
    printf("GAP: Connectable = %u -> advertising_type %u (%s)\n", gap_connectable, adv_type, ad_event_types[adv_type]);
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint16_t adv_int_min = 0x800;
    uint16_t adv_int_max = 0x800;
    switch (adv_type){
        case 0:
        case 2:
        case 3:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
            break;
        case 1:
        case 4:
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, public_pts_address_type, public_pts_address, 0x07, 0x00);
            break;
        }
}

static void gap_run(void){
    if (!hci_can_send_command_packet_now()) return;
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint16_t aHandle;
    sm_event_t * sm_event;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        show_usage();
                        gap_run();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = READ_BT_16(packet, 4);
                            printf("Connection complete, handle 0x%04x\n", handle);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    aHandle = READ_BT_16(packet, 3);
                    printf("Disconnected from handle 0x%04x\n", aHandle);
                    break;
                    
                case SM_PASSKEY_INPUT_NUMBER: 
                    // store peer address for input
                    sm_event = (sm_event_t *) packet;
                    memcpy(peer_address, sm_event->address, 6);
                    peer_addr_type = sm_event->addr_type;
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(sm_event->address), sm_event->addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;

                case SM_PASSKEY_DISPLAY_NUMBER:
                    sm_event = (sm_event_t *) packet;
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(sm_event->address), sm_event->addr_type, sm_event->passkey);
                    break;

                case SM_PASSKEY_DISPLAY_CANCEL: 
                    sm_event = (sm_event_t *) packet;
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(sm_event->address), sm_event->addr_type);
                    break;

                case SM_JUST_WORKS_REQUEST:
                    // auto-authorize connection if requested
                    sm_event = (sm_event_t *) packet;
                    sm_just_works_confirm(sm_event->addr_type, sm_event->address);
                    printf("Just Works request confirmed\n");
                    break;

                case SM_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_event = (sm_event_t *) packet;
                    sm_authorization_grant(sm_event->addr_type, sm_event->address);
                    break;

                case GAP_LE_ADVERTISING_REPORT:
                    handle_advertising_event(packet, size);
                    break;

                case SM_IDENTITY_RESOLVING_SUCCEEDED:
                    memcpy(current_pts_address, ((sm_event_t*) packet)->address, 6);
                    current_pts_address_type =  ((sm_event_t*) packet)->addr_type;
                    le_device_db_index       =  ((sm_event_t*) packet)->le_device_db_index;
                    // skip already detected pts
                    if (memcmp( ((sm_event_t*) packet)->address, current_pts_address, 6) == 0) break;
                    printf("Address resolving succeeded: resolvable address %s, addr type %u\n",
                        bd_addr_to_str(current_pts_address), current_pts_address_type);
                    break;
                default:
                    break;
            }
    }
    gap_run();
}

static void use_public_pts_address(void){
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;                    
}

static void handle_gatt_client_event(le_event_t * event){
    le_characteristic_value_event_t * value;
    le_characteristic_event_t * characteristic_event;
    uint8_t address_type;
    bd_addr_t flipped_address;
    le_service_t * service;
    gatt_complete_event_t * complete_event;
    le_characteristic_descriptor_event_t * characteristic_descriptor_event;
    switch(event->type){
        case GATT_SERVICE_QUERY_RESULT:
            switch (central_state){
                case CENTRAL_W4_PRIMARY_SERVICES:
                    service = &((le_service_event_t *) event)->service;
                    printf("Primary Service with UUID ");
                    printUUID(service->uuid128, service->uuid16);
                    printf(", start group handle 0x%04x, end group handle 0x%04x\n", service->start_group_handle, service->end_group_handle);
                    break;
                default:
                    break;
                }
            break;
        case GATT_INCLUDED_SERVICE_QUERY_RESULT:
            service = &((le_service_event_t *) event)->service;
            printf("Included Service with UUID ");
            printUUID(service->uuid128, service->uuid16);
            printf(", start group handle 0x%04x, end group handle 0x%04x\n", service->start_group_handle, service->end_group_handle);
            break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
            characteristic_event = ((le_characteristic_event_t *) event);
            switch (central_state) {
                case CENTRAL_W4_NAME_QUERY_COMPLETE:
                    gap_name_characteristic = characteristic_event->characteristic;
                    printf("GAP Name Characteristic found, value handle: 0x04%x\n", gap_name_characteristic.value_handle);
                    break;
                case CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE:
                    gap_reconnection_address_characteristic = characteristic_event->characteristic;
                    printf("GAP Reconnection Address Characteristic found, value handle: 0x04%x\n", gap_reconnection_address_characteristic.value_handle);
                    break;
                case CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE:
                    gap_peripheral_privacy_flag_characteristic = characteristic_event->characteristic;
                    printf("GAP Peripheral Privacy Flag Characteristic found, value handle: 0x04%x\n", gap_peripheral_privacy_flag_characteristic.value_handle);
                    break;
                case CENTRAL_W4_SIGNED_WRITE_QUERY_COMPLETE:
                    signed_write_characteristic = characteristic_event->characteristic;
                    printf("Characteristic for Signed Write found, value handle: 0x%04x\n", signed_write_characteristic.value_handle);
                    break;
                case CENTRAL_W4_CHARACTERISTICS:
                    printf("Characteristic found with handle 0x%04x, uuid ", (characteristic_event->characteristic).value_handle);
                    if ((characteristic_event->characteristic).uuid16){
                        printf("%04x\n", (characteristic_event->characteristic).uuid16);
                    } else {
                        printf_hexdump((characteristic_event->characteristic).uuid128, 16);                        
                    }
                    break;
                case CENTRAL_GPA_W4_RESPONSE2:
                    switch (ui_uuid16){
                        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
                        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
                            ui_attribute_handle = characteristic_event->characteristic.value_handle;
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case GATT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            value = (le_characteristic_value_event_t *) event;
            switch (central_state){
                case CENTRAL_W4_NAME_VALUE:
                    value->blob[value->blob_length] = 0;
                    printf("GAP Service: Device Name: %s\n", value->blob);
                    break;
                case CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_HANDLE:
                case CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID:
                case CENTRAL_W4_READ_MULTIPLE_CHARACTERISTIC_VALUES:
                    printf("Value: ");
                    printf_hexdump(value->blob, value->blob_length);
                    break;
                case CENTRAL_GPA_W4_RESPONSE:
                    switch (ui_uuid16){
                        case GATT_PRIMARY_SERVICE_UUID:
                            printf ("Attribute handle 0x%04x, primary service 0x%04x\n", value->value_handle, READ_BT_16(value->blob,0));
                            break;
                        case GATT_SECONDARY_SERVICE_UUID:
                            printf ("Attribute handle 0x%04x, secondary service 0x%04x\n", value->value_handle, READ_BT_16(value->blob,0));
                            break;
                        case GATT_INCLUDE_SERVICE_UUID:
                            printf ("Attribute handle 0x%04x, included service attribute handle 0x%04x, end group handle 0x%04x, uuid %04x\n",
                             value->value_handle, READ_BT_16(value->blob,0), READ_BT_16(value->blob,2), READ_BT_16(value->blob,4));
                            break;
                        case GATT_CHARACTERISTICS_UUID:
                            printf ("Attribute handle 0x%04x, properties 0x%02x, value handle 0x%04x, uuid ",
                             value->value_handle, value->blob[0], READ_BT_16(value->blob,1));
                            if (value->blob_length < 19){
                                printf("%04x\n", READ_BT_16(value->blob, 3));
                            } else {
                                printUUID128(&value->blob[3]);
                                printf("\n");
                            }
                            break;
                        case GATT_CHARACTERISTIC_EXTENDED_PROPERTIES:
                            printf ("Attribute handle 0x%04x, gatt characteristic properties 0x%04x\n", value->value_handle, READ_BT_16(value->blob,0));
                            break;
                        case GATT_CHARACTERISTIC_USER_DESCRIPTION:
                            // go the value, but PTS 6.3 requires another request
                            printf("Read by type request received, store attribute handle for read request\n");
                            ui_attribute_handle = value->value_handle;
                            break;                            
                        case GATT_CLIENT_CHARACTERISTICS_CONFIGURATION:
                            printf ("Attribute handle 0x%04x, gatt client characteristic configuration 0x%04x\n", value->value_handle, READ_BT_16(value->blob,0));
                            break;
                        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
                            ui_handles_count = value->blob_length >> 1;
                            printf ("Attribute handle 0x%04x, gatt characteristic aggregate format. Handles: ", value->value_handle);
                            for (ui_handles_index = 0; ui_handles_index < ui_handles_count ; ui_handles_index++){
                                ui_handles[ui_handles_index] = READ_BT_16(value->blob, (ui_handles_index << 1));
                                printf("0x%04x, ", ui_handles[ui_handles_index]);
                            }
                            printf("\n");
                            ui_handles_index = 0;
                            ui_aggregate_handle = value->value_handle;
                            break;
                        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
                            printf("Presentation format: ");
                            printf_hexdump(value->blob, value->blob_length);
                            memcpy(ui_presentation_format, value->blob, 7);
                            break;
                        default:
                            printf("Value: ");
                            printf_hexdump(value->blob, value->blob_length);
                            break;
                    }
                    break;
                case CENTRAL_GPA_W4_RESPONSE3:
                    switch (ui_uuid16){
                        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
                            printf("Value: ");
                            printf_hexdump(value->blob, value->blob_length);
                            printf("Format 0x%02x, Exponent 0x%02x, Unit 0x%04x\n",
                                ui_presentation_format[0], ui_presentation_format[1], READ_BT_16(ui_presentation_format, 2));                            
                            break;
                        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
                            printf("Aggregated value: ");
                            printf_hexdump(value->blob, value->blob_length);
                            memcpy(ui_value_data, value->blob, value->blob_length);
                            ui_value_pos = 0;      
                            central_state = CENTRAL_GPA_W4_RESPONSE4;                 
                        default:
                            break;
                    }
                    break;
               default:
                    break;
            }
            break;
        case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
            value = (le_characteristic_value_event_t *) event;
            central_state = CENTRAL_IDLE;
            printf("Value (offset %02u): ", value->value_offset);
            printf_hexdump(value->blob, value->blob_length);
            break;
        case GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
            characteristic_descriptor_event = (le_characteristic_descriptor_event_t *) event;
            switch (central_state){
                case CENTRAL_GPA_W4_RESPONSE2:
                    switch (ui_uuid16){
                        case GATT_CHARACTERISTIC_USER_DESCRIPTION:
                            characteristic_descriptor_event->value[characteristic_descriptor_event->value_length] = 0;
                            printf ("Attribute handle 0x%04x, characteristic user descriptor: %s\n",
                                characteristic_descriptor_event->handle, characteristic_descriptor_event->value);
                            break;
                        default:
                            break;
                    }
                    break;
                case CENTRAL_GPA_W4_RESPONSE4:
                    // only characteristic aggregate format
                    printf("Value: ");
                    printf_hexdump(&ui_value_data[ui_value_pos], gpa_format_type_len[characteristic_descriptor_event->value[0]]);
                    ui_value_pos +=  gpa_format_type_len[characteristic_descriptor_event->value[0]];
                    printf("Format 0x%02x, Exponent 0x%02x, Unit 0x%04x\n",
                        characteristic_descriptor_event->value[0], characteristic_descriptor_event->value[1],
                        READ_BT_16(characteristic_descriptor_event->value, 2));                            
                    break;
                default:
                    printf("Value: ");
                    printf_hexdump(characteristic_descriptor_event->value, characteristic_descriptor_event->value_length);
                    break;
            }
            break;
        case GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
            characteristic_descriptor_event = (le_characteristic_descriptor_event_t *) event;
            printf("Value (offset %02u): ", characteristic_descriptor_event->value_offset);
            printf_hexdump(characteristic_descriptor_event->value, characteristic_descriptor_event->value_length);
            break;

        case GATT_QUERY_COMPLETE:
            complete_event = (gatt_complete_event_t*) event;
            if (complete_event->status){
                central_state = CENTRAL_IDLE;
                printf("GATT_QUERY_COMPLETE: %s 0x%02x\n",
                att_error_string_for_code(complete_event->status),  complete_event->status);
                break;
            }
            switch (central_state){
                case CENTRAL_W4_NAME_QUERY_COMPLETE:
                    central_state = CENTRAL_W4_NAME_VALUE;
                    gatt_client_read_value_of_characteristic(gc_id, handle, &gap_name_characteristic);
                    break;
                case CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE:
                    central_state = CENTRAL_IDLE;
                    hci_le_advertisement_address(&address_type, our_private_address);
                    printf("Our private address: %s\n", bd_addr_to_str(our_private_address));
                    bt_flip_addr(flipped_address, our_private_address);
                    gatt_client_write_value_of_characteristic(gc_id, handle, gap_reconnection_address_characteristic.value_handle, 6, flipped_address);
                    reconnection_address_set = 1;
#ifdef PTS_USES_RECONNECTION_ADDRESS_FOR_ITSELF
                    memcpy(current_pts_address, our_private_address, 6);
                    current_pts_address_type = 1;
#endif
                    break;
                case CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE:
                    central_state = CENTRAL_IDLE;
                    switch (pts_privacy_flag){
                        case 0:
                            use_public_pts_address();
                            printf("Peripheral Privacy Flag set to FALSE, connecting to public PTS address again\n");
                            gatt_client_write_value_of_characteristic(gc_id, handle, gap_peripheral_privacy_flag_characteristic.value_handle, 1, &pts_privacy_flag);
                            break;
                        case 1:
                            printf("Peripheral Privacy Flag set to TRUE\n");
                            gatt_client_write_value_of_characteristic(gc_id, handle, gap_peripheral_privacy_flag_characteristic.value_handle, 1, &pts_privacy_flag);
                            break;
                        default:
                            break;
                        }
                    break;
                case CENTRAL_W4_SIGNED_WRITE_QUERY_COMPLETE:
                    printf("Signed write on Characteristic with UUID 0x%04x\n", pts_signed_write_characteristic_uuid);
                    gatt_client_signed_write_without_response(gc_id, handle, signed_write_characteristic.value_handle, sizeof(signed_write_value), signed_write_value);
                    break;
                case CENTRAL_W4_PRIMARY_SERVICES:
                    printf("Primary Service Discovery complete\n");
                    central_state = CENTRAL_IDLE;
                    break;
                case CENTRAL_GPA_W4_RESPONSE:
                    switch (ui_uuid16){
                        case GATT_CHARACTERISTIC_USER_DESCRIPTION:
                            central_state = CENTRAL_GPA_W4_RESPONSE2;
                            printf("Sending Read Characteristic Descriptor at 0x%04x\n", ui_attribute_handle);
                            gatt_client_read_characteristic_descriptor_using_descriptor_handle(gc_id, handle, ui_attribute_handle);
                            break;
                        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
                        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
                            {
                                printf("Searching Characteristic Declaration\n");
                                central_state = CENTRAL_GPA_W4_RESPONSE2;
                                le_service_t aService;                  
                                aService.start_group_handle = ui_start_handle;
                                aService.end_group_handle   = ui_end_handle;
                                gatt_client_discover_characteristics_for_service(gc_id, handle, &aService);
                                break;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case CENTRAL_GPA_W4_RESPONSE2:
                    switch(ui_uuid16){
                        case GATT_CHARACTERISTIC_PRESENTATION_FORMAT:
                        case GATT_CHARACTERISTIC_AGGREGATE_FORMAT:
                            printf("Reading characteristic value at 0x%04x\n", ui_attribute_handle);
                            central_state = CENTRAL_GPA_W4_RESPONSE3;
                            gatt_client_read_value_of_characteristic_using_value_handle(gc_id, handle, ui_attribute_handle);
                            break;
                        default:
                            break;
                    }
                    break;
                case CENTRAL_GPA_W4_RESPONSE4:
                    // so far, only GATT_CHARACTERISTIC_AGGREGATE_FORMAT
                    if (ui_handles_index < ui_handles_count) {
                        printf("Reading Characteristic Presentation Format at 0x%04x\n", ui_handles[ui_handles_index]);
                        gatt_client_read_characteristic_descriptor_using_descriptor_handle(gc_id, handle, ui_handles[ui_handles_index]);
                        ui_handles_index++;
                        break;
                    }
                    if (ui_handles_index == ui_handles_count ) {
                        // PTS rqequires to read the characteristic aggregate descriptor again (no idea why)
                        gatt_client_read_value_of_characteristic_using_value_handle(gc_id, handle, ui_aggregate_handle);
                        ui_handles_index++;
                    }
                    break;
                default:
                    central_state = CENTRAL_IDLE;
                    break;
            }
            break;
        case GATT_NOTIFICATION:
            value = (le_characteristic_value_event_t *) event;
            printf("Notification handle 0x%04x, value: ", value->value_handle);
            printf_hexdump(value->blob, value->blob_length);
            break;
        case GATT_INDICATION:
            value = (le_characteristic_value_event_t *) event;
            printf("Indication handle 0x%04x, value: ", value->value_handle);
            printf_hexdump(value->blob, value->blob_length);
            break;
        default:
            break;
    }
}

uint16_t value_handle = 1;
uint16_t attribute_size = 1;
int scanning_active = 0;

int num_rows = 0;
int num_lines = 0;
const char * rows[100];
const char * lines[100];
const char * empty_string = "";
const int width = 70;

static void reset_screen(void){
    // free memory
    int i = 0;
    for (i=0;i<num_rows;i++) {
        free((void*)rows[i]);
        rows[i] = NULL;
    }
    num_rows = 0;
    for (i=0;i<num_lines;i++) {
        free((void*)lines[i]);
        lines[i] = NULL;
    }
    num_lines = 0;
}

static void print_line(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    char * line = malloc(80);
    vsnprintf(line, 80, format, argptr);
    va_end(argptr);
    lines[num_lines] = line;
    num_lines++;
}

static void printf_row(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    char * row = malloc(80);
    vsnprintf(row, 80, format, argptr);
    va_end(argptr);
    rows[num_rows] = row;
    num_rows++;
}

static void print_screen(void){

    // clear screen
    printf("\e[1;1H\e[2J");

    // full lines on top
    int i;
    for (i=0;i<num_lines;i++){
        printf("%s\n", lines[i]);
    }
    printf("\n");

    // two columns
    int second_half = (num_rows + 1) / 2;
    for (i=0;i<second_half;i++){
        int pos = strlen(rows[i]);
        printf("%s", rows[i]);
        while (pos < width){
            printf(" ");
            pos++;
        }
        if (i + second_half < num_rows){
            printf("|  %s", rows[i+second_half]);
        }
        printf("\n");
    }
    printf("\n");
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    hci_le_advertisement_address(&iut_address_type, iut_address);

    reset_screen();

    print_line("--- CLI for LE Central ---");
    print_line("PTS: addr type %u, addr %s", current_pts_address_type, bd_addr_to_str(current_pts_address));
    print_line("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));
    print_line("--------------------------");
    print_line("GAP: connectable %u, bondable %u", gap_connectable, gap_bondable);
    print_line("SM: %s, MITM protection %u", sm_io_capabilities, sm_mitm_protection);
    print_line("SM: key range [%u..16], OOB data: %s", sm_min_key_size, 
        sm_have_oob_data ? (sm_have_oob_data == 1 ? (const char*) sm_oob_data_A : (const char*) sm_oob_data_B) : "None");
    print_line("Privacy %u", gap_privacy);
    print_line("Device name: %s", gap_device_name);

    printf_row("c/C - connectable off");
    printf_row("d/D - bondable off/on");
    printf_row("---");
    printf_row("1   - enable privacy using random non-resolvable private address");
    printf_row("2   - clear Peripheral Privacy Flag on PTS");
    printf_row("3   - set Peripheral Privacy Flag on PTS");
    printf_row("9   - create HCI Classic connection to addr %s", bd_addr_to_str(public_pts_address));
    printf_row("s/S - passive/active scanning");
    printf_row("a   - enable Advertisements");
    printf_row("b   - start bonding");
    printf_row("n   - query GAP Device Name");
    printf_row("o   - set GAP Reconnection Address");
    printf_row("t   - terminate connection, stop connecting");
    printf_row("p   - auto connect to PTS");
    printf_row("P   - direct connect to PTS");
    printf_row("w   - signed write on characteristic with UUID %04x", pts_signed_write_characteristic_uuid);
    printf_row("W   - signed write on attribute with handle 0x%04x and value 0x12", pts_signed_write_characteristic_handle);
    printf_row("z   - Update L2CAP Connection Parameters");
    printf_row("---");
    printf_row("e   - Discover all Primary Services");
    printf_row("f/F - Discover Primary Service by UUID16/UUID128");
    printf_row("g   - Discover all characteristics by UUID16");
    printf_row("h   - Discover all characteristics in range");
    printf_row("i   - Find all included services");
    printf_row("j/J - Read (Long) Characteristic Value by handle");
    printf_row("k/K - Read Characteristic Value by UUID16/UUID128");
    printf_row("l/L - Read (Long) Characteristic Descriptor by handle");
    printf_row("N   - Read Multiple Characteristic Values");
    printf_row("O   - Write without Response");
    printf_row("q/Q - Write (Long) Characteristic Value");
    printf_row("r   - Characteristic Reliable Write");
    printf_row("R   - Signed Write");
    printf_row("u/U - Write (Long) Characteristic Descriptor");
    printf_row("T   - Read Generic Profile Attributes by Type");
    printf_row("---");
    printf_row("4   - IO_CAPABILITY_DISPLAY_ONLY");
    printf_row("5   - IO_CAPABILITY_DISPLAY_YES_NO");
    printf_row("6   - IO_CAPABILITY_NO_INPUT_NO_OUTPUT");
    printf_row("7   - IO_CAPABILITY_KEYBOARD_ONLY");
    printf_row("8   - IO_CAPABILITY_KEYBOARD_DISPLAY");
    printf_row("m/M - MITM protection off");
    printf_row("x/X - encryption key range [7..16]/[16..16]");
    printf_row("y/Y - OOB data off/on/toggle A/B");
    printf_row("---");
    printf_row("Ctrl-c - exit");

    print_screen();
}

static void update_auth_req(void){
    uint8_t auth_req = 0;
    if (sm_mitm_protection){
        auth_req |= SM_AUTHREQ_MITM_PROTECTION;
    }
    if (gap_bondable){
        auth_req |= SM_AUTHREQ_BONDING;
    }
    sm_set_authentication_requirements(auth_req);
}

static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    int value_length = sizeof(signed_write_value);
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_SIGNED_WRITE_COMMAND;
    bt_store_16(request, 1, pts_signed_write_characteristic_handle);
    memcpy(&request[3], signed_write_value, value_length);
    bt_store_32(request, 3 + value_length, 0);
    swap64(hash, &request[3 + value_length + 4]);
    l2cap_send_prepared_connectionless(handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3 + value_length + 12);
}

static int hexForChar(char c){
    if (c >= '0' && c <= '9'){
        return c - '0';
    } 
    if (c >= 'a' && c <= 'f'){
        return c - 'a' + 10;
    }      
    if (c >= 'A' && c <= 'F'){
        return c - 'A' + 10;
    } 
    return -1;
} 

static void ui_request_uint16(const char * message){
    printf("%s", message);
    fflush(stdout);
    ui_uint16_request = 1;
    ui_uint16 = 0;
    ui_uint16_pos = 0;
}

static void ui_request_uud128(const char * message){
    printf("%s", message);
    fflush(stdout);
    ui_uuid128_request = 1;
    ui_uuid128_pos = 0;
    memset(ui_uuid128, 0, 16);
}

static void ui_request_data(const char * message){
    printf("%s", message);
    fflush(stdout);
    ui_value_request = 1;
    ui_value_pos = 0;
    memset(ui_value_data, 0, sizeof(ui_value_data));
}

static int ui_process_digits_for_passkey(char buffer){
    if (buffer < '0' || buffer > '9') {
        return 0;
    }
    printf("%c", buffer);
    fflush(stdout);
    ui_passkey = ui_passkey * 10 + buffer - '0';
    ui_digits_for_passkey--;
    if (ui_digits_for_passkey == 0){
        printf("\nSending Passkey %u (0x%x)\n", ui_passkey, ui_passkey);
        sm_passkey_input(peer_addr_type, peer_address, ui_passkey);
    }
    return 0;
}

static int ui_process_uint16_request(char buffer){
    if (buffer == 0x7f || buffer == 0x08) {
        if (ui_uint16_pos){
            printf("\b \b");
            fflush(stdout);
            ui_uint16 >>= 4;
            ui_uint16_pos--;
        }
        return 0;
    }
    if (buffer == '\n' || buffer == '\r'){
        ui_uint16_request = 0;
        printf("\n");
        switch (central_state){
            case CENTRAL_W4_PRIMARY_SERVICES:
                printf("Discover Primary Services with UUID16 %04x\n", ui_uint16);
                gatt_client_discover_primary_services_by_uuid16(gc_id, handle, ui_uint16);
                return 0;
            case CENTRAL_ENTER_START_HANDLE_4_DISCOVER_CHARACTERISTICS:
                ui_attribute_handle = ui_uint16;
                ui_request_uint16("Please enter end handle: ");
                central_state = CENTRAL_ENTER_END_HANDLE_4_DISCOVER_CHARACTERISTICS;
                return 0;
            case CENTRAL_ENTER_END_HANDLE_4_DISCOVER_CHARACTERISTICS: {
                printf("Discover Characteristics from 0x%04x to 0x%04x\n", ui_attribute_handle, ui_uint16);
                central_state = CENTRAL_W4_CHARACTERISTICS;
                le_service_t service;
                service.start_group_handle = ui_attribute_handle;
                service.end_group_handle   = ui_uint16;
                gatt_client_discover_characteristics_for_service(gc_id, handle, &service);
                return 0;
            }
            case CENTRAL_W4_CHARACTERISTICS:
                printf("Discover Characteristics with UUID16 %04x\n", ui_uint16);
                gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 0x0001, 0xffff, ui_uint16);
                return 0;
            case CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_HANDLE:
                printf("Read Characteristic Value with handle 0x%04x\n", ui_uint16);
                gatt_client_read_value_of_characteristic_using_value_handle(gc_id, handle, ui_uint16);
                return 0;
            case CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE:
                ui_attribute_handle = ui_uint16;
                ui_request_uint16("Please enter long value offset: ");
                central_state = CENTRAL_W4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE;
                return 0;
            case CENTRAL_W4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE:
                printf("Read Long Characteristic Value with handle 0x%04x, offset 0x%04x\n", ui_attribute_handle, ui_uint16);
                gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(gc_id, handle, ui_attribute_handle, ui_uint16);
                return 0;
            case CENTRAL_W4_READ_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE:
                printf("Read Characteristic Descriptor with handle 0x%04x\n", ui_uint16);
                gatt_client_read_characteristic_descriptor_using_descriptor_handle(gc_id, handle, ui_uint16);
                return 0;
            case CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE:
                ui_attribute_handle = ui_uint16;
                ui_request_uint16("Please enter long characteristic offset: ");
                central_state = CENTRAL_W4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE;
                return 0;
            case CENTRAL_W4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE:
                printf("Read Long Characteristic Descriptor with handle 0x%04x, offset 0x%04x\n", ui_attribute_handle, ui_uint16);
                gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(gc_id, handle, ui_attribute_handle, ui_uint16);
                return 0;
            case CENTRAL_ENTER_HANDLE_4_READ_CHARACTERISTIC_VALUE_BY_UUID:
                ui_uuid16 = ui_uint16;
                ui_request_uint16("Please enter start handle: ");
                central_state = CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID;
                return 0;
            case CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID:
                printf("Read Characteristic Value with UUID16 0x%04x\n", ui_uint16);
                gatt_client_read_value_of_characteristics_by_uuid16(gc_id, handle, ui_uint16, 0xffff, ui_uuid16);
                return 0;
            case CENTRAL_W4_READ_MULTIPLE_CHARACTERISTIC_VALUES:
                if (ui_uint16){
                    ui_handles[ui_handles_count++] = ui_uint16;
                    ui_request_uint16("Please enter handle: ");
                } else {
                    int i;                        
                    printf("Read multiple values, handles: ");
                    for (i=0;i<ui_handles_count;i++){
                        printf("0x%04x, ", ui_handles[i]);
                    }
                    printf("\n");
                    gatt_client_read_multiple_characteristic_values(gc_id, handle, ui_handles_count, ui_handles);
                }
                return 0;

            case CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_VALUE:
                ui_attribute_handle = ui_uint16;
                ui_request_uint16("Please enter offset: ");
                central_state = CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_VALUE;
                return 0;
            case CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
                ui_attribute_handle = ui_uint16;
                ui_request_uint16("Please enter offset: ");
                central_state = CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR;
                return 0;
            case CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_VALUE:
            case CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
                ui_attribute_offset = ui_uint16;
                ui_request_data("Please enter data: ");
                return 0;
            case CENTRAL_W4_WRITE_WITHOUT_RESPONSE:
            case CENTRAL_W4_WRITE_CHARACTERICISTIC_VALUE:
            case CENTRAL_W4_RELIABLE_WRITE:
            case CENTRAL_W4_WRITE_CHARACTERISTIC_DESCRIPTOR:
            case CENTRAL_W4_SIGNED_WRITE:            
                ui_attribute_handle = ui_uint16;
                ui_request_data("Please enter data: ");
                return 0;
            case CENTRAL_GPA_ENTER_START_HANDLE:
                ui_start_handle = ui_uint16;
                central_state = CENTRAL_GPA_ENTER_END_HANDLE;
                ui_request_uint16("Please enter end handle: ");
                return 0;
            case CENTRAL_GPA_ENTER_END_HANDLE:
                ui_end_handle = ui_uint16;
                central_state = CENTRAL_GPA_W4_RESPONSE;
                ui_request_uint16("Please enter uuid: ");
                return 0;
            case CENTRAL_GPA_W4_RESPONSE:
                ui_uuid16 = ui_uint16;
                printf("Read by type: range 0x%04x-0x%04x, uuid %04x\n", ui_start_handle, ui_end_handle, ui_uuid16);
                gatt_client_read_value_of_characteristics_by_uuid16(gc_id, handle, ui_start_handle, ui_end_handle, ui_uuid16);
                return 0;
            default:
                return 0;
        }
    }
    int hex = hexForChar(buffer);
    if (hex < 0){
        return 0;
    }
    printf("%c", buffer);
    fflush(stdout);
    ui_uint16 = ui_uint16 << 4 | hex;
    ui_uint16_pos++;
    return 0;    
}

static int uuid128_pos_starts_with_dash(int pos){
    switch(pos){
        case 8:
        case 12:
        case 16:
        case 20:
#ifdef PTS_UUID128_REPRESENTATION
        case 4:
        case 24:
#endif
            return 1;
        default:
            return 0;
    }
}

static int ui_process_uuid128_request(char buffer){
    if (buffer == '-') return 0;    // skip - 

    if (buffer == 0x7f || buffer == 0x08) {
        if (ui_uuid128_pos){
            if (uuid128_pos_starts_with_dash(ui_uuid128_pos)){
                printf("\b \b");
                fflush(stdout);
            }
            printf("\b \b");
            fflush(stdout);
            ui_uuid128_pos--;
        }
        return 0;
    }

    int hex = hexForChar(buffer);
    if (hex < 0){
        return 0;
    }
    printf("%c", buffer);
    fflush(stdout);
    if (ui_uuid128_pos & 1){
        ui_uuid128[ui_uuid128_pos >> 1] = (ui_uuid128[ui_uuid128_pos >> 1] & 0xf0) | hex;
    } else {
        ui_uuid128[ui_uuid128_pos >> 1] = hex << 4;
    }
    ui_uuid128_pos++;
    if (ui_uuid128_pos == 32){
        ui_uuid128_request = 0;
        printf("\n");
        switch (central_state){
            case CENTRAL_W4_PRIMARY_SERVICES:
                printf("Discover Primary Services with UUID128 ");
                printUUID128(ui_uuid128);
                printf("\n");
                gatt_client_discover_primary_services_by_uuid128(gc_id, handle, ui_uuid128);
                return 0;
            case CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID:
                printf("Read Characteristic Value with UUID128 ");
                printUUID128(ui_uuid128);
                printf("\n");
                gatt_client_read_value_of_characteristics_by_uuid128(gc_id, handle, 0x0001, 0xffff, ui_uuid128);
                return 0;
            default:
                return 0;
        }
    }
    if (uuid128_pos_starts_with_dash(ui_uuid128_pos)){
        printf("-");
        fflush(stdout);
    }
    return 0;
}

static void ui_announce_write(const char * method){
    printf("Request: %s handle 0x%04x data: ", method, ui_uint16);
    printf_hexdump(ui_value_data, ui_value_pos >> 1);
    printf("\n");
}

static int ui_process_data_request(char buffer){
    if (buffer == 0x7f || buffer == 0x08) {
        if (ui_value_pos){
            if ((ui_value_pos & 1) == 0){
                printf("\b");
            }
            printf("\b \b");
            fflush(stdout);
            ui_value_pos--;
        }
        return 0;
    }
    if (buffer == '\n' || buffer == '\r'){
        ui_value_request = 0;
        printf("\n");
        uint16_t value_len = ui_value_pos >> 1;
        switch (central_state){
            case CENTRAL_W4_WRITE_WITHOUT_RESPONSE:
                ui_announce_write("Write without response");
                gatt_client_write_value_of_characteristic_without_response(gc_id, handle, ui_attribute_handle, value_len, ui_value_data);
                break;
            case CENTRAL_W4_WRITE_CHARACTERICISTIC_VALUE:
                ui_announce_write("Write Characteristic Value");
                gatt_client_write_value_of_characteristic(gc_id, handle, ui_attribute_handle, value_len, ui_value_data);
                break;
            case CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_VALUE:
                ui_announce_write("Write Long Characteristic Value");
                gatt_client_write_long_value_of_characteristic_with_offset(gc_id, handle, ui_attribute_handle, ui_attribute_offset, value_len, ui_value_data);
                break;
            case CENTRAL_W4_RELIABLE_WRITE:
                ui_announce_write("Reliabe Write");
                gatt_client_reliable_write_long_value_of_characteristic(gc_id, handle, ui_attribute_handle, value_len, ui_value_data);
                break;
            case CENTRAL_W4_WRITE_CHARACTERISTIC_DESCRIPTOR:
                ui_announce_write("Write Characteristic Descriptor");
                gatt_client_write_characteristic_descriptor_using_descriptor_handle(gc_id, handle, ui_attribute_handle, value_len, ui_value_data);
                break;
            case CENTRAL_W4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
                ui_announce_write("Write Long Characteristic Descriptor");
                gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(gc_id, handle, ui_attribute_handle, ui_attribute_offset, value_len, ui_value_data);
                break;
            case CENTRAL_W4_SIGNED_WRITE:
                ui_announce_write("Signed Write");
                gatt_client_signed_write_without_response(gc_id, handle, ui_attribute_handle, value_len, ui_value_data);
            default:
                break;
        }             
        return 0;   
    }
    int hex = hexForChar(buffer);
    if (hex < 0){
        return 0;
    }

    printf("%c", buffer);

    if (ui_value_pos & 1){
        ui_value_data[ui_value_pos >> 1] = (ui_value_data[ui_value_pos >> 1] & 0xf0) | hex;
        printf(" ");
    } else {
        ui_value_data[ui_value_pos >> 1] = hex << 4;
    }
    ui_value_pos++;

    fflush(stdout);
    return 0;
}

static void ui_process_command(char buffer){
    int res;
    switch (buffer){
        case '1':
            printf("Enabling non-resolvable private address\n");
            gap_random_address_set_mode(GAP_RANDOM_ADDRESS_NON_RESOLVABLE);
            gap_privacy = 1;
            update_advertisment_params(); 
            show_usage();
            break;
        case '2':
            pts_privacy_flag = 0;
            central_state = CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_PERIPHERAL_PRIVACY_FLAG);
            break;
        case '3':
            pts_privacy_flag = 1;
            central_state = CENTRAL_W4_PERIPHERAL_PRIVACY_FLAG_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_PERIPHERAL_PRIVACY_FLAG);
            break;
        case '4':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
            show_usage();
            break;
        case '5':
            sm_io_capabilities = "IO_CAPABILITY_DISPLAY_YES_NO";
            sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
            show_usage();
            break;
        case '6':
            sm_io_capabilities = "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
            sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            show_usage();
            break;
        case '7':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_ONLY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_ONLY);
            show_usage();
            break;
        case '8':
            sm_io_capabilities = "IO_CAPABILITY_KEYBOARD_DISPLAY";
            sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_DISPLAY);
            show_usage();
            break;
        case '9':
            printf("Creating HCI Classic Connection to %s\n", bd_addr_to_str(public_pts_address));
            hci_send_cmd(&hci_create_connection, public_pts_address, hci_usable_acl_packet_types(), 0, 0, 0, 1);
            break;
        case 'a':
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
            show_usage();
            break;
        case 'b':
            sm_request_pairing(current_pts_address_type, current_pts_address);
            break;
        case 'c':
            gap_connectable = 0;
            update_advertisment_params();
            hci_connectable_control(gap_connectable);
            show_usage();
            break;
        case 'C':
            gap_connectable = 1;
            update_advertisment_params();
            hci_connectable_control(gap_connectable);
            show_usage();
            break;
        case 'd':
            gap_bondable = 0;
            update_auth_req();
            show_usage();
            break;
        case 'D':
            gap_bondable = 1;
            update_auth_req();
            show_usage();
            break;
        case 'm':
            sm_mitm_protection = 0;
            update_auth_req();
            show_usage();
            break;
        case 'M':
            sm_mitm_protection = 1;
            update_auth_req();
            show_usage();
            break;
        case 'n':
            central_state = CENTRAL_W4_NAME_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_DEVICE_NAME_UUID);
            break;
        case 'o':
            central_state = CENTRAL_W4_RECONNECTION_ADDRESS_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, GAP_RECONNECTION_ADDRESS_UUID);
            break;
        case 'p':
            res = gap_auto_connection_start(current_pts_address_type, current_pts_address);
            printf("Auto Connection Establishment to type %u, addr %s -> %x\n", current_pts_address_type, bd_addr_to_str(current_pts_address), res);
            break;
        case 'P':
            le_central_connect(current_pts_address, current_pts_address_type);
            printf("Direct Connection Establishment to type %u, addr %s\n", current_pts_address_type, bd_addr_to_str(current_pts_address));
            break;
        case 's':
            if (scanning_active){
                le_central_stop_scan();
                scanning_active = 0;
                break;
            }
            printf("Start passive scanning\n");
            le_central_set_scan_parameters(0, 48, 48);
            le_central_start_scan();
            scanning_active = 1;
            break;
        case 'S':
            if (scanning_active){
                printf("Stop scanning\n");
                le_central_stop_scan();
                scanning_active = 0;
                break;
            }
            printf("Start active scanning\n");
            le_central_set_scan_parameters(1, 48, 48);
            le_central_start_scan();
            scanning_active = 1;
            break;
        case 't':
            printf("Terminating connection\n");
            hci_send_cmd(&hci_disconnect, handle, 0x13);
            gap_auto_connection_stop_all();
            le_central_connect_cancel();
            break;
        case 'w':
            pts_privacy_flag = 2;
            central_state = CENTRAL_W4_SIGNED_WRITE_QUERY_COMPLETE;
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gc_id, handle, 1, 0xffff, pts_signed_write_characteristic_uuid);
            break;
        case 'W':
            // fetch csrk
            le_device_db_local_csrk_get(le_device_db_index, signing_csrk);
            // calc signature
            sm_cmac_start(signing_csrk, ATT_SIGNED_WRITE_COMMAND, pts_signed_write_characteristic_handle, sizeof(signed_write_value), signed_write_value, 0, att_signed_write_handle_cmac_result);
            break;
        case 'x':
            sm_min_key_size = 7;
            sm_set_encryption_key_size_range(7, 16);
            show_usage();
            break;
        case 'X':
            sm_min_key_size = 16;
            sm_set_encryption_key_size_range(16, 16);
            show_usage();
            break; 
       case 'y':
            sm_have_oob_data = 0;
            show_usage();
            break;
        case 'Y':
            if (sm_have_oob_data){
                sm_have_oob_data = 3 - sm_have_oob_data;
            } else {
                sm_have_oob_data = 1;
            }
            show_usage();
            break;
        case 'z':
            printf("Updating l2cap connection parameters\n");
            gap_update_connection_parameters(handle, 50, 120, 0, 550);
            break;

        // GATT commands
        case 'e':
            central_state = CENTRAL_W4_PRIMARY_SERVICES;
            gatt_client_discover_primary_services(gc_id, handle);
            break;
        case 'f':
            central_state = CENTRAL_W4_PRIMARY_SERVICES;
            ui_request_uint16("Please enter UUID16: ");
            break;
        case 'F':
            central_state = CENTRAL_W4_PRIMARY_SERVICES;
            ui_request_uud128("Please enter UUID128: ");
            break;
        case 'g':
            {
                central_state = CENTRAL_W4_CHARACTERISTICS;
                le_service_t service;
                service.start_group_handle = 0x0001;
                service.end_group_handle   = 0xffff;
                gatt_client_discover_characteristics_for_service(gc_id, handle, &service);
                break;
            }
        case 'h':
            central_state = CENTRAL_ENTER_START_HANDLE_4_DISCOVER_CHARACTERISTICS;
            ui_request_uint16("Please enter start_handle: ");
            break;
        case 'i':
            {
                central_state = CENTRAL_W4_CHARACTERISTICS;
                le_service_t service;
                service.start_group_handle = 0x0001;
                service.end_group_handle   = 0xffff;
                gatt_client_find_included_services_for_service(gc_id, handle, &service);
            }
            break;
        case 'j':
            central_state = CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_HANDLE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'J':
            central_state = CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_VALUE_BY_HANDLE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'l':
            central_state = CENTRAL_W4_READ_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'L':
            central_state = CENTRAL_ENTER_OFFSET_4_READ_LONG_CHARACTERISTIC_DESCRIPTOR_BY_HANDLE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'k':
            central_state = CENTRAL_ENTER_HANDLE_4_READ_CHARACTERISTIC_VALUE_BY_UUID;
            ui_request_uint16("Please enter UUID16: ");
            break;
        case 'K':
            central_state = CENTRAL_W4_READ_CHARACTERISTIC_VALUE_BY_UUID;
            ui_request_uud128("Please enter UUID128: ");
            break;
        case 'N':
            ui_request_uint16("Read Multiple Characteristic Values - enter 0 to complete list.\nPlease enter handle: ");
            ui_handles_count = 0;
            central_state = CENTRAL_W4_READ_MULTIPLE_CHARACTERISTIC_VALUES;
            break;
        case 'O':
            central_state = CENTRAL_W4_WRITE_WITHOUT_RESPONSE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'q':
            central_state = CENTRAL_W4_WRITE_CHARACTERICISTIC_VALUE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'Q':
            central_state = CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_VALUE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'r':
            central_state = CENTRAL_W4_RELIABLE_WRITE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'u':
            central_state = CENTRAL_W4_WRITE_CHARACTERISTIC_DESCRIPTOR;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'U':
            central_state = CENTRAL_ENTER_HANDLE_4_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'R':
            central_state = CENTRAL_W4_SIGNED_WRITE;
            ui_request_uint16("Please enter handle: ");
            break;
        case 'T': 
            central_state = CENTRAL_GPA_ENTER_START_HANDLE;
            ui_request_uint16("Please enter start handle: ");
            break;
        default:
            show_usage();
            break;
    }
}

static int stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);

    if (ui_digits_for_passkey){
        return ui_process_digits_for_passkey(buffer);
    }

    if (ui_uint16_request){
        return ui_process_uint16_request(buffer);
    }

    if (ui_uuid128_request){
        return ui_process_uuid128_request(buffer);
    }

    if (ui_value_request){
        return ui_process_data_request(buffer);        
    }

    ui_process_command(buffer);

    return 0;
}

static int get_oob_data_callback(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data){
    switch(sm_have_oob_data){
        case 1:
            memcpy(oob_data, sm_oob_data_A, 16);
            return 1;
        case 2:
            memcpy(oob_data, sm_oob_data_B, 16);
            return 1;
        default:
            return 0;
    }
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){

    printf("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size);
    uint16_t  att_value_len;

    uint16_t uuid16 = att_uuid_for_handle(handle);
    switch (uuid16){
        case 0x2a00:
            att_value_len = strlen(gap_device_name);
            if (buffer) {
                memcpy(buffer, gap_device_name, att_value_len);
            }
            return att_value_len;        
        default:
            break;
    }
    return 0;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    printf("BTstack LE Peripheral starting up...\n");

    memset(rows, 0, sizeof(char *) * 100);
    memset(lines, 0, sizeof(char *) * 100);

    strcpy(gap_device_name, "BTstack");

    // set up l2cap_le
    l2cap_init();
    
    // Setup SM: Display only
    sm_init();
    sm_register_packet_handler(app_packet_handler);
    sm_register_oob_data_callback(get_oob_data_callback);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    sm_set_encryption_key_size_range(sm_min_key_size, 16);
    sm_test_set_irk(test_irk);
    sm_test_use_fixed_local_csrk();

    // setup GATT Client
    gatt_client_init();
    gc_id = gatt_client_register_packet_handler(handle_gatt_client_event);;

    // Setup ATT/GATT Server
    att_server_init(profile_data, att_read_callback, NULL);    
    att_server_register_packet_handler(app_packet_handler);

    // Setup LE Device DB
    le_device_db_init();

    // add bonded device with IRK 0x00112233..FF for gap-conn-prda-bv-2
    uint8_t pts_irk[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
    le_device_db_add(public_pts_address_type, public_pts_address, pts_irk);

    // set adv params
    update_advertisment_params();

    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    // classic discoverable / connectable
    hci_connectable_control(0);
    hci_discoverable_control(1);

    // allow foor terminal input
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}

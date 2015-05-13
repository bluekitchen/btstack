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
// BLE Client
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include <btstack/sdp_util.h>

#include "btstack-config.h"

#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"
#include "gatt_client.h"

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

void (*ble_client_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;
static void ble_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void ble_client_init(void){
    gatt_client_init();
    l2cap_register_packet_handler(ble_packet_handler);
}


void ble_client_register_gatt_client_event_packet_handler(void (*le_callback)(le_event_t* event)){
    gatt_client_register_packet_handler(le_callback);
}

void ble_client_register_hci_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    ble_client_packet_handler = handler;
}


static void ble_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    // forward to app
    if (!ble_client_packet_handler) return;
    ble_client_packet_handler(packet_type, packet, size);
}



// TEST CODE

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    printf(", uuid ");
    if (uuid16){
        printf(" 0x%02x",uuid16);
    } else {
        printUUID128(uuid128);
    }
    printf("\n");
}

static void dump_characteristic(le_characteristic_t * characteristic){
    printf("    *** characteristic *** properties %x, start handle 0x%02x, value handle 0x%02x, end handle 0x%02x", 
                            characteristic->properties, characteristic->start_handle, characteristic->value_handle, characteristic->end_handle);
    printUUID(characteristic->uuid128, characteristic->uuid16);
}

static void dump_ad_event(ad_event_t * e){
    printf("    *** adv. event *** evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type, 
                            e->address_type, bd_addr_to_str(e->address), e->rssi, e->length);
    printf_hexdump(e->data, e->length);
    
}

static void dump_service(le_service_t * service){
    printf("    *** service *** start group handle %02x, end group handle %02x", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
}


static void dump_descriptor(le_event_t * event){
    le_characteristic_descriptor_event_t * devent = (le_characteristic_descriptor_event_t *) event;
    le_characteristic_descriptor_t descriptor = devent->characteristic_descriptor;
    printf("    *** descriptor *** handle 0x%02x, value ", descriptor.handle);
    printf_hexdump(devent->value, devent->value_length);
    printUUID(descriptor.uuid128, descriptor.uuid16);
}

static void dump_characteristic_value(le_characteristic_value_event_t * event){
    printf("    *** characteristic value of length %d *** ", event->blob_length );
    printf_hexdump(event->blob , event->blob_length);
    printf("\n");
}



gatt_client_t test_gatt_client_context;
uint16_t test_gatt_client_handle;

le_service_t services[40];
le_characteristic_t characteristics[100];
le_service_t service;
le_characteristic_t enable_characteristic;
le_characteristic_t config_characteristic;
le_characteristic_descriptor_t descriptor;

int service_count = 0;
int service_index = 0;
int characteristic_count = 0;
int characteristic_index = 0;

uint8_t acc_service_uuid[16] =           {0xf0, 0x00, 0xaa, 0x10, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t acc_chr_client_config_uuid[16] = {0xf0, 0x00, 0xaa, 0x11, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t acc_chr_enable_uuid[16] =        {0xf0, 0x00, 0xaa, 0x12, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t  acc_enable[1]  = {0x01};
uint8_t  acc_disable[1] = {0x00};


static uint8_t   test_device_addr_type = 0;
// pick one:
//static bd_addr_t test_device_addr = {0x00, 0x1b, 0xdc, 0x05, 0xb5, 0xdc};
static bd_addr_t sensor_tag1_addr = {0x1c, 0xba, 0x8c, 0x20, 0xc7, 0xf6};// SensorTag 1
static bd_addr_t sensor_tag2_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b}; // SensorTag 2
// static bd_addr_t test_device_addr = {0x00, 0x02, 0x72, 0xdc, 0x31, 0xc1}; // delock40 

typedef enum {
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,

    TC_W4_SERVICE_RESULT,
    TC_W4_SERVICE_WITH_UUID_RESULT,

    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_CHARACTERISTIC_WITH_UUID_RESULT,
    TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT,

    TC_W4_INCLUDED_SERVICE_RESULT,
    
    TC_W4_READ_RESULT,
    TC_W4_READ_LONG_RESULT,

    TC_W2_WRITE_WITHOUT_RESPONSE,
    TC_W4_WRITE_WITHOUT_RESPONSE_SENT,

    TC_W4_WRITE_RESULT,
    TC_W4_LONG_WRITE_RESULT,
    TC_W4_RELIABLE_WRITE_RESULT,

    TC_W4_ACC_ENABLE,
    TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT,
    TC_W4_ACC_SUBSCRIBE,
    TC_W4_ACC_DATA,

    TC_W4_DISCONNECT,
    TC_DISCONNECTED

} tc_state_t;

tc_state_t tc_state = TC_IDLE;
le_characteristic_t characteristic;

uint8_t chr_long_value[26] = {
    0x76, 0x75, 0x74, 0x73, 0x72, 
    0x71, 0x70, 0x6f, 0x6e, 0x6d, 
    0x6c, 0x6b, 0x6a, 0x69, 0x68, 
    0x67, 0x66, 0x65, 0x64, 0x63, 
    0x62, 0x61, 0x60, 0x5f, 0x5e,
    0x5d};
uint8_t chr_short_value[1] = {0x86};

// void sm_test_basic_cmds(le_event_t * event){
//     le_service_t service;
//     le_characteristic_descriptor_t descriptor;
    
//     switch(tc_state){
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_SERVICE_RESULT;
//             printf("\n test client - CONNECTED, query services now\n");
//             le_central_discover_primary_services(&test_gatt_client_context);
//             break;
        
//         case TC_W4_SERVICE_RESULT:
//             switch(event->type){
//                 case GATT_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     dump_service(&service);
//                     services[service_count++] = service;
//                     break;
                
//                 case GATT_SERVICE_QUERY_COMPLETE:
//                     tc_state = TC_W4_SERVICE_WITH_UUID_RESULT;
//                     printf("\n test client - SERVICE by SERVICE UUID QUERY\n"); 
//                     le_central_discover_primary_services_by_uuid16(&test_gatt_client_context, 0xffff);
//                     break;
                
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_SERVICE_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     dump_service(&service);
//                     break;
                
//                 case GATT_SERVICE_QUERY_COMPLETE:
//                     tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                     service_index = 0;
//                     printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY\n", services[service_index].uuid16);
//                     le_central_discover_characteristics_for_service(&test_gatt_client_context, &services[service_index]);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_CHARACTERISTIC_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     characteristics[characteristic_count++] = characteristic;
//                     break;

//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     if (service_index < service_count) {
//                         tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                         service = services[service_index++];
//                         printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY\n", service.uuid16);
//                         le_central_discover_characteristics_for_service(&test_gatt_client_context, &service);
//                         break;
//                     }
//                     tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT; 
//                     service_index = 0;
//                     characteristic_index = 0;
//                     printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY with UUID 0x%02x\n", 
//                             services[0].uuid16, characteristics[0].uuid16);
//                     le_central_discover_characteristics_for_handle_range_by_uuid16(&test_gatt_client_context,
//                         services[0].start_group_handle, services[0].end_group_handle, characteristics[0].uuid16);
//                     break;
//                 default:
//                     break;
//             }
//             break;
        
//         case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT;
//                     printf("\n test client - DESCRIPTOR for CHARACTERISTIC \n");
//                     dump_characteristic(&characteristics[0]);
//                     le_central_discover_characteristic_descriptors(&test_gatt_client_context, &characteristics[0]);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
//             switch(event->type){
//                 case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
//                     descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                     dump_descriptor(&descriptor);
//                     break;
//                 case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE:
//                     service_index = 0;
//                     service = services[service_index];
//                     tc_state = TC_W4_INCLUDED_SERVICE_RESULT;
//                     printf("\n test client - INCLUDED SERVICE QUERY, for service %02x\n", service.uuid16);
//                     le_central_find_included_services_for_service(&test_gatt_client_context, &service);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_INCLUDED_SERVICE_RESULT:
//             switch(event->type){
//                 case GATT_INCLUDED_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     printf("    *** found included service *** start group handle %02x, end group handle %02x \n", service.start_group_handle, service.end_group_handle);
//                     break;
//                 case GATT_INCLUDED_SERVICE_QUERY_COMPLETE:
//                     if (service_index < service_count) {
//                         service = services[service_index++];
//                         printf("\n test client - INCLUDED SERVICE QUERY, for service %02x\n", service.uuid16);
//                         le_central_find_included_services_for_service(&test_gatt_client_context, &service);
//                         break;
//                     }
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     break;
//             }
//             break;
//         default:
//             break;
//     }
// }


// void sm_test_reads(le_event_t * event){
//     switch(tc_state){
//         // 59 5a 5d f200
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_gatt_client_context, 0x59, 0x5d, 0xf200);
//             break;

//          case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_READ_LONG_RESULT;
//                     printf("\n\n test client - LONG VALUE for CHARACTERISTIC \n");
//                     le_central_write_value_of_characteristic_without_response(&test_gatt_client_context, characteristic.value_handle, 1, chr_short_value);
//                     break;
//                 default:
//                     break;
//             }
//             break;   

//          case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;

//         default:
//             break;
//         }
// }

// void sm_test_write_no_response(le_event_t * event){
//     switch(tc_state){
//         case TC_W4_SCAN_RESULT: {
//             if (event->type != GATT_ADVERTISEMENT) break;
//             printf("test client - SCAN ACTIVE\n");
//             ad_event_t * ad_event = (ad_event_t*) event;
//             dump_ad_event(ad_event);
//             // copy found addr
//             test_device_addr_type = ad_event->address_type;
//             bd_addr_t found_device_addr;
//             memcpy(found_device_addr, ad_event->address, 6);

//             if (memcmp(&found_device_addr, &test_device_addr, 6) != 0) break;
//             // memcpy(test_device_addr, ad_event->address, 6);

//             tc_state = TC_W4_CONNECT;
//             le_central_stop_scan();
//             le_central_connect(test_device, test_device_addr_type, test_device_addr);
//             break;
//         }
//         // 59 5a 5d f200
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_gatt_client_context, 0x59, 0x5d, 0xf200);
//             break;

//         case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:{
//                     printf("\n\n test client - Write VALUE for CHARACTERISTIC \n");
//                     tc_state = TC_W2_WRITE_WITHOUT_RESPONSE;
//                     le_command_status_t  status = le_central_write_value_of_characteristic_without_response(&test_gatt_client_context, characteristic.value_handle, 1, chr_short_value);
//                     if (status != BLE_PERIPHERAL_OK) break;
                    
//                     tc_state = TC_W4_READ_LONG_RESULT;
//                     le_central_read_long_value_of_characteristic(&test_gatt_client_context, &characteristic);
//                     break;
//                 }
//                 default:
//                     break;
//             }
//             break;   

//         case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;
        
//         default:
//             break;
//         }
// }

// void sm_test_long_writes(le_event_t * event){
//     switch(tc_state){
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_gatt_client_context, 0x59, 0x5d, 0xf200);
//             break;

//          case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:{
//                     printf("\n\n test client - Write LONG VALUE for CHARACTERISTIC \n");
//                     tc_state = TC_W4_WRITE_RESULT;
//                     // le_central_write_value_of_characteristic(&test_gatt_client_context, characteristic.value_handle, 1, chr_short_value);
//                     // le_central_write_long_value_of_characteristic(&test_gatt_client_context, characteristic.value_handle, 26, chr_long_value);
//                     le_central_reliable_write_long_value_of_characteristic(&test_gatt_client_context, characteristic.value_handle, 26, chr_long_value);
//                     break;
//                 }
//                 default:
//                     break;
//             }
//             break;   

//          case TC_W4_WRITE_RESULT:
//             printf("\n\n test client - Write LONG VALUE for CHARACTERISTIC \n");
//             tc_state = TC_W4_READ_LONG_RESULT;
//             le_central_read_long_value_of_characteristic(&test_gatt_client_context, &characteristic);
//             break;

//          case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;

//         default:
//             printf("Client, unhandled state %d\n", tc_state);
//             break;
//         }
// }

//
//void sm_test_read_characteristic_descriptor(le_event_t * event){
//    switch(tc_state){
//        case TC_W4_CONNECT:
//            if (event->type != GATT_CONNECTION_COMPLETE) break;
//            tc_state = TC_W4_SERVICE_RESULT;
//            printf("\n test client - CONNECTED, query ACC service\n");
//            le_central_discover_primary_services_by_uuid128(&test_gatt_client_context, acc_service_uuid);
//            break;
//            
//        case TC_W4_SERVICE_RESULT:
//            switch(event->type){
//                case GATT_SERVICE_QUERY_RESULT:
//                    service = ((le_service_event_t *) event)->service;
//                    dump_service(&service);
//                    break;
//                case GATT_SERVICE_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                    printf("\n test client - FIND ENABLE CHARACTERISTIC for SERVICE QUERY : \n");
//                    dump_service(&service);
//                    le_central_discover_characteristics_for_service_by_uuid128(&test_gatt_client_context, &service, acc_chr_enable_uuid);
//                    break;
//                default:
//                    break;
//            }
//            break;
//            
//        case TC_W4_CHARACTERISTIC_RESULT:
//            switch(event->type){
//                case GATT_CHARACTERISTIC_QUERY_RESULT:
//                    enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                    dump_characteristic(&enable_characteristic);
//                    break;
//                case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT;
//                    printf("\n test client - ACC ENABLE\n");
//                    le_central_discover_characteristic_descriptors(&test_gatt_client_context, &enable_characteristic);
//                    break;
//                default:
//                    break;
//            }
//            break;
//            
//        case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
//            switch(event->type){
//                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
//                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                    dump_descriptor(&descriptor);
//                    break;
//                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE:
//                    le_central_read_characteristic_descriptor(&test_gatt_client_context, &descriptor);
//                    break;
//                case GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:{
//                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                    dump_descriptor(&descriptor);
//                    tc_state = TC_W4_DISCONNECT;
//                    printf("\n\n test client - DISCONNECT ");
//                    le_central_disconnect(&test_device);
//                    break;
//                }
//                default:
//                    break;
//            }
//            break;
//            
//        default:
//            printf("Client, unhandled state %d\n", tc_state);
//            break;
//    }
//
//}



//static void handle_gatt_client_event(le_event_t * event){
//    switch(tc_state){
//        case TC_W4_SERVICE_RESULT:
//            switch(event->type){
//                case GATT_SERVICE_QUERY_RESULT:
//                    service = ((le_service_event_t *) event)->service;
//                    dump_service(&service);
//                    break;
//                case GATT_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                    printf("\n test client - FIND ENABLE CHARACTERISTIC for SERVICE QUERY : \n"); 
//                    dump_service(&service);
//                    gatt_client_discover_characteristics_for_service_by_uuid128(&test_gatt_client_context, &service, acc_chr_enable_uuid);
//                    break;
//                default:
//                    break;
//            }
//            break;
//
//        case TC_W4_CHARACTERISTIC_RESULT:
//            switch(event->type){
//                case GATT_CHARACTERISTIC_QUERY_RESULT:
//                    enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                    dump_characteristic(&enable_characteristic);
//                    break;
//                case GATT_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT; 
//                    printf("\n test client - ACC ENABLE\n");
//                    gatt_client_discover_characteristic_descriptors(&test_gatt_client_context, &enable_characteristic);
//                    break;
//                default:
//                    break;
//            }
//            break;
//        
//        case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
//            switch(event->type){
//                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
//                    dump_descriptor(event);
//                    break;
//                    
//                case GATT_QUERY_COMPLETE:
//                    tc_state = TC_W4_DISCONNECT;
//                    printf("\n\n test client - DISCONNECT ");
//                    gap_disconnect(test_gatt_client_handle);
//                    break;
//                    
//                default:
//                    break;
//            }
//            break;
//
//        default:
//            printf("Client, unhandled state %d\n", tc_state);
//            break;
//        }
//
//
//}

 void handle_gatt_client_event(le_event_t * event){
     switch(tc_state){
         case TC_W4_SERVICE_RESULT:
             switch(event->type){
                 case GATT_SERVICE_QUERY_RESULT:
                     service = ((le_service_event_t *) event)->service;
                     dump_service(&service);
                     break;
                 case GATT_QUERY_COMPLETE:
                     tc_state = TC_W4_CHARACTERISTIC_RESULT;
                     printf("\n test client - ENABLE CHARACTERISTIC for SERVICE QUERY 1: \n");
                     dump_service(&service);
                     gatt_client_discover_characteristics_for_service_by_uuid128(&test_gatt_client_context, &service, acc_chr_enable_uuid);
                     break;
                 default:
                     break;
             }
             break;

         case TC_W4_CHARACTERISTIC_RESULT:
             switch(event->type){
                 case GATT_CHARACTERISTIC_QUERY_RESULT:
                     enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                     dump_characteristic(&enable_characteristic);
                     break;
                 case GATT_QUERY_COMPLETE:
                     tc_state = TC_W4_ACC_ENABLE;
                     printf("\n test client - ACC ENABLE\n");
                     gatt_client_write_value_of_characteristic(&test_gatt_client_context, enable_characteristic.value_handle, 1, acc_enable);
                     break;
                 default:
                     break;
             }
             break;

         case TC_W4_ACC_ENABLE:
             tc_state = TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT;
             printf("\n test client - CLIENT CONFIG CHARACTERISTIC for SERVICE QUERY with UUID");
             printUUID128(service.uuid128);
             printf("\n");
             gatt_client_discover_characteristics_for_service_by_uuid128(&test_gatt_client_context, &service, acc_chr_client_config_uuid);
             break;
         case TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT:
             switch(event->type){
                 case GATT_CHARACTERISTIC_QUERY_RESULT:
                     config_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                     dump_characteristic(&config_characteristic);
                     break;
                 case GATT_QUERY_COMPLETE:
                     tc_state = TC_W4_ACC_DATA;
                     printf("\n test client - ACC Client Configuration\n");
                     gatt_client_write_client_characteristic_configuration(&test_gatt_client_context, &config_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                     break;
                 default:
                     break;
             }
             break;
         case TC_W4_ACC_DATA:
             printf("ACC Client Data: ");
             if ( event->type != GATT_NOTIFICATION && event->type != GATT_INDICATION ) break;
             dump_characteristic_value((le_characteristic_value_event_t *) event);
             break;
         default:
             printf("Client, unhandled state %d\n", tc_state);
             break;
     }
 }

static void handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size){
    le_command_status_t status;
    
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch (packet[0]) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("test client - DISCONNECTED\n");
            break;
        case GAP_LE_ADVERTISING_REPORT:
            if (tc_state != TC_W4_SCAN_RESULT) return;
            printf("test client - SCAN ACTIVE\n");
            ad_event_t ad_event;
            int pos = 2;
            ad_event.event_type = packet[pos++];
            ad_event.address_type = packet[pos++];
            memcpy(ad_event.address, &packet[pos], 6);
            
            pos += 6;
            ad_event.rssi = packet[pos++];
            ad_event.length = packet[pos++];
            ad_event.data = &packet[pos];
            pos += ad_event.length;
            dump_ad_event(&ad_event);
            
            test_device_addr_type = ad_event.address_type;
            bd_addr_t found_device_addr;
            
            memcpy(found_device_addr, ad_event.address, 6);
            swapX(ad_event.address, found_device_addr, 6);
            
            if (memcmp(&found_device_addr, &sensor_tag1_addr, 6) == 0
                || memcmp(&found_device_addr, &sensor_tag2_addr, 6) == 0) {
                    
                    tc_state = TC_W4_CONNECT;
                    le_central_stop_scan();
                    le_central_connect(found_device_addr, test_device_addr_type);
            }
            break;
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (packet[2] == HCI_STATE_WORKING) {
                printf("BTstack activated, get started!\n");
                tc_state = TC_W4_SCAN_RESULT;
                le_central_start_scan(); 
            }
            break;
        case HCI_EVENT_LE_META:
            switch (packet[2]) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    if (tc_state != TC_W4_CONNECT) return;
                    tc_state = TC_W4_SERVICE_RESULT;
                    printf("\n test client - CONNECTED, query ACC service\n");
                    test_gatt_client_handle = READ_BT_16(packet, 4);
                    gatt_client_start(&test_gatt_client_context, test_gatt_client_handle);
                    
                    // let's start
                    gatt_client_discover_primary_services_by_uuid128(&test_gatt_client_context, acc_service_uuid);
                    break;
                }
                default:
                    break;
            }
            break;
            
        case DAEMON_EVENT_HCI_PACKET_SENT:
            switch(tc_state){
                case TC_W2_WRITE_WITHOUT_RESPONSE:
                    status = gatt_client_write_value_of_characteristic_without_response(&test_gatt_client_context, characteristic.value_handle, 1, chr_short_value);
                    if (status != BLE_PERIPHERAL_OK) break;
                    tc_state = TC_W4_READ_LONG_RESULT;
                    gatt_client_read_long_value_of_characteristic(&test_gatt_client_context, &characteristic);
                default:
                    break;
            } 

        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    l2cap_init();
    ble_client_init();
    ble_client_register_gatt_client_event_packet_handler(handle_gatt_client_event);
    ble_client_register_hci_packet_handler(handle_hci_event);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}





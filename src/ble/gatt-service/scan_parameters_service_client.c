/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "scan_parameters_service_client.c"

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "scan_parameters_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

#include "ble/gatt_service_client.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

static btstack_linked_list_t clients;
static uint16_t scan_parameters_service_cid_counter = 0;
static uint16_t scan_parameters_service_scan_window = 0;
static uint16_t scan_parameters_service_scan_interval = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void scan_parameters_service_send_next_query(void * context);
static btstack_context_callback_registration_t scan_parameters_service_handle_can_send_now;

static uint16_t scan_parameters_service_get_next_cid(void){
    if (scan_parameters_service_cid_counter == 0xffff) {
        scan_parameters_service_cid_counter = 1;
    } else {
        scan_parameters_service_cid_counter++;
    }
    return scan_parameters_service_cid_counter;
}

static uint8_t scan_parameters_client_request_send_gatt_query(scan_parameters_service_client_t * client){
    scan_parameters_service_handle_can_send_now.context = (void *) (uintptr_t)client->cid;
    uint8_t status = gatt_client_request_to_send_gatt_query(&scan_parameters_service_handle_can_send_now, client->con_handle);
    if (status != ERROR_CODE_SUCCESS){
        if (client->state >= SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE){
            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_IDLE;
        }
    }
    return status;
}

static scan_parameters_service_client_t * scan_parameters_service_create_client(hci_con_handle_t con_handle, uint16_t cid){
    scan_parameters_service_client_t * client = btstack_memory_scan_parameters_service_client_get();
    if (!client){
        log_error("Not enough memory to create client");
        return NULL;
    }

    client->cid = cid;
    client->con_handle = con_handle;
    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_IDLE;

    client->start_handle = 0;
    client->end_handle = 0;

    client->scan_interval_window_value_handle = 0;
    client->scan_interval_window_value_update = false;
    btstack_linked_list_add(&clients, (btstack_linked_item_t *) client);
    return client;
}

static void scan_parameters_service_finalize_client(scan_parameters_service_client_t * client){
    gatt_client_stop_listening_for_characteristic_value_updates(&client->notification_listener);
    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    btstack_memory_scan_parameters_service_client_free(client); 
}

static scan_parameters_service_client_t * scan_parameters_service_get_client_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        scan_parameters_service_client_t * client = (scan_parameters_service_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->con_handle != con_handle) continue;
        return client;
    }
    return NULL;
}

static scan_parameters_service_client_t * scan_parameters_service_get_client_for_cid(uint16_t scan_parameters_service_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        scan_parameters_service_client_t * client = (scan_parameters_service_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->cid != scan_parameters_service_cid) continue;
        return client;
    }
    return NULL;
}

static void scan_parameters_service_emit_connection_established(scan_parameters_service_client_t * client, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_CONNECTED;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = status;
    (*client->client_handler)(HCI_EVENT_GATTSERVICE_META, 0, event, sizeof(event));
}

static void scan_parameters_service_emit_disconnected(btstack_packet_handler_t packet_handler, uint16_t cid){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_DISCONNECTED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    btstack_assert(pos == sizeof(event));
    (*packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void handle_notification_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) return;

    scan_parameters_service_client_t * client = scan_parameters_service_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
    btstack_assert(client != NULL);
    client->scan_interval_window_value_update = true;
    scan_parameters_client_request_send_gatt_query(client);
}

static void scan_parameters_service_send_next_query(void * context){
    uint16_t cid = (uint16_t)(uintptr_t)context;
    scan_parameters_service_client_t * client = scan_parameters_service_get_client_for_cid(cid);
    if (client == NULL){
        return;
    }

    uint8_t att_status;
    gatt_client_service_t service;

    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\n\nQuery Services:\n");
#endif
            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            att_status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_SCAN_PARAMETERS);
            // TODO handle status
            UNUSED(att_status);
            break;
        
        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\n\nQuery Characteristics of service\n");
#endif
            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            service.start_group_handle = client->start_handle;
            service.end_group_handle = client->end_handle;
            att_status = gatt_client_discover_characteristics_for_service(
                handle_gatt_client_event, client->con_handle, &service);
            // TODO handle status
            UNUSED(att_status);
            break;

        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED:
            if (client->scan_interval_window_value_update){
                client->scan_interval_window_value_update = false;
                
#ifdef ENABLE_TESTING_SUPPORT
                printf("\n\nUpdate - interval %d, window %d:\n", scan_parameters_service_scan_interval, scan_parameters_service_scan_window);
#endif
                uint8_t value[4];
                little_endian_store_16(value, 0, scan_parameters_service_scan_interval);
                little_endian_store_16(value, 2, scan_parameters_service_scan_window);

                att_status = gatt_client_write_value_of_characteristic_without_response(client->con_handle, client->scan_interval_window_value_handle, 4, value);
                // TODO handle status
                UNUSED(att_status);
            }
            break;
#ifdef ENABLE_TESTING_SUPPORT
        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CCC:
            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CCC;

            characteristic.value_handle = client->scan_refresh_value_handle;
            characteristic.end_handle   =  client->scan_refresh_end_handle;

            // result in GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT
            att_status = gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, client->con_handle, &characteristic);
            UNUSED(att_status);
            break;
#endif
        case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_CONFIGURE_NOTIFICATIONS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Notification configuration enable ");
#endif

            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_NOTIFICATIONS_CONFIGURED;

            characteristic.value_handle = client->scan_refresh_value_handle;
            characteristic.end_handle = client->scan_refresh_end_handle;
            characteristic.properties = client->scan_refresh_properties;
            
            // end of write marked in GATT_EVENT_QUERY_COMPLETE

            att_status = gatt_client_write_client_characteristic_configuration(&handle_gatt_client_event, client->con_handle, &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
            
            if (att_status == ERROR_CODE_SUCCESS){
                gatt_client_listen_for_characteristic_value_updates(
                            &client->notification_listener, 
                            &handle_notification_event, client->con_handle, &characteristic);
            } 
            client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED;
            break;
        default:
            break;
    }
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);     
    UNUSED(size);        
    
    scan_parameters_service_client_t * client = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    uint8_t status;

#ifdef ENABLE_TESTING_SUPPORT
    gatt_client_characteristic_descriptor_t characteristic_descriptor;
#endif

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = scan_parameters_service_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->state != SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT) {
                scan_parameters_service_emit_connection_established(client, GATT_CLIENT_IN_WRONG_STATE);  
                scan_parameters_service_finalize_client(client);      
                return;
            }

            gatt_event_service_query_result_get_service(packet, &service);
            client->start_handle = service.start_group_handle;
            client->end_handle = service.end_group_handle;
#ifdef ENABLE_TESTING_SUPPORT
            printf("ScS Service: start handle 0x%04X, end handle 0x%04X\n", client->start_handle, client->end_handle);
#endif
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = scan_parameters_service_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            // found scan_interval_window_value_handle, check att_status
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_SCAN_INTERVAL_WINDOW:
                    client->scan_interval_window_value_handle = characteristic.value_handle;

#ifdef ENABLE_TESTING_SUPPORT
                    printf("ScS Scan Interval Characteristic:  \n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X\n", 
                        characteristic.start_handle, 
                        characteristic.properties, 
                        characteristic.value_handle, characteristic.uuid16);
#endif
                    break;
                case ORG_BLUETOOTH_CHARACTERISTIC_SCAN_REFRESH:
                    client->scan_refresh_value_handle = characteristic.value_handle;
                    client->scan_refresh_end_handle = characteristic.end_handle;
                    client->scan_refresh_properties = characteristic.properties;

#ifdef ENABLE_TESTING_SUPPORT
                    printf("ScS Scan Refresh Characteristic:  \n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X\n", 
                        characteristic.start_handle, 
                        characteristic.properties, 
                        characteristic.value_handle, characteristic.uuid16);
#endif
                    break;
                default:
                    break;
            }
            break;

#ifdef ENABLE_TESTING_SUPPORT    
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            client = scan_parameters_service_get_client_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                printf("    Client Characteristic Configuration Descriptor: Handle 0x%04X, UUID 0x%04X\n", 
                    characteristic_descriptor.handle,
                    characteristic_descriptor.uuid16);
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            client = scan_parameters_service_get_client_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            printf("    Received CCC value: ");
            printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
            break;
#endif

        case GATT_EVENT_QUERY_COMPLETE:
            client = scan_parameters_service_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);
            
            status = gatt_client_att_status_to_error_code(gatt_event_query_complete_get_att_status(packet));
            
            switch (client->state){
                case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
                    if (status != ERROR_CODE_SUCCESS){
                        scan_parameters_service_emit_connection_established(client, status);
                        scan_parameters_service_finalize_client(client);      
                        return;  
                    }
                    
                    if (client->start_handle != 0){
                        client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC;
                        break;   
                    }
                    
                    scan_parameters_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);  
                    scan_parameters_service_finalize_client(client);
                    return;

                case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                    if (status != ERROR_CODE_SUCCESS){
                        scan_parameters_service_emit_connection_established(client, status);
                        scan_parameters_service_finalize_client(client);      
                        break;  
                    }
                    if (client->scan_interval_window_value_handle == 0){
                        scan_parameters_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);  
                        scan_parameters_service_finalize_client(client);
                        return;   
                    }
#ifdef ENABLE_TESTING_SUPPORT
                    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CCC;
#else
                    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED;
                    client->scan_interval_window_value_update = true;
                    scan_parameters_service_emit_connection_established(client, ERROR_CODE_SUCCESS);  
#endif
                    break;

#ifdef ENABLE_TESTING_SUPPORT
                case SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CCC:
                    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED;
                    client->scan_interval_window_value_update = true;
                    scan_parameters_service_emit_connection_established(client, ERROR_CODE_SUCCESS);  
                    break;
#endif
                default:
                    break;
            }
            if (client != NULL){
                scan_parameters_client_request_send_gatt_query(client);
            }
            break;
        default:
            break;
    }


}

static void handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); // ok: only hci events
    UNUSED(channel);     // ok: there is no channel
    UNUSED(size);        // ok: fixed format events read from HCI buffer

    hci_con_handle_t con_handle;
    scan_parameters_service_client_t * client;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            client = scan_parameters_service_get_client_for_con_handle(con_handle);
            if (client != NULL){
                // emit disconnected
                btstack_packet_handler_t packet_handler = client->client_handler;
                uint16_t cid = client->cid;
                scan_parameters_service_emit_disconnected(packet_handler, cid);
                // finalize
                scan_parameters_service_finalize_client(client);
            }
            break;
        default:
            break;
    }
}

void scan_parameters_service_client_set(uint16_t scan_interval, uint16_t scan_window){
    scan_parameters_service_scan_interval = scan_interval; 
    scan_parameters_service_scan_window = scan_window; 

    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        scan_parameters_service_client_t * client = (scan_parameters_service_client_t*) btstack_linked_list_iterator_next(&it);
        client->scan_interval_window_value_update = true;
        scan_parameters_client_request_send_gatt_query(client);
    }
}

uint8_t scan_parameters_service_client_enable_notifications(uint16_t scan_parameters_service_cid){
    scan_parameters_service_client_t * client = scan_parameters_service_get_client_for_cid(scan_parameters_service_cid);
    
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (client->state != SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_CONFIGURE_NOTIFICATIONS;
    return scan_parameters_client_request_send_gatt_query(client);
}

uint8_t scan_parameters_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint16_t * scan_parameters_service_cid){
    btstack_assert(packet_handler != NULL);

    scan_parameters_service_client_t * client = scan_parameters_service_get_client_for_con_handle(con_handle);
    if (client != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = scan_parameters_service_get_next_cid();
    if (scan_parameters_service_cid != NULL) {
        *scan_parameters_service_cid = cid;
    }

    client = scan_parameters_service_create_client(con_handle, cid);
    if (client == NULL) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    client->client_handler = packet_handler; 
    client->state = SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    return scan_parameters_client_request_send_gatt_query(client);
}

uint8_t scan_parameters_service_client_disconnect(uint16_t scan_parameters_service_cid){
    scan_parameters_service_client_t * client = scan_parameters_service_get_client_for_cid(scan_parameters_service_cid);
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connections
    scan_parameters_service_finalize_client(client);
    return ERROR_CODE_SUCCESS;
}

void scan_parameters_service_client_init(void){
    hci_event_callback_registration.callback = &handle_hci_event;
    hci_add_event_handler(&hci_event_callback_registration);
    scan_parameters_service_handle_can_send_now.callback = &scan_parameters_service_send_next_query;
}

void scan_parameters_service_client_deinit(void){
}


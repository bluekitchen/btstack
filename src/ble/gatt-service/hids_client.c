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

#define BTSTACK_FILE__ "hids_client.c"

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#include "ble/gatt-service/hids_client.h"

#include "btstack_memory.h"
#include "ble/att_db.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "ble/sm.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

static btstack_linked_list_t clients;
static uint16_t hids_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t hids_get_next_cid(void){
    if (hids_cid_counter == 0xffff) {
        hids_cid_counter = 1;
    } else {
        hids_cid_counter++;
    }
    return hids_cid_counter;
}

static hids_client_t * hids_create_client(hci_con_handle_t con_handle, uint16_t cid){
    hids_client_t * client = btstack_memory_hids_client_get();
    if (!client){
        log_error("Not enough memory to create client");
        return NULL;
    }
    client->state = HIDS_CLIENT_STATE_IDLE;
    client->cid = cid;
    client->con_handle = con_handle;
    
    btstack_linked_list_add(&clients, (btstack_linked_item_t *) client);
    return client;
}

static void hids_finalize_client(hids_client_t * client){
    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    btstack_memory_hids_client_free(client); 
}

static hids_client_t * hids_get_client_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        hids_client_t * client = (hids_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->con_handle != con_handle) continue;
        return client;
    }
    return NULL;
}

static hids_client_t * hids_get_client_for_cid(uint16_t hids_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        hids_client_t * client = (hids_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->cid != hids_cid) continue;
        return client;
    }
    return NULL;
}

static void hids_emit_connection_established(hids_client_t * client, uint8_t status){
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = client->protocol_mode;
    event[pos++] = client->num_instances;
    (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static void hids_run_for_client(hids_client_t * client){
    uint8_t att_status;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case HIDS_CLIENT_STATE_W2_QUERY_SERVICE:
            client->state = HIDS_CLIENT_STATE_W4_SERVICE_RESULT;
            att_status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
            // TODO handle status
            UNUSED(att_status);
            break;
        
        case HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC:
            client->state = HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = client->services[client->service_index].start_handle;
            service.end_group_handle = client->services[client->service_index].end_handle;
            att_status = gatt_client_discover_characteristics_for_service(&handle_gatt_client_event, client->con_handle, &service);
            
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_ENABLE_KEYBOARD:
            client->state = HIDS_CLIENT_STATE_W4_KEYBOARD_ENABLED;
            characteristic.value_handle = client->boot_keyboard_input_value_handle;
            characteristic.end_handle = client->boot_keyboard_input_end_handle;
            characteristic.properties = client->boot_keyboard_input_properties;
            att_status = gatt_client_write_client_characteristic_configuration(&handle_gatt_client_event, client->con_handle, &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);

            if (att_status != ERROR_CODE_SUCCESS){
                client->boot_keyboard_input_value_handle = 0;
            }
            break;

        case HIDS_CLIENT_STATE_W2_ENABLE_MOUSE:
            client->state = HIDS_CLIENT_STATE_W4_MOUSE_ENABLED;
            characteristic.value_handle = client->boot_mouse_input_value_handle;
            characteristic.end_handle = client->boot_mouse_input_end_handle;
            characteristic.properties = client->boot_mouse_input_properties;
            att_status = gatt_client_write_client_characteristic_configuration(&handle_gatt_client_event, client->con_handle, &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);

            if (att_status != ERROR_CODE_SUCCESS){
                client->boot_mouse_input_value_handle = 0;
            }
            break;
        
        case HIDS_CLIENT_STATE_W2_SET_PROTOCOL_MODE:
            client->state = HIDS_CLIENT_STATE_W4_SET_PROTOCOL_MODE;
            att_status = gatt_client_write_value_of_characteristic_without_response(client->con_handle, client->protocol_mode_value_handle, 1, (uint8_t *)&client->required_protocol_mode);
            UNUSED(att_status);
            
            client->protocol_mode = client->required_protocol_mode;
            client->state = HIDS_CLIENT_STATE_CONNECTED;
            hids_emit_connection_established(client, ERROR_CODE_SUCCESS); 
            break;
        
        default:
            break;
    }
}

static void hids_client_setup_report_event(hids_client_t * client, uint8_t report_id, uint8_t *buffer, uint16_t report_len){
    uint16_t pos = 0;
    buffer[pos++] = HCI_EVENT_GATTSERVICE_META;
    pos++;  // skip len
    buffer[pos++] = GATTSERVICE_SUBEVENT_HID_REPORT;
    little_endian_store_16(buffer, pos, client->cid);
    pos += 2;
    buffer[pos++] = report_id;
    little_endian_store_16(buffer, pos, report_len);
    pos += 2;
    buffer[1] = pos + report_len - 2;
}

static void handle_boot_keyboard_hid_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) return;

    hids_client_t * client = hids_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
    btstack_assert(client != NULL);

    uint8_t * in_place_event = packet;
    hids_client_setup_report_event(client, HID_BOOT_MODE_KEYBOARD_ID, in_place_event, gatt_event_notification_get_value_length(packet));
    (*client->client_handler)(HCI_EVENT_PACKET, client->cid, in_place_event, size);
}

static void handle_boot_mouse_hid_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) return;

    hids_client_t * client = hids_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
    btstack_assert(client != NULL);

    uint8_t * in_place_event = packet;
    hids_client_setup_report_event(client, HID_BOOT_MODE_MOUSE_ID, in_place_event, gatt_event_notification_get_value_length(packet));
    (*client->client_handler)(HCI_EVENT_PACKET, client->cid, in_place_event, size);
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);     
    UNUSED(size);

    hids_client_t * client = NULL;
    uint8_t att_status;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    
    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = hids_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->state != HIDS_CLIENT_STATE_W4_SERVICE_RESULT) {
                hids_emit_connection_established(client, GATT_CLIENT_IN_WRONG_STATE);  
                hids_finalize_client(client);       
                break;
            }

            if (client->num_instances < MAX_NUM_HID_SERVICES){
                gatt_event_service_query_result_get_service(packet, &service);
                client->services[client->num_instances].start_handle = service.start_group_handle;
                client->services[client->num_instances].end_handle = service.end_group_handle;
            } 
            client->num_instances++;
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = hids_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT:
                    client->boot_keyboard_input_value_handle = characteristic.value_handle;
                    client->boot_keyboard_input_end_handle = characteristic.end_handle;
                    client->boot_keyboard_input_properties = characteristic.properties;
                    break;
                case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT:
                    client->boot_mouse_input_value_handle = characteristic.value_handle;
                    client->boot_mouse_input_end_handle = characteristic.end_handle;
                    client->boot_mouse_input_properties = characteristic.properties;
                    break;
                case ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE:
                    client->protocol_mode_value_handle = characteristic.value_handle;
                    break;
                default:
                    break;
            }
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            client = hids_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);
            
            att_status = gatt_event_query_complete_get_att_status(packet);
            
            switch (client->state){
                case HIDS_CLIENT_STATE_W4_SERVICE_RESULT:
                    if (att_status != ATT_ERROR_SUCCESS){
                        hids_emit_connection_established(client, att_status);  
                        hids_finalize_client(client);
                        break;  
                    }

                    if (client->num_instances == 0){
                        hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
                        hids_finalize_client(client);
                        break;   
                    }

                    if (client->num_instances > MAX_NUM_HID_SERVICES) {
                        log_info("%d hid services found, only first %d can be stored, increase MAX_NUM_HID_SERVICES", client->num_instances, MAX_NUM_HID_SERVICES);
                        client->num_instances = MAX_NUM_HID_SERVICES;
                    }
                    
                    client->state = HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC;
                    client->service_index = 0;
                    break;
                
                case HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                    if (att_status != ATT_ERROR_SUCCESS){
                        hids_emit_connection_established(client, att_status);  
                        hids_finalize_client(client);
                        break;  
                    }
                    
                    switch (client->required_protocol_mode){
                        case HID_PROTOCOL_MODE_BOOT:
                            if (client->boot_keyboard_input_value_handle != 0){
                                client->state = HIDS_CLIENT_STATE_W2_ENABLE_KEYBOARD;
                                break;
                            } 
                            if (client->boot_mouse_input_value_handle != 0){
                                client->state = HIDS_CLIENT_STATE_W2_ENABLE_MOUSE;
                                break;
                            }
                            hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);  
                            hids_finalize_client(client);
                            break;
                        default:
                            break;
                    }
                    break;

                case HIDS_CLIENT_STATE_W4_KEYBOARD_ENABLED:
                    if (client->boot_keyboard_input_value_handle != 0){
                        // setup listener
                        characteristic.value_handle = client->boot_keyboard_input_value_handle;
                        gatt_client_listen_for_characteristic_value_updates(&client->boot_keyboard_notifications, &handle_boot_keyboard_hid_event, client->con_handle, &characteristic);
                    } else {
                        if (client->boot_mouse_input_value_handle == 0){
                            hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);  
                            hids_finalize_client(client);
                            break;
                        }
                    }
                    
                    if (client->boot_mouse_input_value_handle != 0){
                        client->state = HIDS_CLIENT_STATE_W2_ENABLE_MOUSE;
                        break;
                    } 
                    
                    // set protocol
                    if (client->protocol_mode_value_handle != 0){
                        client->state = HIDS_CLIENT_STATE_W2_SET_PROTOCOL_MODE;
                    } else {
                        client->protocol_mode = HID_PROTOCOL_MODE_BOOT;
                        client->state = HIDS_CLIENT_STATE_CONNECTED;
                        hids_emit_connection_established(client, ERROR_CODE_SUCCESS); 
                    }
                    hids_run_for_client(client);
                    break;
                
                case HIDS_CLIENT_STATE_W4_MOUSE_ENABLED:
                    if (client->boot_mouse_input_value_handle != 0){
                        // setup listener
                        characteristic.value_handle = client->boot_mouse_input_value_handle;
                        gatt_client_listen_for_characteristic_value_updates(&client->boot_mouse_notifications, &handle_boot_mouse_hid_event, client->con_handle, &characteristic);
                    } else {
                        if (client->boot_keyboard_input_value_handle == 0){
                            hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);  
                            hids_finalize_client(client);
                            break;
                        }
                    }

                    if (client->protocol_mode_value_handle != 0){
                        client->state = HIDS_CLIENT_STATE_W2_SET_PROTOCOL_MODE;
                    } else {
                        client->protocol_mode = HID_PROTOCOL_MODE_BOOT;
                        client->state = HIDS_CLIENT_STATE_CONNECTED;
                        hids_emit_connection_established(client, ERROR_CODE_SUCCESS); 
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    if (client != NULL){
        hids_run_for_client(client);
    }
}

uint8_t hids_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, hid_protocol_mode_t protocol_mode, uint16_t * hids_cid){
    btstack_assert(packet_handler != NULL);
    
    hids_client_t * client = hids_get_client_for_con_handle(con_handle);
    if (client != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = hids_get_next_cid();
    if (hids_cid != NULL) {
        *hids_cid = cid;
    }

    client = hids_create_client(con_handle, cid);
    if (client == NULL) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    client->required_protocol_mode = protocol_mode;
    client->client_handler = packet_handler; 
    client->state = HIDS_CLIENT_STATE_W2_QUERY_SERVICE;

    hids_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t hids_client_disconnect(uint16_t hids_cid){
    hids_client_t * client = hids_get_client_for_cid(hids_cid);
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connection
    hids_finalize_client(client);
    return ERROR_CODE_SUCCESS;
}

void hids_client_init(void){}

void hids_client_deinit(void){}

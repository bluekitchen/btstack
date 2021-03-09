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

#define BTSTACK_FILE__ "battery_service_client.c"

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>


#include "ble/gatt-service/battery_service_client.h"


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

#define BATTERY_SERVICE_INVALID_INDEX 0xFF

static btstack_linked_list_t clients;
static uint16_t battery_service_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void battery_service_poll_timer_start(battery_service_client_t * client);

static uint16_t battery_service_get_next_cid(void){
    if (battery_service_cid_counter == 0xffff) {
        battery_service_cid_counter = 1;
    } else {
        battery_service_cid_counter++;
    }
    return battery_service_cid_counter;
}

static battery_service_client_t * battery_service_create_client(hci_con_handle_t con_handle, uint16_t cid){
    battery_service_client_t * client = btstack_memory_battery_service_client_get();
    if (!client){
        log_error("Not enough memory to create client");
        return NULL;
    }
    client->cid = cid;
    client->con_handle = con_handle;
    client->poll_interval_ms = 0;
    client->num_instances = 0;
    client->battery_service_index = 0;
    client->poll_bitmap = 0;
    client->need_poll_bitmap = 0;
    client->polled_service_index = BATTERY_SERVICE_INVALID_INDEX;
    client->state = BATTERY_SERVICE_CLIENT_STATE_IDLE;
    
    btstack_linked_list_add(&clients, (btstack_linked_item_t *) client);
    return client;
}

static void battery_service_finalize_client(battery_service_client_t * client){
    // stop listening
    uint8_t i;
    for (i = 0; i < client->num_instances; i++){
        gatt_client_stop_listening_for_characteristic_value_updates(&client->services[i].notification_listener);
    }

    // remove timer
    btstack_run_loop_remove_timer(&client->poll_timer);

    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    btstack_memory_battery_service_client_free(client); 
}

static battery_service_client_t * battery_service_get_client_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_client_t * client = (battery_service_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->con_handle != con_handle) continue;
        return client;
    }
    return NULL;
}

static battery_service_client_t * battery_service_get_client_for_cid(uint16_t battery_service_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        battery_service_client_t * client = (battery_service_client_t *)btstack_linked_list_iterator_next(&it);
        if (client->cid != battery_service_cid) continue;
        return client;
    }
    return NULL;
}

static void battery_service_emit_connection_established(battery_service_client_t * client, uint8_t status){
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = client->num_instances;
    event[pos++] = client->poll_bitmap;

    (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void battery_service_emit_battery_level(battery_service_client_t * client, uint16_t value_handle, uint8_t att_status, uint8_t battery_level){
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;

    uint8_t i;
    for (i = 0; i < client->num_instances; i++){
        if (value_handle == client->services[i].value_handle){
            event[pos++] = i;
            event[pos++] = att_status;
            event[pos++] = battery_level;
            (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
    }
}

static void battery_service_run_for_client(battery_service_client_t * client){
    uint8_t status;
    uint8_t i;
    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case BATTERY_SERVICE_CLIENT_STATE_CONNECTED:
            for (i = 0; i < client->num_instances; i++){
                if ( ((client->need_poll_bitmap >> i) & 0x01) == 0x01 ){
                    // clear bit of polled service
                    client->need_poll_bitmap &= ~(1u << i);
                    client->polled_service_index = i;
    
                    // poll value of characteristic
                    characteristic.value_handle = client->services[i].value_handle;
                    characteristic.properties   = client->services[i].properties;
                    characteristic.end_handle   = client->services[i].end_handle;
                    gatt_client_read_value_of_characteristic(handle_gatt_client_event, client->con_handle, &characteristic);
                    break;
                }
            }
            break;

        case BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:   
            client->state = BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
            // TODO handle status
            break;

        case BATTERY_SERVICE_CLIENT_STATE_W4_REGISTER_NOTIFICATION:
            // if there are services without notification, register pool timer, 
            // othervise register for notifications
            characteristic.value_handle = client->services[client->battery_service_index].value_handle;
            characteristic.properties   = client->services[client->battery_service_index].properties;
            characteristic.end_handle   = client->services[client->battery_service_index].end_handle;

            status = gatt_client_write_client_characteristic_configuration(
                handle_gatt_client_event, 
                client->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                
            // notification supported, register for value updates
            if (status == ERROR_CODE_SUCCESS){
                gatt_client_listen_for_characteristic_value_updates(
                    &client->services[client->battery_service_index].notification_listener, 
                    handle_gatt_client_event, 
                    client->con_handle, 
                    &characteristic);
            } else {
                client->poll_bitmap |= 1u << client->battery_service_index;
            }
            
            if (client->battery_service_index + 1 < client->num_instances){
                client->battery_service_index++;
            } else {
                client->state = BATTERY_SERVICE_CLIENT_STATE_CONNECTED;
                battery_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
                
                if (client->poll_interval_ms > 0){
                    client->need_poll_bitmap = client->poll_bitmap;
                    battery_service_poll_timer_start(client);
                }
            }
            break;
        default:
            break;
    }
}

static void battery_service_poll_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t battery_service_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);

    battery_service_client_t * client =  battery_service_get_client_for_cid(battery_service_cid);
    btstack_assert(client != NULL);

    client->need_poll_bitmap = client->poll_bitmap;
    battery_service_poll_timer_start(client);
    battery_service_run_for_client(client);
}

static void battery_service_poll_timer_start(battery_service_client_t * client){
    btstack_run_loop_set_timer_handler(&client->poll_timer,  battery_service_poll_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&client->poll_timer, (void *)(uintptr_t)client->cid);

    btstack_run_loop_set_timer(&client->poll_timer, client->poll_interval_ms); 
    btstack_run_loop_add_timer(&client->poll_timer);
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);     

    battery_service_client_t * client;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    uint8_t status;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = battery_service_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->state != BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT) {
                battery_service_emit_connection_established(client, GATT_CLIENT_IN_WRONG_STATE);  
                battery_service_finalize_client(client);       
                break;
            }

            if (client->num_instances < MAX_NUM_BATTERY_SERVICES){
                gatt_event_service_query_result_get_service(packet, &service);
                client->services[client->num_instances].start_handle = service.start_group_handle;
                client->services[client->num_instances].end_handle = service.end_group_handle;
            } 
            client->num_instances++;
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = battery_service_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            if (client->battery_service_index < client->num_instances){
                client->services[client->battery_service_index].value_handle = characteristic.value_handle;
                client->services[client->battery_service_index].properties = characteristic.properties;
            }
            break;

        case GATT_EVENT_NOTIFICATION:
            // ignore if wrong (event type 1, length 1, handle 2, value handle 2, value length 2, value 1)
            if (size != 9) break;
            
            client = battery_service_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
            btstack_assert(client != NULL);

            if (gatt_event_notification_get_value_length(packet) != 1) break;

            battery_service_emit_battery_level(client, 
                gatt_event_notification_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_notification_get_value(packet)[0]);
            break;
        
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            // ignore if wrong (event type 1, length 1, handle 2, value handle 2, value length 2, value 1)
            if (size != 9) break;

            client = battery_service_get_client_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(client != NULL);                
        
            if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 1) break;

            battery_service_emit_battery_level(client, 
                gatt_event_characteristic_value_query_result_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_characteristic_value_query_result_get_value(packet)[0]);

            // call run for client function to trigger next poll
            battery_service_run_for_client(client);
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            client = battery_service_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);
            
            status = gatt_event_query_complete_get_att_status(packet);
            
            switch (client->state){
                case GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED:
                    if (client->polled_service_index != BATTERY_SERVICE_INVALID_INDEX){
                        if (status != ATT_ERROR_SUCCESS){
                            battery_service_emit_battery_level(client, client->services[client->polled_service_index].value_handle, status, 0);
                        }
                        client->polled_service_index = BATTERY_SERVICE_INVALID_INDEX;
                    }
                    break;
                case BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
                    if (status != ATT_ERROR_SUCCESS){
                        battery_service_emit_connection_established(client, status);  
                        battery_service_finalize_client(client);
                        break;  
                    }

                    if (client->num_instances == 0){
                        battery_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
                        battery_service_finalize_client(client);
                        break;   
                    }

                    if (client->num_instances > MAX_NUM_BATTERY_SERVICES) {
                        log_info("%d battery services found, only first %d can be stored, increase MAX_NUM_BATTERY_SERVICES", client->num_instances, MAX_NUM_BATTERY_SERVICES);
                        client->num_instances = MAX_NUM_BATTERY_SERVICES;
                    }
                    
                    client->state = BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
                    client->battery_service_index = 0;
                    gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                        handle_gatt_client_event, 
                        client->con_handle, 
                        client->services[client->battery_service_index].start_handle, 
                        client->services[client->battery_service_index].end_handle, 
                        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
                    break;

                case BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                    if (status != ATT_ERROR_SUCCESS){
                        battery_service_emit_connection_established(client, status);  
                        battery_service_finalize_client(client);
                        break;  
                    }

                    // check if there is another service to query
                    if ((client->battery_service_index + 1) < client->num_instances){
                        client->battery_service_index++;
                        gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                            handle_gatt_client_event, 
                            client->con_handle, 
                            client->services[client->battery_service_index].start_handle, 
                            client->services[client->battery_service_index].end_handle, 
                            ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
                        break;
                    }

                    // we are done with quering all services, wait for notification registration
                    // to send connection established event
                    client->state = BATTERY_SERVICE_CLIENT_STATE_W4_REGISTER_NOTIFICATION;
                    client->battery_service_index = 0;

                    battery_service_run_for_client(client);
                    break;

                default:
                    break;

            }
            break;


        default:
            break;
    }
}


uint8_t battery_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint32_t poll_interval_ms, uint16_t * battery_service_cid){
    btstack_assert(packet_handler != NULL);
    
    battery_service_client_t * client = battery_service_get_client_for_con_handle(con_handle);
    if (client != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = battery_service_get_next_cid();
    if (battery_service_cid != NULL) {
        *battery_service_cid = cid;
    }

    client = battery_service_create_client(con_handle, cid);
    if (client == NULL) {
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    client->client_handler = packet_handler; 
    client->poll_interval_ms = poll_interval_ms;
    client->state = BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    battery_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_client_disconnect(uint16_t battery_service_cid){
    battery_service_client_t * client = battery_service_get_client_for_cid(battery_service_cid);
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connections
    battery_service_finalize_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_client_read_battery_level(uint16_t battery_service_cid, uint8_t service_index){
    battery_service_client_t * client = battery_service_get_client_for_cid(battery_service_cid);
    if (client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (client->state != BATTERY_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }
    if (service_index >= client->num_instances) {
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    client->need_poll_bitmap |= (1u << service_index);
    battery_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

void battery_service_client_init(void){}

void battery_service_client_deinit(void){}


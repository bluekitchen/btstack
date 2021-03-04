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


#include "battery_service_client.h"


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

#define BATTERY_LEVEL_POLL_TIMEOUT 2000

static btstack_linked_list_t clients;
static uint16_t battery_service_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

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
    client->state = BATTERY_SERVICE_IDLE;
    client->cid = cid;
    client->con_handle = con_handle;
    btstack_linked_list_add(&clients, (btstack_linked_item_t *) client);
    return client;
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

static void battery_service_emit_connection_established(battery_service_client_t * client, uint8_t status, uint8_t num_instances){
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BATTERY_SERVICE_CONNECTED;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = status;
    event[pos++] = num_instances;

    (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void battery_service_emit_battery_level(battery_service_client_t * client, uint16_t value_handle, uint8_t battery_level){
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_BATTERY_SERVICE_LEVEL;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;

    int i;
    for (i = 0; i < client->num_battery_services; i++){
        if (value_handle == client->services[i].value_handle){
            event[pos++] = i;
            event[pos++] = battery_level;
            (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
    }
}


static void battery_service_poll_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t battery_service_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);

    battery_service_client_t * client =  battery_service_get_client_for_cid(battery_service_cid);
    if (client == NULL) return;

    uint8_t i;
    for (i = 0; i < client->num_battery_services; i++){
        if (!client->services[i].notification_supported){
            gatt_client_characteristic_t characteristic;
            characteristic.value_handle = client->services[i].value_handle;
            characteristic.properties   = client->services[i].properties;
            characteristic.end_handle   = client->services[i].end_handle;
            // poll value of characteristic
            gatt_client_read_value_of_characteristic(handle_gatt_client_event, client->con_handle, &characteristic);
        }
    } 
}

static void battery_service_poll_timer_start(battery_service_client_t * client){
    btstack_run_loop_set_timer_handler(&client->poll_timer,  battery_service_poll_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&client->poll_timer, (void *)(uintptr_t)client->cid);

    btstack_run_loop_set_timer(&client->poll_timer, BATTERY_LEVEL_POLL_TIMEOUT); 
    btstack_run_loop_add_timer(&client->poll_timer);
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); // ok: only hci events
    UNUSED(channel);     // ok: there is no channel
    UNUSED(size);        // ok: fixed format events read from HCI buffer

    battery_service_client_t * client;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    uint8_t status;
    uint8_t i;
    bool register_poll_timer;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            // ignore if wrong (event type 1, length 1, handle 2, service 6)
            if (size != 10) break;

            client = battery_service_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->state != BATTERY_SERVICE_W4_SERVICE_RESULT) {
                battery_service_emit_connection_established(client, GATT_CLIENT_IN_WRONG_STATE, 0);         
                break;
            }

            if (client->num_battery_services < MAX_NUM_BATTERY_SERVICES){
                gatt_event_service_query_result_get_service(packet, &service);
                client->services[client->num_battery_services].start_handle = service.start_group_handle;
                client->services[client->num_battery_services].end_handle = service.end_group_handle;
            } 
            client->num_battery_services++;
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            // ignore if wrong (event type 1, length 1, handle 2, chr 10)
            if (size != 14) break;

            client = battery_service_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            if (client->battery_service_index < client->num_battery_services){
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
                gatt_event_characteristic_value_query_result_get_value(packet)[0]);
            break;

        case GATT_EVENT_QUERY_COMPLETE:
            client = battery_service_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);
            
            status = gatt_event_query_complete_get_att_status(packet);
            
            switch (client->state){
                case BATTERY_SERVICE_W4_SERVICE_RESULT:
                    if (status != ERROR_CODE_SUCCESS){
                        battery_service_emit_connection_established(client, status, 0);  
                        client->state = BATTERY_SERVICE_IDLE;
                        break;  
                    }

                    if (client->num_battery_services == 0){
                        battery_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE, 0);  
                        client->state = BATTERY_SERVICE_IDLE;
                        break;   
                    }

                    if (client->num_battery_services > MAX_NUM_BATTERY_SERVICES) {
                        log_info("%d battery services found, only first %d can be stored, increase MAX_NUM_BATTERY_SERVICES", client->num_battery_services, MAX_NUM_BATTERY_SERVICES);
                        client->num_battery_services = MAX_NUM_BATTERY_SERVICES;
                    }
                    
                    client->state = BATTERY_SERVICE_W4_CHARACTERISTIC_RESULT;
                    client->battery_service_index = 0;

                    gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                        handle_gatt_client_event, 
                        client->con_handle, 
                        client->services[client->battery_service_index].start_handle, 
                        client->services[client->battery_service_index].end_handle, 
                        ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
                    break;

                case BATTERY_SERVICE_W4_CHARACTERISTIC_RESULT:
                    if (status != ERROR_CODE_SUCCESS){
                        battery_service_emit_connection_established(client, status, 0);  
                        client->state = BATTERY_SERVICE_IDLE;
                        break;  
                    }

                    // check if there is another service to query
                    if (client->battery_service_index + 1 < client->num_battery_services){
                        client->battery_service_index++;
                        gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                            handle_gatt_client_event, 
                            client->con_handle, 
                            client->services[client->battery_service_index].start_handle, 
                            client->services[client->battery_service_index].end_handle, 
                            ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);
                        break;
                    }

                    // we are done with quering all services
                    battery_service_emit_connection_established(client, ERROR_CODE_SUCCESS, client->num_battery_services);
                    client->state = BATTERY_SERVICE_W4_BATTERY_DATA;

                    // if there are services without notification, register pool timer, 
                    // othervise register for notifications
                    register_poll_timer = false;

                    for (i = 0; i < client->num_battery_services; i++){
                        characteristic.value_handle = client->services[i].value_handle;
                        characteristic.properties   = client->services[i].properties;
                        characteristic.end_handle   = client->services[i].end_handle;

                        status = gatt_client_write_client_characteristic_configuration(
                            handle_gatt_client_event, 
                            client->con_handle, 
                            &characteristic, 
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                        
                        // notification supported, register for value updates
                        if (status == ERROR_CODE_SUCCESS){
                            client->services[i].notification_supported = true;
                            gatt_client_listen_for_characteristic_value_updates(
                                &client->services[i].notification_listener, 
                                handle_gatt_client_event, 
                                client->con_handle, 
                                &characteristic);
                        } else {
                            register_poll_timer = true;
                        }
                    }

                    if (register_poll_timer){
                        battery_service_poll_timer_start(client);
                    }
                    break;
                default:
                    break;

            }
            break;


        default:
            break;
    }
}

static void battery_service_run_for_client(battery_service_client_t * client){
    uint8_t status;
    switch (client->state){
        case BATTERY_SERVICE_W2_QUERY_SERVICE:   
            client->state = BATTERY_SERVICE_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
            // TODO handle status
            break;
        default:
            break;
    }
}

uint8_t battery_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t callback, uint16_t * battery_service_cid){
    btstack_assert(callback != NULL);

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

    client->client_handler = callback; 
    client->state = BATTERY_SERVICE_W2_QUERY_SERVICE;
    battery_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t battery_service_client_disconnect(uint16_t battery_service_cid){
    battery_service_client_t * client = battery_service_get_client_for_cid(battery_service_cid);
    if (client == NULL){
        return ERROR_CODE_SUCCESS;
    }

    // remove timer
    btstack_run_loop_remove_timer(&client->poll_timer);

    // stop listening
    int i;
    for (i = 0; i < client->num_battery_services; i++){
        if (client->services[i].notification_enabled){
            client->services[i].notification_enabled = false;
            gatt_client_stop_listening_for_characteristic_value_updates(&client->services[i].notification_listener);
        }
    }

    // finalize connections
    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    btstack_memory_battery_service_client_free(client);
    return ERROR_CODE_SUCCESS;
}

void battery_service_client_init(void){}

void battery_service_client_deinit(void){}


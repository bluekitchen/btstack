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

#define BTSTACK_FILE__ "battery_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>


#include "ble/gatt-service/battery_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

#define BATTERY_SERVICE_INVALID_INDEX 0xFF

static btstack_context_callback_registration_t battery_service_handle_can_send_now;

static btstack_linked_list_t clients;
static uint16_t battery_service_cid_counter = 0;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void battery_service_poll_timer_start(battery_service_client_t * client);

static uint16_t battery_service_get_next_cid(void){
    battery_service_cid_counter = btstack_next_cid_ignoring_zero(battery_service_cid_counter);
    return battery_service_cid_counter;
}


static uint8_t battery_service_client_request_send_gatt_query(battery_service_client_t * client){
    battery_service_handle_can_send_now.context = (void *)(uintptr_t)client->cid;
    return gatt_client_request_to_send_gatt_query(&battery_service_handle_can_send_now, client->con_handle);
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
    client->service_index = 0;
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

static bool battery_service_registered_notification(battery_service_client_t * client, uint16_t service_index){
    gatt_client_characteristic_t characteristic;
    // if there are services without notification, register pool timer, 
    // otherwise register for notifications
    characteristic.value_handle = client->services[service_index].value_handle;
    characteristic.properties   = client->services[service_index].properties;
    characteristic.end_handle   = client->services[service_index].end_handle;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
                &handle_gatt_client_event, 
                client->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &client->services[service_index].notification_listener, 
            &handle_gatt_client_event, 
            client->con_handle, &characteristic);
    } else {
        client->poll_bitmap |= 1u << client->service_index;
    }
    return status;
}

static bool battery_service_is_polling_needed(battery_service_client_t * client){
    return (client->poll_bitmap > 0u) && (client->poll_interval_ms > 0u);
}

static void battery_service_start_polling(battery_service_client_t * client){
    client->need_poll_bitmap = client->poll_bitmap;
    battery_service_poll_timer_start(client);
}

static void battery_service_send_next_query(void * context){
    uint16_t cid = (uint16_t)(uintptr_t)context;
    battery_service_client_t * client = battery_service_get_client_for_cid(cid);

    if (client == NULL){
        return;
    }

    uint8_t status;
    uint8_t i;
    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case BATTERY_SERVICE_CLIENT_STATE_CONNECTED:
            for (i = 0; i < client->num_instances; i++){
                if ( ((client->need_poll_bitmap >> i) & 0x01) == 0x01 ){
                    client->state = BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ;
                    // clear bit of polled service
                    client->need_poll_bitmap &= ~(1u << i);
                    client->polled_service_index = i;

#ifdef ENABLE_TESTING_SUPPORT
                    printf("Poll value [%d]\n", i);
#endif
                    // poll value of characteristic
                    characteristic.value_handle = client->services[i].value_handle;
                    characteristic.properties   = client->services[i].properties;
                    characteristic.end_handle   = client->services[i].end_handle;
                    gatt_client_read_value_of_characteristic(&handle_gatt_client_event, client->con_handle, &characteristic);
                    break;
                }
            }
            break;

        case BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:   
            client->state = BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(&handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE);
            // TODO handle status
            break;

        case BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            client->state = BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;

            gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                &handle_gatt_client_event, 
                client->con_handle, 
                client->services[client->service_index].start_handle, 
                client->services[client->service_index].end_handle, 
                ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);

            break;

#ifdef ENABLE_TESTING_SUPPORT
        case BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
            client->state = BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            // if there are services without notification, register pool timer, 
            // othervise register for notifications
            characteristic.value_handle = client->services[client->service_index].value_handle;
            characteristic.properties   = client->services[client->service_index].properties;
            characteristic.end_handle   = client->services[client->service_index].end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, client->con_handle, &characteristic);
            break;

        case BATTERY_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION:
            printf("Read client characteristic value [Service %d, handle 0x%04X]:\n", 
                client->service_index,
                client->services[client->service_index].ccc_handle);

            client->state = BATTERY_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                &handle_gatt_client_event,
                client->con_handle, 
                client->services[client->service_index].ccc_handle);
            break;
#endif
            
        case BATTERY_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            client->state = BATTERY_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            for (; client->service_index < client->num_instances; client->service_index++){
                status = battery_service_registered_notification(client, client->service_index);
                if (status == ERROR_CODE_SUCCESS) return;
            }
            
                
#ifdef ENABLE_TESTING_SUPPORT
            for (client->service_index = 0; client->service_index < client->num_instances; client->service_index++){
                bool need_polling = (client->poll_bitmap & (1 << client->service_index)) != 0;
                if ( (client->services[client->service_index].ccc_handle != 0) && !need_polling ){
                    client->state = BATTERY_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION;
                    break;
                }
            }
#endif
            client->state = BATTERY_SERVICE_CLIENT_STATE_CONNECTED;
            battery_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
            if (battery_service_is_polling_needed(client)){
                battery_service_start_polling(client);
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
    battery_service_client_request_send_gatt_query(client);
}

static void battery_service_poll_timer_start(battery_service_client_t * client){
    btstack_run_loop_set_timer_handler(&client->poll_timer,  battery_service_poll_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&client->poll_timer, (void *)(uintptr_t)client->cid);

    btstack_run_loop_set_timer(&client->poll_timer, client->poll_interval_ms);
    btstack_run_loop_remove_timer(&client->poll_timer);
    btstack_run_loop_add_timer(&client->poll_timer);
}

static void battery_service_client_validate_service(battery_service_client_t * client){
    // remove all services without characteristic (array in-place)
    uint8_t src_index  = 0;  // next entry to check
    uint8_t dest_index = 0;  // to store entry
    for (src_index = 0; src_index < client->num_instances; src_index++){
        if (client->services[src_index].value_handle != 0){
            if (src_index != dest_index) {
                client->services[dest_index] = client->services[src_index];
            } 
            dest_index++;
        }
    }
    client->num_instances = dest_index;
}

// @return true if client valid / run function should be called
static bool battery_service_client_handle_query_complete_for_connection_setup(battery_service_client_t * client, uint8_t status){
    switch (client->state){
        case BATTERY_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                battery_service_emit_connection_established(client, status);
                battery_service_finalize_client(client);
                return false;
            }

            if (client->num_instances == 0){
                battery_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                battery_service_finalize_client(client);
                return false;
            }

            client->service_index = 0;
            client->state = BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;

        case BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                battery_service_emit_connection_established(client, status);
                battery_service_finalize_client(client);
                return false;
            }

            // check if there is another service to query
            if ((client->service_index + 1) < client->num_instances){
                client->service_index++;
                client->state = BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
                break;
            }

            battery_service_client_validate_service(client);

            if (client->num_instances == 0){
                battery_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                battery_service_finalize_client(client);
                return false;
            }

            // we are done with querying all services
            client->service_index = 0;

#ifdef ENABLE_TESTING_SUPPORT
            client->state = BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
#else
            // wait for notification registration
            // to send connection established event
            client->state = BATTERY_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
#endif
            break;

#ifdef ENABLE_TESTING_SUPPORT
            case BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    if ((client->service_index + 1) < client->num_instances){
                        client->service_index++;
                        client->state = BATTERY_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
                        break;
                    }
                    client->service_index = 0;
                    client->state = BATTERY_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                    break;

                case BATTERY_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
                    client->state = BATTERY_SERVICE_CLIENT_STATE_CONNECTED;
                    battery_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
                    if (battery_service_is_polling_needed(client)){
                        battery_service_start_polling(client);
                        return false;
                    }
                    break;
#endif
        case BATTERY_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            if ((client->service_index + 1) < client->num_instances){
                client->service_index++;
                client->state = BATTERY_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;
            }
#ifdef ENABLE_TESTING_SUPPORT
            for (client->service_index = 0; client->service_index < client->num_instances; client->service_index++){
                bool need_polling = (client->poll_bitmap & (1 << client->service_index)) != 0;
                printf("read CCC 1 0x%02x, polling %d \n", client->services[client->service_index].ccc_handle, (int) need_polling);
                if ( (client->services[client->service_index].ccc_handle != 0) && !need_polling ) {
                    client->state = BATTERY_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION;
                    break;
                }
            }
#endif
            client->state = BATTERY_SERVICE_CLIENT_STATE_CONNECTED;
            battery_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
            if (battery_service_is_polling_needed(client)){
                battery_service_start_polling(client);
                return false;
            }
            break;

        default:
            return false;

    }
    return true;
}

static bool battery_service_client_handle_query_complete(battery_service_client_t * client, uint8_t status){
    switch (client->state){

        case BATTERY_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ:
            if (client->polled_service_index != BATTERY_SERVICE_INVALID_INDEX){
                if (status != ATT_ERROR_SUCCESS){
                    battery_service_emit_battery_level(client, client->services[client->polled_service_index].value_handle, status, 0);
                }
                client->polled_service_index = BATTERY_SERVICE_INVALID_INDEX;
            }
            client->state = BATTERY_SERVICE_CLIENT_STATE_CONNECTED;
            return (client->need_poll_bitmap != 0u);

        default:
            break;

    }
    return false;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    battery_service_client_t * client = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    bool send_next_gatt_query = false;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = battery_service_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->num_instances < MAX_NUM_BATTERY_SERVICES){
                gatt_event_service_query_result_get_service(packet, &service);
                client->services[client->num_instances].start_handle = service.start_group_handle;
                client->services[client->num_instances].end_handle = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("Battery Service: start handle 0x%04X, end handle 0x%04X\n", client->services[client->num_instances].start_handle, client->services[client->num_instances].end_handle);
#endif          
                client->num_instances++;
            } else {
                log_info("Found more then %d, Battery Service instances. Increase MAX_NUM_BATTERY_SERVICES to store all.", MAX_NUM_BATTERY_SERVICES);
            }
            // for sending next query w4 GATT_EVENT_QUERY_COMPLETE 
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = battery_service_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            btstack_assert(client->service_index < client->num_instances);
            btstack_assert(characteristic.uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL); 
                
            client->services[client->service_index].value_handle = characteristic.value_handle;
            client->services[client->service_index].properties = characteristic.properties;

#ifdef ENABLE_TESTING_SUPPORT
            printf("Battery Level Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, service %d\n", 
                // hid_characteristic_name(characteristic.uuid16),
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16,
                client->service_index);
#endif               
            // for sending next query w4 GATT_EVENT_QUERY_COMPLETE 
            break;

        case GATT_EVENT_NOTIFICATION:
            if (gatt_event_notification_get_value_length(packet) != 1) break;
            
            client = battery_service_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
            btstack_assert(client != NULL);

            battery_service_emit_battery_level(client, 
                gatt_event_notification_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_notification_get_value(packet)[0]);

            break;
        
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            client = battery_service_get_client_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(client != NULL);                

#ifdef ENABLE_TESTING_SUPPORT
            if (client->state == BATTERY_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
                break;
            }
#endif  
            if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 1) break;
                
            battery_service_emit_battery_level(client, 
                gatt_event_characteristic_value_query_result_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_characteristic_value_query_result_get_value(packet)[0]);
            // reset need_poll_bitmap in GATT_EVENT_QUERY_COMPLETE
            break;

#ifdef ENABLE_TESTING_SUPPORT
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:{
            gatt_client_characteristic_descriptor_t characteristic_descriptor;

            client = battery_service_get_client_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                client->services[client->service_index].ccc_handle = characteristic_descriptor.handle;

                printf("    Battery Level Client Characteristic Configuration Descriptor[%d]:  Handle 0x%04X, UUID 0x%04X\n", 
                    client->service_index,
                    characteristic_descriptor.handle,
                    characteristic_descriptor.uuid16);
            }
            break;
            // for sending next query w4 GATT_EVENT_QUERY_COMPLETE 
        }
#endif

        case GATT_EVENT_QUERY_COMPLETE:
            client = battery_service_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);

            // 1. handle service establishment/notification subscription query results (client->state < BATTERY_SERVICE_CLIENT_STATE_CONNECTED)
            if (client->state < BATTERY_SERVICE_CLIENT_STATE_CONNECTED){
                send_next_gatt_query = battery_service_client_handle_query_complete_for_connection_setup(client, gatt_event_query_complete_get_att_status(packet));
                break;
            }

            // 2. handle battery value query result when devices is connected
            send_next_gatt_query = battery_service_client_handle_query_complete(client, gatt_event_query_complete_get_att_status(packet));
            if (!send_next_gatt_query){
                // if there are no further queries, and we're connected, trigger next polling read
                if (battery_service_is_polling_needed(client)){
                    battery_service_poll_timer_start(client);
                }
            }
            break;

        default:
            break;
    }

    if (send_next_gatt_query){
        battery_service_client_request_send_gatt_query(client);
        return;
    }
}


static void handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); // ok: only hci events
    UNUSED(channel);     // ok: there is no channel
    UNUSED(size);        // ok: fixed format events read from HCI buffer

    hci_con_handle_t con_handle;
    battery_service_client_t * client;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            client = battery_service_get_client_for_con_handle(con_handle);
            if (client != NULL){
                // finalize
                uint16_t cid = client->cid;
                battery_service_finalize_client(client);
                // TODO: emit disconnected event
                UNUSED(cid);
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

    uint8_t status = battery_service_client_request_send_gatt_query(client);
    if (status != ERROR_CODE_SUCCESS){
        client->state = BATTERY_SERVICE_CLIENT_STATE_IDLE;
    }
    return status;
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
    if (client->state < BATTERY_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }
    if (service_index >= client->num_instances) {
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    client->need_poll_bitmap |= (1u << service_index);

    uint8_t status = battery_service_client_request_send_gatt_query(client);
    if (status != ERROR_CODE_SUCCESS){
        client->need_poll_bitmap &= ~(1u << client->polled_service_index);
    }
    return status;
}

void battery_service_client_init(void){
    hci_event_callback_registration.callback = &handle_hci_event;
    hci_add_event_handler(&hci_event_callback_registration);
    battery_service_handle_can_send_now.callback = &battery_service_send_next_query;
}

void battery_service_client_deinit(void){
    battery_service_cid_counter = 0;
    clients = NULL;    
}


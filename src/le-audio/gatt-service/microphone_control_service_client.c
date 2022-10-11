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

#define BTSTACK_FILE__ "microphone_control_service_client.c"

#include "btstack_config.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <string.h>


#include "le-audio/gatt-service/microphone_control_service_client.h"

#include "btstack_memory.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"

static microphone_control_service_client_t mic_client;
static uint16_t mic_service_cid_counter = 0;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t microphone_control_service_get_next_cid(void){
    mic_service_cid_counter = btstack_next_cid_ignoring_zero(mic_service_cid_counter);
    return mic_service_cid_counter;
}


static void microphone_control_service_finalize_client(microphone_control_service_client_t * client){
    client->cid = 0;
    client->con_handle = HCI_CON_HANDLE_INVALID;
    client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_IDLE;
    client->need_polling = false;
}

static microphone_control_service_client_t * microphone_control_service_get_client_for_con_handle(hci_con_handle_t con_handle){
    if (mic_client.con_handle == con_handle){
        return &mic_client;
    }
    return NULL;
}

static microphone_control_service_client_t * microphone_control_service_get_client_for_cid(uint16_t microphone_control_service_cid){
    if (mic_client.cid == microphone_control_service_cid){
        return &mic_client;
    }
    return NULL;
}

static void microphone_control_service_emit_connection_established(microphone_control_service_client_t * client, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_MICS_CONNECTED;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = status;
    (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void microphone_control_service_emit_mute_state(microphone_control_service_client_t * client, uint16_t value_handle, uint8_t att_status, uint8_t mute_state){
    if (value_handle != client->mute_value_handle){
        return;
    }
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_REMOTE_MICS_MUTE;
    little_endian_store_16(event, pos, client->cid);
    pos += 2;
    event[pos++] = att_status;
    event[pos++] = mute_state;
    (*client->client_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static bool microphone_control_service_registered_notification(microphone_control_service_client_t * client){
    gatt_client_characteristic_t characteristic;
    // if there are services without notification, register pool timer, 
    // othervise register for notifications
    characteristic.value_handle = client->mute_value_handle;
    characteristic.properties   = client->properties;
    characteristic.end_handle   = client->end_handle;

    uint8_t status = gatt_client_write_client_characteristic_configuration(
                &handle_gatt_client_event, 
                client->con_handle, 
                &characteristic, 
                GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
  
    // notification supported, register for value updates
    if (status == ERROR_CODE_SUCCESS){
        gatt_client_listen_for_characteristic_value_updates(
            &client->notification_listener, 
            &handle_gatt_client_event, 
            client->con_handle, &characteristic);
    } 
    return status;
}

static void microphone_control_service_run_for_client(microphone_control_service_client_t * client){
    uint8_t status;
    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED:
            if (client->need_polling){
                // clear bit of polled service
                client->need_polling = false;
                
                // poll value of characteristic
                characteristic.value_handle = client->mute_value_handle;
                characteristic.properties   = client->properties;
                characteristic.end_handle   = client->end_handle;
                gatt_client_read_value_of_characteristic(&handle_gatt_client_event, client->con_handle, &characteristic);
                break;
            }
            break;
        
        case MICROPHONE_CONTROL_SERVICE_CLIENT_W2_WRITE_MUTE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("    Write mute [0x%04X, %d]:\n", 
                client->mute_value_handle, client->requested_mute);
#endif

            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_W4_WRITE_MUTE_RESULT;

            // see GATT_EVENT_QUERY_COMPLETE for end of write
            status = gatt_client_write_value_of_characteristic(
                &handle_gatt_client_event, client->con_handle, 
                client->mute_value_handle, 
                1, &client->requested_mute);
            UNUSED(status);

    break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE:   
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT;
            status = gatt_client_discover_primary_services_by_uuid16(&handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_MICROPHONE_CONTROL);
            // TODO handle status
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS:
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;

            gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                &handle_gatt_client_event, 
                client->con_handle, 
                client->start_handle, 
                client->end_handle, 
                ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL);

            break;

#ifdef ENABLE_TESTING_SUPPORT
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS:
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT;
            // if there are services without notification, register pool timer, 
            // othervise register for notifications
            characteristic.value_handle = client->mute_value_handle;
            characteristic.properties   = client->properties;
            characteristic.end_handle   = client->end_handle;

            (void) gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, client->con_handle, &characteristic);
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION:
            printf("Read client characteristic value [handle 0x%04X]:\n", client->ccc_handle);

            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT;

            // result in GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT
            (void) gatt_client_read_characteristic_descriptor_using_descriptor_handle(
                &handle_gatt_client_event,
                client->con_handle, 
                client->ccc_handle);
            break;
#endif
            
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION:
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED;
    
            status = microphone_control_service_registered_notification(client);
            if (status == ERROR_CODE_SUCCESS) return;
            
                
#ifdef ENABLE_TESTING_SUPPORT
            if (client->ccc_handle != 0){
                client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
            
#endif
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED;
            microphone_control_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
            break;
        default:
            break;
    }
}

// @return true if client valid / run function should be called
static bool microphone_control_service_client_handle_query_complete(microphone_control_service_client_t * client, uint8_t status){
    switch (client->state){
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                microphone_control_service_emit_connection_established(client, status);
                microphone_control_service_finalize_client(client);
                return false;
            }

            if (client->num_instances == 0){
                microphone_control_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                microphone_control_service_finalize_client(client);
                return false;
            }

            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS;
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
            if (status != ATT_ERROR_SUCCESS){
                microphone_control_service_emit_connection_established(client, status);
                microphone_control_service_finalize_client(client);
                return false;
            }

            if ((client->num_instances == 0) || (client->mute_value_handle == 0)){
                microphone_control_service_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                microphone_control_service_finalize_client(client);
                return false;
            }

#ifdef ENABLE_TESTING_SUPPORT
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS;
#else
            // wait for notification registration
            // to send connection established event
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
#endif
            break;

#ifdef ENABLE_TESTING_SUPPORT
            case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT:
                client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION;
                break;

            case MICROPHONE_CONTROL_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT:
                client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED;
                microphone_control_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
                break;
#endif
        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED:
            
#ifdef ENABLE_TESTING_SUPPORT
            printf("read CCC 0x%02x, polling %d \n", client->ccc_handle, client->need_polling ? 1 : 0);
            if ( (client->ccc_handle != 0) && !client->need_polling ) {
                client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION;
                break;
            }
#endif
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED;
            microphone_control_service_emit_connection_established(client, ERROR_CODE_SUCCESS);
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED:
            if (status != ATT_ERROR_SUCCESS){
                microphone_control_service_emit_mute_state(client, client->mute_value_handle, status, 0);
            }
            break;

        case MICROPHONE_CONTROL_SERVICE_CLIENT_W4_WRITE_MUTE_RESULT:
            client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED;
            microphone_control_service_emit_mute_state(client, client->mute_value_handle, status, client->requested_mute);
            break;
        default:
            break;

    }
    return true;
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type); 
    UNUSED(channel);
    UNUSED(size);

    microphone_control_service_client_t * client = NULL;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    bool call_run = true;

    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            client = microphone_control_service_get_client_for_con_handle(gatt_event_service_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            if (client->num_instances < 1){
                gatt_event_service_query_result_get_service(packet, &service);
                client->start_handle = service.start_group_handle;
                client->end_handle = service.end_group_handle;

#ifdef ENABLE_TESTING_SUPPORT
                printf("MIC Service: start handle 0x%04X, end handle 0x%04X\n", client->start_handle, client->end_handle);
#endif          
                client->num_instances++;
            } else {
                log_info("Found more then one MIC Service instance. ");
            }
            break;
        
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = microphone_control_service_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);

            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            btstack_assert(characteristic.uuid16 == ORG_BLUETOOTH_CHARACTERISTIC_MUTE); 
                
            client->mute_value_handle = characteristic.value_handle;
            client->properties = characteristic.properties;

#ifdef ENABLE_TESTING_SUPPORT
            printf("Mute Characteristic:\n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X\n", 
                // hid_characteristic_name(characteristic.uuid16),
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16);
#endif               
            break;

        case GATT_EVENT_NOTIFICATION:
            if (gatt_event_notification_get_value_length(packet) != 1) break;
            
            client = microphone_control_service_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
            btstack_assert(client != NULL);

            microphone_control_service_emit_mute_state(client, 
                gatt_event_notification_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_notification_get_value(packet)[0]);
            break;
        
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            client = microphone_control_service_get_client_for_con_handle(gatt_event_characteristic_value_query_result_get_handle(packet));
            btstack_assert(client != NULL);                

#ifdef ENABLE_TESTING_SUPPORT
            if (client->state == MICROPHONE_CONTROL_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT){
                printf("    Received CCC value: ");
                printf_hexdump(gatt_event_characteristic_value_query_result_get_value(packet),  gatt_event_characteristic_value_query_result_get_value_length(packet));
                break;
            }
#endif  
            if (gatt_event_characteristic_value_query_result_get_value_length(packet) != 1) break;
                
            microphone_control_service_emit_mute_state(client, 
                gatt_event_characteristic_value_query_result_get_value_handle(packet), 
                ATT_ERROR_SUCCESS,
                gatt_event_characteristic_value_query_result_get_value(packet)[0]);
            break;

#ifdef ENABLE_TESTING_SUPPORT
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:{
            gatt_client_characteristic_descriptor_t characteristic_descriptor;

            client = microphone_control_service_get_client_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                client->ccc_handle = characteristic_descriptor.handle;

                printf("    MICS Characteristic Configuration Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                    characteristic_descriptor.handle,
                    characteristic_descriptor.uuid16);
            }
            break;
        }
#endif

        case GATT_EVENT_QUERY_COMPLETE:
            client = microphone_control_service_get_client_for_con_handle(gatt_event_query_complete_get_handle(packet));
            btstack_assert(client != NULL);
            call_run = microphone_control_service_client_handle_query_complete(client, gatt_event_query_complete_get_att_status(packet));
            break;

        default:
            break;
    }

    if (call_run && (client != NULL)){
        microphone_control_service_run_for_client(client);
    }
}


uint8_t microphone_control_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint16_t * mics_cid){
    btstack_assert(packet_handler != NULL);
    
    microphone_control_service_client_t * client = microphone_control_service_get_client_for_con_handle(con_handle);
    if (client != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = microphone_control_service_get_next_cid();
    if (mics_cid != NULL) {
        *mics_cid = cid;
    }
    
    client->cid = cid;
    client->con_handle = con_handle;
    client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_IDLE;
    client->client_handler = packet_handler; 
    client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE;
    microphone_control_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t microphone_control_service_client_disconnect(uint16_t mics_cid){
    microphone_control_service_client_t * client = microphone_control_service_get_client_for_cid(mics_cid);
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // finalize connections
    microphone_control_service_finalize_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t microphone_control_service_client_read_mute_state(uint16_t mics_cid){
    microphone_control_service_client_t * client = microphone_control_service_get_client_for_cid(mics_cid);
    if (client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (client->state != MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }
    
    client->need_polling = true;
    microphone_control_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

static uint8_t microphone_control_service_client_mute_write(uint16_t mics_cid, gatt_microphone_control_mute_t mute_status){
    microphone_control_service_client_t * client = microphone_control_service_get_client_for_cid(mics_cid);
    if (client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (client->state != MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED) {
        return GATT_CLIENT_IN_WRONG_STATE;
    }
    client->requested_mute = (uint8_t)mute_status;
    client->state = MICROPHONE_CONTROL_SERVICE_CLIENT_W2_WRITE_MUTE;
    microphone_control_service_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

uint8_t microphone_control_service_client_mute_turn_on(uint16_t mics_cid){
    return microphone_control_service_client_mute_write(mics_cid, GATT_MICROPHONE_CONTROL_MUTE_ON);
}

uint8_t microphone_control_service_client_mute_turn_off(uint16_t mics_cid){
    return microphone_control_service_client_mute_write(mics_cid, GATT_MICROPHONE_CONTROL_MUTE_OFF);
}

void microphone_control_service_client_init(void){}

void microphone_control_service_client_deinit(void){
    mic_service_cid_counter = 0;
    microphone_control_service_finalize_client(&mic_client); 
}


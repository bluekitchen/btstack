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

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

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

#define HID_REPORT_MODE_REPORT_ID               3
#define HID_REPORT_MODE_REPORT_MAP_ID           4
#define HID_REPORT_MODE_HID_INFORMATION_ID      5
#define HID_REPORT_MODE_HID_CONTROL_POINT_ID    6

static btstack_linked_list_t clients;
static uint16_t hids_cid_counter = 0;

static uint8_t * hids_client_descriptor_storage;
static uint16_t  hids_client_descriptor_storage_len;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

#ifdef ENABLE_TESTING_SUPPORT
static char * hid_characteristic_name(uint16_t uuid){
    switch (uuid){
        case ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE:
            return "PROTOCOL_MODE";

        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT:
            return "BOOT_KEYBOARD_INPUT_REPORT";
        
        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT:
            return "BOOT_MOUSE_INPUT_REPORT";
        
        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_OUTPUT_REPORT:
            return "BOOT_KEYBOARD_OUTPUT_REPORT";

        case ORG_BLUETOOTH_CHARACTERISTIC_REPORT:
            return "REPORT";

        case ORG_BLUETOOTH_CHARACTERISTIC_REPORT_MAP:
            return "REPORT_MAP";

        case ORG_BLUETOOTH_CHARACTERISTIC_HID_INFORMATION:
            return "HID_INFORMATION";
        
        case ORG_BLUETOOTH_CHARACTERISTIC_HID_CONTROL_POINT:
            return "HID_CONTROL_POINT";
        default:
            return "UKNOWN";
    }
}
#endif

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


// START Descriptor Storage Util

static uint16_t hids_client_descriptor_storage_get_available_space(void){
    // assumes all descriptors are back to back
    uint16_t free_space = hids_client_descriptor_storage_len;
    uint8_t i;
    
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        hids_client_t * client = (hids_client_t *)btstack_linked_list_iterator_next(&it);
        for (i = 0; i < client->num_instances; i++){
            free_space -= client->services[i].hid_descriptor_len;
        }
    }
    return free_space;
}

static void hids_client_descriptor_storage_init(hids_client_t * client, uint8_t service_index){
    client->services[service_index].hid_descriptor_len = 0;
    client->services[service_index].hid_descriptor_max_len = hids_client_descriptor_storage_get_available_space();
    client->services[service_index].hid_descriptor_offset = hids_client_descriptor_storage_len - client->services[service_index].hid_descriptor_max_len;
}

static bool hids_client_descriptor_storage_store(hids_client_t * client, uint8_t service_index, uint8_t byte){
    if (client->services[service_index].hid_descriptor_len >= client->services[service_index].hid_descriptor_max_len) return false;

    hids_client_descriptor_storage[client->services[service_index].hid_descriptor_offset + client->services[service_index].hid_descriptor_len] = byte;
    client->services[service_index].hid_descriptor_len++;
    return true;
}

static void hids_client_descriptor_storage_delete(hids_client_t * client){
    uint8_t service_index = 0;
    uint16_t next_offset = 0;

    for (service_index = 0; service_index < client->num_instances; service_index++){
        next_offset += client->services[service_index].hid_descriptor_offset + client->services[service_index].hid_descriptor_len;
        client->services[service_index].hid_descriptor_len = 0;
        client->services[service_index].hid_descriptor_offset = 0;
    }

    memmove(&hids_client_descriptor_storage[client->services[0].hid_descriptor_offset], 
            &hids_client_descriptor_storage[next_offset],
            hids_client_descriptor_storage_len - next_offset);
    
    uint8_t i;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        hids_client_t * conn = (hids_client_t *)btstack_linked_list_iterator_next(&it);
        for (i = 0; i < client->num_instances; i++){
            if (conn->services[i].hid_descriptor_offset >= next_offset){
                conn->services[i].hid_descriptor_offset = next_offset;
                next_offset += conn->services[service_index].hid_descriptor_len;
            }
        }
    }
}

const uint8_t * hids_client_descriptor_storage_get_descriptor_data(uint16_t hids_cid, uint8_t service_index){
    hids_client_t * client = hids_get_client_for_cid(hids_cid);
    if (client == NULL){
        return NULL;
    }
    if (service_index >= client->num_instances){
        return NULL;
    }
    return &hids_client_descriptor_storage[client->services[service_index].hid_descriptor_offset];
}

uint16_t hids_client_descriptor_storage_get_descriptor_len(uint16_t hids_cid, uint8_t service_index){
    hids_client_t * client = hids_get_client_for_cid(hids_cid);
    if (client == NULL){
        return 0;
    }
    if (service_index >= client->num_instances){
        return 0;
    }
    return client->services[service_index].hid_descriptor_len;
}

// END Descriptor Storage Util

static uint16_t hids_get_next_cid(void){
    if (hids_cid_counter == 0xffff) {
        hids_cid_counter = 1;
    } else {
        hids_cid_counter++;
    }
    return hids_cid_counter;
}

static uint8_t find_report_index_for_value_handle(hids_client_t * client, uint16_t value_handle){
    uint8_t i;
    for (i = 0; i < client->num_reports; i++){
        if (client->reports[i].value_handle == value_handle){
            return i;
        }
    }
    return HIDS_CLIENT_INVALID_REPORT_INDEX;
}

static uint8_t find_external_report_index_for_value_handle(hids_client_t * client, uint16_t value_handle){
    uint8_t i;
    for (i = 0; i < client->num_external_reports; i++){
        if (client->external_reports[i].value_handle == value_handle){
            return i;
        }
    }
    return HIDS_CLIENT_INVALID_REPORT_INDEX;
}

static bool external_report_index_for_uuid_exists(hids_client_t * client, uint16_t uuid16){
    uint8_t i;
    for (i = 0; i < client->num_external_reports; i++){
        if (client->external_reports[i].external_report_reference_uuid == uuid16){
            return true;
        }
    }
    return false;
}



static uint8_t find_report_index_for_report_id_and_type(hids_client_t * client, uint8_t report_id, hid_report_type_t report_type){
    uint8_t i;
    for (i = 0; i < client->num_reports; i++){
        if ( (client->reports[i].report_id == report_id) && (client->reports[i].report_type == report_type)){
            return i;
        }
    }
    return HIDS_CLIENT_INVALID_REPORT_INDEX;
}

static uint8_t hids_client_add_characteristic(hids_client_t * client, gatt_client_characteristic_t * characteristic, uint8_t report_id, hid_report_type_t report_type, bool boot_report){
    
    uint8_t report_index = find_external_report_index_for_value_handle(client, characteristic->value_handle);
    if (report_index != HIDS_CLIENT_INVALID_REPORT_INDEX){
        return report_index; 
    }
    report_index = client->num_reports;

    if (report_index < HIDS_CLIENT_NUM_REPORTS) {
        client->reports[report_index].value_handle = characteristic->value_handle;
        client->reports[report_index].end_handle = characteristic->end_handle;
        client->reports[report_index].properties = characteristic->properties;

        client->reports[report_index].service_index = client->service_index;
        client->reports[report_index].report_id = report_id; 
        client->reports[report_index].report_type = report_type; 
        client->reports[report_index].boot_report = boot_report; 

        log_info("add index %d, id %d, type %d, value handle 0x%02x, properties 0x%02x", report_index, report_id, report_type, characteristic->value_handle, characteristic->properties);
        client->num_reports++;
        return report_index;
    } else {
        log_info("not enough storage, increase HIDS_CLIENT_NUM_REPORTS");
        return HIDS_CLIENT_INVALID_REPORT_INDEX;
    }
}

static uint8_t hids_client_add_external_report(hids_client_t * client, gatt_client_characteristic_descriptor_t * characteristic_descriptor){
    uint8_t report_index = client->num_external_reports;

    if (report_index < HIDS_CLIENT_NUM_REPORTS) {
        client->external_reports[report_index].value_handle = characteristic_descriptor->handle;
        client->external_reports[report_index].service_index = client->service_index;

        client->num_external_reports++;
        log_info("add external index %d [%d], value handle 0x%02x", report_index, client->num_external_reports, characteristic_descriptor->handle);
        return report_index;
    } else {
        log_info("not enough storage, increase HIDS_CLIENT_NUM_REPORTS");
        return HIDS_CLIENT_INVALID_REPORT_INDEX;
    }
}


static bool hid_clients_has_reports_in_report_mode(hids_client_t * client){
    uint8_t i;
    for (i = 0; i < client->num_reports; i++){
        if (!client->reports[i].boot_report){
            return true;
        }
    }
    return false;
}

static bool hid_clients_has_reports_in_boot_mode(hids_client_t * client){
    uint8_t i;
    for (i = 0; i < client->num_reports; i++){
        if (client->reports[i].boot_report){
            return true;
        }
    }
    return false;
}

static uint8_t hids_client_get_next_active_report_map_index(hids_client_t * client){
    uint8_t i;    
    for (i = client->service_index; i < client->num_instances; i++){
        if (client->services[i].report_map_value_handle != 0){
            return i;
        }
    }
    client->service_index = HIDS_CLIENT_INVALID_REPORT_INDEX;
    return HIDS_CLIENT_INVALID_REPORT_INDEX;
}

static bool hids_client_report_query_next_report_map(hids_client_t * client){
    client->service_index++;
    if (hids_client_get_next_active_report_map_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_READ_REPORT_MAP_HID_DESCRIPTOR;
        return true;
    }
    return false;
}

static bool hids_client_report_map_query_init(hids_client_t * client){
    client->service_index = 0;

    if (hids_client_get_next_active_report_map_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_READ_REPORT_MAP_HID_DESCRIPTOR;
        return true;
    }
    return false;
}

static bool hids_client_report_query_next_report_map_uuid(hids_client_t * client){
    client->report_index++;
    if (client->report_index < client->num_external_reports){
        client->state = HIDS_CLIENT_STATE_W2_REPORT_MAP_READ_EXTERNAL_REPORT_REFERENCE_UUID;
        return true;
    }
    return false;
}

static bool hids_client_report_map_uuid_query_init(hids_client_t * client){
    client->report_index = 0;
    if (client->num_external_reports > 0){
        client->state = HIDS_CLIENT_STATE_W2_REPORT_MAP_READ_EXTERNAL_REPORT_REFERENCE_UUID;
        return true;    
    }
    return false;
}

static uint8_t hids_client_get_next_report_index(hids_client_t * client){
    uint8_t i;    
    uint8_t index = HIDS_CLIENT_INVALID_REPORT_INDEX;
    switch (client->protocol_mode){
        case HID_PROTOCOL_MODE_REPORT:
            for (i = client->report_index; i < client->num_reports && (index == HIDS_CLIENT_INVALID_REPORT_INDEX); i++){
                hids_client_report_t report = client->reports[i];
                if (report.report_type == HID_REPORT_TYPE_RESERVED && report.report_id == HID_REPORT_MODE_REPORT_ID){
                    index = i;
                    client->service_index = report.service_index;
                }
            }
            break;
        case HID_PROTOCOL_MODE_BOOT:
            for (i = client->report_index; i < client->num_reports && (index == HIDS_CLIENT_INVALID_REPORT_INDEX); i++){
                hids_client_report_t report = client->reports[i];
                if (report.boot_report){
                    index = i;
                    client->service_index = report.service_index;
                }
            }
            break;
        default:
            break;
    }

    client->report_index = index;
    return index;
}

static bool hids_client_report_query_next_report(hids_client_t * client){
    client->report_index++;
    if (hids_client_get_next_report_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_FIND_REPORT;
        return true;
    }
    return false;
}

static bool hids_client_report_query_init(hids_client_t * client){
    client->report_index = 0;

    if (hids_client_get_next_report_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_FIND_REPORT;
        return true;
    }
    return false;
}

static uint8_t hids_client_get_next_notification_report_index(hids_client_t * client){
    uint8_t i;    
    uint8_t index = HIDS_CLIENT_INVALID_REPORT_INDEX;

    switch (client->protocol_mode){
        case HID_PROTOCOL_MODE_REPORT:
            for (i = client->report_index; i < client->num_reports && (index == HIDS_CLIENT_INVALID_REPORT_INDEX); i++){
                hids_client_report_t report = client->reports[i];
                if (report.report_type != HID_REPORT_TYPE_INPUT){
                    continue;
                }
                if (!report.boot_report){
                    index = i;
                }
            }    
            break;
        
        case HID_PROTOCOL_MODE_BOOT:
            for (i = client->report_index; i < client->num_reports && (index == HIDS_CLIENT_INVALID_REPORT_INDEX); i++){
                hids_client_report_t report = client->reports[i];
                if (report.report_type != HID_REPORT_TYPE_INPUT){
                    continue;
                }
                if (report.boot_report){
                    index = i;
                }
            }    
            break;

        default:
            break;
    }
    
    client->report_index = index;
    return index;
}

static bool hids_client_report_next_notification_report_index(hids_client_t * client){
    client->report_index++;
    if (hids_client_get_next_notification_report_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_ENABLE_INPUT_REPORTS;
        return true;
    }
    return false;
}

static bool hids_client_report_notifications_init(hids_client_t * client){
    client->report_index = 0;

    if (hids_client_get_next_notification_report_index(client) != HIDS_CLIENT_INVALID_REPORT_INDEX){
        client->state = HIDS_CLIENT_STATE_W2_ENABLE_INPUT_REPORTS;
        return true;
    }
    return false;
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
    // stop listening
    uint8_t i;
    for (i = 0; i < client->num_reports; i++){
        gatt_client_stop_listening_for_characteristic_value_updates(&client->reports[i].notification_listener);
    }

    hids_client_descriptor_storage_delete(client);
    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    btstack_memory_hids_client_free(client); 
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

static void hids_client_setup_report_event(hids_client_t * client, uint8_t report_index, uint8_t *buffer, uint16_t report_len){
    uint16_t pos = 0;
    buffer[pos++] = HCI_EVENT_GATTSERVICE_META;
    pos++;  // skip len
    buffer[pos++] = GATTSERVICE_SUBEVENT_HID_REPORT;
    little_endian_store_16(buffer, pos, client->cid);
    pos += 2;
    buffer[pos++] = client->reports[report_index].service_index;
    buffer[pos++] = client->reports[report_index].report_id;
    little_endian_store_16(buffer, pos, report_len + 1);
    pos += 2;
    buffer[pos++] = client->reports[report_index].report_id;
    buffer[1] = pos + report_len + 1 - 2;
}

static void handle_report_hid_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION) return;

    hids_client_t * client = hids_get_client_for_con_handle(gatt_event_notification_get_handle(packet));
    btstack_assert(client != NULL);

    uint8_t report_index = find_report_index_for_value_handle(client, gatt_event_notification_get_value_handle(packet));
    if (report_index == HIDS_CLIENT_INVALID_REPORT_INDEX){
        return;
    }

    uint8_t * in_place_event = &packet[-1];
    hids_client_setup_report_event(client, report_index, in_place_event, gatt_event_notification_get_value_length(packet));

    (*client->client_handler)(HCI_EVENT_GATTSERVICE_META, client->cid, in_place_event, size);
}


static void hids_run_for_client(hids_client_t * client){
    uint8_t att_status;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;

    switch (client->state){
        case HIDS_CLIENT_STATE_W2_QUERY_SERVICE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\n\nQuery Services:\n");
#endif
            client->state = HIDS_CLIENT_STATE_W4_SERVICE_RESULT;
            
            // result in GATT_EVENT_SERVICE_QUERY_RESULT
            att_status = gatt_client_discover_primary_services_by_uuid16(handle_gatt_client_event, client->con_handle, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
            UNUSED(att_status);
            break;
        
        case HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\n\nQuery Characteristics of service %d:\n", client->service_index);
#endif
            client->state = HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = client->services[client->service_index].start_handle;
            service.end_group_handle = client->services[client->service_index].end_handle;

            // result in GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
            att_status = gatt_client_discover_characteristics_for_service(&handle_gatt_client_event, client->con_handle, &service);
            
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_SET_BOOT_PROTOCOL_MODE:
            att_status = gatt_client_write_value_of_characteristic_without_response(client->con_handle, client->protocol_mode_value_handle, 1, (uint8_t *)&client->required_protocol_mode);
            UNUSED(att_status);
            
            client->protocol_mode = client->required_protocol_mode;
            if (hids_client_report_query_init(client)){
                break;
            }

            hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
            hids_finalize_client(client);
            break;
        
        case HIDS_CLIENT_W2_SEND_REPORT:{
            client->state = HIDS_CLIENT_STATE_CONNECTED;
            
            att_status = gatt_client_write_value_of_characteristic_without_response(client->con_handle, 
                client->reports[client->report_index].value_handle, 
                client->report_len, (uint8_t *)client->report);
            UNUSED(att_status);
            break;
        }

        case HIDS_CLIENT_STATE_W2_READ_REPORT_MAP_HID_DESCRIPTOR:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\n\nRead REPORT_MAP (Handle 0x%04X) HID Descriptor of service %d:\n", client->services[client->service_index].report_map_value_handle, client->service_index);
#endif
            client->state = HIDS_CLIENT_STATE_W4_REPORT_MAP_HID_DESCRIPTOR;

            // result in GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT
            att_status = gatt_client_read_long_value_of_characteristic_using_value_handle(&handle_gatt_client_event, client->con_handle, client->services[client->service_index].report_map_value_handle);
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_REPORT_MAP_DISCOVER_CHARACTERISTIC_DESCRIPTORS:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\nDiscover REPORT_MAP (Handle 0x%04X) Characteristic Descriptors of service %d:\n", client->services[client->service_index].report_map_value_handle, client->service_index);
#endif
            client->state = HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTORS_RESULT;

            characteristic.value_handle = client->services[client->service_index].report_map_value_handle;
            characteristic.end_handle = client->services[client->service_index].report_map_end_handle;

            // result in GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT
            att_status = gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, client->con_handle, &characteristic);
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_REPORT_MAP_READ_EXTERNAL_REPORT_REFERENCE_UUID:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\nRead external chr UUID (Handle 0x%04X) Characteristic Descriptors, service index %d:\n", client->external_reports[client->report_index].value_handle, client->service_index);
#endif       
            client->state = HIDS_CLIENT_STATE_W4_REPORT_MAP_EXTERNAL_REPORT_REFERENCE_UUID;
            
            // result in GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT
            att_status = gatt_client_read_characteristic_descriptor_using_descriptor_handle(&handle_gatt_client_event, client->con_handle, client->external_reports[client->report_index].value_handle);  
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_DISCOVER_EXTERNAL_REPORT_CHARACTERISTIC:
 #ifdef ENABLE_TESTING_SUPPORT
            printf("\nDiscover External Report Characteristic:\n");
#endif       
            client->state = HIDS_CLIENT_STATE_W4_EXTERNAL_REPORT_CHARACTERISTIC_RESULT;
            
            service.start_group_handle = 0x0001;
            service.end_group_handle = 0xffff;

            // Result in GATT_EVENT_CHARACTERISTIC_QUERY_RESULT
            att_status = gatt_client_discover_characteristics_for_service(&handle_gatt_client_event, client->con_handle, &service);
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_FIND_REPORT:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\nQuery Report (Handle 0x%04X, index %d) Characteristic Descriptors of service %d:\n", 
                client->reports[client->report_index].value_handle,
                client->report_index,
                client->reports[client->report_index].service_index);
#endif
            client->state = HIDS_CLIENT_STATE_W4_REPORT_FOUND;
            client->descriptor_handle = 0;
            
            characteristic.value_handle = client->reports[client->report_index].value_handle;
            characteristic.end_handle = client->reports[client->report_index].end_handle;
            characteristic.properties = client->reports[client->report_index].properties;

            // result in GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT
            att_status = gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, client->con_handle, &characteristic);
            UNUSED(att_status);
            break;

        case HIDS_CLIENT_STATE_W2_READ_REPORT_ID_AND_TYPE:
#ifdef ENABLE_TESTING_SUPPORT
            printf("\nQuery Report ID and Type (Handle 0x%04X, index %d) Characteristic Descriptors of service %d:\n", 
                client->reports[client->report_index].value_handle,
                client->report_index,
                client->reports[client->report_index].service_index);
#endif            
            client->state = HIDS_CLIENT_STATE_W4_REPORT_ID_AND_TYPE;

            // result in GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT
            att_status = gatt_client_read_characteristic_descriptor_using_descriptor_handle(&handle_gatt_client_event, client->con_handle, client->descriptor_handle);  
            client->descriptor_handle = 0;  
            UNUSED(att_status);
            break;
        
        case HIDS_CLIENT_STATE_W2_ENABLE_INPUT_REPORTS:
            client->state = HIDS_CLIENT_STATE_W4_INPUT_REPORTS_ENABLED;

            characteristic.value_handle = client->reports[client->report_index].value_handle;
            characteristic.end_handle = client->reports[client->report_index].end_handle;
            characteristic.properties = client->reports[client->report_index].properties;
            
            // end of write marked in GATT_EVENT_QUERY_COMPLETE
            att_status = gatt_client_write_client_characteristic_configuration(&handle_gatt_client_event, client->con_handle, &characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
            
            if (att_status != ERROR_CODE_SUCCESS){
                if (hids_client_report_next_notification_report_index(client)){
                    hids_run_for_client(client);
                    break;
                }
                client->state = HIDS_CLIENT_STATE_CONNECTED;
                hids_emit_connection_established(client, ERROR_CODE_SUCCESS);
            } else {
                gatt_client_listen_for_characteristic_value_updates(
                    &client->reports[client->report_index].notification_listener, 
                    &handle_report_hid_event, client->con_handle, &characteristic);
                
                client->state = HIDS_CLIENT_STATE_CONNECTED;
                hids_emit_connection_established(client, ERROR_CODE_SUCCESS);
            }
            UNUSED(att_status);
            break;
            
        default:
            break;
    }
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);     
    UNUSED(size);

    hids_client_t * client = NULL;
    uint8_t att_status;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t characteristic_descriptor;

    // hids_client_report_t * boot_keyboard_report;
    // hids_client_report_t * boot_mouse_report;
    const uint8_t * characteristic_descriptor_value;
    uint8_t i;
    uint8_t report_index;

    const uint8_t * descriptor_value;
    uint16_t descriptor_value_len;

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
                uint8_t index = client->num_instances;
                gatt_event_service_query_result_get_service(packet, &service);
                client->services[index].start_handle = service.start_group_handle;
                client->services[index].end_handle = service.end_group_handle;
                client->num_instances++;

#ifdef ENABLE_TESTING_SUPPORT
                printf("HID Service: start handle 0x%04X, end handle 0x%04X\n", client->services[index].start_handle, client->services[index].end_handle);
#endif
                hids_client_descriptor_storage_init(client, index);
            }  else {
                log_info("%d hid services found, only first %d can be stored, increase MAX_NUM_HID_SERVICES", client->num_instances + 1, MAX_NUM_HID_SERVICES);
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            client = hids_get_client_for_con_handle(gatt_event_characteristic_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
            
            report_index = HIDS_CLIENT_INVALID_REPORT_INDEX;
            if (client->state == HIDS_CLIENT_STATE_W4_EXTERNAL_REPORT_CHARACTERISTIC_RESULT){
                if (!external_report_index_for_uuid_exists(client, characteristic.uuid16)){
                    break;
                }
            }

            switch (characteristic.uuid16){
                case ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE:
                    client->protocol_mode_value_handle = characteristic.value_handle;
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT:
                    report_index = hids_client_add_characteristic(client, &characteristic, HID_BOOT_MODE_KEYBOARD_ID, HID_REPORT_TYPE_INPUT, true);
                    break;
                
                case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT:
                    report_index = hids_client_add_characteristic(client, &characteristic, HID_BOOT_MODE_MOUSE_ID, HID_REPORT_TYPE_INPUT, true);
                    break;
                
                case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_OUTPUT_REPORT:
                    report_index = hids_client_add_characteristic(client, &characteristic, HID_BOOT_MODE_KEYBOARD_ID, HID_REPORT_TYPE_OUTPUT, false);
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_REPORT:
                    report_index = hids_client_add_characteristic(client, &characteristic, HID_REPORT_MODE_REPORT_ID, HID_REPORT_TYPE_RESERVED, false);
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_REPORT_MAP:                        
                    client->services[client->service_index].report_map_value_handle = characteristic.value_handle;
                    client->services[client->service_index].report_map_end_handle = characteristic.end_handle;
                    break;

                case ORG_BLUETOOTH_CHARACTERISTIC_HID_INFORMATION:
                    break;
                
                case ORG_BLUETOOTH_CHARACTERISTIC_HID_CONTROL_POINT:
                    break;

                default:
#ifdef ENABLE_TESTING_SUPPORT
                    printf("    TODO: Found external characteristic 0x%04X\n", characteristic.uuid16);
#endif
                    return;
            }

#ifdef ENABLE_TESTING_SUPPORT
            printf("HID Characteristic %s:  \n    Attribute Handle 0x%04X, Properties 0x%02X, Handle 0x%04X, UUID 0x%04X, service %d", 
                hid_characteristic_name(characteristic.uuid16),
                characteristic.start_handle, 
                characteristic.properties, 
                characteristic.value_handle, characteristic.uuid16,
                client->service_index);

            if (report_index != HIDS_CLIENT_INVALID_REPORT_INDEX){
                printf(", report index 0x%02X", report_index);
            } 
            printf("\n");
#endif
            break;

        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
            // Map Report characteristic value == HID Descriptor
            client = hids_get_client_for_con_handle(gatt_event_long_characteristic_value_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            
            descriptor_value = gatt_event_long_characteristic_value_query_result_get_value(packet);
            descriptor_value_len = gatt_event_long_characteristic_value_query_result_get_value_length(packet);

#ifdef ENABLE_TESTING_SUPPORT
            // printf("Report Map HID Desc [%d] for service %d\n", descriptor_len, client->service_index);
            printf_hexdump(descriptor_value, descriptor_value_len);
#endif
            for (i = 0; i < descriptor_value_len; i++){
                bool stored = hids_client_descriptor_storage_store(client, client->service_index, descriptor_value[i]);
                if (!stored){
                    client->services[client->service_index].hid_descriptor_status = ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
                    break;
                }
            }
            break;

        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            client = hids_get_client_for_con_handle(gatt_event_all_characteristic_descriptors_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &characteristic_descriptor);
            
            switch (client->state) {
                case HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    // setup for descriptor value query
                    if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_EXTERNAL_REPORT_REFERENCE){
                        report_index = hids_client_add_external_report(client, &characteristic_descriptor);
                        
#ifdef ENABLE_TESTING_SUPPORT
                        if (report_index != HIDS_CLIENT_INVALID_REPORT_INDEX){
                            printf("    External Report Reference Characteristic Descriptor: Handle 0x%04X, UUID 0x%04X, service %d, report index 0x%02X\n", 
                                characteristic_descriptor.handle,
                                characteristic_descriptor.uuid16,
                                client->service_index, report_index);
                        }
#endif
                    }
                    break;
                case HIDS_CLIENT_STATE_W4_REPORT_FOUND:
                    // setup for descriptor value query
                    if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_REPORT_REFERENCE){
                        client->descriptor_handle = characteristic_descriptor.handle;
#ifdef ENABLE_TESTING_SUPPORT
                        printf("    Report Characteristic Report Reference Characteristic Descriptor:  Handle 0x%04X, UUID 0x%04X\n", 
                            characteristic_descriptor.handle,
                            characteristic_descriptor.uuid16);
#endif 
                    }
                    
#ifdef ENABLE_TESTING_SUPPORT
                    if (characteristic_descriptor.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION){
                        printf("    Report Client Characteristic Configuration Descriptor: Handle 0x%04X, UUID 0x%04X\n", 
                            characteristic_descriptor.handle,
                            characteristic_descriptor.uuid16);
                    }
#endif
                    break;

                default:
                    break;
            }
            break;

        case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
            client = hids_get_client_for_con_handle(gatt_event_characteristic_descriptor_query_result_get_handle(packet));
            btstack_assert(client != NULL);
            
            if (gatt_event_characteristic_descriptor_query_result_get_descriptor_length(packet) != 2){
                break;
            }

            characteristic_descriptor_value = gatt_event_characteristic_descriptor_query_result_get_descriptor(packet);
            switch (client->state) {
                case HIDS_CLIENT_STATE_W4_REPORT_MAP_EXTERNAL_REPORT_REFERENCE_UUID:
                    // get external report characteristic uuid 
                    report_index = find_external_report_index_for_value_handle(client, gatt_event_characteristic_descriptor_query_result_get_descriptor_handle(packet));
                    if (report_index != HIDS_CLIENT_INVALID_REPORT_INDEX){
                        client->external_reports[report_index].external_report_reference_uuid = little_endian_read_16(characteristic_descriptor_value, 0);
#ifdef ENABLE_TESTING_SUPPORT         
                        printf("    Update external_report_reference_uuid of report index 0x%02X, service index 0x%02X, UUID 0x%02X\n", 
                            report_index, client->service_index, client->external_reports[report_index].external_report_reference_uuid);
#endif               
                    }
                    break;

                case HIDS_CLIENT_STATE_W4_REPORT_ID_AND_TYPE:
                    
                    if (client->report_index != HIDS_CLIENT_INVALID_REPORT_INDEX){
                        client->reports[client->report_index].report_id = characteristic_descriptor_value[0];
                        client->reports[client->report_index].report_type = (hid_report_type_t)characteristic_descriptor_value[1];
    #ifdef ENABLE_TESTING_SUPPORT         
                        printf("    Update report ID and type [%d, %d] of report index 0x%02X, service index 0x%02X, UUID 0x%02X\n", 
                            client->reports[client->report_index].report_id,
                            client->reports[client->report_index].report_type, 
                            client->report_index, client->service_index, 
                            client->external_reports[client->report_index].external_report_reference_uuid);
    #endif
                    }
                    break;
                
                default:
                    printf("GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT 3");
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

                    client->service_index = 0;
                    client->state = HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC;
                    break;
                
                case HIDS_CLIENT_STATE_W4_CHARACTERISTIC_RESULT:
                    if (att_status != ATT_ERROR_SUCCESS){
                        hids_emit_connection_established(client, att_status);  
                        hids_finalize_client(client);
                        break;  
                    }
                    
                    if ((client->service_index + 1) < client->num_instances){
                        // discover characteristics of next service
                        client->service_index++;
                        client->state = HIDS_CLIENT_STATE_W2_QUERY_CHARACTERISTIC;
                        break;
                    }

                    switch (client->required_protocol_mode){
                        case HID_PROTOCOL_MODE_REPORT:
                            client->protocol_mode = HID_PROTOCOL_MODE_REPORT;
                            if (hid_clients_has_reports_in_report_mode(client)){
                                client->protocol_mode = HID_PROTOCOL_MODE_REPORT;
                                break;
                            }
                            hids_emit_connection_established(client, att_status);  
                            hids_finalize_client(client);
                            return;

                        case HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT:
                            if (hid_clients_has_reports_in_report_mode(client)){
                                client->protocol_mode = HID_PROTOCOL_MODE_REPORT;
                                break;
                            }
                            if (hid_clients_has_reports_in_boot_mode(client)){
                                if (client->protocol_mode_value_handle != 0){
                                    client->state = HIDS_CLIENT_STATE_W2_SET_BOOT_PROTOCOL_MODE;
                                    break;
                                }
                                hids_emit_connection_established(client, att_status);  
                                hids_finalize_client(client);
                                return;
                            }
                            break;
                        default:
                            if (hid_clients_has_reports_in_boot_mode(client)){
                                if (client->protocol_mode_value_handle != 0){
                                    client->state = HIDS_CLIENT_STATE_W2_SET_BOOT_PROTOCOL_MODE;
                                    break;
                                }
                                hids_emit_connection_established(client, att_status);  
                                hids_finalize_client(client);
                                return;
                            }
                            break;
                    }
                    
                    if (client->state == HIDS_CLIENT_STATE_W2_SET_BOOT_PROTOCOL_MODE){
                        break;
                    }

                    // 1. we need to get HID Descriptor and 
                    // 2. get external Report characteristics if referenced from Report Map
                    if (hids_client_report_map_query_init(client)){
                        break;
                    }
                    hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
                    hids_finalize_client(client);
                    break;


                // HID descriptor found
                case HIDS_CLIENT_STATE_W4_REPORT_MAP_HID_DESCRIPTOR:
                    if (att_status != ATT_ERROR_SUCCESS){
                        hids_emit_connection_established(client, att_status);  
                        hids_finalize_client(client);
                        break;  
                    }
                    client->state = HIDS_CLIENT_STATE_W2_REPORT_MAP_DISCOVER_CHARACTERISTIC_DESCRIPTORS;
                    break;

                // found all descriptors, check if there is one with EXTERNAL_REPORT_REFERENCE
                case HIDS_CLIENT_STATE_W4_REPORT_MAP_CHARACTERISTIC_DESCRIPTORS_RESULT:
                    // go for next report map
                    if (hids_client_report_query_next_report_map(client)){
                        break;
                    }
                    
                    // read UUIDS for external characteristics
                    if (hids_client_report_map_uuid_query_init(client)){
                        break;
                    }   

                    // discover characteristic descriptor for all Report characteristics,
                    // then read value of characteristic descriptor to get Report ID
                    if (hids_client_report_query_init(client)){
                        break;
                    }

                    hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
                    hids_finalize_client(client);
                    break;

                case HIDS_CLIENT_STATE_W4_REPORT_MAP_EXTERNAL_REPORT_REFERENCE_UUID:
                    // go for next map report
                    if (hids_client_report_query_next_report_map_uuid(client)){
                        break;
                    }

                    // update external characteristics with correct value handle and end handle 
                    client->state = HIDS_CLIENT_STATE_W2_DISCOVER_EXTERNAL_REPORT_CHARACTERISTIC;
                    break;

                case HIDS_CLIENT_STATE_W4_EXTERNAL_REPORT_CHARACTERISTIC_RESULT:
                    // discover characteristic descriptor for all Report characteristics,
                    // then read value of characteristic descriptor to get Report ID
                    if (hids_client_report_query_init(client)){
                        break;
                    }

                    hids_emit_connection_established(client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE); 
                    hids_finalize_client(client);
                    break;

                case HIDS_CLIENT_STATE_W4_REPORT_FOUND:
                    if (client->descriptor_handle != 0){
                        printf("descriptor found\n");
                        client->state = HIDS_CLIENT_STATE_W2_READ_REPORT_ID_AND_TYPE;
                        break;
                    }

                    // go for next report
                    if (hids_client_report_query_next_report(client)){
                        break;
                    }

                    hids_emit_connection_established(client, ERROR_CODE_SUCCESS);
                    break;

                case HIDS_CLIENT_STATE_W4_REPORT_ID_AND_TYPE:
                    // go for next report
                    if (hids_client_report_query_next_report(client)){
                        break;
                    }

                    if (hids_client_report_notifications_init(client)){
                        break;
                    }
                    hids_emit_connection_established(client, ERROR_CODE_SUCCESS);
                    break;

                case HIDS_CLIENT_STATE_W4_INPUT_REPORTS_ENABLED:
                    if (hids_client_report_next_notification_report_index(client)){
                        break;
                    }
                    hids_emit_connection_established(client, ERROR_CODE_SUCCESS);
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

uint8_t hids_client_send_report(uint16_t hids_cid, uint8_t report_id, const uint8_t * report, uint8_t report_len){
    hids_client_t * client = hids_get_client_for_cid(hids_cid);
    if (client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (client->state != HIDS_CLIENT_STATE_CONNECTED) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    uint8_t report_index = find_report_index_for_report_id_and_type(client, report_id, HID_REPORT_TYPE_OUTPUT);
    if (report_index == HIDS_CLIENT_INVALID_REPORT_INDEX){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    uint16_t mtu;
    uint8_t status = gatt_client_get_mtu(client->con_handle, &mtu);

    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (mtu - 2 < report_len){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }

    client->state = HIDS_CLIENT_W2_SEND_REPORT;
    client->report_index = report_index;
    client->report = report;
    client->report_len = report_len;

    hids_run_for_client(client);
    return ERROR_CODE_SUCCESS;
}

void hids_client_init(uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len){
    hids_client_descriptor_storage = hid_descriptor_storage;
    hids_client_descriptor_storage_len = hid_descriptor_storage_len;
}

void hids_client_deinit(void){}
